#include "CoreClient.h"
#include <QDataStream>
#include <QDebug>

namespace aegis::station::comms {

    CoreClient::CoreClient(QObject *parent) : QObject(parent) {
        socket_ = new QTcpSocket(this);
        heartbeatTimer_ = new QTimer(this);
        
        // Initialize Command State
        current_cmd_ = {0.0f, 0.0f, false, false};

        // Wire up signals
        connect(socket_, &QTcpSocket::connected, this, &CoreClient::onSocketConnected);
        connect(socket_, &QTcpSocket::disconnected, this, &CoreClient::onSocketDisconnected);
        connect(socket_, &QTcpSocket::readyRead, this, &CoreClient::onSocketReadyRead);
        connect(socket_, &QTcpSocket::errorOccurred, this, &CoreClient::onSocketError);

        // Send Heartbeat/Commands every 50ms (20Hz)
        connect(heartbeatTimer_, &QTimer::timeout, this, &CoreClient::onHeartbeatTimer);
    }

    CoreClient::~CoreClient() {
        socket_->close();
    }

    // --- Properties ---
    bool CoreClient::isConnected() const { return socket_->state() == QAbstractSocket::ConnectedState; }
    QString CoreClient::hostAddress() const { return host_; }
    int CoreClient::port() const { return port_; }
    void CoreClient::setHostAddress(const QString &host) { 
        if(host_ != host) { host_ = host; emit hostChanged(); } 
    }
    void CoreClient::setPort(int port) { 
        if(port_ != port) { port_ = port; emit portChanged(); } 
    }

    // --- Connection Logic ---
    void CoreClient::connectToCore() {
        if (isConnected()) return;
        qInfo() << "Connecting to Core at" << host_ << ":" << port_;
        socket_->connectToHost(host_, port_);
    }

    void CoreClient::disconnectFromCore() {
        socket_->disconnectFromHost();
    }

    void CoreClient::onSocketConnected() {
        qInfo() << "Connected to Aegis Core!";
        emit connectionChanged(true);
        heartbeatTimer_->start(50); // Start sending commands
    }

    void CoreClient::onSocketDisconnected() {
        qWarning() << "Disconnected from Aegis Core.";
        emit connectionChanged(false);
        heartbeatTimer_->stop();
    }

    void CoreClient::onSocketError(QAbstractSocket::SocketError error) {
        qCritical() << "Socket Error:" << socket_->errorString();
        emit connectionChanged(false);
    }

    // --- Command Logic ---
    void CoreClient::setGimbalVector(float pan, float tilt) {
        current_cmd_.pan_velocity = pan;
        current_cmd_.tilt_velocity = tilt;
    }

    void CoreClient::setSystemArmed(bool armed) {
        current_cmd_.arm_system = armed;
        // Safety: If disarmed, ensure trigger is released
        if (!armed) current_cmd_.fire_trigger = false;
    }

    void CoreClient::sendFireCommand() {
        if (current_cmd_.arm_system) {
            current_cmd_.fire_trigger = true;
            // Auto-reset trigger logic happens in the timer loop or UI release
        } else {
            qWarning() << "Cannot fire: System Disarmed!";
        }
    }

    void CoreClient::onHeartbeatTimer() {
        // Send the current command state
        // In a real system, you might only send if changed, 
        // but for safety/deadman switch, sending constant streams is better.
        sendPacket(protocol::PacketType::COMMAND, &current_cmd_, sizeof(current_cmd_));
        
        // Reset momentary switches
        current_cmd_.fire_trigger = false; 
    }

    void CoreClient::sendPacket(protocol::PacketType type, const void* data, size_t size) {
        if (!isConnected()) return;

        protocol::PacketHeader header;
        header.type = type;
        header.payload_size = static_cast<uint32_t>(size);

        // Write Header
        socket_->write(reinterpret_cast<const char*>(&header), sizeof(header));
        // Write Payload
        if (size > 0 && data) {
            socket_->write(reinterpret_cast<const char*>(data), size);
        }
        socket_->flush();
    }

    // --- Receive Logic ---
    void CoreClient::onSocketReadyRead() {
        processIncomingData();
    }

    void CoreClient::processIncomingData() {
        // Simple State Machine for Packet parsing
        // NOTE: In production, handle partial reads (buffering). 
        // For simple local TCP, packets often arrive whole, but don't rely on this in prod.
        
        while (socket_->bytesAvailable() >= sizeof(protocol::PacketHeader)) {
            // Peek header
            protocol::PacketHeader header;
            socket_->peek(reinterpret_cast<char*>(&header), sizeof(header));

            if (header.magic != protocol::MAGIC) {
                qCritical() << "Invalid Magic Number! Sync lost.";
                socket_->readAll(); // Clear buffer
                return;
            }

            if (socket_->bytesAvailable() < (sizeof(header) + header.payload_size)) {
                // Wait for more data
                return;
            }

            // Consume Header
            socket_->read(reinterpret_cast<char*>(&header), sizeof(header));
            
            // Consume Payload
            QByteArray payload = socket_->read(header.payload_size);

            if (header.type == protocol::PacketType::TELEMETRY) {
                // Parse Telemetry
                if (payload.size() >= sizeof(protocol::TelemetryPacket)) {
                    auto* telem = reinterpret_cast<const protocol::TelemetryPacket*>(payload.constData());
                    emit telemetryReceived(telem->timestamp, telem->gimbal_pan, telem->gimbal_tilt);
                    
                    // TODO: Parse list of targets after the fixed struct
                    // emit targetsUpdated(...);
                }
            }
        }
    }
}