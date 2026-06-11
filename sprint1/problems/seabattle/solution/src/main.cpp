#ifdef WIN32
#include <sdkddkver.h>
#endif

#include "seabattle.h"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

void PrintFieldPair(const SeabattleField& left, const SeabattleField& right) {
    auto left_pad = "  "s;
    auto delimeter = "    "s;
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
    for (size_t i = 0; i < SeabattleField::field_size; ++i) {
        std::cout << left_pad;
        left.PrintLine(std::cout, i);
        std::cout << delimeter;
        right.PrintLine(std::cout, i);
        std::cout << std::endl;
    }
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
}

template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket) {
    boost::array<char, sz> buf;
    boost::system::error_code ec;

    net::read(socket, net::buffer(buf), net::transfer_exactly(sz), ec);

    if (ec) {
        return std::nullopt;
    }

    return {{buf.data(), sz}};
}

static bool WriteExact(tcp::socket& socket, std::string_view data) {
    boost::system::error_code ec;

    net::write(socket, net::buffer(data), net::transfer_exactly(data.size()), ec);

    return !ec;
}

class SeabattleAgent {
public:
    SeabattleAgent(const SeabattleField& field)
        : my_field_(field) {
    }

    void StartGame(tcp::socket& socket, bool my_initiative) {
        while (!IsGameEnded()) {
            PrintFields();

            if (my_initiative) {
                std::cout << "Your turn: ";
                std::string move_str;
                std::cin >> move_str;

                auto move_opt = ParseMove(move_str);
                if (!move_opt) {
                    std::cout << "Invalid move format! Try again (e.g. A1)." << std::endl;
                    continue;
                }

                // Отправляем свой выстрел противнику (2 байта)
                if (!WriteExact(socket, move_str)) {
                    std::cout << "Connection lost while sending move." << std::endl;
                    return;
                }

                // Читаем ответ противника (1 байт)
                auto response_opt = ReadExact<1>(socket);
                if (!response_opt) {
                    std::cout << "Connection lost while waiting for response." << std::endl;
                    return;
                }

                char res_char = (*response_opt)[0];
                int x = move_opt->second;
                int y = move_opt->first;

                if (res_char == '0') {
                    std::cout << "Miss!" << std::endl;
                    other_field_.MarkMiss(x, y);
                    my_initiative = false; // Передача хода
                } else if (res_char == '1') {
                    std::cout << "Hit! Extra turn." << std::endl;
                    other_field_.MarkHit(x, y);
                } else if (res_char == '2') {
                    std::cout << "Killed! Extra turn." << std::endl;
                    other_field_.MarkKill(x, y);
                }
            } else {
                std::cout << "Waiting for enemy move..." << std::endl;
                
                // Читаем ход противника (2 байта)
                auto enemy_move_opt = ReadExact<2>(socket);
                if (!enemy_move_opt) {
                    std::cout << "Connection lost while waiting for enemy move." << std::endl;
                    return;
                }

                auto move_opt = ParseMove(*enemy_move_opt);
                if (!move_opt) {
                    std::cout << "Received invalid move from enemy." << std::endl;
                    return;
                }

                int x = move_opt->second;
                int y = move_opt->first;

                // Выполняем выстрел по своему полю
                auto shot_result = my_field_.Shoot(x, y);
                char send_char = '0';

                if (shot_result == SeabattleField::ShotResult::MISS) {
                    std::cout << "Enemy missed at " << *enemy_move_opt << std::endl;
                    my_field_.MarkMiss(x, y);
                    send_char = '0';
                    my_initiative = true; // Ход переходит к нам
                } else if (shot_result == SeabattleField::ShotResult::HIT) {
                    std::cout << "Enemy hit your ship at " << *enemy_move_opt << std::endl;
                    send_char = '1';
                } else if (shot_result == SeabattleField::ShotResult::KILL) {
                    std::cout << "Enemy killed your ship at " << *enemy_move_opt << std::endl;
                    my_field_.MarkKill(x, y);
                    send_char = '2';
                }

                // Отправляем результат выстрела (1 байт)
                std::string resp(1, send_char);
                if (!WriteExact(socket, resp)) {
                    std::cout << "Connection lost while sending shot result." << std::endl;
                    return;
                }
            }
        }

        // Выводим финальное состояние полей и объявляем результат
        PrintFields();
        if (my_field_.IsLoser()) {
            std::cout << "You lost! Better luck next time. 😢" << std::endl;
        } else {
            std::cout << "Victory! You destroyed the enemy fleet! 🎉" << std::endl;
        }
    }
private:
    static std::optional<std::pair<int, int>> ParseMove(const std::string_view& sv) {
        if (sv.size() != 2) return std::nullopt;

        int p1 = sv[0] - 'A', p2 = sv[1] - '1';

        if (p1 < 0 || p1 > 8) return std::nullopt;
        if (p2 < 0 || p2 > 8) return std::nullopt;

        return {{p1, p2}};
    }

    static std::string MoveToString(std::pair<int, int> move) {
        char buff[] = {static_cast<char>(move.first) + 'A', static_cast<char>(move.second) + '1'};
        return {buff, 2};
    }

    void PrintFields() const {
        PrintFieldPair(my_field_, other_field_);
    }

    bool IsGameEnded() const {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

    // TODO: добавьте методы по вашему желанию

private:
    SeabattleField my_field_;
    SeabattleField other_field_;
};

void StartServer(const SeabattleField& field, unsigned short port) {
    try {
        net::io_context io_context;

        // Создаем acceptor для прослушивания входящих соединений
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
        std::cout << "Server started. Waiting for enemy connection on port " << port << "..." << std::endl;

        tcp::socket socket(io_context);
        acceptor.accept(socket);
        std::cout << "Enemy connected! Match begins." << std::endl;

        SeabattleAgent agent(field);
        agent.StartGame(socket, false); // Сервер ходит вторым (инициатива = false)
    }
    catch (std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
};

void StartClient(const SeabattleField& field, const std::string& ip_str, unsigned short port) {
    try {
        net::io_context io_context;

        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        
        std::cout << "Connecting to server " << ip_str << ":" << port << "..." << std::endl;
        net::connect(socket, resolver.resolve(ip_str, std::to_string(port)));
        std::cout << "Connected to enemy! Match begins." << std::endl;

        SeabattleAgent agent(field);
        agent.StartGame(socket, true); // Клиент ходит первым (инициатива = true)
    }
    catch (std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
};

int main(int argc, const char** argv) {
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: program <seed> [<ip>] <port>" << std::endl;
        return 1;
    }

    std::mt19937 engine(std::stoi(argv[1]));
    SeabattleField fieldL = SeabattleField::GetRandomField(engine);

    if (argc == 3) {
        StartServer(fieldL, std::stoi(argv[2]));
    } else if (argc == 4) {
        StartClient(fieldL, argv[2], std::stoi(argv[3]));
    }
}
