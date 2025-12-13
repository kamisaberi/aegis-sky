#pragma once
#include "xtorch/nn/Module.h"
#include "xtorch/nn/Conv2d.h"
#include "xtorch/nn/Linear.h"

namespace aegis::brain::models {

    // A lightweight fusion backbone (ResNet-18 style)
    class AuraNet : public xtorch::nn::Module {
    public:
        AuraNet();

        // Forward Pass: 5-Channel Input -> Detection Tensor
        xtorch::Tensor forward(xtorch::Tensor x) override;

    private:
        // Feature Extractor
        // Input: 5 Channels (RGB + Depth + Velocity)
        xtorch::nn::Conv2d conv1_{nullptr};
        xtorch::nn::Conv2d conv2_{nullptr};
        xtorch::nn::Conv2d conv3_{nullptr};

        // Detection Head (Bounding Box + Class + Confidence)
        xtorch::nn::Linear fc_box_{nullptr};
        xtorch::nn::Linear fc_class_{nullptr};
    };
}