#include "sim/engine/SimEngine.h"
#include "sim/engine/ScenarioLoader.h"
#include "sim/bridge_server/ShmWriter.h"
#include "sim/phenomenology/optics/MockRenderer.h"
#include "sim/physics/RadarPhysics.h"
#include "sim/physics/GimbalPhysics.h"
#include "sim/engine/Environment.h"
#include "sim/math/Random.h" 

#include <spdlog/spdlog.h>
#include <thread>
#include <cmath>

namespace aegis::sim::engine {

    struct Projectile {
        glm::dvec3 position;
        glm::dvec3 velocity;
        bool active = true;
        double age = 0.0;
    };

    SimEngine::SimEngine() : is_running_(false), is_headless_(false) {
        math::Random::init();
        bridge_ = std::make_unique<bridge::ShmWriter>();
        renderer_ = std::make_unique<phenomenology::MockRenderer>(1920, 1080);
        environment_ = std::make_unique<Environment>();
        
        // Add Ground Plane (Implicit at Y=0)
        environment_->add_building(glm::dvec3(0, 10, 200), glm::dvec3(50, 20, 10)); // Warehouse

        drone_phys_config_ = {1.2, 0.3, 30.0};
        radar_config_ = {120.0, 30.0, 2000.0, 0.5, 0.01, 0.2};
        global_wind_ = glm::dvec3(2.0, 0.0, 1.0);
    }

    SimEngine::~SimEngine() { if (bridge_) bridge_->cleanup(); }

    void SimEngine::initialize(const std::string& scenario_path) {
        entities_ = ScenarioLoader::load_mission(scenario_path);
        if (entities_.empty()) throw std::runtime_error("No entities.");
        if (!bridge_->initialize()) throw std::runtime_error("Bridge Failed.");
        is_running_ = true;
    }

    void SimEngine::run() {
        spdlog::info("[Sim] Engine Started. Headless: {}", is_headless_);
        
        glm::dvec3 sensor_pos(0, 0, 0); 
        std::vector<Projectile> projectiles;
        
        while (is_running_) {
            time_manager_.tick();
            double dt = time_manager_.get_delta_time();
            uint64_t frame = time_manager_.get_frame_count();

            // 1. INPUT
            ipc::ControlCommand cmd = bridge_->get_latest_command();
            
            // 2. FIRE CONTROL (Kinetic Physics)
            if (cmd.fire_trigger) {
                // Simple cooldown check
                static double last_fire = 0;
                if (time_manager_.get_total_time() - last_fire > 0.1) { // 600 RPM
                    Projectile p;
                    p.position = sensor_pos;
                    p.velocity = gimbal_.get_forward_vector() * 800.0; // 800 m/s
                    projectiles.push_back(p);
                    last_fire = time_manager_.get_total_time();
                }
            }

            // Update Projectiles
            for (auto& p : projectiles) {
                if (!p.active) continue;
                p.velocity.y += -9.81 * dt; // Gravity
                p.position += p.velocity * dt;
                p.age += dt;

                // Check Kill
                for (auto& entity : entities_) {
                    if (glm::distance(p.position, entity->get_position()) < 1.0) {
                        spdlog::critical("🎯 TARGET DESTROYED: {}", entity->get_name());
                        // Teleport entity away or mark dead
                        entity->set_position(glm::dvec3(0, -1000, 0)); 
                        p.active = false;
                    }
                }
                if (p.position.y < 0 || p.age > 5.0) p.active = false;
            }

            // 3. GIMBAL & DRONE PHYSICS
            gimbal_.update(dt, cmd.pan_velocity, cmd.tilt_velocity);
            glm::dvec3 sensor_facing = gimbal_.get_forward_vector();

            for (auto& entity : entities_) {
                physics::DroneDynamics::apply_physics(*entity, drone_phys_config_, dt);
                // Wind
                glm::dvec3 gust(math::Random::gaussian(0.2), math::Random::gaussian(0.1), math::Random::gaussian(0.2));
                entity->set_velocity(entity->get_velocity() + (global_wind_ * 0.05 + gust) * dt);
                entity->update(dt);
            }

            // 4. RADAR STEP (Multipath Enabled)
            std::vector<ipc::SimRadarPoint> radar_hits;
            double noise_floor = physics::RadarPhysics::calculate_environment_noise(entities_, sensor_pos);

            for (auto& entity : entities_) {
                if (environment_->check_occlusion(sensor_pos, entity->get_position())) continue;

                // ** NEW: Scan for Multipath **
                // We assume scan_target returns a vector (Direct + Ghost)
                // (Using the logic described in the thought process above)
                auto result = physics::RadarPhysics::cast_ray(sensor_pos, sensor_facing, *entity, radar_config_, noise_floor);
                
                if (result.detected) {
                    ipc::SimRadarPoint hit;
                    // ... [Conversion to IPC struct] ...
                    // Add Multipath Ghost if low altitude (y < 10m)
                    if (entity->get_position().y < 10.0) {
                        // 50% chance of ghost
                        if (math::Random::uniform(0, 1) > 0.5) {
                            ipc::SimRadarPoint ghost = hit;
                            ghost.y = -hit.y; // Mirror
                            ghost.snr_db -= 6.0;
                            radar_hits.push_back(ghost);
                        }
                    }
                    radar_hits.push_back(hit);
                }
            }

            // 5. VISION STEP
            if (!is_headless_) {
                renderer_->set_camera_orientation(sensor_facing);
                renderer_->clear();
                for (auto& entity : entities_) {
                    if (!environment_->check_occlusion(sensor_pos, entity->get_position()))
                        renderer_->render_entity(*entity, sensor_pos);
                }
            }

            // 6. BRIDGE
            bridge_->publish_frame(frame, time_manager_.get_total_time(), radar_hits);

            // 7. PACING
            if (!is_headless_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    
    void SimEngine::stop() { is_running_ = false; }
    void SimEngine::set_headless(bool h) { is_headless_ = h; }
}