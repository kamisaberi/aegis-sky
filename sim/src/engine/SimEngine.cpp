#include "sim/engine/SimEngine.h"

// Subsystems
#include "sim/engine/ScenarioLoader.h"
#include "sim/bridge_server/ShmWriter.h"
#include "sim/phenomenology/optics/MockRenderer.h"
#include "sim/physics/RadarPhysics.h"
#include "sim/physics/GimbalPhysics.h"
#include "sim/physics/DroneDynamics.h"
#include "sim/physics/SwarmPhysics.h" // You previously created this
#include "sim/engine/TerrainSystem.h" // You previously created this
#include "sim/engine/Environment.h"
#include "sim/engine/WeatherSystem.h"
#include "sim/math/Random.h" 

#include <spdlog/spdlog.h>
#include <thread>
#include <cmath>

namespace aegis::sim::engine {

    // Simple Projectile for Kinetic Interceptors
    struct Projectile {
        glm::dvec3 pos, vel;
        bool active = true;
        double age = 0;
    };

    SimEngine::SimEngine() : is_running_(false), is_headless_(false) {
        math::Random::init();
        bridge_ = std::make_unique<bridge::ShmWriter>();
        renderer_ = std::make_unique<phenomenology::MockRenderer>(1920, 1080);
        environment_ = std::make_unique<Environment>();
        
        // Add Urban Environment (Warehouse)
        environment_->add_building(glm::dvec3(0, 15, 200), glm::dvec3(60, 30, 20));

        // Physics Defaults
        drone_phys_config_ = {1.2, 0.3, 30.0};
        radar_config_ = {120.0, 30.0, 3000.0, 0.5, 0.01, 0.2};
        global_wind_ = glm::dvec3(2.0, 0.0, 1.0);
    }

    SimEngine::~SimEngine() { if (bridge_) bridge_->cleanup(); }

    void SimEngine::initialize(const std::string& path) {
        entities_ = ScenarioLoader::load_mission(path);
        if (entities_.empty()) throw std::runtime_error("Empty Mission.");
        if (!bridge_->initialize()) throw std::runtime_error("Bridge Failed.");
        
        is_running_ = true;
        spdlog::info("[Sim] Matrix Online. Systems: [RADAR][OPTICS][EW][SWARM]");
    }

