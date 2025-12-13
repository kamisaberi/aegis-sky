#include "sim/physics/DroneDynamics.h"

namespace aegis::sim::physics {

    void DroneDynamics::apply_physics(engine::SimEntity& drone, const DroneConfig& config, double dt) {
        
        glm::dvec3 velocity = drone.get_velocity();
        double speed = glm::length(velocity);

        // 1. Gravity Force
        glm::dvec3 F_gravity(0.0, config.mass_kg * GRAVITY, 0.0);

        // 2. Drag Force (Opposite to velocity)
        // Formula: F = 0.5 * rho * v^2 * Cd * A
        glm::dvec3 F_drag(0.0);
        if (speed > 0.001) {
            double drag_magnitude = 0.5 * AIR_DENSITY * (speed * speed) * config.drag_coeff;
            F_drag = -glm::normalize(velocity) * drag_magnitude;
        }

        // 3. Hover Thrust (Simplified: Counter-act gravity + Noise)
        // In a real sim, this comes from the "Pilot" controller. 
        // Here we simulate a perfect hover.
        glm::dvec3 F_thrust(0.0, -F_gravity.y, 0.0); 

        // Sum of Forces
        glm::dvec3 F_total = F_gravity + F_drag + F_thrust;

        // Newton's Second Law: F = ma  ->  a = F/m
        glm::dvec3 acceleration = F_total / config.mass_kg;

        drone.set_acceleration(acceleration);
    }
}