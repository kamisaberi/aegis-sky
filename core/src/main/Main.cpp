#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

#include "drivers/bridge_client/ShmReader.h"
#include "drivers/bridge_client/SimRadar.cpp" // Include impl for MVP simplicity
// #include "services/comms/StationLink.h"

using namespace aegis::core;

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "   AEGIS CORE: FLIGHT SOFTWARE v1.0     " << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. Initialize Hardware Abstraction (HAL)
    // For Simulation Mode, we use the Bridge Drivers
    auto bridge = std::make_shared<drivers::ShmReader>();

    // Retry loop to connect to Sim
    while (!bridge->connect()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto radar_driver = std::make_unique<drivers::SimRadar>(bridge);
    // auto comms_server = std::make_unique<services::StationLink>(9090);

    // 2. Main Real-Time Loop
    std::cout << "[Core] Entering Guidance Loop..." << std::endl;

    while (true) {
        // A. SENSOR INGESTION
        auto cloud = radar_driver->get_scan();

        if (!cloud.points.empty()) {
            std::cout << "[Core] Radar Contact! Targets: " << cloud.points.size()
                      << " | Nearest Dist: " << cloud.points[0].z << "m" << std::endl;

            // B. FUSION & TRACKING (Placeholder)
            // auto tracks = tracker->process(cloud);

            // C. FIRE CONTROL
            // if (tracks.has_threat()) ...

            // D. COMMAND OUTPUT (To Sim)
            ipc::ControlCommand cmd;
            cmd.pan_velocity = 0.1f; // Test: Rotate camera slowly
            cmd.fire_trigger = false;
            bridge->send_command(cmd);

            // E. TELEMETRY OUTPUT (To Station)
            // comms_server->broadcast_tracks(tracks);
        }

        // High-speed loop (100Hz)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}