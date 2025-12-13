#pragma once
#include <cstring> // for memcpy

namespace aegis::core::services {

    struct CalibrationData {
        // 1. Intrinsics (Camera Lens) - 3x3 Matrix flattened
        // fx  0  cx
        //  0 fy  cy
        //  0  0   1
        float K[9];

        // 2. Extrinsics (Rotation Radar -> Camera) - 3x3 Matrix
        float R[9];

        // 3. Extrinsics (Translation Radar -> Camera) - 3x1 Vector
        float T[3];

        // 4. Image Dimensions
        int width;
        int height;

        // Factory for Simulation (Perfect Alignment)
        static CalibrationData create_perfect_alignment(int w, int h) {
            CalibrationData cal;
            cal.width = w;
            cal.height = h;

            // Identity Rotation (Sensors aligned perfectly)
            float I[9] = {1,0,0, 0,1,0, 0,0,1};
            std::memcpy(cal.R, I, sizeof(I));

            // Translation (Radar is usually slightly below Camera, e.g., 10cm)
            float t_vec[3] = {0.0f, -0.1f, 0.0f};
            std::memcpy(cal.T, t_vec, sizeof(t_vec));

            // Pinhole Intrinsics (60 deg FOV matching Sim)
            // fx = width / (2 * tan(fov/2))
            float f = w / (2.0f * 0.577f); // tan(30deg) ~= 0.577
            float k_mat[9] = {f, 0, w/2.0f,  0, f, h/2.0f,  0, 0, 1};
            std::memcpy(cal.K, k_mat, sizeof(k_mat));

            return cal;
        }
    };
}