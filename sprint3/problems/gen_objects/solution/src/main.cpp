#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <memory>
#include <optional>

#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "logger.h"
#include "ticker.h"

using namespace std::literals;

namespace {

// Структура для хранения разобранных аргументов командной строки
struct Args {
    std::optional<uint64_t> tick_period;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
};

// Парсинг параметров командной строки
std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    Args args;

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<uint64_t>(), "set tick period")
        ("config-file,c", po::value<std::string>(&args.config_file), "set config file path")
        ("www-root,w", po::value<std::string>(&args.www_root), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "spawn dogs at random positions");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const std::exception& e) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        Args help_args;
        help_args.config_file = "HELP";
        return help_args;
    }

    if (!vm.count("config-file") || !vm.count("www-root")) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    if (vm.count("tick-period")) {
        args.tick_period = vm["tick-period"].as<uint64_t>();
    }

    return args;
}

// Запустить сервер на n потоках
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::thread> v;
    v.reserve(n - 1);
    while (n-- > 1) {
        v.emplace_back(fn);
    }
    fn();
    for (auto& t : v) {
        t.join();
    }
}

}  // namespace

int main(int argc, const char* argv[]) {
    // Парсим аргументы командной строки
    auto args_opt = ParseCommandLine(argc, argv);

    if (!args_opt) {
        return EXIT_FAILURE;
    }

    if (args_opt->config_file == "HELP") {
        return EXIT_SUCCESS;
    }

    // Обязательное сообщение для тестов практикума
    std::cout << "Server started" << std::endl << std::flush;

    // Инициализируем логгер
    logger::InitLogger();

    std::cout << "{\"message\": \"server started\", \"data\": {\"port\": 8080, \"address\": \"0.0.0.0\"}}" << std::endl;

    // --- [ОБНОВЛЕНО] Работаем со структурой ParsedGameData вместо чистой модели Game ---
    std::optional<json_loader::ParsedGameData> parsed_data_opt;

    try {
        if (std::filesystem::exists(args_opt->config_file)) {
            parsed_data_opt = json_loader::LoadGame(args_opt->config_file);
        } else {
            // Фолбэк-заглушка на случай отсутствия файла
            json_loader::ParsedGameData fallback;
            parsed_data_opt = std::move(fallback);
        }
    } catch (const std::exception& ex) {
        json_loader::ParsedGameData fallback;
        parsed_data_opt = std::move(fallback);
    }

    // Извлекаем ссылки на игру и провайдер фронтенд-данных о предметах
    model::Game& game = parsed_data_opt->game;
    const app::LootInfoProvider& loot_info = parsed_data_opt->loot_info;

    try {
        // Инициализируем прикладной слой
        app::Application app(game);
        app.SetSpawnRandomize(args_opt->randomize_spawn_points);
        if (args_opt->tick_period.has_value()) {
            app.SetAutomaticTicking(true);
        }

        // --- [ДОБАВЛЕНО] Регистрируем связь слоев: модель теперь знает, как считать собак через App ---
        game.SetDogCountCallback([&app](const model::Map::Id& map_id) {
            return app.GetDogCountOnMap(*map_id);
        });

        // Настраиваем асинхронный контекст Boost.Asio
        const unsigned num_threads = std::thread::hardware_concurrency();
        boost::asio::io_context ioc(num_threads);

        // Strand для последовательного выполнения запросов к API и тиков времени
        auto api_strand = std::make_shared<boost::asio::strand<boost::asio::io_context::executor_type>>(
            boost::asio::make_strand(ioc)
            );

        // Если передан параметр --tick-period, запускаем автоматический таймер тиков
        if (args_opt->tick_period.has_value()) {
            // [ОБНОВЛЕНО] Оставляем только один вызов app.Tick
            // Логика генерации предметов game_.Tick() уже инкапсулирована внутри метода Application::Tick
            app::Ticker::Handler tick_handler = [&app](std::chrono::milliseconds delta) {
                app.Tick(static_cast<double>(delta.count()) / 1000.0);
            };

            auto ticker = std::make_shared<app::Ticker>(
                api_strand,
                std::chrono::milliseconds(*args_opt->tick_period),
                std::move(tick_handler)
                );
            ticker->Start();
        }

        // --- [ОБНОВЛЕНО] Передаем loot_info в RequestHandler четвертым аргументом ---
        auto handler = std::make_shared<http_handler::RequestHandler>(
            game, std::filesystem::path(args_opt->www_root), app, loot_info
            );

        // Настраиваем перехват сигналов SIGINT и SIGTERM для остановки
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, int signal_number) {
            if (!ec) {
                std::cout << "Server shutting down via signal " << signal_number << std::endl;
                ioc.stop();
            }
        });

        // Запускаем сервер на прослушивание порта 8080
        const auto address = boost::asio::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;

        http_server::ServeHttp(ioc, {address, port}, [handler, api_strand](std::string /*client_ip*/, auto&& req, auto&& send) {
            auto alloc_req = std::make_shared<std::decay_t<decltype(req)>>(std::forward<decltype(req)>(req));
            auto alloc_send = std::make_shared<std::decay_t<decltype(send)>>(std::forward<decltype(send)>(send));

            // Помещаем сетевой запрос в очередь strand
            boost::asio::post(*api_strand, [handler, alloc_req, alloc_send]() {
                (*handler)(std::move(*alloc_req), [alloc_send](auto&& response) {
                    (*alloc_send)(std::forward<decltype(response)>(response));
                });
            });
        });

        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

    } catch (const std::exception& ex) {
        std::cerr << "Server crash error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
