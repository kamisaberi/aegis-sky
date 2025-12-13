#include "sim/engine/SimEngine.h"

#include "sim/engine/ScenarioLoader.h"
#include "sim/bridge_server/ShmWriter.h"
#include "sim/phenomenology/optics/MockRenderer.h"
#include "sim/physics/RadarPhysics.h"
#include "sim/math/Random.h" // Needed for clutter

#include <spdlog/spdlog.h>
#include <thread>
#include <cmath>

namespace aegis::sim::engine {

    SimEngine::SimEngine() : is_running_(false) {
        // 1. Initialize Random Number Generator
        math::Random::init();

        // 2. Instantiate Subsystems
        bridge_ = std::make_unique<bridge::ShmWriter>();
        renderer_ = std::make_unique<phenomenology::MockRenderer>(1920, 1080);
        
        // 3. Default Physics (DJI Mavic)
        drone_phys_config_.mass_kg = 1.2;
        drone_phys_config_.drag_coeff = 0.3;
        drone_phys_config_.max_thrust_n = 30.0;

        // 4. Default Radar Config (Echodyne MESA-like)
        radar_config_.fov_azimuth_deg = 120.0;
        radar_config_.fov_elevation_deg = 20.0;
        radar_config_.max_range = 1000.0;
        radar_config_.noise_range_m = 0.5;    // 50cm accuracy
        radar_config_.noise_angle_rad = 0.01; // ~0.5 degree accuracy
        radar_config_.noise_vel_ms = 0.2;     // 0.2 m/s accuracy

        // 5. Environmental Conditions
        global_wind_ = glm::dvec3(2.0, 0.0, 1.0); // Slight breeze
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
        
        glm::dvec3 sensor_pos(0, 0, 0);
        glm::dvec3 radar_facing(0, 0, 1); // Z+

        while (is_running_) {
            // --- TIME UPDATE ---
            time_manager_.tick();
            double dt = time_manager_.get_delta_time();
            double now = time_manager_.get_total_time();
            uint64_t frame = time_manager_.get_frame_count();

            // --- A. PHYSICS STEP (With Wind) ---
            for (auto& entity : entities_) {
                // 1. Base Aerodynamics
                physics::DroneDynamics::apply_physics(*entity, drone_phys_config_, dt);
                
                // 2. Apply Wind Drift (Turbulence)
                // Drones compensate for wind, but imperfectly.
                // Apply 10% of wind vector as drift + random gusts.
                glm::dvec3 gust;
                gust.x = math::Random::gaussian(0.2); 
                gust.y = math::Random::gaussian(0.1); 
                gust.z = math::Random::gaussian(0.2);
                
                glm::dvec3 total_drift = (global_wind_ * 0.1) + gust;
                
                // "Nudge" the position (assuming SimEntity has this method)
                // If not, use: entity->set_position(entity->get_position() + total_drift * dt);
                glm::dvec3 new_pos = entity->get_position() + (total_drift * dt);
                // We assume SimEntity has a setter or we modify update logic. 
                // Ideally SimEntity::update handles velocity, so let's modify velocity:
                entity->set_velocity(entity->get_velocity() + (gust * dt)); // Add impulse
                
                entity->update(dt);
            }

            // --- B. RADAR STEP (With Clutter) ---
            std::vector<ipc::SimRadarPoint> radar_hits;

            // 1. Process Real Targets
            for (auto& entity : entities_) {
                auto result = physics::RadarPhysics::cast_ray(sensor_pos, radar_facing, *entity, radar_config_);
                
                if (result.detected) {
                    ipc::SimRadarPoint hit;
                    // Apply the noise-distorted polar coords back to Cartesian for the output
                    // (Or send raw Cartesian if RadarPhysics returned Cartesian)
                    // Here we convert the Noisy Polar to Noisy Cartesian:
                    double r = result.range;
                    double az = result.azimuth;
                    double el = result.elevation;

                    hit.x = static_cast<float>(r * std::sin(az) * std::cos(el));
                    hit.y = static_cast<float>(r * std::sin(el));
                    hit.z = static_cast<float>(r * std::cos(az) * std::cos(el));
                    
                    hit.velocity = static_cast<float>(result.velocity);
                    hit.snr_db = static_cast<float>(result.snr_db);
                    hit.object_id = 1; // Valid Target

                    radar_hits.push_back(hit);
                }
            }

            // 2. Generate Ground Clutter (False Positives)
            // Generate 3 random ghost points per frame to confuse the tracker
            for (int i = 0; i < 3; ++i) {
                ipc::SimRadarPoint ghost;
                double r = math::Random::uniform(10.0, 500.0);
                double az = math::Random::uniform(-1.0, 1.0); // Within FOV
                
                ghost.x = static_cast<float>(r * std::sin(az));
                ghost.y = static_cast<float>(math::Random::uniform(-2.0, 2.0)); // Low to ground
                ghost.z = static_cast<float>(r * std::cos(az));
                
                ghost.velocity = static_cast<float>(math::Random::gaussian(0.5)); // Near zero velocity
                ghost.snr_db = static_cast<float>(math::Random::uniform(5.0, 15.0)); // Weak signal
                ghost.object_id = 0; // 0 = Noise

                radar_hits.push_back(ghost);
            }

            // --- C. VISION STEP ---
            renderer_->clear();
            for (auto& entity : entities_) {
                renderer_->render_entity(*entity, sensor_pos);
            }

            // --- D. BRIDGE PUBLISH ---
            bridge_->publish_frame(frame, now, radar_hits);

            // Logging
            if (frame % 60 == 0) {
                 spdlog::info("[Sim] Time: {:.2f}s | Real Targets: {} | Radar Returns: {}", 
                    now, entities_.size(), radar_hits.size());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void SimEngine::stop() { is_running_ = false; }
}