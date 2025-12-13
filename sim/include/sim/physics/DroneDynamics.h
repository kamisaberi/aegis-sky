#pragma once
#include "sim/engine/SimEntity.h"
#include <glm/glm.hpp>

namespace aegis::sim::physics {

    struct DroneConfig {
        double mass_kg = 1.2;       // DJI Mavic ~900g + payload
        double drag_coeff = 0.3;    // Aerodynamic drag
        double max_thrust_n = 30.0; // Max motor power in Newtons
    };

    class DroneDynamics {
    public:
        // Applies forces to the entity based on inputs
        static void apply_physics(engine::SimEntity& drone, const DroneConfig& config, double dt);

    private:
        static constexpr double GRAVITY = -9.81;
        static constexpr double AIR_DENSITY = 1.225; // kg/m^3
    };
}