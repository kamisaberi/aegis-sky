#include "sim/bridge_server/ShmWriter.h"
#include <sys/mman.h> // POSIX Shared Memory
#include <fcntl.h>    // O_CREAT, O_RDWR
#include <unistd.h>   // ftruncate, close
#include <cstring>    // memcpy
#include <spdlog/spdlog.h>

namespace aegis::sim::bridge {

    ShmWriter::ShmWriter() : shm_fd_(-1), mapped_ptr_(nullptr), header_(nullptr) {}

    ShmWriter::~ShmWriter() {
        cleanup();
    }

    bool ShmWriter::initialize() {
        spdlog::info("[Bridge] Creating Shared Memory: {}", ipc::BRIDGE_NAME);

        // 1. Open/Create the Shared Memory Object
        shm_fd_ = shm_open(ipc::BRIDGE_NAME, O_CREAT | O_RDWR, 0666);
        if (shm_fd_ == -1) {
            spdlog::error("[Bridge] Failed to shm_open");
            return false;
        }

        // 2. Resize it (ftruncate)
        if (ftruncate(shm_fd_, ipc::BRIDGE_SIZE_BYTES) == -1) {
            spdlog::error("[Bridge] Failed to ftruncate");
            return false;
        }

        // 3. Map it into our address space
        mapped_ptr_ = mmap(0, ipc::BRIDGE_SIZE_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (mapped_ptr_ == MAP_FAILED) {
            spdlog::error("[Bridge] Failed to mmap");
            return false;
        }

        // 4. Initialize Pointers
        header_ = static_cast<ipc::BridgeHeader*>(mapped_ptr_);
        // Radar buffer starts immediately after the header
        radar_buf_ = reinterpret_cast<ipc::SimRadarPoint*>(static_cast<char*>(mapped_ptr_) + sizeof(ipc::BridgeHeader));

        // 5. Write Initial Header
        header_->magic_number = ipc::BRIDGE_MAGIC;
        header_->state_flag = 0; // Not ready yet

        spdlog::info("[Bridge] Shared Memory Initialized @ {}", mapped_ptr_);
        return true;
    }

    void ShmWriter::cleanup() {
        if (mapped_ptr_) {
            munmap(mapped_ptr_, ipc::BRIDGE_SIZE_BYTES);
            mapped_ptr_ = nullptr;
        }
        if (shm_fd_ != -1) {
            close(shm_fd_);
            shm_unlink(ipc::BRIDGE_NAME); // Actually deletes the file
            shm_fd_ = -1;
        }
    }

    void ShmWriter::publish_frame(uint64_t frame_id, double time, const std::vector<ipc::SimRadarPoint>& radar_data) {
        if (!mapped_ptr_) return;

        // 1. Lock (Set State to Writing)
        header_->state_flag = 0;

        // 2. Write Metadata
        header_->frame_id = frame_id;
        header_->sim_time = time;
        header_->num_radar_points = static_cast<uint32_t>(radar_data.size());

        // 3. Write Data (Memcpy is extremely fast here)
        size_t data_size = radar_data.size() * sizeof(ipc::SimRadarPoint);
        
        // Safety check to not overflow buffer
        if (data_size < ipc::BRIDGE_SIZE_BYTES - sizeof(ipc::BridgeHeader)) {
            std::memcpy(radar_buf_, radar_data.data(), data_size);
        } else {
            spdlog::warn("[Bridge] Radar data too large! Dropping frame.");
            header_->num_radar_points = 0;
        }

        // 4. Unlock (Set State to Ready)
        header_->state_flag = 1;
    }
}