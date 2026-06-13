#include "json_loader.h"
#include <fstream>

#include <boost/json.hpp>

// Этот заголовочный файл надо подключить в одном и только одном .cpp-файле программы
//#include <boost/json/src.hpp>
#include <iostream>
#include <string>

namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

model::Game LoadGame(const std::filesystem::path& json_path) {

    std::ifstream input_file(json_path);
    if (!input_file.is_open()) {
        throw std::runtime_error("Failed to open file: " + json_path.string());
    }

    std::stringstream buffer;
    buffer << input_file.rdbuf();

    auto value = json::parse(buffer.str());


    model::Game game;

    // корневой JSON-объект
    const auto& root_object = value.as_object();

    if (root_object.contains("maps")) {
        const auto& maps_array = root_object.at("maps").as_array();

        for (const auto& map_value : maps_array) {
            const auto& map_obj = map_value.as_object();

            std::string id_str = json::value_to<std::string>(map_obj.at("id"));
            std::string name_str = json::value_to<std::string>(map_obj.at("name"));

            model::Map::Id map_id{std::move(id_str)};

            // Создаем экземпляр карты
            model::Map game_map(std::move(map_id), std::move(name_str));

            if (map_obj.contains("roads")) {
                const auto& roads_array = map_obj.at("roads").as_array();

                for (const auto& road_value : roads_array) {
                    const auto& road_obj = road_value.as_object();

                    // начальные координаты для обоих типов дорог
                    int x0 = road_obj.at("x0").as_int64();
                    int y0 = road_obj.at("y0").as_int64();

                    // Проверяем наличие ключа x1, чтобы определить тип дороги
                    if (road_obj.contains("x1")) {
                        // горизонтальная дорога
                        int x1 = road_obj.at("x1").as_int64();

                        model::Road road(model::Road::HORIZONTAL, model::Point{x0, y0}, x1);
                        game_map.AddRoad(std::move(road));

                    } else if (road_obj.contains("y1")) {
                        // вертикальная дорога
                        int y1 = road_obj.at("y1").as_int64();

                        model::Road road(model::Road::VERTICAL, model::Point{x0, y0}, y1);
                        game_map.AddRoad(std::move(road));
                    }
                }
            }

            if (map_obj.contains("buildings")) {
                const auto& builds_array = map_obj.at("buildings").as_array();

                for (const auto& build_value : builds_array) {
                    const auto& build_obj = build_value.as_object();

                    // Координаты верхнего левого угла здания
                    int x = build_obj.at("x").as_int64();
                    int y = build_obj.at("y").as_int64();
                    //ширина
                    int w = build_obj.at("w").as_int64();
                    //высота
                    int h = build_obj.at("h").as_int64();

                    model::Building building(
                        model::Rectangle{
                            model::Point{model::Coord{x}, model::Coord{y}},
                            model::Size{w, h}
                        }
                    );
                    game_map.AddBuilding(std::move(building));
                }
            }

            if (map_obj.contains("offices")) {
                const auto& offices_array = map_obj.at("offices").as_array();

                for (const auto& office_value : offices_array) {
                    const auto& office_obj = office_value.as_object();


                    std::string id = boost::json::value_to<std::string>(office_obj.at("id"));
                    int x = office_obj.at("x").as_int64();
                    int y = office_obj.at("y").as_int64();
                    //смещение по горизонтали
                    int offsetX = office_obj.at("offsetX").as_int64();
                    //смещение по вертикали
                    int offsetY = office_obj.at("offsetY").as_int64();

                    model::Office office(
                        model::Office::Id{id},
                        model::Point{x, y},
                        model::Offset{offsetX, offsetY}
                        );
                    game_map.AddOffice(std::move(office));
                }
            }

            // Добавляем полностью собранную карту в игру
            game.AddMap(std::move(game_map));
        }
    }

    return game;
}

}  // namespace json_loader
