#pragma once
#include <QObject>
#include <QVideoSink>
#include <gst/gst.h>

namespace aegis::station::video {

    class VideoReceiver : public QObject {
        Q_OBJECT
        // The QML VideoOutput element will bind to this sink
        Q_PROPERTY(QVideoSink* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
        Q_PROPERTY(QString uri READ uri WRITE setUri NOTIFY uriChanged)

    public:
        explicit VideoReceiver(QObject *parent = nullptr);
        ~VideoReceiver();

        QVideoSink* videoSink() const { return m_videoSink; }
        void setVideoSink(QVideoSink* sink);

        QString uri() const { return m_uri; }
        void setUri(const QString &uri);

    public slots:
        void start();
        void stop();

    signals:
        void videoSinkChanged();
        void uriChanged();
        void errorOccurred(QString message);

    private:
        // GStreamer Callbacks
        static GstFlowReturn on_new_sample(GstElement *sink, VideoReceiver *self);

        QString m_uri = "udp://127.0.0.1:5000"; // Default to Sim Port
        QVideoSink *m_videoSink = nullptr;
        
        GstElement *m_pipeline = nullptr;
        GstElement *m_sink = nullptr;
    };
}