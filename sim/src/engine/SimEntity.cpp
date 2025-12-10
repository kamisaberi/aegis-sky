#include "sim/engine/SimEntity.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace aegis::sim::engine {

    SimEntity::SimEntity(const std::string& name, glm::dvec3 start_pos)
        : name_(name), type_(EntityType::UNKNOWN), position_(start_pos), velocity_(0) {}

    void SimEntity::set_micro_doppler(double speed, double hz, bool flap) {
        micro_doppler_ = {speed, hz, flap};
    }

    void SimEntity::add_waypoint(glm::dvec3 wp) {
        waypoints_.push(wp);
    }

    void SimEntity::update(double dt) {
        // Simple Path Following
        if (!waypoints_.empty()) {
            glm::dvec3 target = waypoints_.front();
            glm::dvec3 dir = target - position_;
            double dist = glm::length(dir);

            if (dist < 1.0) {
                waypoints_.pop();
            } else {
                velocity_ = glm::normalize(dir) * speed_;
            }
        }
        
        // Kinematics
        position_ += velocity_ * dt;
    }

    double SimEntity::get_instant_doppler_mod(double time) const {
        if (micro_doppler_.blade_speed_mps <= 0.0) return 0.0;

        // Phase = Time * Frequency
        double phase = time * micro_doppler_.blade_rate_hz * 2.0 * M_PI;

        if (micro_doppler_.is_flapping) {
            // Bird: Low freq, high amplitude flapping
            return std::sin(phase) * 2.0; 
        } else {
            // Drone: High freq, 'flash' effect
            // We simulate the blade tip radial velocity component
            // 0.1 factor because blades are small relative to body
            return std::sin(phase) * micro_doppler_.blade_speed_mps * 0.15;
        }
    }
}