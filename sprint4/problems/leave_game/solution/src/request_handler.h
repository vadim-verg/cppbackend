#pragma once
#include "model.h"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <string_view>
#include <string>
#include "logger.h"
#include <chrono>
#include "api_handler.h"
#include "players.h"
#include "loot_provider.h"

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
using namespace std::literals;

boost::json::object SerializeMap(const model::Map& map, const app::LootInfoProvider& loot_info_provider);

struct Endpoints {
    static constexpr std::string_view Maps = "/api/v1/maps"sv;
    static constexpr std::string_view MapsPrefix = "/api/v1/maps/"sv;

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

template <typename SomeRequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(SomeRequestHandler& decorated)
        : decorated_(decorated) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(std::string ip, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string_view target{req.target().data(), req.target().size()};
        std::string_view method{req.method_string().data(), req.method_string().size()};

        logger::LogRequest(ip, target, method);

        auto start_time = std::chrono::steady_clock::now();

        auto logging_send = [start_time, send = std::forward<Send>(send)](auto&& response) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            auto it = response.find(http::field::content_type);
            bool has_ct = (it != response.end());

            std::string_view ct = has_ct ? std::string_view{it->value().data(), it->value().size()} : ""sv;

            logger::LogResponse(duration, response.result_int(), ct, has_ct);

            send(std::forward<decltype(response)>(response));
        };

        decorated_(std::forward<decltype(req)>(req), std::move(logging_send));
    }

private:
    SomeRequestHandler& decorated_;
};

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game,
                            std::filesystem::path static_path,
                            app::Application& app,
                            const app::LootInfoProvider& loot_info,
                            std::shared_ptr<StateManager> state_manager)
        : game_{game}
        , static_path_{std::move(static_path)}
        , app_{app}
        , api_handler_{app, state_manager}
        , loot_info_{loot_info}
    {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {

        std::string_view target{req.target().data(), req.target().size()};

        // Сначала проверяем эндпоинт таблицы рекордов, так как параметры query string (?start=...)
        // не должны отсекаться до парсинга параметров пагинации.
        if (target.starts_with("/api/v1/game/records")) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                send(MakeMethodNotAllowedResponse(req));
                return;
            }

            size_t start = 0;
            size_t max_items = 100; // Значение по умолчанию согласно ТЗ

            size_t query_pos = target.find('?');
            if (query_pos != std::string_view::npos) {
                std::string_view query = target.substr(query_pos + 1);

                // Лямбда-функция для надежного поиска и извлечения параметров внутри Query String
                auto parse_param = [](std::string_view query_str, std::string_view key) -> std::optional<size_t> {
                    size_t pos = query_str.find(key);
                    if (pos == std::string_view::npos) {
                        return std::nullopt;
                    }
                    pos += key.size();
                    size_t end_pos = query_str.find('&', pos);

                    std::string_view val_sv;
                    if (end_pos == std::string_view::npos) {
                        val_sv = query_str.substr(pos);
                    } else {
                        val_sv = query_str.substr(pos, end_pos - pos);
                    }

                    try {
                        return std::stoull(std::string(val_sv));
                    } catch (...) {
                        return std::nullopt;
                    }
                };

                if (auto s_opt = parse_param(query, "start=")) {
                    start = *s_opt;
                }
                if (auto m_opt = parse_param(query, "maxItems=")) {
                    max_items = *m_opt;
                }
            }

            // Если maxItems превышает 100, должна вернуться ошибка с кодом 400 Bad Request
            if (max_items > 100) {
                boost::json::object error_obj;
                error_obj["code"] = "badRequest";
                error_obj["message"] = "maxItems cannot exceed 100";
                send(MakeBaseResponse(req, http::status::bad_request, boost::json::serialize(error_obj)));
                return;
            }

            // Формируем JSON-массив из записей в базе данных PostgreSQL
            boost::json::array records_json;
            if (app_.GetDBRepository()) { // app_ — ссылка/указатель на экземпляр Application в RequestHandler
                auto records = app_.GetDBRepository()->GetRecords(start, max_items);
                for (const auto& rec : records) {
                    boost::json::object obj;
                    obj["name"] = rec.name;
                    obj["score"] = rec.score;
                    obj["playTime"] = rec.play_time;
                    records_json.push_back(std::move(obj));
                }
            }

            // Отправляем успешный ответ с заголовками Content-Type: application/json и Cache-Control: no-cache
            send(MakeBaseResponse(req, http::status::ok, boost::json::serialize(records_json)));
            return;
        }

        // Для всех остальных эндпоинтов отсекаем Query String параметры (например, для статики или карт)
        if (auto query_pos = target.find('?'); query_pos != std::string_view::npos) {
            target = target.substr(0, query_pos);
        }

        std::string decoded_target = UrlDecode(target);
        std::string_view target_sv = decoded_target;

        // Маршрутизация игрового API
        if (target_sv.starts_with("/api/")) {

            if (target_sv == Endpoints::Maps) {
                if (req.method() != http::verb::get && req.method() != http::verb::head) {
                    send(MakeMethodNotAllowedResponse(req));
                    return;
                }
                send(MakeMapListResponse(req));
                return;
            }

            if (std::string_view map_id = Endpoints::ExtractMapId(target_sv); !map_id.empty()) {
                if (req.method() != http::verb::get && req.method() != http::verb::head) {
                    send(MakeMethodNotAllowedResponse(req));
                    return;
                }
                send(MakeMapResponse(req, map_id));
                return;
            }

            if (target_sv.starts_with("/api/v1/game/")) {
                send(api_handler_.HandleRequest(req));
                return;
            }

            send(MakeBadRequestResponse(req));
            return;
        }

        // Маршрутизация статических файлов
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            send(MakeMethodNotAllowedResponse(req));
            return;
        }

        std::string_view req_path = target_sv;
        if (req_path.starts_with('/')) {
            req_path.remove_prefix(1);
        }

        std::filesystem::path absolute_file_path = static_path_ / std::filesystem::path(req_path);
        if (std::filesystem::is_directory(absolute_file_path)) {
            absolute_file_path /= "index.html";
        }

        if (!IsSubpath(absolute_file_path, static_path_)) {
            send(MakeTextErrorResponse(req, http::status::bad_request, "Bad Request: Out of static root"));
            return;
        }

        if (!std::filesystem::exists(absolute_file_path)) {
            send(MakeTextErrorResponse(req, http::status::not_found, "File Not Found"));
            return;
        }

        send(MakeFileResponse(req, absolute_file_path, req.method()));
    }

