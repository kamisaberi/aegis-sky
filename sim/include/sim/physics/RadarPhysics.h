#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "sim/engine/SimEntity.h"

namespace aegis::sim::physics {

// Add to struct RadarConfig
struct RadarConfig {
    double fov_azimuth_deg = 120.0;   // +/- 60 degrees left/right
    double fov_elevation_deg = 30.0;  // +/- 15 degrees up/down
    double max_range = 2000.0;        // Meters
    
    // Error Characteristics (The Noise)
    double noise_range = 0.5;         // +/- 0.5m error
    double noise_angle = 0.01;        // +/- 0.01 rad error
    double noise_vel = 0.2;           // +/- 0.2 m/s error
};

    struct RadarReturn {
        bool detected;
        double range;       // Distance (meters)
        double azimuth;     // Horizontal Angle (radians)
        double elevation;   // Vertical Angle (radians)
        double velocity;    // Radial Velocity (m/s) - Doppler
        double snr_db;      // Signal Strength
    };

    class RadarPhysics {
    public:
        // Simulates a single radar beam interacting with a target
        static RadarReturn cast_ray(
            const glm::dvec3& radar_pos, 
            const glm::dvec3& beam_dir, 
            const engine::SimEntity& target
        );
    };
}