#include "api_handler.h"

namespace http_handler {

// Вспомогательный метод для генерации JSON-ответов с ошибками
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

// Метод для извлечения токена из заголовка Authorization
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
    } catch (...) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", version);
    }

    if (user_name.empty()) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid name", version);
    }

    // Делегируем вход прикладному слою приложения
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

    // Запрашиваем данные у чистого прикладного слоя
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

    // Если метод HEAD, Boost.Beast сам сбросит тело, но мы экономим сериализацию строки
    if (req.method() == http::verb::get) {
        res.body() = boost::json::serialize(players_json);
    }

    res.prepare_payload();
    return res;
}

// GET /api/v1/game/state
http::response<http::string_body> ApiHandler::HandleGetGameState(const http::request<http::string_body>& req) const {
    unsigned int version = req.version();

    // 1. Проверка метода: разрешены только GET и HEAD (иначе 405)
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", version);
        res.set(http::field::allow, "GET, HEAD");
        return res;
    }

    // 2. Проверка токена: должен быть передан в формате Bearer (иначе 401)
    auto token = TryExtractToken(req);
    if (!token) {
        return MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", version);
    }

    // 3. Поиск сессии: токен должен существовать в базе приложения (иначе 401)
    auto players_opt = app_.GetPlayersInSession(*token);
    if (!players_opt) {
        return MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", version);
    }

    // 4. Успех: Формируем JSON-объект состояния всех игроков текущей сессии
    boost::json::object players_json;
    for (const auto& player : *players_opt) {
        auto dog_ptr = player->GetDog(); // Получаем связанного пса из модели

        boost::json::object dog_data;
        // По ТЗ pos — массив из двух вещественных чисел
        dog_data["pos"] = boost::json::array{dog_ptr->GetPosition().x, dog_ptr->GetPosition().y};
        // По ТЗ speed — массив из двух вещественных чисел (пока 0.0)
        dog_data["speed"] = boost::json::array{dog_ptr->GetSpeed().ux, dog_ptr->GetSpeed().uy};
        // По ТЗ dir — строка направления по умолчанию "U" (вверх/север)
        dog_data["dir"] = std::string(json_loader::DirectionToString(dog_ptr->GetDirection()));

        // Идентификатор игрока преобразуем в строку как ключ объекта
        players_json[std::to_string(player->GetId())] = dog_data;
    }

    boost::json::object root_json;
    root_json["players"] = players_json;

    http::response<http::string_body> res(http::status::ok, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");

    // Если метод HEAD, Boost.Beast сам сбросит тело, но мы экономим сериализацию строки
    if (req.method() == http::verb::get) {
        res.body() = boost::json::serialize(root_json);
    }

    res.prepare_payload();
    return res;
}

// POST /api/v1/game/player/action
http::response<http::string_body> ApiHandler::HandlePlayerAction(const http::request<http::string_body>& req) const {
    unsigned int version = req.version();

    // 1. Проверка метода (только POST)
    if (req.method() != http::verb::post) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", version);
        res.set(http::field::allow, "POST");
        return res;
    }

    // 2. Проверка заголовка Content-Type
    auto ct_it = req.find(http::field::content_type);
    if (ct_it == req.end() || ct_it->value() != "application/json") {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid Content-Type", version);
    }

    // 3. Валидация токена (401)
    auto token = TryExtractToken(req);
    if (!token) {
        return MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", version);
    }

    // 4. Парсинг тела JSON
    std::string move_cmd;
    try {
        auto json_doc = boost::json::parse(req.body());
        const auto& json_obj = json_doc.as_object();
        move_cmd = boost::json::value_to<std::string>(json_obj.at("move"));
    } catch (...) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", version);
    }

    // Валидация допустимых значений move
    if (move_cmd != "L" && move_cmd != "R" && move_cmd != "U" && move_cmd != "D" && !move_cmd.empty()) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid move command", version);
    }

    // 5. Применяем действие в прикладном слое
    if (!app_.MovePlayer(*token, move_cmd)) {
        return MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", version);
    }

    // 6. Успех -> Возвращаем пустой JSON-объект {}
    http::response<http::string_body> res(http::status::ok, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");
    res.body() = "{}";
    res.prepare_payload();

    return res;
}

// Маршрутизатор игрового API
http::response<http::string_body> ApiHandler::HandleRequest(const http::request<http::string_body>& req) const {

    std::string_view target{req.target().data(), req.target().size()};

    if (target == "/api/v1/game/join") {
        return HandleJoinGame(req);
    }

    if (target == "/api/v1/game/players") {
        return HandleGetPlayers(req);
    }

    if (target == "/api/v1/game/state") {
        return HandleGetGameState(req);
    }

    if (target == "/api/v1/game/player/action") {
        return HandlePlayerAction(req);
    }

    return MakeErrorResponse(http::status::bad_request, "badRequest", "Invalid API endpoint", req.version());
}

} // namespace http_handler
