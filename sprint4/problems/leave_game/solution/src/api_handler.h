#pragma once

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string_view>
#include <optional>
#include <memory.h>
#include "model.h"
#include "players.h"
#include "json_loader.h"
#include "state_manager.h"
#include "postgres.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

class ApiHandler {
public:

    explicit ApiHandler(app::Application& app, std::shared_ptr<StateManager> state_manager)
        : app_(app)
        , state_manager_(state_manager)
    {}

 //   http::response<http::string_body> HandleGetRecords(const http::request<http::string_body>& req) const;

    http::response<http::string_body> HandleGetGameState(const http::request<http::string_body>& req) const;

    // Главная точка входа для всех запросов игрового API (/api/v1/game/...)
    http::response<http::string_body> HandleRequest(const http::request<http::string_body>& req) const;

    //POST /api/v1/game/player/action
    http::response<http::string_body> HandlePlayerAction(const http::request<http::string_body>& req) const;

    //POST /api/v1/game/tick
    http::response<http::string_body> HandleGameTick(const http::request<http::string_body>& req) const;

private:
    app::Application& app_;
    std::shared_ptr<StateManager> state_manager_;
//    std::shared_ptr<database::Database> db_;

    // Универсальный метод генерации ответов с ошибками
    http::response<http::string_body> MakeErrorResponse(
        http::status status,
        const std::string& code,
        const std::string& message,
        unsigned int version) const;

    // Вход в игру (POST /api/v1/game/join)
    http::response<http::string_body> HandleJoinGame(const http::request<http::string_body>& req) const;

    // Вывод списка игроков (GET /api/v1/game/players)
    http::response<http::string_body> HandleGetPlayers(const http::request<http::string_body>& req) const;

    // Парсинг токена из заголовка Authorization
    std::optional<std::string> TryExtractToken(const http::request<http::string_body>& req) const;
};

} // namespace http_handler
