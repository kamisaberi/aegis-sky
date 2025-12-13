#pragma once
#include <vector>
#include <array>

namespace aegis::core::services::tracking {

    class KalmanFilter {
    public:
        // Initialize with starting position and timestamp
        KalmanFilter(float x, float y, float z, double timestamp);

        // 1. PREDICT step: Estimate position based on velocity and dt
        void predict(double current_time);

        // 2. UPDATE step: Correct estimation using real measurement
        void update(float x, float y, float z);

        // Getters
        std::array<float, 3> get_position() const;
        std::array<float, 3> get_velocity() const;
        double get_last_update_time() const { return last_time_; }

    private:
        // State Vector: [x, y, z, vx, vy, vz]
        float state_[6];

        // Covariance Matrix (Uncertainty) 6x6 (Simplified diagonal)
        float P_[6];

        double last_time_;

        // Tuning Parameters (Process Noise vs Measurement Noise)
        const float process_noise_ = 0.1f; // Uncertainty in physics (wind/maneuvers)
        const float measurement_noise_ = 0.5f; // Uncertainty in sensor (Radar error)
    };
}