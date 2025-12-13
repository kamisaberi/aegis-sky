#pragma once

#include <vector>
#include <memory>
#include <string>

// Internal Module Headers
#include "sim/engine/TimeManager.h"
#include "sim/engine/SimEntity.h"
#include "sim/physics/DroneDynamics.h"

// Forward Declarations (Keeps compile time fast)
namespace aegis::sim::bridge { class ShmWriter; }
namespace aegis::sim::phenomenology { class MockRenderer; }

namespace aegis::sim::engine {

    class SimEngine {
    public:
        SimEngine();
        ~SimEngine();

        // Loads the scenario JSON and initializes Shared Memory
        void initialize(const std::string& scenario_path);

        // The Infinite Loop (Physics -> Render -> Bridge)
        void run();

        // Graceful shutdown
        void stop();

    private:
        bool is_running_;
        
        // Subsystems
        TimeManager time_manager_;
        
        // The World State
        std::vector<std::shared_ptr<SimEntity>> entities_;

        // Physics Configuration (Mass, Drag)
        physics::DroneConfig drone_phys_config_;

        // Communication Bridge (Unique Ptr for memory management)
        std::unique_ptr<bridge::ShmWriter> bridge_;

        // Vision Renderer
        std::unique_ptr<phenomenology::MockRenderer> renderer_;
    };

}