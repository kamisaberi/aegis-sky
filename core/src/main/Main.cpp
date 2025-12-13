#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <cmath>

// --- INFRASTRUCTURE ---
#include "platform/threading/Scheduler.h"
#include "aegis_ipc/shm_layout.h"

// --- DRIVERS (HAL) ---
#include "drivers/bridge_client/ShmReader.h"
#include "drivers/bridge_client/SimRadar.cpp"  // Including impl for direct compilation in MVP
#include "drivers/bridge_client/SimCamera.cpp" // Including impl for direct compilation in MVP

// --- SERVICES ---
#include "services/comms/StationLink.h"
#include "services/fusion/FusionEngine.h"

// --- LOGGING ---
#include <spdlog/spdlog.h>

using namespace aegis::core;

int main(int argc, char** argv) {
    // 1. SETUP LOGGING
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] [Core] %v");
    spdlog::info("========================================");
    spdlog::info("   AEGIS CORE: FLIGHT SOFTWARE v1.0     ");
    spdlog::info("========================================");

    // 2. REAL-TIME PRIORITY
    // Try to set SCHED_FIFO. This requires 'sudo' to work.
    if (!platform::Scheduler::set_realtime_priority(50)) {
        spdlog::warn("Running in Standard Scheduling Mode (Latency not guaranteed)");
    } else {
        spdlog::info("Running in Real-Time Mode");
    }

    // 3. INITIALIZE HARDWARE BRIDGE (Zero-Copy Link to Sim)
    auto bridge = std::make_shared<drivers::ShmReader>();

    spdlog::info("Connecting to Matrix Bridge...");
    while (!bridge->connect()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    spdlog::info("Bridge Connected. Sensors Online.");

    // 4. INITIALIZE DRIVERS
    auto radar_driver = std::make_unique<drivers::SimRadar>(bridge);
    auto camera_driver = std::make_unique<drivers::SimCamera>(bridge); // Now connected

    // 5. INITIALIZE FUSION ENGINE
    // Create calibration for 1080p (matches Sim)
    auto cal_data = services::CalibrationData::create_perfect_alignment(1920, 1080);
    auto fusion_engine = std::make_unique<services::FusionEngine>(cal_data);

    // 6. INITIALIZE COMMS (Station Link)
    auto station_link = std::make_unique<services::StationLink>(9090);
    if (!station_link->start()) {
        spdlog::critical("Failed to bind TCP Port 9090. Is Station already running?");
        return -1;
    }

    // 7. MAIN GUIDANCE LOOP
    spdlog::info("Entering Guidance Loop...");

    // Performance metrics
    auto last_time = std::chrono::high_resolution_clock::now();
    uint64_t frame_count = 0;

    while (true) {
        // --- PHASE A: SENSOR INGESTION (Zero-Copy) ---
        // Get pointers to data. In Sim mode, these point to /dev/shm
        auto cloud = radar_driver->get_scan();
        auto image = camera_driver->get_frame();
        double sys_time = cloud.timestamp;

        // --- PHASE B: SENSOR FUSION (CUDA) ---
        // Project 3D Radar onto 2D Camera Plane
        auto fused_frame = fusion_engine->process(image, cloud);

        // --- PHASE C: PERCEPTION (Placeholder for xInfer) ---
        // auto detections = inference_mgr->detect(fused_frame);
        // int threat_count = detections.size();
        int threat_count = cloud.points.size(); // Proxy for now

        // --- PHASE D: COMMAND HANDLING (From Station) ---
        ipc::station::CommandPacket ui_cmd;
        bool has_new_cmd = station_link->get_latest_command(ui_cmd);

        // Map Station Commands -> Flight Commands
        ipc::ControlCommand flight_cmd;
        flight_cmd.pan_velocity = 0.0f;
        flight_cmd.tilt_velocity = 0.0f;
        flight_cmd.fire_trigger = false;

        if (has_new_cmd) {
            // Manual Override from UI
            flight_cmd.pan_velocity = ui_cmd.pan_velocity;
            flight_cmd.tilt_velocity = ui_cmd.tilt_velocity;

            // Weapons Logic
            if (ui_cmd.arm_system && ui_cmd.fire_trigger) {
                flight_cmd.fire_trigger = true;
                spdlog::warn("WEAPONS RELEASE AUTHORIZED");
            }
        }

        // --- PHASE E: ACTUATION (To Sim/Hardware) ---
        bridge->send_command(flight_cmd);

        // --- PHASE F: TELEMETRY (To Station) ---
        // Send status back to UI at 60Hz
        // Note: In real system, read actual encoder values. Here we echo command/time.
        station_link->broadcast_telemetry(
            sys_time,
            0.0f, // TODO: Read actual Pan angle from Bridge
            0.0f, // TODO: Read actual Tilt angle from Bridge
            threat_count
        );

        // --- PHASE G: LOGGING & PACING ---
        if (frame_count % 60 == 0 && threat_count > 0) {
            spdlog::info("TRACKING: {} Targets | Radar SNR: {:.1f}dB",
                threat_count,
                cloud.points[0].snr);
        }

        frame_count++;

        // Maintain ~60Hz Loop (16ms)
        // In Prod, use a sleep_until for precise timing
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}