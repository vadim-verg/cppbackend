#pragma once
#include "model.h"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string_view>
#include <string>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
using namespace std::literals;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    // Главный оператор обработки запросов
    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Безопасное приведение target к std::string_view для MSVC
        std::string_view target{req.target().data(), req.target().size()};

        // 1. По ТЗ обрабатываются только GET-запросы к API
        if (req.method() != http::verb::get) {
            send(MakeMethodNotAllowedResponse(req));
            return;
        }

        // 2. Маршрутизация эндпоинтов
        if (target == "/api/v1/maps"sv) {
            send(MakeMapListResponse(req));
        }
        else if (target.starts_with("/api/v1/maps/"sv)) {
            std::string_view map_id = target.substr("/api/v1/maps/"sv.size());
            send(MakeMapResponse(req, map_id));
        }
        else {
            send(MakeBadRequestResponse(req));
        }
    }

private:
    model::Game& game_;

    // Формирование базового шаблона ответа (Явно указываем тип возврата)
    template <typename Body, typename Allocator>
    static http::response<http::string_body> MakeBaseResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        http::status status,
        std::string_view body)
    {
        http::response<http::string_body> res(status, req.version());
        res.set(http::field::content_type, "application/json"); // Без sv для MSVC
        res.body() = body;
        res.prepare_payload();
        res.keep_alive(req.keep_alive());
        return res;
    }

    // 1. Формирование списка всех карт
    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeMapListResponse(const http::request<Body, http::basic_fields<Allocator>>& req) {
        boost::json::array json_maps;
        for (const auto& map : game_.GetMaps()) {
            boost::json::object json_map;
            json_map["id"] = *map.GetId();
            json_map["name"] = map.GetName();
            json_maps.push_back(std::move(json_map));
        }
        return MakeBaseResponse(req, http::status::ok, boost::json::serialize(json_maps));
    }

    // 2. Детальная информация по конкретной карте
    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeMapResponse(const http::request<Body, http::basic_fields<Allocator>>& req, std::string_view map_id) {
        model::Map::Id id{std::string(map_id)};
        const auto* map = game_.FindMap(id);

        if (!map) {
            boost::json::object error_obj;
            error_obj["code"] = "mapNotFound";
            error_obj["message"] = "Map not found";
            return MakeBaseResponse(req, http::status::not_found, boost::json::serialize(error_obj));
        }

        boost::json::object json_map;
        json_map["id"] = *map->GetId();
        json_map["name"] = map->GetName();

        // Дороги
        boost::json::array json_roads;
        for (const auto& road : map->GetRoads()) {
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
        json_map["roads"] = std::move(json_roads);

        // Здания
        boost::json::array json_buildings;
        for (const auto& building : map->GetBuildings()) {
            boost::json::object json_build;
            auto bounds = building.GetBounds();
            json_build["x"] = bounds.position.x;
            json_build["y"] = bounds.position.y;
            json_build["w"] = bounds.size.width;
            json_build["h"] = bounds.size.height;
            json_buildings.push_back(std::move(json_build));
        }
        json_map["buildings"] = std::move(json_buildings);

        // Офисы
        boost::json::array json_offices;
        for (const auto& office : map->GetOffices()) {
            boost::json::object json_office;
            json_office["id"] = *office.GetId();
            json_office["x"] = office.GetPosition().x;
            json_office["y"] = office.GetPosition().y;
            json_office["offsetX"] = office.GetOffset().dx;
            json_office["offsetY"] = office.GetOffset().dy;
            json_offices.push_back(std::move(json_office));
        }
        json_map["offices"] = std::move(json_offices);

        return MakeBaseResponse(req, http::status::ok, boost::json::serialize(json_map));
    }

    // 3. Обработка Bad Request
    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeBadRequestResponse(const http::request<Body, http::basic_fields<Allocator>>& req) {
        boost::json::object error_obj;
        error_obj["code"] = "badRequest";
        error_obj["message"] = "Bad request";
        return MakeBaseResponse(req, http::status::bad_request, boost::json::serialize(error_obj));
    }

    // 4. Обработка Method Not Allowed
    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeMethodNotAllowedResponse(const http::request<Body, http::basic_fields<Allocator>>& req) {
        boost::json::object error_obj;
        error_obj["code"] = "invalidMethod";
        error_obj["message"] = "Invalid method";
        auto res = MakeBaseResponse(req, http::status::method_not_allowed, boost::json::serialize(error_obj));
        res.set(http::field::allow, "GET"); // Без sv для MSVC
        return res;
    }
};

}  // namespace http_handler
