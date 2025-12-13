#include "StationLink.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <spdlog/spdlog.h>

namespace aegis::core::services {

    StationLink::StationLink(int port) : port_(port) {}

    StationLink::~StationLink() { stop(); }

    bool StationLink::start() {
        // 1. Create Socket
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ == -1) {
            spdlog::error("[Comms] Failed to create socket");
            return false;
        }

        // 2. Bind
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            spdlog::error("[Comms] Bind failed on port {}", port_);
            return false;
        }

        // 3. Listen
        if (listen(server_fd_, 1) < 0) {
            spdlog::error("[Comms] Listen failed");
            return false;
        }

        is_running_ = true;
        listen_thread_ = std::thread(&StationLink::listen_loop, this);
        spdlog::info("[Comms] StationLink Listening on TCP {}", port_);
        return true;
    }

    void StationLink::stop() {
        is_running_ = false;
        if (server_fd_ != -1) close(server_fd_);
        if (client_fd_ != -1) close(client_fd_);
        if (listen_thread_.joinable()) listen_thread_.join();
        if (client_thread_.joinable()) client_thread_.join();
    }

    void StationLink::listen_loop() {
        while (is_running_) {
            // Blocking Accept
            int new_socket = accept(server_fd_, nullptr, nullptr);
            if (new_socket < 0) {
                if (is_running_) spdlog::warn("[Comms] Accept failed");
                continue;
            }

            spdlog::info("[Comms] STATION CONNECTED!");

            // Close existing client if any
            if (client_fd_ != -1) close(client_fd_);
            if (client_thread_.joinable()) client_thread_.join();

            client_fd_ = new_socket;
            client_connected_ = true;
            client_thread_ = std::thread(&StationLink::client_loop, this);
        }
    }

    void StationLink::client_loop() {
        while (is_running_ && client_connected_) {
            ipc::station::PacketHeader header;

            // 1. Read Header
            ssize_t bytes = recv(client_fd_, &header, sizeof(header), 0);
            if (bytes <= 0) {
                spdlog::warn("[Comms] Station Disconnected");
                client_connected_ = false;
                close(client_fd_);
                client_fd_ = -1;
                break;
            }

            if (header.magic != ipc::station::TCP_MAGIC) {
                spdlog::error("[Comms] Invalid Magic Packet");
                continue; // Or disconnect
            }

            // 2. Read Payload
            if (header.type == ipc::station::PacketType::COMMAND) {
                ipc::station::CommandPacket cmd;
                recv(client_fd_, &cmd, sizeof(cmd), 0);

                std::lock_guard<std::mutex> lock(cmd_mutex_);
                latest_cmd_ = cmd;
                new_cmd_available_ = true;
            }
            else {
                // Skip unknown payload
                char junk[1024];
                recv(client_fd_, junk, header.payload_size, 0);
            }
        }
    }

    void StationLink::broadcast_telemetry(double timestamp, float pan, float tilt, int targets) {
        if (!client_connected_ || client_fd_ == -1) return;

        // Prepare Header
        ipc::station::PacketHeader header;
        header.type = ipc::station::PacketType::TELEMETRY;
        header.payload_size = sizeof(ipc::station::TelemetryPacket);

        // Prepare Payload
        ipc::station::TelemetryPacket packet;
        packet.timestamp = timestamp;
        packet.gimbal_pan = pan;
        packet.gimbal_tilt = tilt;
        packet.active_target_count = targets;

        // Send (Using MSG_NOSIGNAL to prevent crash if client disconnects mid-send)
        send(client_fd_, &header, sizeof(header), MSG_NOSIGNAL);
        send(client_fd_, &packet, sizeof(packet), MSG_NOSIGNAL);
    }

    bool StationLink::get_latest_command(ipc::station::CommandPacket& out_cmd) {
        std::lock_guard<std::mutex> lock(cmd_mutex_);
        if (new_cmd_available_) {
            out_cmd = latest_cmd_;
            new_cmd_available_ = false;
            return true;
        }
        return false;
    }
}