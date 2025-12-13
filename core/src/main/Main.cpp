#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <cmath>
#include <csignal>

// --- INFRASTRUCTURE ---
#include "platform/threading/Scheduler.h"
#include "aegis_ipc/shm_layout.h"

// --- DRIVERS (HAL) ---
#include "drivers/bridge_client/ShmReader.h"
// Including .cpp directly for MVP single-unit compilation ease.
// In full production, these would be linked via CMake object libraries.
#include "drivers/bridge_client/SimRadar.cpp"
#include "drivers/bridge_client/SimCamera.cpp"

// --- SERVICES ---
#include "services/comms/StationLink.h"
#include "services/fusion/FusionEngine.h"
#include "services/tracking/TrackManager.h"

// --- LOGGING ---
#include <spdlog/spdlog.h>

using namespace aegis::core;

// Global flag for graceful shutdown
volatile std::sig_atomic_t g_running = 1;

void signal_handler(int) {
    g_running = 0;
}

int main(int argc, char** argv) {
    // 1. SETUP LOGGING
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] [Core] %v");
    spdlog::info("========================================");
    spdlog::info("   AEGIS CORE: FLIGHT SOFTWARE v1.0     ");
    spdlog::info("========================================");

    // Register Ctrl+C handler
    std::signal(SIGINT, signal_handler);

    // 2. REAL-TIME PRIORITY
    // Try to set SCHED_FIFO. This requires 'sudo' to work.
    if (!platform::Scheduler::set_realtime_priority(50)) {
        spdlog::warn("Running in Standard Scheduling Mode (Latency not guaranteed)");
    } else {
        spdlog::info("Running in Real-Time Mode (SCHED_FIFO)");
    }

    // 3. INITIALIZE HARDWARE BRIDGE (Zero-Copy Link to Sim)
    auto bridge = std::make_shared<drivers::ShmReader>();

    spdlog::info("Connecting to Matrix Bridge (Shared Memory)...");
    int retry_count = 0;
    while (!bridge->connect() && g_running) {
        if (retry_count++ % 5 == 0) spdlog::warn("Waiting for Simulator...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!g_running) return 0;
    spdlog::info("Bridge Connected. Sensors Online.");

    // 4. INITIALIZE DRIVERS
    auto radar_driver = std::make_unique<drivers::SimRadar>(bridge);
    auto camera_driver = std::make_unique<drivers::SimCamera>(bridge);

    // 5. INITIALIZE FUSION ENGINE
    // Create calibration for 1080p (matches Sim configuration)
    auto cal_data = services::CalibrationData::create_perfect_alignment(1920, 1080);
    auto fusion_engine = std::make_unique<services::FusionEngine>(cal_data);

    // 6. INITIALIZE TRACKER (Kalman Filter Manager)
    auto track_manager = std::make_unique<services::tracking::TrackManager>();

    // 7. INITIALIZE COMMS (Station Link TCP Server)
    auto station_link = std::make_unique<services::StationLink>(9090);
    if (!station_link->start()) {
        spdlog::critical("Failed to bind TCP Port 9090. Is Station already running or port blocked?");
        return -1;
    }

    // 8. MAIN GUIDANCE LOOP
    spdlog::info("Entering Guidance Loop...");

    // State variables
    uint64_t frame_count = 0;
    float current_pan_cmd = 0.0f;
    float current_tilt_cmd = 0.0f;

    while (g_running) {
        auto loop_start = std::chrono::high_resolution_clock::now();

        // --- PHASE A: SENSOR INGESTION (Zero-Copy) ---
        // Get pointers to data. In Sim mode, these point directly to /dev/shm
        auto cloud = radar_driver->get_scan();
        auto image = camera_driver->get_frame();
        double sys_time = cloud.timestamp;

        // --- PHASE B: SENSOR FUSION (CUDA) ---
        // Project 3D Radar points onto 2D Camera Plane (Depth + Velocity Maps)
        // This runs on the GPU.
        auto fused_frame = fusion_engine->process(image, cloud);

        // --- PHASE C: TRACKING (Kalman Filter) ---
        // Smooth out raw radar hits, handle occlusion coasting, assign IDs
        track_manager->process_scan(cloud);

        auto active_tracks = track_manager->get_tracks();

        // Filter for "Confirmed" tracks (to avoid displaying noise)
        int confirmed_threats = 0;
        for (const auto& t : active_tracks) {
            if (t.is_confirmed) confirmed_threats++;
        }

        // --- PHASE D: COMMAND HANDLING (From Station) ---
        ipc::station::CommandPacket ui_cmd;
        bool has_new_cmd = station_link->get_latest_command(ui_cmd);

        // Map Station Commands -> Flight Commands
        ipc::ControlCommand flight_cmd;
        flight_cmd.timestamp = (uint64_t)(sys_time * 1000.0);
        flight_cmd.pan_velocity = 0.0f;
        flight_cmd.tilt_velocity = 0.0f;
        flight_cmd.fire_trigger = false;
        flight_cmd.jammer_active = false;

        if (has_new_cmd) {
            // Manual Override from UI
            current_pan_cmd = ui_cmd.pan_velocity;
            current_tilt_cmd = ui_cmd.tilt_velocity;

            flight_cmd.pan_velocity = current_pan_cmd;
            flight_cmd.tilt_velocity = current_tilt_cmd;

            // Weapons Logic (Dead Man's Switch)
            if (ui_cmd.arm_system && ui_cmd.fire_trigger) {
                flight_cmd.fire_trigger = true;
                spdlog::warn("⚠️  WEAPONS RELEASE AUTHORIZED | FIRING INTERCEPTOR");
            }
        } else {
            // No new command, maintain last known velocity (or auto-stabilize)
            flight_cmd.pan_velocity = current_pan_cmd;
            flight_cmd.tilt_velocity = current_tilt_cmd;
        }

        // --- PHASE E: ACTUATION (To Sim/Hardware) ---
        // Write the command structure to Shared Memory for the Simulator to read
        bridge->send_command(flight_cmd);

        // --- PHASE F: TELEMETRY (To Station) ---
        // Send status back to UI at 60Hz.
        // We act as the "Feedback Loop" here.
        station_link->broadcast_telemetry(
            sys_time,
            0.0f, // TODO: In TRL-9, read actual Encoder Pan from Hardware
            0.0f, // TODO: In TRL-9, read actual Encoder Tilt from Hardware
            confirmed_threats
        );

        // --- PHASE G: LOGGING & PACING ---
        if (frame_count % 60 == 0) {
            // 1Hz Health Check
            spdlog::info("[System] FPS: 60 | Radar Raw: {} | Tracks: {} | Cmd Pan: {:.2f}",
                cloud.points.size(),
                confirmed_threats,
                current_pan_cmd);
        }

        frame_count++;

        // Maintain ~60Hz Loop (16.6ms)
        // Adjust for processing time
        auto loop_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = loop_end - loop_start;
        double sleep_ms = 16.66 - elapsed.count();

        if (sleep_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds((int)sleep_ms));
        } else {
            spdlog::warn("[System] CPU Overload! Loop took {:.2f}ms", elapsed.count());
        }
    }

    // Cleanup
    station_link->stop();
    spdlog::info("[Core] Shutdown complete.");
    return 0;
}