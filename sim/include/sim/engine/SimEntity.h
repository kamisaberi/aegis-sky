#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <queue>
#include <memory>

namespace aegis::sim::engine {

    enum class EntityType { QUADCOPTER, FIXED_WING, BIRD, UNKNOWN };

    // Physics configuration for Micro-Doppler signatures
    struct MicroDopplerProfile {
        double blade_speed_mps = 0.0; // Tip speed
        double blade_rate_hz = 0.0;   // Rotation rate
        bool is_flapping = false;     // Bird vs Rotor
    };

    class SimEntity {
    public:
        SimEntity(const std::string& name, glm::dvec3 start_pos);
        virtual ~SimEntity() = default;

        // Core Physics Update
        void update(double dt);

        // --- Configuration Setters ---
        void set_type(EntityType t) { type_ = t; }
        void set_rcs(double rcs) { rcs_ = rcs; }
        void set_speed(double s) { max_speed_ = s; }
        void set_temperature(double c) { temperature_k_ = c + 273.15; }
        void set_velocity(glm::dvec3 v) { velocity_ = v; }
        void set_position(glm::dvec3 p) { position_ = p; }
        void set_swarm_id(int id) { swarm_id_ = id; }
        
        // Micro-Doppler Setup
        void set_micro_doppler(double speed, double hz, bool flap);

        // Waypoint Logic
        void add_waypoint(glm::dvec3 wp);

        // --- Combat Logic ---
        // Apply Laser/Thermal damage (Joules)
        void apply_thermal_damage(double joules);
        bool is_destroyed() const { return is_destroyed_; }
        void destroy(); // Trigger destruction effects

        // --- Getters ---
        glm::dvec3 get_position() const { return position_; }
        glm::dvec3 get_velocity() const { return velocity_; }
        double get_speed() const { return max_speed_; }
        double get_rcs() const { return rcs_; }
        double get_temperature() const { return temperature_k_; }
        std::string get_name() const { return name_; }
        int get_swarm_id() const { return swarm_id_; }

        // Returns the instant velocity modulation (Blade Flash) at time t
        double get_instant_doppler_mod(double time) const;

    private:
        std::string name_;
        EntityType type_;
        int swarm_id_ = -1; // -1 = Independent, >0 = Swarm Member

        // Kinematics
        glm::dvec3 position_;
        glm::dvec3 velocity_;
        double max_speed_ = 10.0;

        // Signatures
        double rcs_ = 0.01;
        double temperature_k_ = 300.0;
        MicroDopplerProfile micro_doppler_;

        // Pathing
        std::queue<glm::dvec3> waypoints_;

        // Health
        double thermal_health_ = 1000.0; // Joules required to melt
        double max_health_ = 1000.0;
        bool is_destroyed_ = false;
    };
}