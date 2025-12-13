#include "aegis/hal/ICamera.h" // Public Interface
#include "ShmReader.h"
#include <iostream>

namespace aegis::core::drivers {

    // Concrete Implementation of the Abstract ICamera
    class SimCamera : public hal::ICamera {
    public:
        SimCamera(std::shared_ptr<ShmReader> reader) : reader_(reader) {}

        bool initialize() override {
            std::cout << "[Driver] Initialized Virtual Camera (Bridge Mode)" << std::endl;
            return true;
        }

        hal::ImageFrame get_frame() override {
            hal::ImageFrame frame;
            // In a real Zero-Copy implementation, we would return a pointer
            // directly into the mmap region (video_buf_).
            // This avoids the memcpy entirely!

            // For now, return empty frame just to satisfy interface
            frame.width = 1920;
            frame.height = 1080;
            frame.timestamp = 0.0;
            return frame;
        }

    private:
        std::shared_ptr<ShmReader> reader_;
    };
}