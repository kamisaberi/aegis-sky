#include "sim/engine/SimEngine.h"

#include <spdlog/spdlog.h>
#include <iostream>
#include <csignal>
#include <filesystem>
#include <unistd.h>     // fork, execlp
#include <sys/wait.h>   // waitpid
#include <sys/types.h>

// -----------------------------------------------------------------------------
// GLOBAL STATE FOR CLEANUP
// -----------------------------------------------------------------------------
namespace {
    aegis::sim::engine::SimEngine* g_engine_ptr = nullptr;
    pid_t g_viz_pid = -1; // Process ID of the Python script
}

// -----------------------------------------------------------------------------
// CLEANUP FUNCTION
// Kills the simulator loop AND the Python GUI
// -----------------------------------------------------------------------------
void shutdown_system(int signal) {
    spdlog::warn("\n[Main] Shutdown signal ({}) received...", signal);

    // 1. Stop the C++ Engine
    if (g_engine_ptr) {
        g_engine_ptr->stop();
    }

    // 2. Kill the Python Child Process
    if (g_viz_pid > 0) {
        spdlog::info("[Main] Terminating Visualization (PID: {})...", g_viz_pid);
        kill(g_viz_pid, SIGTERM);
        int status;
        waitpid(g_viz_pid, &status, 0); // Wait for it to die to prevent Zombies
        g_viz_pid = -1;
    }
}

// -----------------------------------------------------------------------------
// MAIN ENTRY POINT
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    // 1. Setup Logging
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::debug);

    spdlog::info("   AEGIS SKY: THE MATRIX (SIMULATOR)    ");

    // 2. Parse Arguments
    std::string scenario_path = "assets/scenarios/default.json";
    bool use_viz = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--viz") {
            use_viz = true;
        } else if (arg.find(".json") != std::string::npos) {
            scenario_path = arg;
        }
    }

    // 3. Verify Scenario Exists
    if (!std::filesystem::exists(scenario_path)) {
        spdlog::critical("[Main] Scenario not found: {}", scenario_path);
        return -1;
    }

    // 4. Instantiate Engine
    aegis::sim::engine::SimEngine matrix;
    g_engine_ptr = &matrix;

    // 5. Register Signals (Ctrl+C)
    std::signal(SIGINT, shutdown_system);
    std::signal(SIGTERM, shutdown_system);

    try {
        // 6. Initialize (Creates Shared Memory /dev/shm/aegis_bridge_v1)
        matrix.initialize(scenario_path);

        // 7. LAUNCH PYTHON VIZ (Fork Logic)
        if (use_viz) {
            // Check if python script exists
            std::string py_script = "tools/bridge_viz.py";
            if (!std::filesystem::exists(py_script)) {
                spdlog::error("[Main] Cannot find '{}'. Run from repo root!", py_script);
            } else {
                g_viz_pid = fork(); // Split process into two

                if (g_viz_pid == 0) {
                    // --- CHILD PROCESS (Python) ---
                    // Replace this process with Python
                    // execlp(Executable, Arg0, Arg1, NULL)
                    spdlog::info("[Child] Launching Python...");
                    execlp("python3", "python3", py_script.c_str(), NULL);
                    
                    // If we get here, execlp failed
                    spdlog::critical("[Child] Failed to spawn Python!");
                    exit(1);
                }
                else if (g_viz_pid > 0) {
                    // --- PARENT PROCESS (C++) ---
                    spdlog::info("[Main] Viz Tool spawned with PID: {}", g_viz_pid);
                }
                else {
                    spdlog::error("[Main] Fork failed!");
                }
            }
        }

        // 8. Run Simulation Loop (Blocking)
        matrix.run();

    }
    catch (const std::exception& e) {
        spdlog::critical("[Main] CRASH: {}", e.what());
        shutdown_system(SIGTERM);
        return -1;
    }

    shutdown_system(0); // Clean exit
    return 0;
}