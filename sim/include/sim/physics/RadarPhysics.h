#include "sim/physics/RadarPhysics.h"
#include "sim/math/Random.h"
#include <glm/gtx/norm.hpp> // For length2
#include <cmath>
#include <algorithm>
#include <iostream>

namespace aegis::sim::physics {

    // -------------------------------------------------------------------------
    // CONSTANTS
    // -------------------------------------------------------------------------
    static constexpr double TX_POWER_WATTS = 200.0;     
    static constexpr double THERMAL_NOISE_FLOOR = 1e-13; // Base noise without jamming 
    static constexpr double HITBOX_RADIUS = 0.5; // Meters
    static constexpr double JAMMER_POWER_WATTS = 10.0; // Power of an enemy jammer

    // -------------------------------------------------------------------------
    // HELPER: Calculate Dynamic Noise Floor (EW Logic)
    // -------------------------------------------------------------------------
    double RadarPhysics::calculate_environment_noise(
        const std::vector<std::shared_ptr<engine::SimEntity>>& entities, 
        const glm::dvec3& radar_pos
    ) {
        double total_noise = THERMAL_NOISE_FLOOR;

        for (const auto& entity : entities) {
            // Check if this entity is a jammer (Simulating EW)
            // For MVP: We assume 'FIXED_WING' types might have jammers enabled
            // In a full system, check entity->is_jammer_active()
            // Here we use a heuristic for demonstration:
            bool is_jamming = (entity->get_name().find("Jammer") != std::string::npos);

            if (is_jamming) {
                double dist_sq = glm::distance2(radar_pos, entity->get_position());
                if (dist_sq < 1.0) dist_sq = 1.0; // Protect div by zero

                // One-way Friis Transmission Equation for Jamming
                // Power drops by R^2 (Jamming is very effective vs R^4 Radar)
                double received_jamming = JAMMER_POWER_WATTS / (4.0 * M_PI * dist_sq);
                
                total_noise += received_jamming;
            }
        }
        return total_noise;
    }

    // -------------------------------------------------------------------------
    // CORE RAYCAST LOGIC
    // -------------------------------------------------------------------------
    RadarReturn RadarPhysics::cast_ray(
        const glm::dvec3& radar_pos, 
        const glm::dvec3& radar_forward, 
        const engine::SimEntity& target,
        const RadarConfig& config,
        double current_noise_floor // <--- NEW PARAMETER
    ) {
        RadarReturn ret = {false, 0.0, 0.0, 0.0, 0.0, -100.0};

        // 1. GEOMETRY CHECK (Is the target hit by the ray?)
        glm::dvec3 L = target.get_position() - radar_pos;
        double dist_sq = glm::length2(L);
        if (dist_sq > (config.max_range * config.max_range)) return ret;

        // 2. FIELD OF VIEW CHECK
        glm::dvec3 to_target_dir = glm::normalize(L);
        
        // Flatten vectors to XZ plane for Azimuth check
        glm::dvec3 flat_target = glm::normalize(glm::dvec3(to_target_dir.x, 0, to_target_dir.z));
        glm::dvec3 flat_forward = glm::normalize(glm::dvec3(radar_forward.x, 0, radar_forward.z));
        
        double dot_az = glm::dot(flat_target, flat_forward);
        double min_dot_az = std::cos(glm::radians(config.fov_azimuth_deg / 2.0));
        
        if (dot_az < min_dot_az) return ret; // Outside Horizontal FOV

        // Elevation Check (Simple cone)
        if (std::abs(to_target_dir.y) > std::sin(glm::radians(config.fov_elevation_deg / 2.0))) {
             // return ret; // Optional strict elevation check
        }

        // 3. HIT DETECTED
        ret.detected = true;
        ret.range = std::sqrt(dist_sq);
        ret.azimuth = std::atan2(to_target_dir.x, to_target_dir.z); // Relative to World Z (North)
        ret.elevation = std::asin(to_target_dir.y);

        // 4. DOPPLER PHYSICS
        ret.velocity = glm::dot(target.get_velocity(), to_target_dir);

        // 5. SIGNAL STRENGTH (With Jamming)
        double r4 = ret.range * ret.range * ret.range * ret.range;
        double rcs = target.get_rcs();
        double power_received = (TX_POWER_WATTS * rcs) / (r4 + 1e-9);
        
        // Use the DYNAMIC noise floor (Thermal + Jamming)
        ret.snr_db = 10.0 * std::log10(power_received / current_noise_floor);

        // 6. INJECT SENSOR ERROR (Gaussian Noise)
        ret.range += math::Random::gaussian(config.noise_range_m);
        ret.azimuth += math::Random::gaussian(config.noise_angle_rad);
        ret.velocity += math::Random::gaussian(config.noise_vel_ms);
        ret.snr_db += math::Random::gaussian(1.0); // Scintillation

        return ret;
    }
}