private:
    model::Game& game_;
    std::filesystem::path static_path_;
    app::Application& app_;
    ApiHandler api_handler_;
    const app::LootInfoProvider& loot_info_;

    template <typename Body, typename Allocator>
    static http::response<http::string_body> MakeBaseResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        http::status status, std::string_view body) {

        http::response<http::string_body> res(status, req.version());
        res.set(http::field::content_type, "application/json");
        res.set(http::field::cache_control, "no-cache");

        // 1. ЯВНО задаем Content-Length на основе размера сгенерированного JSON-текста.
        // Это гарантирует, что размер будет указан всегда, даже если тело ответа пустое!
        res.set(http::field::content_length, std::to_string(body.size()));

        // 2. Тело ответа прикрепляем ТОЛЬКО если это обычный GET запрос.
        // Если это HEAD запрос — по стандарту HTTP тело должно оставаться пустым.
        if (req.method() != boost::beast::http::verb::head) {
            res.body() = body;
        }

        res.prepare_payload();
        res.keep_alive(req.keep_alive());
        return res;
    }

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

        boost::json::object json_map = SerializeMap(*map, loot_info_);
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

    // Вспомогательный метод для текстовых ошибок статики
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

    // Конвертер из string_body в file_body
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

    // Извлечение токена из заголовка
    std::optional<std::string> TryExtractToken(const auto& req) const;

    // Генератор ответа состояния игры
    template <typename Body, typename Allocator>
    auto MakeGameStateResponse(const boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>& req, const app::Application& app_facade) const {
        namespace http = boost::beast::http;

        auto token_opt = TryExtractToken(req);
        // Ошибка отсутствие заголовка - 401 invalidToken
        if (!token_opt) {
            boost::json::object err{{"code", "invalidToken"}, {"message", "Authorization header is required"}};
            http::response<http::string_body> res(http::status::unauthorized, req.version());
            res.body() = boost::json::serialize(err);
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            res.prepare_payload();
            return res;
        }

        auto players_opt = app_facade.GetPlayersInSession(*token_opt);
        // Ошибка токен не найден - 401 unknownToken
        if (!players_opt) {
            boost::json::object err{{"code", "unknownToken"}, {"message", "Player token has not been found"}};
            http::response<http::string_body> res(http::status::unauthorized, req.version());
            res.body() = boost::json::serialize(err);
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            res.prepare_payload();
            return res;
        }

        // Сбор JSON с вещественными координатами
        boost::json::object players_json;
        for (const auto& player : *players_opt) {
            auto dog_ptr = player->GetDog();

            boost::json::object dog_data;
            dog_data["pos"] = boost::json::array{dog_ptr->GetPosition().x, dog_ptr->GetPosition().y};
            dog_data["speed"] = boost::json::array{dog_ptr->GetSpeed().ux, dog_ptr->GetSpeed().uy};
            dog_data["dir"] = "U";

            players_json[std::to_string(player->GetId())] = dog_data;
        }

        boost::json::object root{{"players", players_json}};
        http::response<http::string_body> res(http::status::ok, req.version());
        res.body() = boost::json::serialize(root);
        res.set(http::field::content_type, "application/json");
        res.set(http::field::cache_control, "no-cache");
        res.prepare_payload();
        return res;
    }

};

}  // namespace http_handler
