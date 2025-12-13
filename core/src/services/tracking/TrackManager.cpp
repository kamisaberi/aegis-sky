#include "TrackManager.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <spdlog/spdlog.h>

namespace aegis::core::services::tracking {

    TrackManager::TrackManager() {}

    void TrackManager::process_scan(const hal::PointCloud& cloud) {
        // 1. Predict all existing tracks to current time
        for (auto& track : tracks_) {
            track.filter.predict(cloud.timestamp);
        }

        // 2. Associate & Update
        associate_and_update(cloud.points, cloud.timestamp);

        // 3. Delete dead tracks
        prune_tracks();
    }

    void TrackManager::associate_and_update(const std::vector<hal::RadarPoint>& measurements, double time) {
        std::vector<bool> matched_meas(measurements.size(), false);

        // Simple Nearest Neighbor Association
        // (For production, use Hungarian Algorithm / Munkres)

        for (auto& track : tracks_) {
            auto predicted_pos = track.filter.get_position();

            float best_dist = std::numeric_limits<float>::max();
            int best_idx = -1;

            for (size_t i = 0; i < measurements.size(); ++i) {
                if (matched_meas[i]) continue;

                float dx = measurements[i].x - predicted_pos[0];
                float dy = measurements[i].y - predicted_pos[1];
                float dz = measurements[i].z - predicted_pos[2];
                float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

                if (dist < best_dist) {
                    best_dist = dist;
                    best_idx = i;
                }
            }

            // Gating
            if (best_idx != -1 && best_dist < match_threshold_dist_) {
                // Match Found
                matched_meas[best_idx] = true;
                track.missed_frames = 0;

                // Update Filter
                const auto& m = measurements[best_idx];
                track.filter.update(m.x, m.y, m.z);

                // Promotion Logic (Confirm track after few hits)
                if (!track.is_confirmed) track.is_confirmed = true;
            } else {
                // No Match
                track.missed_frames++;
            }
        }

        // Create new tracks for unmatched measurements
        for (size_t i = 0; i < measurements.size(); ++i) {
            if (!matched_meas[i]) {
                create_track(measurements[i], time);
            }
        }
    }

    void TrackManager::create_track(const hal::RadarPoint& meas, double time) {
        // Filter out obvious noise (e.g. low SNR) before creating track
        if (meas.snr < 10.0f) return;

        Track t = {
            next_id_++,
            KalmanFilter(meas.x, meas.y, meas.z, time),
            0,
            false // Unconfirmed initially
        };
        tracks_.push_back(t);
        spdlog::debug("[Tracker] New Track ID: {}", t.id);
    }

    void TrackManager::prune_tracks() {
        tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
            [this](const Track& t) {
                if (t.missed_frames > max_missed_frames_) {
                    spdlog::debug("[Tracker] Dropped Track ID: {}", t.id);
                    return true;
                }
                return false;
            }),
        tracks_.end());
    }

    std::vector<Track> TrackManager::get_tracks() const {
        return tracks_;
    }
}