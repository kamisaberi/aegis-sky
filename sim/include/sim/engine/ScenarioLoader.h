#pragma once
#include <string>
#include <vector>
#include <memory>
#include "sim/engine/SimEntity.h"

namespace aegis::sim::engine {

    class ScenarioLoader {
    public:
        // Reads JSON and returns a list of entities to spawn
        static std::vector<std::shared_ptr<SimEntity>> load_mission(const std::string& filepath);
    };

}