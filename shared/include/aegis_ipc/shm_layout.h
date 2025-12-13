#pragma once
#include <cstdint>

namespace aegis::ipc {

    
struct ControlCommand {
    uint64_t timestamp;
    float pan_velocity;  // Radians per second
    float tilt_velocity; // Radians per second
    bool fire_trigger;   // True = SHOOT
    bool jammer_active;  // True = RF Jamming
};
    // Magic Number to ensure Sim and Core are talking about the same memory
    static constexpr uint32_t BRIDGE_MAGIC = 0xAE6155KY; 
    static constexpr char BRIDGE_NAME[] = "/aegis_bridge_v1";
    static constexpr size_t BRIDGE_SIZE_BYTES = 1024 * 1024 * 64; // 64 MB

    // 1. Point Cloud Structure (What the Radar "sees")
    struct SimRadarPoint {
        float x, y, z;      // Position (meters) relative to sensor
        float velocity;     // Radial Doppler velocity (m/s)
        float snr_db;       // Signal Strength
        uint32_t object_id; // Ground Truth ID (for evaluation only, Core ignores this)
    };

    // 2. The Header (Metadata)
    struct BridgeHeader {
        uint32_t magic_number;      // Safety check
        uint64_t frame_id;          // Increments every tick
        double   sim_time;          // Game time in seconds
        uint32_t num_radar_points;  // How many points follow in the buffer?
        
        // Synchronization Primitive (Simple Spinlock)
        // 0 = Writing, 1 = Ready to Read
        volatile uint32_t state_flag; 
    };

    // The Memory Layout:
    // [ BridgeHeader (64 bytes) ]
    // [ SimRadarPoint * 10,000  ]
    // [ Raw Video Bytes...      ]
}