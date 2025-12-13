#pragma once
#include <random>

namespace aegis::sim::math {

    class Random {
    public:
        static void init();

        // Returns a value with Mean=0 and StdDev=sigma
        // Example: gaussian(0.5) returns values mostly between -0.5 and 0.5
        static double gaussian(double sigma);

        // Returns random float between min and max
        static double uniform(double min, double max);

    private:
        static std::mt19937 generator_;
    };
}