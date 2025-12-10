#include "sim/phenomenology/optics/MockRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>

namespace aegis::sim::phenomenology {

    MockRenderer::MockRenderer(int width, int height) 
        : width_(width), height_(height), mode_(RenderMode::VISIBLE) {
        
        buffer_.resize(width * height * 3);
        // Standard Pinhole Model: 60 deg FOV, 2km Draw Distance
        proj_matrix_ = glm::perspective(glm::radians(60.0), (double)width/height, 0.1, 2000.0);
        
        // Default Sun: High Noon
        sun_direction_ = glm::normalize(glm::dvec3(0.5, 1.0, 0.5));
    }

    void MockRenderer::set_camera_orientation(const glm::dvec3& forward_vector) {
        current_facing_ = glm::normalize(forward_vector);
        glm::dvec3 eye(0, 0, 0);
        glm::dvec3 up(0, 1, 0);
        view_matrix_ = glm::lookAt(eye, eye + current_facing_, up);
    }

    void MockRenderer::clear() {
        // Base Sky Color
        uint8_t r = (mode_ == RenderMode::VISIBLE) ? 10 : 0;
        uint8_t g = (mode_ == RenderMode::VISIBLE) ? 15 : 0;
        uint8_t b = (mode_ == RenderMode::VISIBLE) ? 40 : 0;

        std::fill(buffer_.begin(), buffer_.end(), 0); // Reset
        for (size_t i = 0; i < buffer_.size(); i += 3) {
            buffer_[i] = r; buffer_[i+1] = g; buffer_[i+2] = b;
        }
    }

    void MockRenderer::apply_environmental_effects(double fog_density) {
        if (mode_ != RenderMode::VISIBLE) return; // Thermal cuts through fog/glare better

        // 1. Sun Glare (Saturation)
        double dot = glm::dot(current_facing_, sun_direction_);
        if (dot > 0.90) { // Within ~25 degrees of sun
            double intensity = (dot - 0.90) / 0.10; // 0.0 to 1.0
            uint8_t washout = static_cast<uint8_t>(intensity * 255.0);
            
            for (auto& byte : buffer_) {
                int val = byte + washout;
                byte = (val > 255) ? 255 : val;
            }
        }

        // 2. Fog (Contrast Reduction)
        if (fog_density > 0.0) {
            uint8_t fog_color = 100; // Grey
            for (auto& byte : buffer_) {
                // Lerp(current, fog_color, density)
                byte = static_cast<uint8_t>(byte * (1.0 - fog_density) + fog_color * fog_density);
            }
        }
    }

    void MockRenderer::render_entity(const engine::SimEntity& entity, const glm::dvec3& camera_pos) {
        glm::dvec4 clip_pos = proj_matrix_ * view_matrix_ * glm::dvec4(entity.get_position(), 1.0);
        
        if (clip_pos.w <= 0.1) return; // Behind camera
        
        glm::dvec3 ndc = glm::dvec3(clip_pos) / clip_pos.w;
        
        // Viewport Transform
        int screen_x = (ndc.x + 1.0) * 0.5 * width_;
        int screen_y = (1.0 - ndc.y) * 0.5 * height_;

        if (screen_x < 2 || screen_x >= width_ - 2 || screen_y < 2 || screen_y >= height_ - 2) return;

        // Color Logic
        uint8_t r, g, b;
        if (mode_ == RenderMode::THERMAL) {
            // Map Kelvin to Grayscale (Hotter = Whiter)
            double temp_k = entity.get_temperature();
            double norm = std::clamp((temp_k - 280.0) / 60.0, 0.0, 1.0);
            r = g = b = static_cast<uint8_t>(norm * 255.0);
        } else {
            // Visible: White Dot
            r = 255; g = 255; b = 255;
        }

        // Draw 3x3 Dot
        for(int y = -1; y <= 1; y++) {
            for(int x = -1; x <= 1; x++) {
                int idx = ((screen_y + y) * width_ + (screen_x + x)) * 3;
                buffer_[idx] = r; buffer_[idx+1] = g; buffer_[idx+2] = b;
            }
        }
    }
}