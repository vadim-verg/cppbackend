#pragma once

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string_view>
#include <optional>
#include "model.h"
#include "players.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

class ApiHandler {
public:

    explicit ApiHandler(app::Application& app)
        : app_(app) {}

    http::response<http::string_body> HandleGetGameState(const http::request<http::string_body>& req) const;

    // Главная точка входа для всех запросов игрового API (/api/v1/game/...)
    http::response<http::string_body> HandleRequest(const http::request<http::string_body>& req) const;

private:
    app::Application& app_;

    // Универсальный метод генерации ответов с ошибками
    http::response<http::string_body> MakeErrorResponse(
        http::status status,
        const std::string& code,
        const std::string& message,
        unsigned int version) const;

    // Обработчик входа в игру (POST /api/v1/game/join)
    http::response<http::string_body> HandleJoinGame(const http::request<http::string_body>& req) const;

    // Обработчик списка игроков (GET /api/v1/game/players)
    http::response<http::string_body> HandleGetPlayers(const http::request<http::string_body>& req) const;

    // Метод парсинга токена из заголовка Authorization
    std::optional<std::string> TryExtractToken(const http::request<http::string_body>& req) const;
};

} // namespace http_handler
