#pragma once
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "sim/engine/SimEntity.h"

namespace aegis::sim::phenomenology {

    class MockRenderer {
    public:
        MockRenderer(int width, int height);

        // Clears screen to black
        void clear();

        // Projects 3D entity to 2D and draws a dot
        void render_entity(const engine::SimEntity& entity, const glm::dvec3& camera_pos);

        // Returns raw RGB pointer for the Bridge
        const std::vector<uint8_t>& get_buffer() const;

    private:
        int width_;
        int height_;
        std::vector<uint8_t> buffer_; // RGB Array: [R, G, B, R, G, B...]
        
        // Camera Intrinsics (Field of View)
        glm::dmat4 proj_matrix_;
        glm::dmat4 view_matrix_;
    };
}