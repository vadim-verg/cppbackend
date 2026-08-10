#include "api_handler.h"
#include <charconv>

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

    std::string body_str = boost::json::serialize(obj);

    // === ВРЕМЕННЫЙ ОТЛАДОЧНЫЙ ВЫВОД ДЛЯ ТЕСТОВ ===
    std::cout << "[API ERROR LOG] Code: " << code << ", Message: " << message << std::endl << std::flush;
    // =============================================

    res.body() = body_str;
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

    auto join_result = app_.JoinGame(user_name, map_id);
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

    // Проверка метода - только GET и HEAD
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", version);
        res.set(http::field::allow, "GET, HEAD");
        return res;
    }

    // Проверка токена - в формате Bearer
    auto token = TryExtractToken(req);
    if (!token) {
        return MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", version);
    }

    // Поиск сессии - токен должен существовать в базе приложения
    auto players_opt = app_.GetPlayersInSession(*token);
    if (!players_opt) {
        return MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", version);
    }

    // Строим JSON-объект состояния всех игроков текущей сессии
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

    auto ct_it = req.find(http::field::content_type);
    if (ct_it == req.end() || ct_it->value() != "application/json") {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid Content-Type", version);
    }

    // Валидация токена
    auto token = TryExtractToken(req);
    if (!token) {
        return MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", version);
    }

    // Парсинг тела JSON
    std::string move_cmd;
    try {
        auto json_doc = boost::json::parse(req.body());
        const auto& json_obj = json_doc.as_object();
        if (json_obj.contains("move")) {
            move_cmd = boost::json::value_to<std::string>(json_obj.at("move"));
        }
    } catch (const std::exception& e) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action JSON", version);
    }

    // Передаем команду в Application слой
    if (!app_.MovePlayer(*token, move_cmd)) {
        return MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", version);
    }

    // Успешный ответ
    http::response<http::string_body> res(http::status::ok, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");
    res.body() = "{}";
    res.prepare_payload();

    return res;
}

// POST /api/v1/game/tick
http::response<http::string_body> ApiHandler::HandleGameTick(const http::request<http::string_body>& req) const {
    unsigned int version = req.version();

    if (app_.IsAutomaticTicking()) {
        return MakeErrorResponse(http::status::bad_request, "badRequest", "Invalid endpoint", version);
    }

    if (req.method() != http::verb::post) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", version);
        res.set(http::field::allow, "POST");
        return res;
    }

    auto ct_it = req.find(http::field::content_type);
    if (ct_it == req.end() || ct_it->value() != "application/json") {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid Content-Type", version);
    }

    // Парсим дельту времени из JSON
    int64_t delta_ms = 0;
    try {
        auto json_doc = boost::json::parse(req.body());
        const auto& json_obj = json_doc.as_object();
        delta_ms = json_obj.at("timeDelta").as_int64();
    } catch (...) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", version);
    }

    // шаг симуляции
    app_.Tick(static_cast<double>(delta_ms) / 1000.0);

    // Если сработал таймер автоматического сохранения — записываем на диск
    if (state_manager_ && app_.PopShouldSaveState()) {
        state_manager_->SaveState(app_);
    }

    // Успешный ответ
    http::response<http::string_body> res(http::status::ok, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");
    res.body() = "{}";
    res.prepare_payload();

    return res;
}

http::response<http::string_body> ApiHandler::HandleRequest(const http::request<http::string_body>& req) const {
    auto target_boost = req.target();
    std::string_view target{target_boost.data(), target_boost.size()};
    unsigned int version = req.version();

    if (target == "/api/v1/game/join") {
        return HandleJoinGame(req);
    }
    else if (target == "/api/v1/game/players") {
        return HandleGetPlayers(req);
    }
    else if (target == "/api/v1/game/state") {
        return HandleGetGameState(req);
    }
    else if (target == "/api/v1/game/player/action") {
        return HandlePlayerAction(req);
    }
    else if (target == "/api/v1/game/tick") {
        return HandleGameTick(req);
    }

    return MakeErrorResponse(http::status::bad_request, "badRequest", "Invalid endpoint", version);
}

// Вспомогательная функция для парсинга query-параметров из URL
std::optional<std::string_view> FindQueryParam(std::string_view target, std::string_view key) {
    auto query_pos = target.find('?');
    if (query_pos == std::string_view::npos) {
        return std::nullopt;
    }

    std::string_view query = target.substr(query_pos + 1);
    size_t pos = 0;

    while (pos < query.size()) {
        size_t next_amp = query.find('&', pos);
        std::string_view pair = (next_amp == std::string_view::npos)
                                    ? query.substr(pos)
                                    : query.substr(pos, next_amp - pos);

        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string_view::npos) {
            std::string_view k = pair.substr(0, eq_pos);
            std::string_view v = pair.substr(eq_pos + 1);
            if (k == key) {
                return v;
            }
        }

        if (next_amp == std::string_view::npos) {
            break;
        }
        pos = next_amp + 1;
    }
    return std::nullopt;
}
/*
// GET /api/v1/game/records
http::response<http::string_body> ApiHandler::HandleGetRecords(const http::request<http::string_body>& req) const {

    unsigned int version = req.version();
    // Если БД отключена — возвращаем пустой список
    if (!db_) {
        http::response<http::string_body> res(http::status::ok, version);
        res.set(http::field::content_type, "application/json");
        res.set(http::field::cache_control, "no-cache");
        if (req.method() == http::verb::get) {
            res.body() = "[]";
        }
        res.prepare_payload();
        return res;
    }

    // Проверяем метод HTTP (ТЗ разрешает только GET и HEAD)
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", version);
        res.set(http::field::allow, "GET, HEAD");
        return res;
    }

    std::string_view target{req.target().data(), req.target().size()};

    // Параметры по умолчанию из ТЗ
    int start = 0;
    int max_items = 100;

    // Парсим параметр start
    if (auto start_opt = FindQueryParam(target, "start")) {
        auto val = *start_opt;
        auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), start);
        if (ec != std::errc{} || start < 0) {
            return MakeErrorResponse(http::status::bad_request, "badRequest", "Invalid start parameter", version);
        }
    }

    // Парсим параметр maxItems
    if (auto max_items_opt = FindQueryParam(target, "maxItems")) {
        auto val = *max_items_opt;
        auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), max_items);
        if (ec != std::errc{} || max_items < 0) {
            return MakeErrorResponse(http::status::bad_request, "badRequest", "Invalid maxItems parameter", version);
        }
        // Защита из ТЗ: если больше 100 — возвращаем 400 Bad Request
        if (max_items > 100) {
            return MakeErrorResponse(http::status::bad_request, "badRequest", "maxItems cannot exceed 100", version);
        }
    }

    // Получаем записи из базы данных через наш пул
    std::vector<database::PlayerRecord> records;
    try {
        records = db_->GetRecords(start, max_items);
    } catch (const std::exception& ex) {
        return MakeErrorResponse(http::status::internal_server_error, "internalError", "Database error occurred", version);
    }

    // Собираем JSON-массив из рекордов
    boost::json::array json_records;
    for (const auto& rec : records) {
        boost::json::object obj;
        obj["name"] = rec.name;
        obj["score"] = rec.score;
        obj["playTime"] = rec.play_time;
        json_records.push_back(std::move(obj));
    }

    // Формируем HTTP-ответ
    http::response<http::string_body> res(http::status::ok, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");

    // Если метод GET — пишем тело, если HEAD — тело остается пустым
    if (req.method() == http::verb::get) {
        res.body() = boost::json::serialize(json_records);
    }

    res.prepare_payload();
    return res;
}
*/
} // namespace http_handler
