#include "json_loader.h"
#include <fstream>
#include <boost/json.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <chrono>

namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

double GetDoubleValue(const json::value& val) {
    if (val.is_double()) {
        return val.as_double();
    }
    if (val.is_int64()) {
        return static_cast<double>(val.as_int64());
    }
    return 1.0;
}

void ParseRoads(model::Map& game_map, const json::array& roads_array) {
    for (const auto& road_value : roads_array) {
        const auto& road_obj = road_value.as_object();
        int x0 = static_cast<int>(road_obj.at("x0").as_int64());
        int y0 = static_cast<int>(road_obj.at("y0").as_int64());

        if (road_obj.contains("x1")) {
            int x1 = static_cast<int>(road_obj.at("x1").as_int64());
            game_map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{x0, y0}, x1));
        } else if (road_obj.contains("y1")) {
            int y1 = static_cast<int>(road_obj.at("y1").as_int64());
            game_map.AddRoad(model::Road(model::Road::VERTICAL, model::Point{x0, y0}, y1));
        }
    }
}

void ParseBuildings(model::Map& game_map, const json::array& builds_array) {
    for (const auto& build_value : builds_array) {
        const auto& build_obj = build_value.as_object();
        int x = static_cast<int>(build_obj.at("x").as_int64());
        int y = static_cast<int>(build_obj.at("y").as_int64());
        int w = static_cast<int>(build_obj.at("w").as_int64());
        int h = static_cast<int>(build_obj.at("h").as_int64());

        game_map.AddBuilding(model::Building(
            model::Rectangle{
                model::Point{model::Coord{x}, model::Coord{y}},
                model::Size{w, h}
            }
            ));
    }
}

void ParseOffices(model::Map& game_map, const json::array& offices_array) {
    for (const auto& office_value : offices_array) {
        const auto& office_obj = office_value.as_object();
        std::string id = json::value_to<std::string>(office_obj.at("id"));
        int x = static_cast<int>(office_obj.at("x").as_int64());
        int y = static_cast<int>(office_obj.at("y").as_int64());
        int offsetX = static_cast<int>(office_obj.at("offsetX").as_int64());
        int offsetY = static_cast<int>(office_obj.at("offsetY").as_int64());

        game_map.AddOffice(model::Office(
            model::Office::Id{std::move(id)},
            model::Point{x, y},
            model::Offset{offsetX, offsetY}
            ));
    }
}

ParsedGameData LoadGame(const std::filesystem::path& json_path) {
    std::ifstream input_file(json_path);
    if (!input_file.is_open()) {
        throw std::runtime_error("Failed to open file: " + json_path.string());
    }

    std::stringstream buffer;
    buffer << input_file.rdbuf();

    json::value value;
    try {
        value = json::parse(buffer.str());
    }
    catch (const std::exception& e) {
        std::cerr << "json load error: " << json_path.string() << "\n"
                  << "details: " << e.what() << "\n";
        throw std::runtime_error("Failed to parse game config: "s + e.what());
    }

    model::Game game;
    app::LootInfoProvider loot_info;

    const auto& root_object = value.as_object();

    double default_speed = 1.0;
    if (root_object.contains("defaultDogSpeed")) {
        default_speed = GetDoubleValue(root_object.at("defaultDogSpeed"));
    }
    game.SetDefaultDogSpeed(default_speed);

    size_t default_bag_capacity = 3;
    if (root_object.contains("defaultBagCapacity")) {
        default_bag_capacity = static_cast<size_t>(root_object.at("defaultBagCapacity").as_int64());
    }

    if (root_object.contains("dogRetirementTime")) {
        double retirement_time = GetDoubleValue(root_object.at("dogRetirementTime"));
        game.SetDogRetirementTime(retirement_time);
    } else {
        game.SetDogRetirementTime(0.0); // 0.0 означает, что таймаута бездействия нет
    }

    if (root_object.contains("lootGeneratorConfig")) {
        const auto& gen_config = root_object.at("lootGeneratorConfig").as_object();
        double period = GetDoubleValue(gen_config.at("period"));
        double probability = GetDoubleValue(gen_config.at("probability"));

        auto period_ms = std::chrono::milliseconds(static_cast<long long>(period * 1000.0));
        game.SetLootGeneratorConfig(period_ms, probability);
    }

    if (root_object.contains("maps")) {
        const auto& maps_array = root_object.at("maps").as_array();

        for (const auto& map_value : maps_array) {
            const auto& map_obj = map_value.as_object();

            std::string id_str = json::value_to<std::string>(map_obj.at("id"));
            std::string name_str = json::value_to<std::string>(map_obj.at("name"));

            // Сохраняем копию id для провайдера лута перед std::move
            std::string map_id_copy = id_str;

            model::Map game_map(model::Map::Id{std::move(id_str)}, std::move(name_str));

            // Читаем индивидуальную скорость для конкретной карты.
            if (map_obj.contains("dogSpeed")) {
                game_map.SetDogSpeed(GetDoubleValue(map_obj.at("dogSpeed")));
            } else {
                game_map.SetDogSpeed(default_speed);
            }

            size_t current_map_bag_capacity = default_bag_capacity;
            if (map_obj.contains("bagCapacity")) {
                current_map_bag_capacity = static_cast<size_t>(map_obj.at("bagCapacity").as_int64());
            }
            game_map.SetBagCapacity(current_map_bag_capacity);

            if (map_obj.contains("roads")) {
                ParseRoads(game_map, map_obj.at("roads").as_array());
            }

            if (map_obj.contains("buildings")) {
                ParseBuildings(game_map, map_obj.at("buildings").as_array());
            }

            if (map_obj.contains("offices")) {
                ParseOffices(game_map, map_obj.at("offices").as_array());
            }

            if (map_obj.contains("lootTypes")) {
                const auto& loot_types_array = map_obj.at("lootTypes").as_array();

                // Передаем в модель только количество
                game_map.SetLootTypesCount(loot_types_array.size());

                // Парсим ценность (очки) каждого типа лута
                std::vector<int> loot_values;
                loot_values.reserve(loot_types_array.size());

                for (const auto& loot_type_val : loot_types_array) {
                    const auto& loot_type_obj = loot_type_val.as_object();
                    int points = static_cast<int>(loot_type_obj.at("value").as_int64());
                    loot_values.push_back(points);
                }

                game_map.SetLootValues(std::move(loot_values));

                loot_info.AddLootTypesForMap(map_id_copy, loot_types_array);
            }

            game.AddMap(std::move(game_map));
        }
    }

    return {std::move(game), std::move(loot_info)};
}

std::string_view DirectionToString(model::Direction dir) {
    using namespace std::literals;
    switch (dir) {
    case model::Direction::NORTH: return "U"sv; // Up
    case model::Direction::SOUTH: return "D"sv; // Down
    case model::Direction::WEST:  return "L"sv; // Left
    case model::Direction::EAST:  return "R"sv; // Right
    }
    return "U"sv;
}

}  // namespace json_loader
