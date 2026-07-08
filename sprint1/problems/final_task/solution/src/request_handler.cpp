#include "request_handler.h"

namespace http_handler {

//вспомогательные функции для декомпоновки и возможности удобного расширения в дальнейшем

boost::json::array SerializeRoads(const model::Map& map) {
    boost::json::array json_roads;
    for (const auto& road : map.GetRoads()) {
        boost::json::object json_road;
        auto start = road.GetStart();
        json_road["x0"] = start.x;
        json_road["y0"] = start.y;
        if (road.IsHorizontal()) {
            json_road["x1"] = road.GetEnd().x;
        } else {
            json_road["y1"] = road.GetEnd().y;
        }
        json_roads.push_back(std::move(json_road));
    }
    return json_roads;
}

boost::json::array SerializeBuildings(const model::Map& map) {
    boost::json::array json_buildings;
    for (const auto& building : map.GetBuildings()) {
        boost::json::object json_build;
        auto bounds = building.GetBounds();
        json_build["x"] = bounds.position.x;
        json_build["y"] = bounds.position.y;
        json_build["w"] = bounds.size.width;
        json_build["h"] = bounds.size.height;
        json_buildings.push_back(std::move(json_build));
    }
    return json_buildings;
}

boost::json::array SerializeOffices(const model::Map& map) {
    boost::json::array json_offices;
    for (const auto& office : map.GetOffices()) {
        boost::json::object json_office;
        json_office["id"] = *office.GetId();
        json_office["x"] = office.GetPosition().x;
        json_office["y"] = office.GetPosition().y;
        json_office["offsetX"] = office.GetOffset().dx;
        json_office["offsetY"] = office.GetOffset().dy;
        json_offices.push_back(std::move(json_office));
    }
    return json_offices;
}

// Собираем полную карту в один JSON-объект
boost::json::object SerializeMap(const model::Map& map) {
    boost::json::object json_map;
    json_map["id"] = *map.GetId();
    json_map["name"] = map.GetName();

    // Делегируем сборку подобъектов
    json_map["roads"] = SerializeRoads(map);
    json_map["buildings"] = SerializeBuildings(map);
    json_map["offices"] = SerializeOffices(map);

    return json_map;
}

}  // namespace http_handler
