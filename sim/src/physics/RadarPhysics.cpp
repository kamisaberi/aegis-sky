#include "sim/physics/RadarPhysics.h"
#include "sim/math/Random.h"
#include <glm/gtx/norm.hpp>
#include <cmath>
#include <algorithm>

namespace aegis::sim::physics {

    static constexpr double TX_POWER = 200.0;     
    static constexpr double NOISE_FLOOR = 1e-13; 
    static constexpr double JAMMER_PWR = 10.0; 

    // --- EW LOGIC ---
    double RadarPhysics::calculate_environment_noise(
        const std::vector<std::shared_ptr<engine::SimEntity>>& entities, 
        const glm::dvec3& radar_pos
    ) {
        double total_noise = NOISE_FLOOR;
        for (const auto& e : entities) {
            // Check name for "Jammer" (Simplified)
            if (e->get_name().find("Jammer") != std::string::npos) {
                double dist_sq = glm::distance2(radar_pos, e->get_position());
                if (dist_sq < 1.0) dist_sq = 1.0;
                total_noise += JAMMER_PWR / (4.0 * M_PI * dist_sq);
            }
        }
        return total_noise;
    }

    // --- INTERNAL RAYCAST ---
    RadarReturn cast_internal(
        const glm::dvec3& origin, const glm::dvec3& beam_dir, 
        const engine::SimEntity& target, bool is_ghost,
        const RadarConfig& cfg, double noise, const engine::WeatherState& wx, double time
    ) {
        RadarReturn ret = {false};
        
        // 1. Geometry
        glm::dvec3 target_pos = is_ghost ? glm::dvec3(target.get_position().x, -target.get_position().y, target.get_position().z) : target.get_position();
        glm::dvec3 L = target_pos - origin;
        double dist_sq = glm::length2(L);
        
        if (dist_sq > cfg.max_range * cfg.max_range) return ret;

        // 2. FOV
        glm::dvec3 to_target = glm::normalize(L);
        glm::dvec3 flat_fwd = glm::normalize(glm::dvec3(beam_dir.x, 0, beam_dir.z));
        glm::dvec3 flat_tgt = glm::normalize(glm::dvec3(to_target.x, 0, to_target.z));

        if (glm::dot(flat_fwd, flat_tgt) < std::cos(glm::radians(cfg.fov_azimuth_deg/2.0))) return ret;

        // 3. Physics
        ret.detected = true;
        ret.range = std::sqrt(dist_sq);
        ret.azimuth = std::atan2(to_target.x, to_target.z);
        ret.elevation = std::asin(to_target.y);
        
        // Doppler: Base + Micro-Doppler
        ret.velocity = glm::dot(target.get_velocity(), to_target);
        if (!is_ghost) {
            ret.velocity += target.get_instant_doppler_mod(time);
        }

        // Signal
        double r4 = ret.range * ret.range * ret.range * ret.range;
        double rain_loss = 0.02 * wx.rain_intensity * (ret.range/1000.0) * 2.0;
        double power = (TX_POWER * target.get_rcs()) / (r4 + 1e-9);
        
        if (is_ghost) power *= 0.25; // Reflection loss (-6dB)

        ret.snr_db = 10.0 * std::log10(power / noise) - rain_loss;

        // Noise
        ret.range += math::Random::gaussian(cfg.noise_range_m);
        ret.azimuth += math::Random::gaussian(cfg.noise_angle_rad);
        ret.velocity += math::Random::gaussian(cfg.noise_vel_ms);
        ret.snr_db += math::Random::gaussian(1.0);

        return ret;
    }

    // --- PUBLIC SCAN ---
    std::vector<RadarReturn> RadarPhysics::scan_target(
        const glm::dvec3& pos, const glm::dvec3& fwd, 
        const engine::SimEntity& target, const RadarConfig& cfg,
        double noise, const engine::WeatherState& wx, double time
    ) {
        std::vector<RadarReturn> hits;

        // Direct
        auto r1 = cast_internal(pos, fwd, target, false, cfg, noise, wx, time);
        if (r1.detected) hits.push_back(r1);

        // Multipath (Ghost) - Only if low altitude
        if (target.get_position().y < 15.0 && target.get_position().y > 0.5) {
            auto r2 = cast_internal(pos, fwd, target, true, cfg, noise, wx, time);
            if (r2.detected) hits.push_back(r2);
        }
        return hits;
    }
}