#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace aegis::sim::engine {

    class SimEntity {
    public:
        SimEntity(const std::string& name, glm::dvec3 start_pos);
        virtual ~SimEntity() = default;

        virtual void update(double dt);

        // Getters - Returning GLM types
        glm::dvec3 get_position() const { return position_; }
        glm::dvec3 get_velocity() const { return velocity_; }
        glm::dquat get_orientation() const { return orientation_; }
        std::string get_name() const { return name_; }

        // Setters
        void set_velocity(glm::dvec3 v) { velocity_ = v; }
        void set_acceleration(glm::dvec3 a) { acceleration_ = a; }

    protected:
        std::string name_;
        
        // Physics State
        glm::dvec3 position_;
        glm::dvec3 velocity_;
        glm::dvec3 acceleration_;
        glm::dquat orientation_; // Quaternion for rotation
    };

}