#include "data/SimDataset.h"
#include <filesystem>
#include <fstream>
#include <opencv2/opencv.hpp> // Used only for image decoding
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace aegis::brain::data {

    SimDataset::SimDataset(const std::string& root_dir) {
        // Scan directory for matched pairs (frame_001.png + frame_001.json)
        for (const auto& entry : fs::directory_iterator(root_dir)) {
            if (entry.path().extension() == ".json") {
                Sample s;
                s.radar_path = entry.path().string(); // Radar/Truth data
                s.image_path = entry.path().replace_extension(".png").string();

                if (fs::exists(s.image_path)) {
                    samples_.push_back(s);
                }
            }
        }
    }

    size_t SimDataset::size() const { return samples_.size(); }

    std::tuple<xtorch::Tensor, xtorch::Tensor> SimDataset::get_item(size_t index) {
        const auto& sample = samples_[index];

        // 1. Load Image (HWC -> CHW)
        cv::Mat img = cv::imread(sample.image_path);
        // Normalize 0-255 to 0-1
        xtorch::Tensor t_img = xtorch::from_blob(img.data, {3, img.rows, img.cols}).div(255.0);

        // 2. Load Radar/Truth Data
        std::ifstream f(sample.radar_path);
        json j; f >> j;

        // 3. Create "Depth/Velocity" Channels (The Fusion Step)
        // In a real pipeline, we project points here if not done by Sim.
        // Assuming Sim output pre-projected maps for training efficiency.
        xtorch::Tensor t_radar = xtorch::zeros({2, img.rows, img.cols});
        // ... fill t_radar from JSON point cloud ...

        // 4. Concatenate: [3, H, W] + [2, H, W] = [5, H, W]
        xtorch::Tensor input = xtorch::cat({t_img, t_radar}, 0);

        // 5. Create Label Tensor (Bounding Box)
        std::vector<float> box = j["ground_truth_box"].get<std::vector<float>>();
        xtorch::Tensor label = xtorch::tensor(box);

        return {input, label};
    }
}