# HTTP Message Parser Example Using DPDK

This example demonstrates how to use the **DPDK (Data Plane Development Kit)** packet processing API to parse HTTP messages from network packets. The HTTP parser identifies the request type (e.g., GET, POST), extracts headers, and prints the payload from captured packets.

## Overview

The goal of this project is to create a working example that utilizes **DPDK**'s packet processing capabilities to capture and parse HTTP messages from network traffic.

### Key Features:
- Capture network packets using **libpcap**.
- Parse HTTP messages including the method (GET, POST), headers, and payload.
- Use DPDK's packet processing API to efficiently handle network traffic.

## Prerequisites

Before you begin, ensure you have the following installed:

- **DPDK**: A high-performance packet processing library.
- **libpcap**: A library used for capturing network packets.
- **gcc**: A compiler for building C programs.
- **Make**: A tool for managing the build process.

## Setup Instructions

### Step 1: Install DPDK

To use DPDK, you need to set up the environment and install DPDK on your system.

#### On Linux:

Follow the official DPDK setup instructions:  
[DPDK Installation Guide](https://doc.dpdk.org/guides/linux_gsg/index.html)

#### On macOS:

You can use a **Docker** container to run DPDK if it is not supported natively on macOS.

1. **Clone the Docker repository** and build the DPDK environment.

2. **Run the Docker container** with DPDK:
   ```bash
   docker run --rm -it --privileged dpdk-docker
   ```

### Step 2: Install libpcap

You will need `libpcap` to capture network packets.

#### On Ubuntu/Debian-based systems:
```bash
sudo apt-get install libpcap-dev
```

#### On macOS (using Homebrew):
```bash
brew install libpcap
```

### Step 3: Clone the Repository

Clone the repository containing the HTTP parser example:

```bash
git clone https://github.com/Brijitha1609/dpdk.git
cd dpdk
```

### Step 4: Build the Project

1. **Install dependencies** (on Linux-based systems):
   ```bash
   sudo apt-get update
   sudo apt-get install build-essential libnuma-dev meson ninja-build
   ```

2. **Compile the program**:
   ```bash
   make
   ```

   Alternatively, if you are using `meson`:
   ```bash
   meson build
   ninja -C build
   ```

### Step 5: Run the HTTP Parser

1. **Prepare your network interface** to capture packets (you may need root privileges).

2. **Run the HTTP parser**:
   ```bash
   sudo ./main
   ```

   The program will start capturing network packets from the network interface (default `en0` in the code) and parse HTTP messages from the captured packets.

   Example output:
   ```
   Using device: en0
   HTTP Method: GET
   Headers:
   Host: example.com
   User-Agent: curl/7.64.1
   Payload: <Request Payload Here>
   ```

## Troubleshooting

- **Permission Issues**: Ensure you have permission to capture packets on the network interface. You may need to run the program as `sudo` or grant your user the necessary permissions.
- **DPDK Environment Errors**: Ensure that the environment variables (`RTE_SDK`, `RTE_TARGET`) are correctly configured. Follow the DPDK setup guide for more details.

