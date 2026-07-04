#include "sdk.h"
#include "http_server.h"
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include "logger.h"
#include "json_loader.h"
#include "request_handler.h"
#include "players.h"

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

    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-dir>"sv << std::endl;
        return EXIT_FAILURE;
    }

    try {
        logger::InitLogger();

        std::filesystem::path config_path = argv[1];
        std::filesystem::path static_path = argv[2];

        if (!std::filesystem::is_directory(static_path)) {
            std::cerr << "Static directory not found: " << static_path << std::endl;
            return EXIT_FAILURE;
        }

        model::Game game = json_loader::LoadGame(config_path);

        app::Application app{game};

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;

        http_handler::RequestHandler handler{game, std::move(static_path), app};
        http_handler::LoggingRequestHandler logging_handler{handler};

        http_server::ServeHttp(ioc, {address, port}, [&logging_handler](std::string ip, auto&& req, auto&& send) {
            logging_handler(ip, std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        logger::LogServerStarted(port, address.to_string());

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        logger::LogServerStopped(0);

    } catch (const std::exception& ex) {
        logger::LogServerStopped(EXIT_FAILURE, &ex);
        return EXIT_FAILURE;
    }
}
