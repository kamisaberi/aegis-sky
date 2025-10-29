#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QVector>
#include "StationProtocol.h"

namespace aegis::station::comms {

    class CoreClient : public QObject {
        Q_OBJECT
        // QML Properties
        Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
        Q_PROPERTY(QString hostAddress READ hostAddress WRITE setHostAddress NOTIFY hostChanged)
        Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)

    public:
        explicit CoreClient(QObject *parent = nullptr);
        ~CoreClient();

        bool isConnected() const;
        QString hostAddress() const;
        int port() const;

        void setHostAddress(const QString &host);
        void setPort(int port);

    public slots:
        // Called from QML Buttons
        void connectToCore();
        void disconnectFromCore();
        
        // Command Controls
        void setGimbalVector(float pan, float tilt);
        void sendFireCommand();
        void setSystemArmed(bool armed);

    signals:
        // Updates for QML
        void connectionChanged(bool connected);
        void hostChanged();
        void portChanged();
        
        // High-freq updates for the HUD
        void telemetryReceived(double timestamp, float pan, float tilt);
        void targetsUpdated(const QVariantList &targets); // List of Track objects

    private slots:
        void onSocketConnected();
        void onSocketDisconnected();
        void onSocketReadyRead();
        void onSocketError(QAbstractSocket::SocketError error);
        void onHeartbeatTimer();

    private:
        void sendPacket(protocol::PacketType type, const void* data, size_t size);
        void processIncomingData();

        QTcpSocket *socket_;
        QTimer *heartbeatTimer_;
        
        QString host_ = "127.0.0.1";
        int port_ = protocol::DEFAULT_PORT;
        
        // Command State
        protocol::CommandPacket current_cmd_;
    };
}