#include "sim/phenomenology/optics/MockRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace aegis::sim::phenomenology {

    MockRenderer::MockRenderer(int w, int h) : width_(w), height_(h), mode_(RenderMode::VISIBLE) {
        buffer_.resize(w * h * 3);
        proj_matrix_ = glm::perspective(glm::radians(60.0), (double)w/h, 0.1, 3000.0);
        sun_direction_ = glm::normalize(glm::dvec3(0.5, 1.0, 0.5));
    }

    void MockRenderer::set_camera_orientation(const glm::dvec3& fwd) {
        current_facing_ = glm::normalize(fwd);
        glm::dvec3 eye(0,0,0);
        view_matrix_ = glm::lookAt(eye, eye + current_facing_, glm::dvec3(0,1,0));
    }

    void MockRenderer::clear() {
        uint8_t val = (mode_ == RenderMode::VISIBLE) ? 15 : 0;
        std::fill(buffer_.begin(), buffer_.end(), val);
    }

    // Helper: Project 3D to 2D Screen Space
    glm::dvec2 MockRenderer::project(const glm::dvec3& world) {
        glm::dvec4 clip = proj_matrix_ * view_matrix_ * glm::dvec4(world, 1.0);
        if (clip.w <= 0.1) return {-1, -1};
        glm::dvec3 ndc = glm::dvec3(clip) / clip.w;
        return { (ndc.x + 1.0) * 0.5 * width_, (1.0 - ndc.y) * 0.5 * height_ };
    }

    void MockRenderer::render_entity(const engine::SimEntity& ent, const glm::dvec3& cam_pos, double dt) {
        // 1. Current Pos
        glm::dvec2 curr_px = project(ent.get_position());
        if (curr_px.x < 0) return;

        // 2. Motion Blur (Previous Pos)
        glm::dvec3 prev_world = ent.get_position() - (ent.get_velocity() * dt);
        glm::dvec2 prev_px = project(prev_world);

        // 3. Color
        uint8_t c = 255;
        if (mode_ == RenderMode::THERMAL) {
            double k = ent.get_temperature();
            c = static_cast<uint8_t>(std::clamp((k - 280.0)/60.0, 0.0, 1.0) * 255.0);
        }

        // 4. Draw
        double speed = glm::distance(curr_px, prev_px);
        
        if (speed > 2.0 && speed < 100.0) {
            // Draw Motion Blur Line (Simplified Bresenham)
            int x0 = prev_px.x, y0 = prev_px.y;
            int x1 = curr_px.x, y1 = curr_px.y;
            int dx = abs(x1-x0), dy = abs(y1-y0);
            int sx = x0<x1 ? 1 : -1, sy = y0<y1 ? 1 : -1; 
            int err = dx-dy;
            
            while(true) {
                if (x0>=0 && x0<width_ && y0>=0 && y0<height_) {
                    int idx = (y0*width_ + x0)*3;
                    buffer_[idx] = c; buffer_[idx+1] = c; buffer_[idx+2] = c;
                }
                if (x0==x1 && y0==y1) break;
                int e2 = 2*err;
                if (e2 > -dy) { err -= dy; x0 += sx; }
                if (e2 < dx) { err += dx; y0 += sy; }
            }
        } else {
            // Draw Dot
            int cx = curr_px.x, cy = curr_px.y;
            for(int y=-1; y<=1; y++) {
                for(int x=-1; x<=1; x++) {
                    if (cx+x >=0 && cx+x < width_ && cy+y >=0 && cy+y < height_) {
                        int idx = ((cy+y)*width_ + (cx+x))*3;
                        buffer_[idx]=c; buffer_[idx+1]=c; buffer_[idx+2]=c;
                    }
                }
            }
        }
    }
    
    void MockRenderer::apply_effects(double fog) {
        // Sun Glare
        if (mode_ == RenderMode::VISIBLE) {
            double dot = glm::dot(current_facing_, sun_direction_);
            if (dot > 0.95) {
                uint8_t add = static_cast<uint8_t>((dot-0.95)/0.05 * 255);
                for(auto& b : buffer_) b = std::min(255, b + add);
            }
        }
    }
}