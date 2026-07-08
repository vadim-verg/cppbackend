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

boost::json::object SerializeMap(const model::Map& map);

struct Endpoints {

    // Пути
    static constexpr std::string_view Maps = "/api/v1/maps"sv;
    static constexpr std::string_view MapsPrefix = "/api/v1/maps/"sv;

    // Метод для извлечения ID карты
    static std::string_view ExtractMapId(std::string_view target) {
        if (target.starts_with(MapsPrefix)) {
            return target.substr(MapsPrefix.size());
        }
        return {};
    }
};

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {

        std::string_view target{req.target().data(), req.target().size()};

        if (req.method() != http::verb::get) {
            send(MakeMethodNotAllowedResponse(req));
            return;
        }

        if (target == Endpoints::Maps) {
            send(MakeMapListResponse(req));
        }
        else if (std::string_view map_id = Endpoints::ExtractMapId(target); !map_id.empty()) {
            send(MakeMapResponse(req, map_id));
        }
        else {
            send(MakeBadRequestResponse(req));
        }
    }

private:
    model::Game& game_;

    template <typename Body, typename Allocator>
    static http::response<http::string_body> MakeBaseResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        http::status status,
        std::string_view body)
    {
        http::response<http::string_body> res(status, req.version());
        res.set(http::field::content_type, "application/json");
        res.body() = body;
        res.prepare_payload();
        res.keep_alive(req.keep_alive());
        return res;
    }

    // Формирование списка всех карт
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

    // Информация по конкретной карте
    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeMapResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        std::string_view map_id)
    {
        model::Map::Id id{std::string(map_id)};
        const auto* map = game_.FindMap(id);

        // Обработка ошибки
        if (!map) {
            boost::json::object error_obj;
            error_obj["code"] = "mapNotFound";
            error_obj["message"] = "Map not found";
            return MakeBaseResponse(req, http::status::not_found, boost::json::serialize(error_obj));
        }

        boost::json::object json_map = SerializeMap(*map);
        return MakeBaseResponse(req, http::status::ok, boost::json::serialize(json_map));
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeBadRequestResponse(const http::request<Body, http::basic_fields<Allocator>>& req) {
        boost::json::object error_obj;
        error_obj["code"] = "badRequest";
        error_obj["message"] = "Bad request";
        return MakeBaseResponse(req, http::status::bad_request, boost::json::serialize(error_obj));
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeMethodNotAllowedResponse(const http::request<Body, http::basic_fields<Allocator>>& req) {
        boost::json::object error_obj;
        error_obj["code"] = "invalidMethod";
        error_obj["message"] = "Invalid method";
        auto res = MakeBaseResponse(req, http::status::method_not_allowed, boost::json::serialize(error_obj));
        res.set(http::field::allow, "GET");
        return res;
    }
};

}  // namespace http_handler
