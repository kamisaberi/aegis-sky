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
    static constexpr double NOISE_FLOOR_WATTS = 1e-13;  
    static constexpr double HITBOX_RADIUS = 0.5; // Meters

    RadarReturn RadarPhysics::cast_ray(
        const glm::dvec3& radar_pos, 
        const glm::dvec3& radar_forward, // Normalized vector (0,0,1)
        const engine::SimEntity& target,
        const RadarConfig& config
    ) {
        RadarReturn ret = {false, 0.0, 0.0, 0.0, 0.0, -100.0};

        // ---------------------------------------------------------------------
        // 1. GEOMETRY CHECK (Is the target hit by the ray?)
        // ---------------------------------------------------------------------
        glm::dvec3 L = target.get_position() - radar_pos;
        
        // Distance check first (Optimization)
        double dist_sq = glm::length2(L);
        if (dist_sq > (config.max_range * config.max_range)) return ret;

        // ---------------------------------------------------------------------
        // 2. FIELD OF VIEW CHECK (Is it inside the cone?)
        // ---------------------------------------------------------------------
        glm::dvec3 to_target_dir = glm::normalize(L);

        // Azimuth Check (Horizontal Plane)
        // Flatten vectors to XZ plane
        glm::dvec3 flat_target = glm::normalize(glm::dvec3(to_target_dir.x, 0, to_target_dir.z));
        glm::dvec3 flat_forward = glm::normalize(glm::dvec3(radar_forward.x, 0, radar_forward.z));
        
        double dot_az = glm::dot(flat_target, flat_forward);
        double min_dot_az = std::cos(glm::radians(config.fov_azimuth_deg / 2.0));
        
        if (dot_az < min_dot_az) return ret; // Outside Horizontal FOV

        // Elevation Check (Vertical Plane)
        double dot_el = glm::dot(to_target_dir, flat_target); // Angle vs Horizon
        double min_dot_el = std::cos(glm::radians(config.fov_elevation_deg / 2.0));
        
        // Note: Simple dot check approximation for elevation
        if (std::abs(to_target_dir.y) > std::sin(glm::radians(config.fov_elevation_deg / 2.0))) {
             // return ret; // Strict elevation check (Optional)
        }

        // ---------------------------------------------------------------------
        // 3. CALCULATE PERFECT PHYSICS
        // ---------------------------------------------------------------------
        ret.detected = true;
        ret.range = std::sqrt(dist_sq);
        
        // Azimuth/Elevation calculation
        ret.azimuth = std::atan2(to_target_dir.x, to_target_dir.z);
        ret.elevation = std::asin(to_target_dir.y);

        // Doppler Velocity (Project target velocity onto Line of Sight)
        // Moving AWAY is positive, CLOSING is negative (standard convention varies, using standard)
        ret.velocity = glm::dot(target.get_velocity(), to_target_dir);

        // Signal Strength (Radar Equation)
        double r4 = ret.range * ret.range * ret.range * ret.range;
        double power_received = (TX_POWER_WATTS * target.get_rcs()) / (r4 + 1e-9);
        ret.snr_db = 10.0 * std::log10(power_received / NOISE_FLOOR_WATTS);

        // ---------------------------------------------------------------------
        // 4. INJECT CHAOS (Noise)
        // ---------------------------------------------------------------------
        // Only inject noise if the signal is valid
        
        // Range Noise
        ret.range += math::Random::gaussian(config.noise_range_m);
        
        // Angular Noise
        ret.azimuth += math::Random::gaussian(config.noise_angle_rad);
        ret.elevation += math::Random::gaussian(config.noise_angle_rad);
        
        // Doppler Noise
        ret.velocity += math::Random::gaussian(config.noise_vel_ms);

        // SNR Fluctuation (Scintillation)
        ret.snr_db += math::Random::gaussian(1.0); // +/- 1dB fluctuation

        return ret;
    }
}