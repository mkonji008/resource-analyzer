#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <dirent.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <iterator>

struct ServiceInfo {
    std::string name;
    std::string pid;
    float cpuUsage;
    float memoryUsage;
};

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    return file.is_open() ? std::string((std::istreambuf_iterator<char>(file)),
                                         std::istreambuf_iterator<char>()) : "";
}

float getCpuUsage(int pid) {
    std::string line = readFile("/proc/" + std::to_string(pid) + "/stat");
    if (!line.empty()) {
        std::istringstream iss(line);
        std::vector<std::string> tokens((std::istream_iterator<std::string>(iss)),
                                         std::istream_iterator<std::string>());
        if (tokens.size() > 14) {
            return std::stof(tokens[13]) + std::stof(tokens[14]);
        }
    }
    return 0.0f;
}

float getMemoryUsage(int pid) {
    std::string line = readFile("/proc/" + std::to_string(pid) + "/status");
    std::istringstream iss(line);
    std::string token;
    while (getline(iss, line)) {
        if (line.find("VmSize:") == 0) {
            std::istringstream memoryStream(line);
            std::string memoryValue;
            memoryStream >> token >> memoryValue;
            return std::stof(memoryValue) / 1024;
        }
    }
    return 0.0f;
}

std::string getServiceName(int pid) {
    return readFile("/proc/" + std::to_string(pid) + "/comm");
}

std::vector<ServiceInfo> getRunningServices() {
    std::vector<ServiceInfo> services;
    DIR* dir = opendir("/proc");
    if (!dir) {
        std::cerr << "Failed to open /proc directory." << std::endl;
        return services;
    }

    while (struct dirent* ent = readdir(dir)) {
        if (ent->d_type == DT_DIR && std::all_of(ent->d_name, ent->d_name + strlen(ent->d_name), ::isdigit)) {
            int pid = std::stoi(ent->d_name);
            ServiceInfo info;
            info.pid = ent->d_name;
            info.name = getServiceName(pid);
            info.cpuUsage = getCpuUsage(pid);
            info.memoryUsage = getMemoryUsage(pid);
            if (info.cpuUsage > 0 || info.memoryUsage > 0) {
                services.push_back(info);
            }
        }
    }
    closedir(dir);
    return services;
}

std::vector<ServiceInfo> getTopServices(std::vector<ServiceInfo>& services, bool sortByCpu, size_t count) {
    if (sortByCpu) {
        std::sort(services.begin(), services.end(), [](const ServiceInfo& a, const ServiceInfo& b) {
            return a.cpuUsage > b.cpuUsage;
        });
    } else {
        std::sort(services.begin(), services.end(), [](const ServiceInfo& a, const ServiceInfo& b) {
            return a.memoryUsage > b.memoryUsage;
        });
    }
    return {services.begin(), services.begin() + std::min(count, services.size())};
}

void averageTopServices(std::vector<ServiceInfo>& topServices, std::vector<ServiceInfo>& averagedServices) {
    for (const auto& service : topServices) {
        auto it = std::find_if(averagedServices.begin(), averagedServices.end(),
            [&](const ServiceInfo& avgService) { return avgService.pid == service.pid; });

        if (it != averagedServices.end()) {
            it->cpuUsage += service.cpuUsage;
            it->memoryUsage += service.memoryUsage;
        } else {
            ServiceInfo avgService = service;
            avgService.cpuUsage = service.cpuUsage;
            avgService.memoryUsage = service.memoryUsage;
            averagedServices.push_back(avgService);
        }
    }
}

void outputToJson(const std::vector<ServiceInfo>& topCpuServices, const std::vector<ServiceInfo>& topMemServices) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* ptm = std::localtime(&now_c);
    char filename[64];
    std::strftime(filename, sizeof(filename), "top5_resources_output_%Y-%m-%d_%H-%M-%S.json", ptm);

    std::ofstream jsonFile(filename);
    if (!jsonFile.is_open()) {
        std::cerr << "Could not open file for writing." << std::endl;
        return;
    }

    jsonFile << std::setw(4) << "{\n";
    jsonFile << "  \"Top_CPU_Usage\": [\n";
    for (size_t i = 0; i < topCpuServices.size(); ++i) {
        jsonFile << "    {\n";
        jsonFile << "      \"PID\": \"" << topCpuServices[i].pid << "\",\n";
        jsonFile << "      \"Service_Name\": \"" << topCpuServices[i].name << "\",\n";
        jsonFile << "      \"CPU_Usage\": " << topCpuServices[i].cpuUsage << ",\n";
        jsonFile << "      \"Memory_Usage\": " << topCpuServices[i].memoryUsage << "\n";
        jsonFile << "    }" << (i < topCpuServices.size() - 1 ? "," : "") << "\n";
    }
    jsonFile << "  ],\n";
    jsonFile << "  \"Top_Memory_Usage\": [\n";
    for (size_t i = 0; i < topMemServices.size(); ++i) {
        jsonFile << "    {\n";
        jsonFile << "      \"PID\": \"" << topMemServices[i].pid << "\",\n";
        jsonFile << "      \"Service_Name\": \"" << topMemServices[i].name << "\",\n";
        jsonFile << "      \"CPU_Usage\": " << topMemServices[i].cpuUsage << ",\n";
        jsonFile << "      \"Memory_Usage\": " << topMemServices[i].memoryUsage << "\n";
        jsonFile << "    }" << (i < topMemServices.size() - 1 ? "," : "") << "\n";
    }
    jsonFile << "  ]\n";
    jsonFile << "}\n";

    jsonFile.close();
    std::cout << "Output written to " << filename << "\n";
}

bool isFileOld(const std::string& filePath, int hours) {
    struct stat fileInfo;
    if (stat(filePath.c_str(), &fileInfo) == 0) {
        time_t now = time(nullptr);
        double secondsSinceLastWrite = difftime(now, fileInfo.st_mtime);
        return secondsSinceLastWrite > (hours * 3600);
    }
    return false;
}

void deleteOldFiles(const std::string& directory, int hours) {
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        std::cerr << "Failed to open directory." << std::endl;
        return;
    }

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type == DT_REG) {
            std::string filePath = directory + "/" + ent->d_name;
            if (isFileOld(filePath, hours)) {
                std::cout << "Deleting old file: " << filePath << std::endl;
                std::remove(filePath.c_str());
            }
        }
    }
    closedir(dir);
}

int main() {
    std::vector<ServiceInfo> averagedCpuServices;
    std::vector<ServiceInfo> averagedMemServices;
    
    for (int iteration = 0; iteration < 30; ++iteration) {
        std::vector<ServiceInfo> services = getRunningServices();
        std::vector<ServiceInfo> topCpuServices = getTopServices(services, true, 5);
        std::vector<ServiceInfo> topMemServices = getTopServices(services, false, 5);
        
        averageTopServices(topCpuServices, averagedCpuServices);
        averageTopServices(topMemServices, averagedMemServices);

        std::this_thread::sleep_for(std::chrono::minutes(1));
    }

    for (auto& service : averagedCpuServices) {
        service.cpuUsage /= 30;
    }
    for (auto& service : averagedMemServices) {
        service.memoryUsage /= 30;
    }

    outputToJson(averagedCpuServices, averagedMemServices);

    deleteOldFiles(".", 24);

    return 0;
}


