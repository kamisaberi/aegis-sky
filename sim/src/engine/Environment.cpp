#include "sim/engine/Environment.h"
#include <algorithm>

namespace aegis::sim::engine {

    void Environment::add_building(glm::dvec3 center, glm::dvec3 size) {
        glm::dvec3 half = size * 0.5;
        buildings_.push_back({center - half, center + half});
    }

    // Standard "Slab Method" for Ray-AABB Intersection
    bool ray_hit_box(const AABB& box, glm::dvec3 start, glm::dvec3 end) {
        glm::dvec3 dir = glm::normalize(end - start);
        double dist_to_target = glm::distance(start, end);
        
        glm::dvec3 inv_dir = 1.0 / dir;

        double t1 = (box.min.x - start.x) * inv_dir.x;
        double t2 = (box.max.x - start.x) * inv_dir.x;
        double t3 = (box.min.y - start.y) * inv_dir.y;
        double t4 = (box.max.y - start.y) * inv_dir.y;
        double t5 = (box.min.z - start.z) * inv_dir.z;
        double t6 = (box.max.z - start.z) * inv_dir.z;

        double tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
        double tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

        // Collision if tmax >= tmin and tmin is positive and closer than the target
        if (tmax < 0 || tmin > tmax) return false;
        
        // Only return true if the building is closer than the drone
        return tmin < dist_to_target;
    }

    bool Environment::check_occlusion(glm::dvec3 start, glm::dvec3 end) const {
        for (const auto& box : buildings_) {
            if (ray_hit_box(box, start, end)) return true;
        }
        return false;
    }
}