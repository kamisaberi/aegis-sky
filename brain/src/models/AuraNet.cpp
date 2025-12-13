#include "models/AuraNet.h"
#include "xtorch/nn/Functional.h" // relu, max_pool

namespace aegis::brain::models {

    AuraNet::AuraNet() {
        // Layer 1: Accepts 5 channels (RGBDV), outputs 64 features
        conv1_ = register_module("conv1", xtorch::nn::Conv2d(5, 64, 7, 2)); // Stride 2

        // Layer 2: 64 -> 128
        conv2_ = register_module("conv2", xtorch::nn::Conv2d(64, 128, 3, 2));

        // Layer 3: 128 -> 256
        conv3_ = register_module("conv3", xtorch::nn::Conv2d(128, 256, 3, 2));

        // Heads (assuming global average pooling reduces to vector size 256)
        fc_box_ = register_module("fc_box", xtorch::nn::Linear(256, 4)); // [x, y, w, h]
        fc_class_ = register_module("fc_cls", xtorch::nn::Linear(256, 3)); // [Drone, Bird, Plane]
    }

    xtorch::Tensor AuraNet::forward(xtorch::Tensor x) {
        // 1. Feature Extraction
        x = xtorch::relu(conv1_->forward(x));
        x = xtorch::max_pool2d(x, 2);

        x = xtorch::relu(conv2_->forward(x));
        x = xtorch::max_pool2d(x, 2);

        x = xtorch::relu(conv3_->forward(x));

        // 2. Global Average Pooling (Spatial -> Vector)
        x = x.mean({2, 3});

        // 3. Heads
        auto box = fc_box_->forward(x);         // Regression
        auto cls = fc_class_->forward(x);       // Classification

        // Return concatenated result
        return xtorch::cat({box, cls}, 1);
    }
}