#include "sim/engine/ScenarioLoader.h"

#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

namespace aegis::sim::engine {

    using json = nlohmann::json;

    // -------------------------------------------------------------------------
    // HELPER: String -> Enum Conversion
    // -------------------------------------------------------------------------
    static EntityType string_to_type(const std::string& s) {
        if (s == "QUADCOPTER") return EntityType::QUADCOPTER;
        if (s == "FIXED_WING") return EntityType::FIXED_WING;
        if (s == "BIRD")       return EntityType::BIRD;
        return EntityType::UNKNOWN;
    }

    // -------------------------------------------------------------------------
    // MAIN LOADER LOGIC
    // -------------------------------------------------------------------------
    std::vector<std::shared_ptr<SimEntity>> ScenarioLoader::load_mission(const std::string& filepath) {
        std::vector<std::shared_ptr<SimEntity>> entities;
        std::ifstream file(filepath);

        // 1. File Check
        if (!file.is_open()) {
            spdlog::error("[Loader] Failed to open scenario file: {}", filepath);
            return entities;
        }

        try {
            // 2. Parse JSON
            json j;
            file >> j;

            std::string mission_name = j.value("mission_name", "Unknown Mission");
            spdlog::info("[Loader] Loading Mission: '{}'", mission_name);

            // 3. Iterate Entities
            // Support both "entities" (new standard) and "drones" (legacy) keys
            std::string key = j.contains("entities") ? "entities" : "drones";

            for (const auto& item : j[key]) {
                // --- Mandatory Fields ---
                std::string name = item.at("name");
                
                auto pos_arr = item.at("start_pos");
                glm::dvec3 start_pos(pos_arr[0], pos_arr[1], pos_arr[2]);

                // Create the Entity
                auto entity = std::make_shared<SimEntity>(name, start_pos);

                // --- Optional Physics Fields ---
                
                // Entity Type (Default: UNKNOWN)
                if (item.contains("type")) {
                    entity->set_type(string_to_type(item["type"]));
                }

                // Radar Cross Section (Default: 0.01)
                if (item.contains("rcs")) {
                    entity->set_rcs(item["rcs"]);
                }

                // Max Speed (Default: 10.0 m/s)
                if (item.contains("speed")) {
                    entity->set_speed(item["speed"]);
                }

                // Thermal Temperature (Default: 50.0 C)
                if (item.contains("temperature_c")) {
                    entity->set_temperature(item["temperature_c"]);
                }

                // Initial Velocity (Legacy direct control)
                if (item.contains("velocity")) {
                    auto vel = item["velocity"];
                    entity->set_velocity(glm::dvec3(vel[0], vel[1], vel[2]));
                }

                // --- Pathfinding / Waypoints ---
                if (item.contains("waypoints")) {
                    for (const auto& wp : item["waypoints"]) {
                        if (wp.size() >= 3) {
                            entity->add_waypoint(glm::dvec3(wp[0], wp[1], wp[2]));
                        }
                    }
                    spdlog::debug("  + Added {} waypoints for {}", item["waypoints"].size(), name);
                }

                entities.push_back(entity);
                spdlog::info("  + Spawned Entity: [{}] Type: {} RCS: {}", 
                    name, 
                    item.value("type", "UNKNOWN"), 
                    item.value("rcs", 0.01)
                );
            }
        }
        catch (const json::parse_error& e) {
            spdlog::critical("[Loader] JSON Syntax Error: {}", e.what());
        }
        catch (const std::exception& e) {
            spdlog::critical("[Loader] Data Error: {}", e.what());
        }

        return entities;
    }

}