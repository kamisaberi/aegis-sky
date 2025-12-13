#pragma once
#include "xtorch/data/Dataset.h" // Your library
#include <string>
#include <vector>
#include <tuple>

namespace aegis::brain::data {

    class SimDataset : public xtorch::data::Dataset {
    public:
        // Load data from the Sim's output folder
        explicit SimDataset(const std::string& root_dir);

        // Returns { InputTensor, LabelTensor }
        // Input: [Channels: 5, Height, Width] -> (R, G, B, Depth, Velocity)
        // Label: [BoxX, BoxY, BoxW, BoxH, ClassID]
        std::tuple<xtorch::Tensor, xtorch::Tensor> get_item(size_t index) override;

        size_t size() const override;

    private:
        struct Sample {
            std::string image_path;
            std::string radar_path;
            std::vector<float> labels; // Ground truth bounding boxes
        };
        std::vector<Sample> samples_;
    };
}