#include "aegis/hal/IRadar.h"
#include "ShmReader.h"

namespace aegis::core::drivers {

    class SimRadar : public hal::IRadar {
    public:
        SimRadar(std::shared_ptr<ShmReader> reader) : reader_(reader) {}

        bool initialize() override { return true; }

        hal::PointCloud get_scan() override {
            hal::PointCloud cloud;

            // Re-use the Reader's buffer
            // Note: In production, you'd coordinate this so you don't read twice.
            // Usually the "BridgeManager" reads once and dispatches.

            std::vector<ipc::SimRadarPoint> raw_points;
            double time;
            std::vector<uint8_t> dummy_vid;

            uint64_t fid;
            if (reader_->has_new_frame(fid)) {
                reader_->read_sensor_data(time, raw_points, dummy_vid);

                cloud.timestamp = time;
                cloud.points.reserve(raw_points.size());

                for (const auto& p : raw_points) {
                    hal::RadarPoint pt;
                    pt.x = p.x;
                    pt.y = p.y;
                    pt.z = p.z;
                    pt.velocity = p.velocity;
                    pt.snr = p.snr_db;
                    cloud.points.push_back(pt);
                }
            }
            return cloud;
        }

    private:
        std::shared_ptr<ShmReader> reader_;
    };
}