#pragma once
#include <cstdint>

// Matches the structures used in Core
namespace aegis::station::protocol {

    static constexpr uint32_t MAGIC = 0x54435000; // "TCP\0"
    static constexpr int DEFAULT_PORT = 9090;

    enum class PacketType : uint8_t {
        HEARTBEAT = 0x01,
        COMMAND   = 0x02,
        TELEMETRY = 0x03
    };

    // 1. Header (Fixed Size)
    struct PacketHeader {
        uint32_t magic = MAGIC;
        PacketType type;
        uint32_t payload_size;
    };

    // 2. Command Payload (Station -> Core)
    struct CommandPacket {
        float pan_velocity;
        float tilt_velocity;
        bool arm_system;
        bool fire_trigger;
    };

    // 3. Telemetry Payload (Core -> Station)
    // Note: Video goes over RTSP (UDP), this is for Metadata
    struct TelemetryPacket {
        double timestamp;
        float gimbal_pan;
        float gimbal_tilt;
        uint32_t active_target_count;
        // Followed by dynamic list of Radar/Track objects...
    };
    
    // Simple Track Struct for UI
    struct TrackData {
        uint32_t id;
        float x, y, z;
        float velocity;
        bool is_threat;
    };
}