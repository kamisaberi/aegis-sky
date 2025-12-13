#pragma once
#include "aegis/hal/IRadar.h"
#include "aegis/hal/ICamera.h"
#include "Calibration.h"
#include <cuda_runtime.h>

namespace aegis::core::services {

    // The Output of the Fusion Stage
    struct FusedFrame {
        int width, height;
        // Pointers to GPU memory
        uint8_t* rgb;   // 3 Channels (uint8)
        float* depth;   // 1 Channel (float)
        float* velocity;// 1 Channel (float)

        // Context
        cudaStream_t stream;
    };

    class FusionEngine {
    public:
        FusionEngine(const CalibrationData& cal);
        ~FusionEngine();

        // Process data
        FusedFrame process(const hal::ImageFrame& img, const hal::PointCloud& radar);

    private:
        CalibrationData cal_;
        cudaStream_t stream_;

        // GPU Buffers for Calibration Matrices
        float *d_K_, *d_R_, *d_T_;

        // GPU Buffers for Output Maps
        float *d_depth_map_;
        float *d_vel_map_;

        // GPU Buffer for Radar Points (Upload buffer)
        void* d_radar_points_;
        size_t radar_buf_capacity_ = 0;
    };
}