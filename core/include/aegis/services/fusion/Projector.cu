#include <cuda_runtime.h>
#include <cmath>
#include <cstdio>

// Define the shape of a radar point on the GPU side
struct RadarPointRaw {
    float x, y, z, v, snr;
};

// -----------------------------------------------------------------------------
// KERNEL: Project 3D Points to 2D Maps
// -----------------------------------------------------------------------------
__global__ void project_kernel(
    const RadarPointRaw* __restrict__ points,
    int num_points,
    const float* __restrict__ K, // Intrinsics
    const float* __restrict__ R, // Rotation
    const float* __restrict__ T, // Translation
    float* __restrict__ out_depth_map, // Output buffer (W*H)
    float* __restrict__ out_vel_map,   // Output buffer (W*H)
    int width,
    int height
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_points) return;

    // 1. Load Point
    RadarPointRaw p = points[idx];

    // 2. Extrinsic Transform: P_cam = R * P_radar + T
    float cam_x = R[0]*p.x + R[1]*p.y + R[2]*p.z + T[0];
    float cam_y = R[3]*p.x + R[4]*p.y + R[5]*p.z + T[1];
    float cam_z = R[6]*p.x + R[7]*p.y + R[8]*p.z + T[2];

    // Check if behind camera
    if (cam_z <= 0.1f) return;

    // 3. Intrinsic Projection: P_img = K * P_cam
    // u = (fx * x + cx * z) / z  ->  fx * (x/z) + cx
    // v = (fy * y + cy * z) / z  ->  fy * (y/z) + cy

    float u = (K[0] * cam_x + K[2] * cam_z) / cam_z;
    float v = (K[4] * cam_y + K[5] * cam_z) / cam_z;

    int ui = (int)(u + 0.5f); // Round to nearest pixel
    int vi = (int)(v + 0.5f);

    // 4. Bounds Check & Write
    // Note: Inverted Y check depending on coordinate system.
    // Assuming Sim uses standard Image Y-down.

    // Dilation Loop (Make the dot 3x3 pixels so the CNN can see it)
    int kernel_radius = 2;

    for (int dy = -kernel_radius; dy <= kernel_radius; ++dy) {
        for (int dx = -kernel_radius; dx <= kernel_radius; ++dx) {
            int py = vi + dy;
            int px = ui + dx;

            if (px >= 0 && px < width && py >= 0 && py < height) {
                int pixel_idx = py * width + px;

                // Write Depth (Z) and Velocity (V)
                // RACE CONDITION NOTE: multiple radar points hitting same pixel.
                // For MVP, overwrite is acceptable.
                // For Prod, use atomicMin for Depth.

                out_depth_map[pixel_idx] = cam_z;
                out_vel_map[pixel_idx]   = p.v;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// C++ WRAPPER (To call from .cpp files)
// -----------------------------------------------------------------------------
namespace aegis::core::services::fusion {

    void launch_projection_kernel(
        const void* points_device,
        int num_points,
        const float* K_device,
        const float* R_device,
        const float* T_device,
        float* depth_map_device,
        float* vel_map_device,
        int width,
        int height,
        void* cuda_stream
    ) {
        if (num_points == 0) return;

        int threads = 256;
        int blocks = (num_points + threads - 1) / threads;
        cudaStream_t stream = static_cast<cudaStream_t>(cuda_stream);

        // Reset Maps to 0 before drawing (async)
        cudaMemsetAsync(depth_map_device, 0, width * height * sizeof(float), stream);
        cudaMemsetAsync(vel_map_device, 0, width * height * sizeof(float), stream);

        project_kernel<<<blocks, threads, 0, stream>>>(
            (const RadarPointRaw*)points_device,
            num_points,
            K_device, R_device, T_device,
            depth_map_device,
            vel_map_device,
            width, height
        );
    }
}