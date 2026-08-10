#include "api_handler.h"
#include <iostream>

namespace http_handler {

// Генерация JSON-ответов с ошибками
http::response<http::string_body> ApiHandler::MakeErrorResponse(
    http::status status,
    const std::string& code,
    const std::string& message,
    unsigned int version) const
{
    http::response<http::string_body> res(status, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");

    boost::json::object obj;
    obj["code"] = code;
    obj["message"] = message;

    res.body() = boost::json::serialize(obj);
    res.prepare_payload();
    return res;
}

// Извлечение токена из заголовка Authorization
std::optional<std::string> ApiHandler::TryExtractToken(const http::request<http::string_body>& req) const {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) {
        return std::nullopt;
    }

    std::string_view auth_header{it->value().data(), it->value().size()};
    const std::string_view prefix = "Bearer ";

    if (!auth_header.starts_with(prefix)) {
        return std::nullopt;
    }

    auth_header.remove_prefix(prefix.size());

    // Очищаем от возможных пробелов по краям
    while (!auth_header.empty() && auth_header.front() == ' ') auth_header.remove_prefix(1);
    while (!auth_header.empty() && auth_header.back() == ' ') auth_header.remove_suffix(1);

    if (auth_header.size() != 32) {
        return std::nullopt;
    }

    return std::string(auth_header);
}

// POST /api/v1/game/join
http::response<http::string_body> ApiHandler::HandleJoinGame(const http::request<http::string_body>& req) const {
    unsigned int version = req.version();

    if (req.method() != http::verb::post) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", version);
        res.set(http::field::allow, "POST");
        return res;
    }

    std::string user_name;
    std::string map_id;

    // Безопасный парсинг JSON без перехвата внутренних ошибок бизнес-логики
    try {
        auto json_doc = boost::json::parse(req.body());
        const auto& json_obj = json_doc.as_object();
        user_name = boost::json::value_to<std::string>(json_obj.at("userName"));
        map_id = boost::json::value_to<std::string>(json_obj.at("mapId"));
    } catch (const std::exception& e) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", version);
    }

    if (user_name.empty()) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid name", version);
    }

    // Вынос вызова игровой логики из try-catch парсинга JSON
    std::optional<app::JoinGameResult> join_result;
    try {
        join_result = app_.JoinGame(user_name, map_id);
    } catch (const std::exception& e) {
        std::cerr << "[FATAL JOIN ERROR]: " << e.what() << std::endl;
        return MakeErrorResponse(http::status::internal_server_error, "internalServerError", "Internal server error occurred", version);
    }

    if (!join_result) {
        return MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found", version);
    }

    http::response<http::string_body> res(http::status::ok, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");

    boost::json::object res_body;
    res_body["authToken"] = join_result->token;
    res_body["playerId"] = join_result->player_id;

    res.body() = boost::json::serialize(res_body);
    res.prepare_payload();

    return res;
}

// GET /api/v1/game/players
http::response<http::string_body> ApiHandler::HandleGetPlayers(const http::request<http::string_body>& req) const {
    unsigned int version = req.version();

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", version);
        res.set(http::field::allow, "GET, HEAD");
        return res;
    }

    auto token = TryExtractToken(req);
    if (!token) {
        return MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is missing", version);
    }

    auto players_opt = app_.GetPlayersInSession(*token);
    if (!players_opt) {
        return MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", version);
    }

    boost::json::object players_json;
    for (const auto& player : *players_opt) {
        boost::json::object player_data;
        player_data["name"] = player->GetName();
        players_json[std::to_string(player->GetId())] = player_data;
    }

    http::response<http::string_body> res(http::status::ok, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");

    if (req.method() == http::verb::get) {
        res.body() = boost::json::serialize(players_json);
    }

    res.prepare_payload();
    return res;
}

// GET /api/v1/game/state
http::response<http::string_body> ApiHandler::HandleGetGameState(const http::request<http::string_body>& req) const {
    unsigned int version = req.version();

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", version);
        res.set(http::field::allow, "GET, HEAD");
        return res;
    }

    auto token = TryExtractToken(req);
    if (!token) {
        return MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", version);
    }

    auto players_opt = app_.GetPlayersInSession(*token);
    if (!players_opt) {
        return MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", version);
    }

    boost::json::object players_json;
    model::Map::Id current_map_id{""};

    for (const auto& player : *players_opt) {
        auto dog_ptr = player->GetDog();

        if ((*current_map_id).empty()) {
            current_map_id = model::Map::Id{player->GetMapId()};
        }

        boost::json::object dog_data;
        dog_data["pos"] = boost::json::array{dog_ptr->GetPosition().x, dog_ptr->GetPosition().y};
        dog_data["speed"] = boost::json::array{dog_ptr->GetSpeed().ux, dog_ptr->GetSpeed().uy};
        dog_data["dir"] = std::string(json_loader::DirectionToString(dog_ptr->GetDirection()));

        boost::json::array json_bag;
        for (const auto& bag_item : dog_ptr->GetBag()) {
            boost::json::object json_item;
            json_item["id"] = bag_item.id;
            json_item["type"] = bag_item.type;
            json_bag.push_back(std::move(json_item));
        }
        dog_data["bag"] = std::move(json_bag);
        dog_data["score"] = dog_ptr->GetScore();

        players_json.emplace(std::to_string(player->GetId()), dog_data);
    }

    boost::json::object lost_objects_json;
    const auto& game = app_.GetGame();
    const auto& lost_objects = game.GetLostObjects(current_map_id);

    for (const auto& [obj_id, obj] : lost_objects) {
        boost::json::object obj_data;
        obj_data["type"] = obj.type;
        obj_data["pos"] = boost::json::array{obj.pos.x, obj.pos.y};

        lost_objects_json.emplace(std::to_string(obj_id), obj_data);
    }

    boost::json::object root_json;
    root_json["players"] = players_json;
    root_json["lostObjects"] = lost_objects_json;

    http::response<http::string_body> res(http::status::ok, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");

    if (req.method() == http::verb::get) {
        res.body() = boost::json::serialize(root_json);
    }

    res.prepare_payload();
    return res;
}

