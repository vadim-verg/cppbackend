#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>
#include <iostream>
#include <thread>
#include <memory>
#include <optional>
#include <filesystem>

#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "logger.h"
#include "ticker.h"
#include "state_manager.h"
#include "players.h"

#include "postgres.h"
#include <cstdlib>

using namespace std::literals;

namespace {

// Структура для хранения разобранных аргументов командной строки
struct Args {
    std::optional<uint64_t> tick_period;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
    std::optional<std::string> state_file;
    std::optional<uint32_t> save_state_period;
};

// Парсинг параметров командной строки
std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    Args args;

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<uint64_t>(), "set tick period")
        // Добавляем default_value для config-file:
        ("config-file,c", po::value<std::string>(&args.config_file)->default_value("config.json"), "set config file path")
        // Добавляем default_value для www-root:
        ("www-root,w", po::value<std::string>(&args.www_root)->default_value("static"), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "spawn dogs at random positions")
        ("state-file", po::value<std::string>(), "file to save/restore game state")
        ("save-state-period", po::value<uint32_t>(), "auto-save period in milliseconds");

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
/*
    if (!vm.count("config-file") || !vm.count("www-root")) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }
*/
    if (vm.count("tick-period")) {
        args.tick_period = vm["tick-period"].as<uint64_t>();
    }

    if (vm.count("state-file")) {
        args.state_file = vm["state-file"].as<std::string>();
    }

    if (vm.count("save-state-period")) {
        args.save_state_period = vm["save-state-period"].as<uint32_t>();
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

    // Читаем URL базы данных из переменных окружения (Требование ТЗ)
    // Читаем URL базы данных из переменных окружения
    const char* db_url_env = std::getenv("GAME_DB_URL");
    std::string db_url = db_url_env ? std::string(db_url_env) : ""s;

    // Обязательное сообщение для тестов практикума
    std::cout << "Server started" << std::endl << std::flush;

    // Инициализируем логгер
    logger::InitLogger();

    std::cout << "{\"message\": \"server started\", \"data\": {\"port\": 8080, \"address\": \"0.0.0.0\"}}" << std::endl;

    // Инициализируем базу данных (задаем фиксированный размер пула 10 для стабильности)
    std::shared_ptr<database::Database> db = nullptr;
    if (!db_url.empty()) {
        try {
            db = std::make_shared<database::Database>(db_url, 2);
        } catch (const std::exception& ex) {
            std::cerr << "Database пул ошибка создания: " << ex.what() << std::endl;
        }
    }
    // Считаем количество потоков заранее, чтобы передать в пул соединений БД
//    const unsigned num_threads = std::thread::hardware_concurrency();

    std::optional<json_loader::ParsedGameData> parsed_data_opt;

    try {
        if (std::filesystem::exists(args_opt->config_file)) {
            parsed_data_opt = json_loader::LoadGame(args_opt->config_file);
        } else {
            json_loader::ParsedGameData fallback;
            parsed_data_opt = std::move(fallback);
        }
    } catch (const std::exception& ex) {
        json_loader::ParsedGameData fallback;
        parsed_data_opt = std::move(fallback);
    }

    // Извлекаем ссылки на игру и провайдер данных о предметах
    model::Game& game = parsed_data_opt->game;
    const app::LootInfoProvider& loot_info = parsed_data_opt->loot_info;

    try {
        // Инициализируем прикладной слой
        app::Application app(game);

        // Подписываем базу данных на событие ухода собаки на покой
        if (db) {
            app.dog_retired_signal.connect([db](const std::string& name, int score, double play_time) {
                try {
                    // Гарантируем структуру перед вставкой
                    db->InitializeStructure();

                    auto conn_ptr = db->GetPool().GetConnection();
                    pqxx::work tx(*conn_ptr);
                    tx.exec_params(
                        "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3);",
                        name, score, play_time
                        );
                    tx.commit();
                } catch (const std::exception& ex) {
                    std::cerr << "Failed to save retired dog record: " << ex.what() << std::endl;
                }
            });
        }

        app.SetSpawnRandomize(args_opt->randomize_spawn_points);
        if (args_opt->tick_period.has_value()) {
            app.SetAutomaticTicking(true);
        }

        game.SetDogCountCallback([&app](const model::Map::Id& map_id) {
            return app.GetDogCountOnMap(*map_id);
        });

        // Настройка стейт-менеджера и восстановление состояния
        std::shared_ptr<StateManager> state_manager = nullptr;
        if (args_opt->state_file.has_value()) {
            state_manager = std::make_shared<StateManager>(args_opt->state_file.value());

            // Если в файле битые данные — логируем ошибку и выходим
            if (!state_manager->LoadState(app, game)) {
                BOOST_LOG_TRIVIAL(error) << "Error: Failed to restore state from file: " << args_opt->state_file.value();
                return EXIT_FAILURE;
            }

            // Настраиваем параметры времени внутри Application без циклической зависимости
            app.SetupSaveParameters(args_opt->state_file, args_opt->save_state_period);
        }

        // Настраиваем асинхронный контекст Boost.Asio
        const unsigned num_threads = std::thread::hardware_concurrency();
        boost::asio::io_context ioc(num_threads);

        // Strand для последовательного выполнения запросов к API и тиков времени
        auto api_strand = std::make_shared<boost::asio::strand<boost::asio::io_context::executor_type>>(
            boost::asio::make_strand(ioc)
            );

        // Если передан параметр --tick-period, запускаем автоматический таймер тиков
        if (args_opt->tick_period.has_value()) {
            app::Ticker::Handler tick_handler = [&app, state_manager](std::chrono::milliseconds delta) {
                app.Tick(static_cast<double>(delta.count()) / 1000.0);

                // Проверяем, не наступило ли время сделать автосейв после очередного тика
                if (state_manager && app.PopShouldSaveState()) {
                    state_manager->SaveState(app);
                }
            };

            auto ticker = std::make_shared<app::Ticker>(
                api_strand,
                std::chrono::milliseconds(*args_opt->tick_period),
                std::move(tick_handler)
                );
            ticker->Start();
        }

        auto handler = std::make_shared<http_handler::RequestHandler>(
            game, std::filesystem::path(args_opt->www_root), app, loot_info, state_manager, db
            );

        // Настраиваем перехват сигналов SIGINT и SIGTERM для сохранения при выходе
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, state_manager, &app, &args_opt](const boost::system::error_code& ec, int signal_number) {
            if (!ec) {
                std::cout << "Server shutting down via signal " << signal_number << std::endl;

                // Сохраняем состояние игры на диск перед остановкой контекста
                if (state_manager && args_opt->state_file.has_value()) {
                    state_manager->SaveState(app);
                }

                ioc.stop();
            }
        });

        // Запускаем сервер на прослушивание порта 8080
        const auto address = boost::asio::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;

        http_server::ServeHttp(ioc, {address, port}, [handler, api_strand](std::string /*client_ip*/, auto&& req, auto&& send) {
            auto alloc_req = std::make_shared<std::decay_t<decltype(req)>>(std::forward<decltype(req)>(req));
            auto alloc_send = std::make_shared<std::decay_t<decltype(send)>>(std::forward<decltype(send)>(send));

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
