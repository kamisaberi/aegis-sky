#pragma once
#include <glm/glm.hpp>
#include <cmath>

namespace aegis::sim::engine {
    class TerrainSystem {
    public:
        // Returns the ground height (Y) at world coordinates (X, Z)
        static double get_height(double x, double z) {
            // Simple Rolling Hills generation
            // Wave 1: Large hills
            double h1 = std::sin(x * 0.01) * std::cos(z * 0.01) * 20.0;
            // Wave 2: Small bumps
            double h2 = std::sin(x * 0.05 + 1.5) * 2.0;
            
            // Base floor is 0
            return std::max(0.0, h1 + h2);
        }

        // Checks if line-of-sight is blocked by a hill
        static bool check_terrain_occlusion(glm::dvec3 sensor, glm::dvec3 target) {
            // Raymarching: Step along the ray and check height
            glm::dvec3 dir = target - sensor;
            double dist = glm::length(dir);
            dir /= dist;

            int steps = 20; // Check 20 points along the line
            for(int i = 1; i < steps; ++i) {
                glm::dvec3 p = sensor + dir * (dist * (double)i / steps);
                double ground_h = get_height(p.x, p.z);
                if (p.y < ground_h) return true; // Ray went underground
            }
            return false;
        }
    };
}