# Top 5 Resource Analyzer

This C++ application monitors running services on a Linux server/container, collecting data on CPU and memory usage. The application can be run standalone or deployed as a sidecar container within a Kubernetes pod to provide insights into resource usage without modifying the main application image.

I wrote this due to having to fulfil a requirement for trying to nail down a few performance optimizations on several Kubernetes applications without modifying the existing images. These applications could not be exposed to our monitoring tools so we needed something more minimal and portable.

## Features

- Collects CPU and memory usage for all running services.
- Outputs the top 5 services using the most CPU and memory.
- Generates a well-structured JSON file containing service details.

## Function

- The script will run for 30 iterations, each with a 1-minute sleep. This collects data over a 30-minute interval. After 30 minutes, the CPU and memory usages are averaged by dividing by 30 and output to something like `top5_resources_output_year-month-date_hour-minute-second.json` in the same directory. After 24 hours, anything not written within that time will be deleted as to not bloat the directory.

## Requirements

- Linux environment
- g++ (g++ 7.1 minimum required)

## Standalone Usage

1. Clone the repository or download the script:
```
   git clone https://github.com/mkonji008/top5-resource-analyzer.git
   cd top5-resource-analyzer
```

2. Compile the C++ application:

   `g++ -std=c++17 -o top5-resource-analyzer top5-resource-analyzer.cpp`

3. Run the script from the terminal:

   `./top5-resource-analyzer`

### Output

The JSON output file will contain:
```json
{
    "Top_CPU_Usage": [
        {
            "PID": "1234",
            "Service_Name": "example_service...etc",
            "CPU_Usage": 15.5,
            "Memory_Usage": 25.3
        }
    ],
    "Top_Memory_Usage": [
        {
            "PID": "5678",
            "Service_Name": "example_service...etc",
            "CPU_Usage": 10.2,
            "Memory_Usage": 55.6
        }
    ]
}
```

## Integrating as a Sidecar Container in Kubernetes

To deploy the top5-resource-analyzer as a sidecar container, create a Kubernetes manifest file (example..., `deployment.yaml`) with the example configuration:

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: main-app
spec:
  replicas: 1
  selector:
    matchLabels:
      app: main-app
  template:
    metadata:
      labels:
        app: main-app
    spec:
      containers:
      - name: main-app
        image: <main-app-image>  # replace with your app you want to monitor
        ports:
          - containerPort: 80  # adjust to your main apps port
      - name: top5-resource-analyzer
        image: <registry-username>/top5-resource-analyzer:latest
        resources:
          requests:
            cpu: "100m"
            memory: "128Mi"
          limits:
            cpu: "200m"
            memory: "256Mi"
        volumeMounts:
          - name: output-volume
            mountPath: /app/output  # mount the output directory for shared access
      volumes:
        - name: output-volume
          emptyDir: {}  # temp directory shared between containers
```
### Running the Deployment

1. Apply the Kubernetes configuration:
    `kubectl apply -f deployment.yaml`

2. Monitor logs for both containers:

   - For the main application:
     `kubectl logs <pod-name> -c main-app`

   - For the top5-resource-analyzer:
     `kubectl logs <pod-name> -c top5-resource-analyzer`

### Output

The `top5-resource-analyzer` will generate a JSON output file containing the top 5 CPU and memory-consuming services in the shared output directory:

You can access the output file by copying it from the pod:

`kubectl cp <pod-name>:/app/output/top5_resources_output_year-month-date_hour-minute-second.json ./top5_resources_output_year-month-date_hour-minute-second.json`

