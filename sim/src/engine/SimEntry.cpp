#include "sim/engine/SimEntity.h"

namespace aegis::sim::engine {

    SimEntity::SimEntity(const std::string& name, glm::dvec3 start_pos)
        : name_(name), position_(start_pos), velocity_(0), acceleration_(0), orientation_(1, 0, 0, 0) {}

    void SimEntity::update(double dt) {
        // Semi-Implicit Euler Integration (Standard for Games/Sims)
        velocity_ += acceleration_ * dt;
        position_ += velocity_ * dt;
    }
}