#include "sim/engine/SimEngine.h"

#include <spdlog/spdlog.h>
#include <iostream>
#include <csignal>
#include <filesystem>

// -----------------------------------------------------------------------------
// GLOBAL SIGNAL HANDLER
// Used to capture Ctrl+C (SIGINT) and shut down the bridge cleanly.
// -----------------------------------------------------------------------------
namespace {
    // Pointer to the active engine (only valid during lifetime of main)
    aegis::sim::engine::SimEngine* g_engine_ptr = nullptr;
}

void signal_handler(int signal) {
    if (signal == SIGINT && g_engine_ptr) {
        spdlog::warn("\n[Main] SIGINT received. Initiating graceful shutdown...");
        g_engine_ptr->stop();
    }
}

// -----------------------------------------------------------------------------
// MAIN ENTRY POINT
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    // 1. Setup Logging
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::debug);

    spdlog::info("========================================");
    spdlog::info("   AEGIS SKY: THE MATRIX (SIMULATOR)    ");
    spdlog::info("========================================");

    // 2. Parse Arguments
    std::string scenario_path = "assets/scenarios/default.json";
    if (argc > 1) {
        scenario_path = argv[1];
    }

    // 3. Verify File Exists
    if (!std::filesystem::exists(scenario_path)) {
        spdlog::critical("[Main] Scenario file not found: {}", scenario_path);
        spdlog::info("Usage: ./aegis_sim <path_to_scenario.json>");
        return -1;
    }

    // 4. Instantiate Engine
    aegis::sim::engine::SimEngine matrix;
    g_engine_ptr = &matrix;

    // 5. Register Signal Handler
    std::signal(SIGINT, signal_handler);

    // 6. Run Simulation
    try {
        spdlog::info("[Main] Target Scenario: {}", scenario_path);
        
        // Initialize (Loads JSON, Opens Shared Mem)
        matrix.initialize(scenario_path);
        
        // Enter the Infinite Loop
        matrix.run(); 
    }
    catch (const std::exception& e) {
        spdlog::critical("[Main] UNHANDLED EXCEPTION: {}", e.what());
        
        // Attempt cleanup even on crash
        g_engine_ptr = nullptr;
        return -1;
    }

    spdlog::info("[Main] Shutdown complete. Goodbye.");
    g_engine_ptr = nullptr;
    return 0;
}