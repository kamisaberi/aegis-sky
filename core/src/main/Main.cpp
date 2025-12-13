#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

#include "drivers/bridge_client/ShmReader.h"
#include "drivers/bridge_client/SimRadar.cpp"
#include "services/comms/StationLink.h" // <--- Include this

using namespace aegis::core;

int main(int argc, char** argv) {
    // ... [Init logging] ...

    // 1. Hardware Drivers (Bridge to Sim)
    auto bridge = std::make_shared<drivers::ShmReader>();
    while (!bridge->connect()) {
        std::cout << "Waiting for Sim..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    auto radar_driver = std::make_unique<drivers::SimRadar>(bridge);

    // 2. Start Station Server
    auto station_link = std::make_unique<services::StationLink>(9090);
    if (!station_link->start()) {
        std::cerr << "Failed to start Station TCP Server!" << std::endl;
        return -1;
    }

    // Main Loop
    while (true) {
        // A. READ SENSORS
        auto cloud = radar_driver->get_scan();
        double now = cloud.timestamp;

        // B. READ COMMANDS FROM STATION (UI)
        ipc::station::CommandPacket ui_cmd;
        bool has_cmd = station_link->get_latest_command(ui_cmd);

        // C. SEND COMMANDS TO SIMULATOR
        // The UI controls the Pan/Tilt/Fire
        // We forward this to the physics engine via Shared Memory
        ipc::ControlCommand sim_cmd;

        // Defaults (Stationary)
        sim_cmd.pan_velocity = 0.0f;
        sim_cmd.tilt_velocity = 0.0f;
        sim_cmd.fire_trigger = false;

        if (has_cmd) {
            sim_cmd.pan_velocity = ui_cmd.pan_velocity;
            sim_cmd.tilt_velocity = ui_cmd.tilt_velocity;

            if (ui_cmd.arm_system && ui_cmd.fire_trigger) {
                sim_cmd.fire_trigger = true;
                std::cout << "[Core] FIRE COMMAND RECEIVED" << std::endl;
            }
        }

        bridge->send_command(sim_cmd);

        // D. SEND TELEMETRY TO STATION
        // We define "Pan" as time for testing, or read actual gimbal feedback if we had it
        // For MVP, echo back the timestamp and target count
        station_link->broadcast_telemetry(now, 0.0f, 0.0f, cloud.points.size());

        // Loop pacing
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}