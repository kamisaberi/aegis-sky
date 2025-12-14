#pragma once
#include "aegis/hal/ICamera.h" // The Interface we must implement
#include <gst/gst.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace aegis::core::drivers {

    class GStreamerCamera : public hal::ICamera {
    public:
        // Takes a GStreamer pipeline string as input
        // e.g., "v4l2src device=/dev/video0 ! ..."
        explicit GStreamerCamera(const std::string& pipeline_str);
        ~GStreamerCamera() override;

        // --- ICamera Interface Implementation ---
        bool initialize() override;
        hal::ImageFrame get_frame() override;

    private:
        // Callback function called by GStreamer on a new frame
        static GstFlowReturn on_new_sample(GstElement *sink, GStreamerCamera *self);

        // Callback for GStreamer bus messages (errors, EOS)
        static gboolean on_bus_message(GstBus *bus, GstMessage *msg, GStreamerCamera *self);

        // Internal state
        std::string pipeline_str_;

        GstElement *pipeline_ = nullptr;
        GstElement *appsink_ = nullptr;

        // Zero-Copy Buffer
        uint8_t* pinned_buffer_ = nullptr;
        size_t buffer_size_ = 0;
        int width_ = 0;
        int height_ = 0;

        // Threading & Synchronization
        std::mutex mtx_;
        std::condition_variable cv_;
        std::atomic<bool> new_frame_available_{false};
        std::atomic<bool> is_running_{false};
        double last_timestamp_ = 0.0;
    };
}