#include "aegis/hal/ICamera.h"
#include "aegis/platform/CudaAllocator.h"
#include <gst/gst.h>
#include <spdlog/spdlog.h>

namespace aegis::core::drivers {

    class GStreamerCamera : public hal::ICamera {
    public:
        // Constructor takes device path e.g., "/dev/video0"
        GStreamerCamera(const std::string& device_path) : device_(device_path) {}

        bool initialize() override {
            // 1. Allocate a Zero-Copy buffer using our CudaAllocator
            // 1920 * 1080 * 3 bytes (RGB)
            frame_buffer_ = (uint8_t*)platform::CudaAllocator::alloc_pinned(1920 * 1080 * 3);
            if (!frame_buffer_) return false;

            // 2. Build a GStreamer pipeline to capture from a V4L2 device
            // and write directly into our pinned memory buffer.
            std::string pipeline_str = "v4l2src device=" + device_ +
                " ! video/x-raw,width=1920,height=1080 ! videoconvert ! appsink name=sink";

            // ... [GStreamer initialization logic] ...
            // ... [Connect appsink to a callback that signals frame is ready] ...

            spdlog::info("[Driver] GStreamerCamera initialized for {}", device_);
            return true;
        }

        hal::ImageFrame get_frame() override {
            // Block until the GStreamer callback signals a new frame has been written
            // to our buffer.

            // ... [Wait on a condition variable] ...

            hal::ImageFrame frame;
            frame.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            frame.width = 1920;
            frame.height = 1080;
            frame.data_ptr = frame_buffer_; // Pointer to the GPU-accessible pinned memory

            return frame;
        }
    private:
        std::string device_;
        uint8_t* frame_buffer_ = nullptr;
        // GStreamer pipeline, mutexes, condition variables...
    };
}