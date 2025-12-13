#pragma once
#include <cstdint>

namespace aegis::ipc::station {

    static constexpr uint32_t TCP_MAGIC = 0x54435000;
    static constexpr int DEFAULT_PORT = 9090;

    enum class PacketType : uint8_t {
        HEARTBEAT = 0x01,
        COMMAND   = 0x02,
        TELEMETRY = 0x03
    };

    struct PacketHeader {
        uint32_t magic = TCP_MAGIC;
        PacketType type;
        uint32_t payload_size;
    };

    // Command: Station -> Core
    struct CommandPacket {
        float pan_velocity;
        float tilt_velocity;
        bool arm_system;
        bool fire_trigger;
    };

    // Telemetry: Core -> Station
    struct TelemetryPacket {
        double timestamp;
        float gimbal_pan;
        float gimbal_tilt;
        uint32_t active_target_count;
    };

    // (Optional) Track Struct if sending lists
    struct TrackData {
        uint32_t id;
        float azimuth;
        float elevation;
        bool is_threat;
    };
}