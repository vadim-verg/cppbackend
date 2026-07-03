#include "sdk.h"
#include "http_server.h"
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include <filesystem> // Добавлен заголовок для работы с путями

#include "json_loader.h"
#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cout.setf(std::ios::unitbuf);

    // Изменено требование: теперь ожидаем 3 аргумента (имя программы + 2 параметра)
    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-dir>"sv << std::endl;
        return EXIT_FAILURE;
    }

    try {
        // Конвертируем аргументы в пути файловой системы
        std::filesystem::path config_path = argv[1];
        std::filesystem::path static_path = argv[2];

        // Проверяем существование каталога со статикой перед запуском сервера
        if (!std::filesystem::is_directory(static_path)) {
            std::cerr << "Static directory not found: " << static_path << std::endl;
            return EXIT_FAILURE;
        }

        // Загружаем карту из файла
        model::Game game = json_loader::LoadGame(config_path);

        // Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // Передаем в обработчик и модель игры, и путь к каталогу статики
        http_handler::RequestHandler handler{game, std::move(static_path)};

        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;

        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        std::cout << "Server has started..."sv << std::endl;

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
