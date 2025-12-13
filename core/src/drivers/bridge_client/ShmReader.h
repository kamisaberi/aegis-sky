#pragma once
#include "aegis_ipc/shm_layout.h" // Shared header
#include <string>
#include <vector>

namespace aegis::core::drivers {

    class ShmReader {
    public:
        ShmReader();
        ~ShmReader();

        // Connect to the Shared Memory file
        bool connect();
        void disconnect();

        // Check if a new frame is available (Spinlock check)
        bool has_new_frame(uint64_t& out_frame_id);

        // Copy data from SHM to local buffers
        // Returns true if successful
        bool read_sensor_data(
            double& out_time,
            std::vector<ipc::SimRadarPoint>& out_radar,
            std::vector<uint8_t>& out_video
        );

        // Write control commands back to Sim (Pan/Tilt/Fire)
        void send_command(const ipc::ControlCommand& cmd);

    private:
        int shm_fd_ = -1;
        void* mapped_ptr_ = nullptr;
        
        // Pointers into the mapped memory
        ipc::BridgeHeader* header_ = nullptr;
        ipc::SimRadarPoint* radar_buf_ = nullptr;
        ipc::ControlCommand* cmd_buf_ = nullptr;
        uint8_t* video_buf_ = nullptr;

        // Tracking last frame to avoid re-reading
        uint64_t last_frame_id_ = 0;
    };
}