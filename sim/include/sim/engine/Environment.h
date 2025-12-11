#pragma once
#include <vector>
#include <glm/glm.hpp>

namespace aegis::sim::engine {

    struct AABB {
        glm::dvec3 min;
        glm::dvec3 max;
    };

    class Environment {
    public:
        // Adds a building (Axis Aligned Bounding Box)
        void add_building(glm::dvec3 center, glm::dvec3 size);

        // Returns true if the ray from 'start' to 'end' hits any building
        bool check_occlusion(glm::dvec3 start, glm::dvec3 end) const;

    private:
        std::vector<AABB> buildings_;
    };
}