// POST /api/v1/game/player/action
http::response<http::string_body> ApiHandler::HandlePlayerAction(const http::request<http::string_body>& req) const {
    unsigned int version = req.version();

    if (req.method() != http::verb::post) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", version);
        res.set(http::field::allow, "POST");
        return res;
    }

    auto token = TryExtractToken(req);
    if (!token) {
        return MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", version);
    }

    std::string move_dir;
    try {
        auto json_doc = boost::json::parse(req.body());
        const auto& json_obj = json_doc.as_object();
        move_dir = boost::json::value_to<std::string>(json_obj.at("move"));
    } catch (const std::exception& e) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", version);
    }

    auto success = app_.MovePlayer(*token, move_dir);
    if (!success) {
        return MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", version);
    }

    http::response<http::string_body> res(http::status::ok, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");
    res.body() = "{}";
    res.prepare_payload();
    return res;
}

// POST /api/v1/game/tick
http::response<http::string_body> ApiHandler::HandleTick(const http::request<http::string_body>& req) const {
    unsigned int version = req.version();

    if (req.method() != http::verb::post) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", version);
        res.set(http::field::allow, "POST");
        return res;
    }

    int64_t delta_time_ms = 0;
    try {
        auto json_doc = boost::json::parse(req.body());
        const auto& json_obj = json_doc.as_object();

        delta_time_ms = json_obj.at("timeDelta").as_int64();
    } catch (const std::exception& e) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick delta", version);
    }

    // Если в приложении не включен ручной режим тестирования времени
    if (!app_.IsTickModeManual()) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Tick invocation not allowed in auto mode", version);
    }

    app_.Tick(std::chrono::milliseconds(delta_time_ms));

    http::response<http::string_body> res(http::status::ok, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");
    res.body() = "{}";
    res.prepare_payload();

    return res;
}

// GET /api/v1/game/records
http::response<http::string_body> ApiHandler::HandleGetRecords(const http::request<http::string_body>& req) const {
    unsigned int version = req.version();

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", version);
        res.set(http::field::allow, "GET, HEAD");
        return res;
    }

    size_t start = 0;
    size_t max_items = 100;

    // Парсинг query-параметров из URI (если они переданы)
    std::string_view target = req.target();
    size_t query_pos = target.find('?');

    if (query_pos != std::string_view::npos) {
        std::string_view query = target.substr(query_pos + 1);

        // Простейший разбор параметров start и maxItems
        size_t start_pos = query.find("start=");
        if (start_pos != std::string_view::npos) {
            start = std::stoull(std::string(query.substr(start_pos + 6, query.find('&', start_pos) - (start_pos + 6))));
        }

        size_t max_pos = query.find("maxItems=");
        if (max_pos != std::string_view::npos) {
            max_items = std::stoull(std::string(query.substr(max_pos + 9, query.find('&', max_pos) - (max_pos + 9))));
        }
    }

    if (max_items > 100) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "maxItems cannot exceed 100", version);
    }

    auto records = app_.GetRecords(start, max_items);
    boost::json::array records_array;

    for (const auto& rec : records) {
        boost::json::object rec_obj;
        rec_obj["name"] = rec.name;
        rec_obj["score"] = rec.score;
        rec_obj["playTime"] = rec.play_time;
        records_array.push_back(std::move(rec_obj));
    }

    http::response<http::string_body> res(http::status::ok, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");

    if (req.method() == http::verb::get) {
        res.body() = boost::json::serialize(records_array);
    }

    res.prepare_payload();
    return res;
}

// Главная точка входа для маршрутизации API-запросов (Связующий метод для http_server)
http::response<http::string_body> ApiHandler::HandleRequest(const http::request<http::string_body>& req) const {
    std::string_view target = req.target();

    if (target == "/api/v1/game/join") {
        return HandleJoinGame(req);
    } else if (target == "/api/v1/game/players") {
        return HandleGetPlayers(req);
    } else if (target == "/api/v1/game/state") {
        return HandleGetGameState(req);
    } else if (target == "/api/v1/game/player/action") {
        return HandlePlayerAction(req);
    } else if (target == "/api/v1/game/tick") {
        return HandleTick(req);
    } else if (target.starts_with("/api/v1/game/records")) {
        return HandleGetRecords(req);
    }

    return MakeErrorResponse(http::status::bad_request, "badRequest", "Invalid endpoint", req.version());
}

} // namespace http_handler
