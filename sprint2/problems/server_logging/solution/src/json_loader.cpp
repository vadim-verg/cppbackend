#include "json_loader.h"
#include <fstream>
#include <boost/json.hpp>
#include <iostream>
#include <string>

namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

//вспомогательные функции для декомпоновки парсера и возможности удобного расширения в дальнейшем

void ParseRoads(model::Map& game_map, const json::array& roads_array) {
    for (const auto& road_value : roads_array) {
        const auto& road_obj = road_value.as_object();
        int x0 = road_obj.at("x0").as_int64();
        int y0 = road_obj.at("y0").as_int64();

        if (road_obj.contains("x1")) {
            int x1 = road_obj.at("x1").as_int64();
            game_map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{x0, y0}, x1));
        } else if (road_obj.contains("y1")) {
            int y1 = road_obj.at("y1").as_int64();
            game_map.AddRoad(model::Road(model::Road::VERTICAL, model::Point{x0, y0}, y1));
        }
    }
}

void ParseBuildings(model::Map& game_map, const json::array& builds_array) {
    for (const auto& build_value : builds_array) {
        const auto& build_obj = build_value.as_object();
        int x = build_obj.at("x").as_int64();
        int y = build_obj.at("y").as_int64();
        int w = build_obj.at("w").as_int64();
        int h = build_obj.at("h").as_int64();

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
        int x = office_obj.at("x").as_int64();
        int y = office_obj.at("y").as_int64();
        int offsetX = office_obj.at("offsetX").as_int64();
        int offsetY = office_obj.at("offsetY").as_int64();

        game_map.AddOffice(model::Office(
            model::Office::Id{std::move(id)},
            model::Point{x, y},
            model::Offset{offsetX, offsetY}
            ));
    }
}

model::Game LoadGame(const std::filesystem::path& json_path) {
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

        //выбрасываем исключение, чтобы дальше не выполнять код на пустом value
        throw std::runtime_error("Failed to parse game config: "s + e.what());
    }

    model::Game game;
    const auto& root_object = value.as_object();

    if (root_object.contains("maps")) {
        const auto& maps_array = root_object.at("maps").as_array();

        for (const auto& map_value : maps_array) {
            const auto& map_obj = map_value.as_object();

            std::string id_str = json::value_to<std::string>(map_obj.at("id"));
            std::string name_str = json::value_to<std::string>(map_obj.at("name"));

            model::Map game_map(model::Map::Id{std::move(id_str)}, std::move(name_str));

            if (map_obj.contains("roads")) {
                ParseRoads(game_map, map_obj.at("roads").as_array());
            }

            if (map_obj.contains("buildings")) {
                ParseBuildings(game_map, map_obj.at("buildings").as_array());
            }

            if (map_obj.contains("offices")) {
                ParseOffices(game_map, map_obj.at("offices").as_array());
            }

            game.AddMap(std::move(game_map));
        }
    }

    return game;
}

}  // namespace json_loader
