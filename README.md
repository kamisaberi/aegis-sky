# 🛡️ Aegis Sky: Autonomous Defense Ecosystem

[![Release](https://img.shields.io/badge/release-v1.0.0--alpha-blue)](https://github.com/ignition-ai/aegis-sky/releases)
[![Platform](https://img.shields.io/badge/platform-NVIDIA%20Jetson%20AGX%20Orin-76B900?logo=nvidia)](https://developer.nvidia.com/embedded/jetson-modules)
[![Lang](https://img.shields.io/badge/std-C%2B%2B20-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![CUDA](https://img.shields.io/badge/CUDA-12.2-76B900)](https://developer.nvidia.com/cuda-toolkit)
[![License](https://img.shields.io/badge/license-Proprietary%20%2F%20ITAR-red)](LICENSE)

> **"Latency is the new stealth."**
>
> **Aegis Sky** is a vertically integrated, millisecond-scale kinetic defense system designed to neutralize asymmetric UAS threats. It unifies simulation, AI training, embedded flight software, and fleet command into a single monorepo.

---

## 📑 Table of Contents

1.  [System Architecture](#-system-architecture)
2.  [The Five Pillars](#-the-five-pillars)
    *   [Core (Flight Software)](#1-aegis-core-flight-software)
    *   [Sim (Digital Twin)](#2-aegis-sim-digital-twin)
    *   [Station (C2 Interface)](#3-aegis-station-command--control)
    *   [Brain (AI Factory)](#4-aegis-brain-ai-factory)
    *   [Cloud (Fleet Command)](#5-aegis-cloud-telemetry)
3.  [Hardware Requirements](#-hardware-requirements)
4.  [Build & Deploy](#-build--deploy)
5.  [Operational Manual](#-operational-manual)
6.  [Legal & Compliance](#-legal--compliance)

---

## 🏗️ System Architecture

Aegis Sky effectively eliminates the "OS Jitter" problem by bypassing standard kernel networking and memory copying mechanisms.

### Data Flow Pipeline (The "Zero-Copy" Loop)

The system operates on a strict **16.6ms (60Hz)** control loop.

1.  **Ingestion (t=0ms):** Drivers write sensor data directly into **Pinned CUDA Memory** (DMA).
2.  **Fusion (t=2ms):** A custom CUDA kernel projects 4D Radar Point Clouds onto the 2D Vision Tensor.
3.  **Perception (t=8ms):** The **xInfer** engine (INT8 TensorRT) detects threats in the 5-channel tensor.
4.  **Tracking (t=9ms):** A 6-State Kalman Filter (EKF) smooths trajectories and handles occlusion.
5.  **Fire Control (t=10ms):** Ballistic solution calculated. Command sent to hardware.
6.  **Actuation (t=12ms):** Hardware Gimbal begins movement.

### IPC Architecture (Bridge)

Between the **Simulator** and **Core**, we use a custom Shared Memory Transport.

| Channel | Mechanism | Bandwidth | Latency |
| :--- | :--- | :--- | :--- |
| **Video** | `mmap` Ring Buffer (RGB888) | ~3.0 GB/s | < 50µs |
| **Radar** | `mmap` Linear Buffer (Struct) | ~20 MB/s | < 10µs |
| **Command** | `mmap` Atomic Struct | < 1 KB/s | < 1µs |
| **Telemetry** | TCP/IP (Protobuf) | ~1 MB/s | ~5ms |

---

## 🏛️ The Five Pillars

### 1. Aegis Core (Flight Software)
**Location:** `/core`
**Role:** The "Brain" on the Edge.

*   **Real-Time Scheduler:** Uses `SCHED_FIFO` to preempt standard Linux background tasks.
*   **Fusion Engine:** Implements "Early Fusion". Unlike "Late Fusion" (merging boxes), we merge raw signals. This allows the AI to see the "Doppler Velocity" of a pixel.
*   **HAL (Hardware Abstraction Layer):**
    *   `ICamera`: Supports V4L2, GStreamer, and SimBridge.
    *   `IRadar`: Supports Echodyne, SimRadar, and Generic CAN.
*   **Services:**
    *   `PerceptionService`: Wraps the **xInfer** runtime.
    *   `TrackingService`: Handles ID assignment and coasting.

### 2. Aegis Sim (Digital Twin)
**Location:** `/sim`
**Role:** The Physics Verification Engine.

This is not a game; it is a **parametric physics engine** designed to stress-test the Core.
*   **Sensor Phenomenology:**
    *   **Multipath:** Simulates ground-bounce ghost targets.
    *   **Micro-Doppler:** Simulates the modulation of rotor blades.
    *   **Thermal:** Simulates Blackbody radiation for IR cameras.
*   **Environmental Physics:**
    *   **Occlusion:** Ray-marching against procedural terrain.
    *   **Atmospherics:** Rain/Fog attenuation using Beer-Lambert Law.
    *   **EW:** Electronic Warfare jamming modeling (Noise Floor elevation).

### 3. Aegis Station (Command & Control)
**Location:** `/station`
**Role:** The Human Interface.

A standalone C++ Qt/QML application for the tactical operator.
*   **Low Latency Video:** Uses a custom GStreamer pipeline (`udpsrc -> h265parse -> glimagesink`) to achieve sub-100ms glass-to-glass latency.
*   **Augmented Reality:** Draws 3D bounding boxes and velocity vectors over the video feed.
*   **Guarded Control:** Implements "Two-Man Rule" logic for weapons release.

### 4. Aegis Brain (AI Factory)
**Location:** `/brain`
**Role:** The Training Pipeline.

*   **Data Engine:** `SimDataset` automatically pairs Radar/Video logs from the Simulator.
*   **xTorch Integration:** Uses your proprietary library to train the `AuraNet` (5-Channel ResNet-18 backbone).
*   **Compiler:** Exports trained weights to `.plan` files optimized for Jetson Orin DLA (Deep Learning Accelerator).

### 5. Aegis Cloud (Telemetry)
**Location:** `/cloud`
**Role:** Fleet Management.

*   **Ingestor:** A Golang gRPC microservice capable of handling 10k+ concurrent streams.
*   **Forensics:** Stores "Black Box" data (the last 30 seconds before a shot) for legal verification.
*   **OTA:** Secure Over-The-Air update delivery mechanism.

---

## 💻 Hardware Requirements

### Development Workstation
*   **OS:** Ubuntu 22.04 LTS
*   **CPU:** AMD Ryzen 9 / Intel i9 (for Sim Physics)
*   **GPU:** NVIDIA RTX 3080 or better (for Training/Rendering)
*   **RAM:** 64GB+ (for Dataset caching)

### Deployment Target (The Pod)
*   **Compute:** NVIDIA Jetson AGX Orin (32GB or 64GB)
*   **Camera:** Basler ace 2 (GigE) or e-con Systems (MIPI)
*   **Radar:** Echodyne EchoGuard or Arbe Phoenix
*   **Network:** 10GbE Switch

---

## 🛠️ Build & Deploy

We use **CMake** with **Ninja** for high-speed compilation.

### 1. Install Dependencies
```bash
# System Tools
sudo apt update && sudo apt install -y build-essential cmake ninja-build git \
    libopencv-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    qt6-base-dev qt6-declarative-dev libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc

# Install Proprietary Libs (xTorch / xInfer)
# (Assumes .deb packages are provided)
sudo dpkg -i xtorch-1.0.deb xinfer-2.0.deb
```

### 2. Compile the Monorepo
```bash
# Clone
git clone https://github.com/ignition-ai/aegis-sky.git
cd aegis-sky

# Configure (Release Build)
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DAEGIS_PLATFORM=JETSON_ORIN

# Build All Sections (Core, Sim, Brain, Station)
cmake --build build --parallel $(nproc)
```

### 3. Running the Stack (Live Fire Demo)

**Terminal 1: The Cloud (Backend)**
```bash
cd cloud
docker-compose up
```

**Terminal 2: The Simulator (Physics)**
```bash
./bin/aegis_sim sim/assets/scenarios/terrain_popup.json
```

**Terminal 3: The Core (Flight Software)**
*Note: Requires sudo for Real-Time Scheduler.*
```bash
sudo ./bin/aegis_core
```

**Terminal 4: The Station (UI)**
```bash
./bin/aegis_station
```

---

## 🎮 Operational Manual

### Defining Scenarios
Scenarios are JSON files located in `sim/assets/scenarios/`.
```json
{
    "mission_name": "Swarm Attack",
    "entities": [
        {
            "name": "Alpha-1",
            "type": "QUADCOPTER",
            "rcs": 0.01,
            "micro_doppler": { "blade_speed": 100.0, "hz": 50.0 },
            "waypoints": [[0, 50, 500], [0, 10, 0]]
        }
    ]
}
```

### Map & Calibration
*   **Calibration:** `core/configs/calibration.yaml` defines the extrinsic matrix ($R, T$) between the Radar and Camera.
*   **Geofence:** `station/configs/maps.json` defines No-Fire Zones.

---

## ⚖️ Legal & Compliance

**Copyright © 2025 Aegis Sky, Inc.**

### Proprietary Notice
This source code, architecture, and associated documentation are the **Confidential and Proprietary** property of Aegis Sky, Inc. Unauthorized copying, distribution, or reverse engineering is strictly prohibited.

### ITAR / Export Control
**WARNING:** This repository contains technical data subject to the **International Traffic in Arms Regulations (ITAR)** (22 CFR 120-130). Export, re-export, or transfer of this data to any foreign person, whether in the United States or abroad, without a valid license or exemption from the U.S. Department of State is a violation of U.S. law.

**Violations are punishable by severe fines and imprisonment.**

---

**Maintained by the Aegis Sky Advanced Projects Group.**