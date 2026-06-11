#ifdef WIN32
#include <sdkddkver.h>
#endif
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <thread>
#include <optional>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;

using namespace std::string_literals;
using namespace std::string_view_literals;

// Запрос, тело которого представлено в виде строки
using StringRequest = http::request<http::string_body>;
// Ответ, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>;

// Структура ContentType задаёт область видимости для констант,
// задающих значения HTTP-заголовка Content-Type
struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    // При необходимости внутрь ContentType можно добавить и другие типы контента
};

// Создаёт StringResponse с заданными параметрами
StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                  bool keep_alive,
                                  std::string_view content_type = ContentType::TEXT_HTML) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
}

StringResponse HandleRequest(StringRequest&& req) {
    const auto text_response = [&req](http::status status, std::string_view text) {
        return MakeStringResponse(status, text, req.version(), req.keep_alive());
    };

    // 1. Проверяем метод запроса: разрешены только GET и HEAD
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto response = text_response(http::status::method_not_allowed, "Invalid method"sv);
        response.set(http::field::allow, "GET, HEAD"sv);
        return response;
    }

    // 2. Формируем строку приветствия (удаляем ведущий слэш '/' из req.target())
    std::string_view target = req.target();
    if (!target.empty() && target[0] == '/') {
        target.remove_prefix(1);
    }
    
    std::string body = "Hello, "s + std::string(target);

    // 3. Формируем ответ в зависимости от метода (GET или HEAD)
    if (req.method() == http::verb::get) {
        return text_response(http::status::ok, body);
    } else {
        // Для HEAD-запроса тело должно быть пустым, 
        // но Content-Length должен соответствовать размеру тела GET-запроса
        auto response = text_response(http::status::ok, ""sv);
        response.content_length(body.size());
        return response;
    }
}

std::optional<StringRequest> ReadRequest(tcp::socket& socket, beast::flat_buffer& buffer) {
    beast::error_code ec;
    StringRequest req;
    
    // Читаем HTTP-запрос
    http::read(socket, buffer, req, ec);

    if (ec == http::error::end_of_stream) {
        return std::nullopt;
    }
    if (ec) {
        throw std::runtime_error("Failed to read request: "s + ec.message());
    }
    return req;
}

void DumpRequest(const StringRequest& req) {
    std::cout << req.method_string() << " " << req.target() << std::endl;
    for (const auto& header : req) {
        std::cout << "  " << header.name_string() << ": " << header.value() << std::endl;
    }
}

template <typename RequestHandler>
void HandleConnection(tcp::socket& socket, RequestHandler&& handle_request) {
    try {
        // Буфер для чтения данных в рамках текущей сессии.
        beast::flat_buffer buffer;

        // Продолжаем обработку запросов, пока клиент их отправляет
        while (auto request = ReadRequest(socket, buffer)) {
            DumpRequest(*request);
            StringResponse response = handle_request(*std::move(request));
            http::write(socket, response);
            if (response.need_eof()) {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    beast::error_code ec;
    // Запрещаем дальнейшую отправку данных через сокет
    socket.shutdown(tcp::socket::shutdown_send, ec);
} 


int main() {
    try {
        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;

        net::io_context ioc;
        tcp::acceptor acceptor{ioc, {address, port}};

std::cout << "Server has started..." << std::endl;

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            
            beast::flat_buffer buffer;
            auto req = ReadRequest(socket, buffer);
            if (req) {
                DumpRequest(*req);
                
                auto response = HandleRequest(std::move(*req));
                
                beast::error_code ec;
                http::write(socket, response, ec);
                if (ec) {
                    std::cerr << "Write error: " << ec.message() << std::endl;
                }
                
                socket.shutdown(tcp::socket::shutdown_send, ec);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}