    void SimEngine::run() {
        std::vector<Projectile> projectiles;
        glm::dvec3 sensor_pos(0,0,0);
        physics::BoidConfig boid_cfg; // Defaults

        while(is_running_) {
            // --- 0. TIME & WEATHER ---
            time_manager_.tick();
            double dt = time_manager_.get_delta_time();
            double now = time_manager_.get_total_time();
            uint64_t frame = time_manager_.get_frame_count();

            // Dynamic Weather Event (Example: Storm at T=30)
            if (now > 30.0) weather_.set_condition(25.0, 0.3, 8.0);

            // --- 1. BRIDGE INPUT ---
            ipc::ControlCommand cmd = bridge_->get_latest_command();

            // --- 2. EFFECTORS (Weapons) ---
            
            // A. Kinetic (Projectiles)
            if (cmd.fire_trigger) {
                static double last_shot = 0;
                if (now - last_shot > 0.1) { // 600 RPM
                    projectiles.push_back({sensor_pos, gimbal_.get_forward_vector()*850.0, true, 0});
                    last_shot = now;
                    spdlog::info("💥 KINETIC ROUND FIRED");
                }
            }
            
            // B. Directed Energy (Laser)
            if (cmd.laser_active) {
                glm::dvec3 laser_dir = gimbal_.get_forward_vector();
                for (auto& e : entities_) {
                    if (e->is_destroyed()) continue;
                    
                    glm::dvec3 to_target = glm::normalize(e->get_position() - sensor_pos);
                    // Check alignment (Beam width ~1 mrad)
                    if (glm::dot(to_target, laser_dir) > 0.99995) {
                        double range = glm::distance(sensor_pos, e->get_position());
                        // Simple HEL Damage Model: 10kW laser degraded by weather
                        double power = 10000.0 * std::exp(-0.0001 * range); // Atmospheric loss
                        if (weather_.get_state().fog_density > 0) power *= 0.1; // Fog blocks lasers
                        
                        e->apply_thermal_damage(power * dt);
                        if (e->is_destroyed()) spdlog::critical("🔥 LASER KILL: {}", e->get_name());
                    }
                }
            }
            
            // Update Projectiles
            for (auto& p : projectiles) {
                if (!p.active) continue;
                p.vel.y += -9.81 * dt; p.pos += p.vel * dt; p.age += dt;
                
                // Terrain Collision
                if (p.pos.y < TerrainSystem::get_height(p.pos.x, p.pos.z)) p.active = false;

                // Entity Collision
                for (auto& e : entities_) {
                    if (!e->is_destroyed() && glm::distance(p.pos, e->get_position()) < 1.0) {
                        spdlog::critical("🎯 KINETIC HIT: {}", e->get_name());
                        e->destroy();
                        p.active = false;
                    }
                }
                if (p.age > 5.0) p.active = false;
            }

            // --- 3. HARDWARE DYNAMICS ---
            gimbal_.update(dt, cmd.pan_velocity, cmd.tilt_velocity);
            glm::dvec3 facing = gimbal_.get_forward_vector();

            // --- 4. ENTITY PHYSICS (Swarms & Aerodynamics) ---
            for(auto& e : entities_) {
                if (e->is_destroyed()) { e->update(dt); continue; }

                // A. Swarm Logic (Boids)
                if (e->get_swarm_id() != -1) {
                    glm::dvec3 flock = physics::SwarmPhysics::calculate_flocking_force(*e, entities_, boid_cfg);
                    e->set_velocity(e->get_velocity() + flock * dt);
                }

                // B. Base Aerodynamics
                physics::DroneDynamics::apply_physics(*e, drone_phys_config_, dt);
                
                // C. Wind & Turbulence
                glm::dvec3 gust(math::Random::gaussian(0.5), math::Random::gaussian(0.2), math::Random::gaussian(0.5));
                e->set_velocity(e->get_velocity() + (global_wind_ * 0.1 + gust) * dt);
                
                e->update(dt);

                // D. Terrain Crash Check
                double h = TerrainSystem::get_height(e->get_position().x, e->get_position().z);
                if (e->get_position().y < h) {
                    e->set_position(glm::dvec3(e->get_position().x, h, e->get_position().z));
                    e->destroy();
                }
            }

            // --- 5. RADAR SENSOR (Multipath + EW) ---
            std::vector<ipc::SimRadarPoint> radar_hits;
            double noise = physics::RadarPhysics::calculate_environment_noise(entities_, sensor_pos);
            
            for(auto& e : entities_) {
                // Occlusion Checks
                if (environment_->check_occlusion(sensor_pos, e->get_position())) continue;
                if (TerrainSystem::check_occlusion(sensor_pos, e->get_position())) continue;

                // Scan (Returns Direct + Multipath ghosts)
                auto returns = physics::RadarPhysics::scan_target(
                    sensor_pos, facing, *e, radar_config_, noise, weather_.get_state(), now
                );

                for(const auto& ret : returns) {
                    ipc::SimRadarPoint hit;
                    // Polar -> Cartesian
                    hit.x = ret.range * sin(ret.azimuth) * cos(ret.elevation);
                    hit.y = ret.range * sin(ret.elevation);
                    hit.z = ret.range * cos(ret.azimuth) * cos(ret.elevation);
                    hit.velocity = ret.velocity;
                    hit.snr_db = ret.snr_db;
                    hit.object_id = 1;
                    radar_hits.push_back(hit);
                }
            }

            // --- 6. VISION SENSOR (Optics) ---
            if (!is_headless_) {
                // Auto Day/Night cycle
                // renderer_->set_render_mode(now > 3600 ? RenderMode::THERMAL : RenderMode::VISIBLE);
                renderer_->set_camera_orientation(facing);
                renderer_->clear();
                
                for(auto& e : entities_) {
                    if (!environment_->check_occlusion(sensor_pos, e->get_position()) &&
                        !TerrainSystem::check_occlusion(sensor_pos, e->get_position())) 
                    {
                        renderer_->render_entity(*e, sensor_pos, dt);
                    }
                }
                renderer_->apply_environmental_effects(weather_.get_state().fog_density);
            }

            // --- 7. BRIDGE PUBLISH ---
            bridge_->publish_frame(frame, now, radar_hits);
            
            // --- 8. PACING ---
            if (!is_headless_) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    void SimEngine::stop() { is_running_ = false; }
    void SimEngine::set_headless(bool h) { is_headless_ = h; }
}