#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

namespace aegis::sim::engine {
    class TerrainSystem {
    public:
        // Rolling Hills Generator
        static double get_height(double x, double z) {
            // Wave 1: Large terrain features
            double h1 = std::sin(x * 0.005) * std::cos(z * 0.005) * 30.0;
            // Wave 2: Local bumps
            double h2 = std::sin(x * 0.02 + 1.0) * 5.0;
            return std::max(0.0, h1 + h2);
        }

        // Raymarching for Occlusion (Is the target behind a hill?)
        static bool check_occlusion(const glm::dvec3& sensor, const glm::dvec3& target) {
            glm::dvec3 dir = target - sensor;
            double dist = glm::length(dir);
            dir /= dist;

            // Check 20 points along the line of sight
            for(int i = 1; i < 20; ++i) {
                glm::dvec3 p = sensor + dir * (dist * (double)i / 20.0);
                if (p.y < get_height(p.x, p.z)) return true; 
            }
            return false;
        }
    };
}