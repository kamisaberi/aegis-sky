#include "sim/engine/SimEngine.h"

#include "sim/engine/ScenarioLoader.h"
#include "sim/bridge_server/ShmWriter.h"
#include "sim/phenomenology/optics/MockRenderer.h"
#include "sim/physics/RadarPhysics.h"
#include "sim/math/Random.h" 

#include <spdlog/spdlog.h>
#include <thread>
#include <cmath>

namespace aegis::sim::engine {

    SimEngine::SimEngine() : is_running_(false) {
        // 1. Math Init
        math::Random::init();

        // 2. Subsystems Init
        bridge_ = std::make_unique<bridge::ShmWriter>();
        
        // 1920x1080 resolution
        renderer_ = std::make_unique<phenomenology::MockRenderer>(1920, 1080);
        
        // 3. Physics Defaults
        drone_phys_config_.mass_kg = 1.2;
        drone_phys_config_.drag_coeff = 0.3;
        drone_phys_config_.max_thrust_n = 30.0;

        // 4. Radar Config (Echodyne-style)
        radar_config_.fov_azimuth_deg = 120.0;
        radar_config_.fov_elevation_deg = 30.0;
        radar_config_.max_range = 1000.0;
        radar_config_.noise_range_m = 0.5;
        radar_config_.noise_angle_rad = 0.01;
        radar_config_.noise_vel_ms = 0.2;

        // 5. Environment
        global_wind_ = glm::dvec3(2.0, 0.0, 1.0);
    }

    SimEngine::~SimEngine() {
        if (bridge_) bridge_->cleanup();
    }

    void SimEngine::initialize(const std::string& scenario_path) {
        spdlog::info("[Sim] Booting Aegis Matrix Kernel...");

        // Load Entities
        entities_ = ScenarioLoader::load_mission(scenario_path);
        if (entities_.empty()) {
            throw std::runtime_error("ScenarioLoader returned 0 entities.");
        }

        // Init Bridge
        if (!bridge_->initialize()) {
            throw std::runtime_error("Failed to initialize /dev/shm bridge.");
        }

        is_running_ = true;
        spdlog::info("[Sim] Ready. Loaded {} entities.", entities_.size());
    }

    void SimEngine::run() {
        spdlog::info("[Sim] Loop Started.");
        
        glm::dvec3 sensor_pos(0, 0, 0); // Pod is at Origin
        
        while (is_running_) {
            // --- TIME UPDATE ---
            time_manager_.tick();
            double dt = time_manager_.get_delta_time();
            double now = time_manager_.get_total_time();
            uint64_t frame = time_manager_.get_frame_count();

            // --- 1. READ INPUT (Bridge) ---
            // Read latest PTU command from Core
            ipc::ControlCommand cmd = bridge_->get_latest_command();
            
            // --- 2. UPDATE GIMBAL ---
            // Physically rotate the sensor based on velocity commands
            gimbal_.update(dt, cmd.pan_velocity, cmd.tilt_velocity);
            
            // Get the new Forward Vector (Where are we looking?)
            glm::dvec3 sensor_facing = gimbal_.get_forward_vector();

            // --- 3. PHYSICS STEP (With Wind) ---
            for (auto& entity : entities_) {
                // Apply Aerodynamics
                physics::DroneDynamics::apply_physics(*entity, drone_phys_config_, dt);
                
                // Apply Wind Drift (Turbulence)
                glm::dvec3 gust;
                gust.x = math::Random::gaussian(0.2); 
                gust.y = math::Random::gaussian(0.1); 
                gust.z = math::Random::gaussian(0.2);
                
                // Simple drift application
                glm::dvec3 current_vel = entity->get_velocity();
                entity->set_velocity(current_vel + (global_wind_ * 0.05 + gust) * dt);
                
                entity->update(dt);
            }

            // --- 4. RADAR STEP ---
            std::vector<ipc::SimRadarPoint> radar_hits;

            // A. Real Targets (filtered by FOV and Physics)
            for (auto& entity : entities_) {
                auto result = physics::RadarPhysics::cast_ray(sensor_pos, sensor_facing, *entity, radar_config_);
                
                if (result.detected) {
                    ipc::SimRadarPoint hit;
                    // Convert Spherical result back to Cartesian relative to sensor for the output
                    // (Or pass raw spherical if your Core expects that)
                    // For Simplicity, we construct the Noisy Cartesian position:
                    double r = result.range;
                    double az = result.azimuth; // Relative to bore-sight
                    double el = result.elevation;

                    // Note: This needs complex rotation math to put back into World Space
                    // For MVP, we will simplify and trust the RadarPhysics output is 'Sensor Relative'
                    // We just pass the noisy values directly.
                    hit.x = static_cast<float>(r * std::sin(az));
                    hit.y = static_cast<float>(r * std::sin(el));
                    hit.z = static_cast<float>(r * std::cos(az)); // Depth
                    
                    hit.velocity = static_cast<float>(result.velocity);
                    hit.snr_db = static_cast<float>(result.snr_db);
                    hit.object_id = 1; 

                    radar_hits.push_back(hit);
                }
            }

            // B. Ground Clutter
            for (int i = 0; i < 3; ++i) {
                ipc::SimRadarPoint ghost;
                double r = math::Random::uniform(10.0, 500.0);
                ghost.x = static_cast<float>(math::Random::uniform(-50, 50));
                ghost.y = static_cast<float>(math::Random::uniform(-10, 10)); 
                ghost.z = static_cast<float>(r);
                ghost.velocity = static_cast<float>(math::Random::gaussian(0.5));
                ghost.snr_db = static_cast<float>(math::Random::uniform(5.0, 12.0));
                ghost.object_id = 0; 
                radar_hits.push_back(ghost);
            }

            // --- 5. VISION STEP ---
            // Update the renderer to look where the gimbal is pointing
            renderer_->set_camera_orientation(sensor_facing);
            
            // Set mode (Day vs Night based on simulation time?)
            // For now, toggle every 10 seconds for testing
            if (static_cast<int>(now) / 10 % 2 == 0) 
                renderer_->set_render_mode(phenomenology::RenderMode::VISIBLE);
            else 
                renderer_->set_render_mode(phenomenology::RenderMode::THERMAL);

            renderer_->clear();
            for (auto& entity : entities_) {
                renderer_->render_entity(*entity, sensor_pos);
            }

            // --- 6. BRIDGE PUBLISH ---
            // Send Radar + Frame ID
            bridge_->publish_frame(frame, now, radar_hits);
            
            // (Optional) Send Video Buffer logic here
            // bridge_->publish_video(renderer_->get_buffer());

            // --- LOGGING ---
            if (frame % 60 == 0) {
                 spdlog::info("[Sim] T:{:.1f}s | Gimbal Pan:{:.2f} | Mode:{} | Radar Hits:{}", 
                    now, gimbal_.get_current_pan(), 
                    (renderer_->get_mode() == phenomenology::RenderMode::VISIBLE ? "RGB" : "IR"),
                    radar_hits.size());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void SimEngine::stop() { is_running_ = false; }
}