#include "sim/engine/ScenarioLoader.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h> // Industry standard logging

namespace aegis::sim::engine {

    using json = nlohmann::json;

    std::vector<std::shared_ptr<SimEntity>> ScenarioLoader::load_mission(const std::string& filepath) {
        std::vector<std::shared_ptr<SimEntity>> entities;

        std::ifstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("[Scenario] Failed to open file: {}", filepath);
            return entities;
        }

        try {
            json j;
            file >> j;

            spdlog::info("[Scenario] Loading Mission: {}", j["mission_name"].get<std::string>());

            for (const auto& drone_data : j["drones"]) {
                std::string name = drone_data["name"];
                
                // Parse Position array [x, y, z]
                auto pos_arr = drone_data["start_pos"];
                glm::dvec3 pos(pos_arr[0], pos_arr[1], pos_arr[2]);

                // Create Entity
                auto entity = std::make_shared<SimEntity>(name, pos);

                // Parse Velocity if it exists
                if (drone_data.contains("velocity")) {
                    auto vel_arr = drone_data["velocity"];
                    entity->set_velocity(glm::dvec3(vel_arr[0], vel_arr[1], vel_arr[2]));
                }

                entities.push_back(entity);
                spdlog::info("  + Spawned: {} at [{}, {}, {}]", name, pos.x, pos.y, pos.z);
            }
        }
        catch (const json::parse_error& e) {
            spdlog::error("[Scenario] JSON Parse Error: {}", e.what());
        }

        return entities;
    }
}