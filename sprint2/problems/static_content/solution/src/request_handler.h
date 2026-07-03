#pragma once
#include "model.h"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <filesystem>
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

std::string_view GetMimeType(const std::filesystem::path& path);
std::string UrlDecode(std::string_view src);
bool IsSubpath(std::filesystem::path path, std::filesystem::path base);

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, std::filesystem::path static_path)
        : game_{game}
        , static_path_{std::move(static_path)} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {

        // Проверяем валидность метода (разрешены только GET и HEAD)
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            send(MakeMethodNotAllowedResponse(req));
            return;
        }

        std::string_view target{req.target().data(), req.target().size()};

        // 1. Нормализация URL: убираем Query-параметры (?...)
        if (auto query_pos = target.find('?'); query_pos != std::string_view::npos) {
            target = target.substr(0, query_pos);
        }

        // 2. Декодируем URL от %20, плюсов и т.д.
        std::string decoded_target = UrlDecode(target);

        // 3. МАРШРУТИЗАЦИЯ: Если это API, отдаем старому роутеру
        if (decoded_target.starts_with("/api/")) {
            if (req.method() != http::verb::get) { // API поддерживает только GET
                send(MakeMethodNotAllowedResponse(req));
                return;
            }
            if (decoded_target == Endpoints::Maps) {
                send(MakeMapListResponse(req));
            } else if (std::string_view map_id = Endpoints::ExtractMapId(decoded_target); !map_id.empty()) {
                send(MakeMapResponse(req, map_id));
            } else {
                send(MakeBadRequestResponse(req));
            }
            return;
        }

        // 4. МАРШРУТИЗАЦИЯ СТАТИКИ
        std::filesystem::path req_path = decoded_target;
        std::filesystem::path absolute_file_path = static_path_ / req_path.relative_path();

        // Если запрашивают каталог (например, /), добавляем index.html
        if (std::filesystem::is_directory(absolute_file_path)) {
            absolute_file_path /= "index.html";
        }

        // Проверяем безопасность пути (чтобы не выйти из корня статики)
        if (!IsSubpath(absolute_file_path, static_path_)) {
            send(MakeTextErrorResponse(req, http::status::bad_request, "Bad Request: Out of static root"));
            return;
        }

        // Проверяем существование файла
        if (!std::filesystem::exists(absolute_file_path)) {
            send(MakeTextErrorResponse(req, http::status::not_found, "File Not Found"));
            return;
        }

        // Если всё успешно — отправляем файл
        send(MakeFileResponse(req, absolute_file_path, req.method()));
    }

private:
    model::Game& game_;
    std::filesystem::path static_path_;

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
        res.set(http::field::allow, "GET, HEAD");
        return res;
    }

    // Вспомогательный метод для текстовых ошибок статики (Content-Type: text/plain)
    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeTextErrorResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        http::status status,
        std::string_view message)
    {
        http::response<http::string_body> res(status, req.version());
        res.set(http::field::content_type, "text/plain");
        res.body() = message;
        res.prepare_payload();
        res.keep_alive(req.keep_alive());
        return res;
    }

    // Конвертер из string_body в file_body для красивой обработки ошибок внутри асинхронной отправки файлов
    http::response<http::file_body> MakeFileErrorFromText(http::response<http::string_body>&& text_res) {
        http::response<http::file_body> res(text_res.result(), text_res.version());
        res.set(http::field::content_type, std::string(text_res.at(http::field::content_type)));
        res.keep_alive(text_res.keep_alive());
        res.prepare_payload();
        return res;
    }

    template <typename Body, typename Allocator>
    http::response<http::file_body> MakeFileResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        const std::filesystem::path& file_path,
        http::verb method)
    {
        http::response<http::file_body> res;
        res.version(req.version());
        res.result(http::status::ok);
        res.set(http::field::content_type, std::string(GetMimeType(file_path)));
        res.keep_alive(req.keep_alive());

        http::file_body::value_type file;
        boost::beast::error_code ec;
        file.open(file_path.string().c_str(), boost::beast::file_mode::read, ec);

        if (ec) {
            auto err_res = MakeTextErrorResponse(req, http::status::not_found, "File not found");
            return MakeFileErrorFromText(std::move(err_res));
        }

        res.content_length(file.size());

        if (method == http::verb::get) {
            res.body() = std::move(file);
        }

        res.prepare_payload();
        return res;
    }
};

}  // namespace http_handler

