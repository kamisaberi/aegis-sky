#include "sim/physics/RadarPhysics.h"
#include "sim/math/Random.h"
#include <glm/gtx/norm.hpp> // For length2
#include <cmath>
#include <algorithm>
#include <iostream>

namespace aegis::sim::physics {

    // -------------------------------------------------------------------------
    // CONSTANTS & CONFIG
    // -------------------------------------------------------------------------
    static constexpr double TX_POWER_WATTS = 200.0;     
    static constexpr double THERMAL_NOISE_FLOOR = 1e-13; 
    static constexpr double JAMMER_POWER_WATTS = 10.0; 
    static constexpr double GROUND_REFLECTIVITY = 0.5; // for Multipath

    // -------------------------------------------------------------------------
    // EW: Calculate Noise Floor (Thermal + Jammers)
    // -------------------------------------------------------------------------
    double RadarPhysics::calculate_environment_noise(
        const std::vector<std::shared_ptr<engine::SimEntity>>& entities, 
        const glm::dvec3& radar_pos
    ) {
        double total_noise = THERMAL_NOISE_FLOOR;

        for (const auto& entity : entities) {
            // Check for 'Jammer' in name as a simplified capability flag
            // In prod, use entity->capabilities.has_jammer
            if (entity->get_name().find("Jammer") != std::string::npos) {
                double dist_sq = glm::distance2(radar_pos, entity->get_position());
                if (dist_sq < 1.0) dist_sq = 1.0; 

                // Jamming follows 1/R^2 (One-Way Trip) - Extremely effective
                double received_jamming = JAMMER_POWER_WATTS / (4.0 * M_PI * dist_sq);
                total_noise += received_jamming;
            }
        }
        return total_noise;
    }

    // -------------------------------------------------------------------------
    // INTERNAL: Single Ray Math
    // -------------------------------------------------------------------------
    RadarReturn cast_ray_internal(
        const glm::dvec3& origin, 
        const glm::dvec3& beam_dir, 
        const glm::dvec3& target_pos,
        const glm::dvec3& target_vel,
        double target_rcs,
        const RadarConfig& config,
        double noise_floor,
        const engine::WeatherState& weather
    ) {
        RadarReturn ret = {false, 0,0,0,0, -100.0};

        // 1. Geometry Check
        glm::dvec3 L = target_pos - origin;
        double dist_sq = glm::length2(L);
        if (dist_sq > (config.max_range * config.max_range)) return ret;

        // 2. FOV Check
        glm::dvec3 to_target = glm::normalize(L);
        glm::dvec3 flat_target = glm::normalize(glm::dvec3(to_target.x, 0, to_target.z));
        glm::dvec3 flat_forward = glm::normalize(glm::dvec3(beam_dir.x, 0, beam_dir.z));
        
        if (glm::dot(flat_target, flat_forward) < std::cos(glm::radians(config.fov_azimuth_deg / 2.0))) {
            return ret; 
        }

        // 3. Hit Detected - Calculate Physics
        ret.detected = true;
        ret.range = std::sqrt(dist_sq);
        ret.azimuth = std::atan2(to_target.x, to_target.z);
        ret.elevation = std::asin(to_target.y);
        ret.velocity = glm::dot(target_vel, to_target); // Doppler

        // 4. Signal Strength (Radar Equation)
        double r4 = ret.range * ret.range * ret.range * ret.range;
        double power_received = (TX_POWER_WATTS * target_rcs) / (r4 + 1e-9);
        
        // 5. Atmospheric Attenuation (Rain Fade)
        // Approx 0.02 dB per km per mm/hr
        double dist_km = ret.range / 1000.0;
        double rain_loss_db = 0.02 * weather.rain_intensity * dist_km * 2.0; // Round trip
        
        double snr_linear = power_received / noise_floor;
        ret.snr_db = 10.0 * std::log10(snr_linear) - rain_loss_db;

        // 6. Sensor Noise Injection
        ret.range    += math::Random::gaussian(config.noise_range_m);
        ret.azimuth  += math::Random::gaussian(config.noise_angle_rad);
        ret.elevation+= math::Random::gaussian(config.noise_angle_rad);
        ret.velocity += math::Random::gaussian(config.noise_vel_ms);
        ret.snr_db   += math::Random::gaussian(1.0); // Scintillation

        return ret;
    }

    // -------------------------------------------------------------------------
    // PUBLIC: Scan Target (Includes Multipath)
    // -------------------------------------------------------------------------
    std::vector<RadarReturn> RadarPhysics::scan_target(
        const glm::dvec3& radar_pos, 
        const glm::dvec3& radar_forward, 
        const engine::SimEntity& target,
        const RadarConfig& config,
        double noise_floor,
        const engine::WeatherState& weather
    ) {
        std::vector<RadarReturn> hits;

        // 1. Direct Path
        RadarReturn direct = cast_ray_internal(
            radar_pos, radar_forward, 
            target.get_position(), target.get_velocity(), target.get_rcs(),
            config, noise_floor, weather
        );
        
        if (direct.detected) hits.push_back(direct);

        // 2. Multipath (Ghosting)
        // If target is low altitude (< 20m), assume ground bounce
        if (target.get_position().y < 20.0 && target.get_position().y > 0.5) {
            
            // Mirror target underground
            glm::dvec3 ghost_pos = target.get_position();
            ghost_pos.y = -ghost_pos.y; 

            // Calculate ghost return
            RadarReturn ghost = cast_ray_internal(
                radar_pos, radar_forward, 
                ghost_pos, target.get_velocity(), target.get_rcs(),
                config, noise_floor, weather
            );

            if (ghost.detected) {
                // Apply Ground Reflection Loss
                ghost.snr_db -= 6.0; 
                // Flag as ghost (optional, mostly for debug/truth data)
                // In real radar, this just looks like a target underground
                hits.push_back(ghost);
            }
        }

        return hits;
    }
}