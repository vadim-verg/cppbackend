#include "sdk.h"
#include "http_server.h"
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <string>
#include <string_view>

namespace {
namespace net = boost::asio;
namespace sys = boost::system;
namespace http = boost::beast::http;

using namespace std::literals;

// Функция создания HTTP-ответа на основе входящего запроса
auto HandleRequest(http::request<http::string_body>&& req) {
    // Вспомогательная функция для генерации ответов со статус-кодом 405
    auto const method_not_allowed = [&req](std::string_view text) {
        http::response<http::string_body> res{http::status::method_not_allowed, req.version()};
        res.set(http::field::content_type, "text/html"sv);
        res.set(http::field::allow, "GET, HEAD"sv);
        res.body() = std::string(text);
        res.prepare_payload();
        return res;
    };

    // Вспомогательная функция для генерации ответов 200 OK
    auto const make_response = [&req](std::string_view target_name, bool head_only) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "text/html"sv);
        
        std::string body_text = "Hello, " + std::string(target_name);
        
        if (head_only) {
            // Для HEAD вычисляем Content-Length, но само тело оставляем пустым
            res.set(http::field::content_length, std::to_string(body_text.size()));
        } else {
            res.body() = std::move(body_text);
            res.prepare_payload(); // Автоматически выставит Content-Length
        }
        return res;
    };

    // 1. Проверяем валидность метода (разрешены только GET и HEAD)
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return method_not_allowed("Invalid method"sv);
    }

    // 2. Вычленяем имя таргета из URL (удаляем ведущий слэш '/')
    std::string_view target = req.target();
    if (!target.empty() && target[0] == '/') {
        target.remove_prefix(1);
    }

    // 3. Формируем успешный ответ
    bool is_head = (req.method() == http::verb::head);
    return make_response(target, is_head);
}
}  // namespace

int main() {
    try {
        // Контекст для асинхронных операций
        net::io_context ioc;

        // Настраиваем перехват сигналов SIGINT и SIGTERM для плавного закрытия
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, int signal_number) {
            if (!ec) {
                std::cout << "Signal " << signal_number << " received. Shutting down..." << std::endl;
                ioc.stop(); // Останавливаем io_context, прерывая цикл ioc.run()
            }
        });

        // Создаем обработчик сетевых запросов
        auto handler = [](auto&& req, auto&& send) {
            send(HandleRequest(std::forward<decltype(req)>(req)));
        };

        // Сервер слушает порт 8080 на всех сетевых интерфейсах (0.0.0.0)
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;

        // Запуск HTTP-сервера через созданный нами Listener
        http_server::ServeHttp(ioc, {address, port}, handler);

        std::cout << "Server has started on port " << port << "..." << std::endl;
        ioc.run();
        std::cout << "Server stopped gracefully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Critical error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
