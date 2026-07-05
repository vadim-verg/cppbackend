#include "players.h"
#include <iomanip>
#include <sstream>

namespace app {

std::string PlayerTokens::GenerateToken() {
    std::stringstream ss;
    // Генерируем два случайных 64-битных числа
    // Каждое число переводим в 16-значную hex-строку с ведущими нулями
    // Итого получается строка ровно из 32 символов (16 + 16)
    ss << std::hex << std::setfill('0')
       << std::setw(16) << generator_()
       << std::setw(16) << generator_();
    return ss.str();
}

std::string PlayerTokens::AddPlayer(std::shared_ptr<Player> player) {
    // Генерируем новый токен
    std::string token = GenerateToken();
    // Связываем токен с умным указателем на игрока
    token_to_player_[token] = std::move(player);
    return token;
}

std::shared_ptr<Player> PlayerTokens::FindPlayerByToken(const std::string& token) const {
    // Ищем токен в нашей хеш-таблице
    if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
        return it->second;
    }
    // Если токена нет — возвращаем пустой указатель
    return nullptr;
}

std::optional<JoinGameResult> Application::JoinGame(const std::string& user_name, const std::string& map_id) {
    // 1. Проверяем, существует ли такая карта в модели игры
    auto map_ptr = game_.FindMap(model::Map::Id{map_id});
    if (!map_ptr) {
        return std::nullopt;
    }

    // 2. Рассчитываем случайную стартовку на дороге с помощью метода из model
    // Поскольку метод объявлен внутри класса Map, вызываем его у объекта карты
    model::Point2D random_pos = const_cast<model::Map*>(map_ptr)->CalculateRandomDogPosition(*map_ptr);

    // 3. Создаем уникальный ID игрока (и собаки одновременно)
    auto player = player_manager_.CreatePlayer(user_name, map_id);

    // 4. Конструируем объект Dog и связываем его с игроком
    model::Dog::Id dog_id{player->GetId()};
    auto dog = std::make_shared<model::Dog>(dog_id, user_name, random_pos);
    player->SetDog(dog);

    // 5. Генерируем токен авторизации
    std::string token = player_tokens_.AddPlayer(player);

    return JoinGameResult{token, player->GetId()};
}

std::optional<std::vector<std::shared_ptr<Player>>> Application::GetPlayersInSession(const std::string& token) const {
    auto current_player = player_tokens_.FindPlayerByToken(token);
    if (!current_player) {
        return std::nullopt; // Неавторизованный токен
    }

    std::vector<std::shared_ptr<Player>> session_players;
    const std::string& target_map = current_player->GetMapId();

    // Собираем всех игроков, находящихся на той же карте
    for (const auto& [id, player] : player_manager_.GetPlayers()) {
        if (player->GetMapId() == target_map) {
            session_players.push_back(player);
        }
    }

    return session_players;
}

bool Application::MovePlayer(const std::string& token, const std::string& move_cmd) {
    // 1. Ищем игрока по токену
    auto player = player_tokens_.FindPlayerByToken(token);
    if (!player) {
        return false; // Токен не найден
    }

    // 2. Получаем карту, чтобы узнать её скорость s
    auto map_ptr = game_.FindMap(model::Map::Id{player->GetMapId()});
    if (!map_ptr) {
        return false;
    }
    double s = map_ptr->GetDogSpeed();

    auto dog_ptr = player->GetDog();
    if (!dog_ptr) {
        return false;
    }

    // 3. Вычисляем скорость и направление на основе команды move
    if (move_cmd == "L") {
        dog_ptr->SetSpeed({-s, 0.0});
        dog_ptr->SetDirection(model::Direction::WEST);
    } else if (move_cmd == "R") {
        dog_ptr->SetSpeed({s, 0.0});
        dog_ptr->SetDirection(model::Direction::EAST);
    } else if (move_cmd == "U") {
        dog_ptr->SetSpeed({0.0, -s});
        dog_ptr->SetDirection(model::Direction::NORTH);
    } else if (move_cmd == "D") {
        dog_ptr->SetSpeed({0.0, s});
        dog_ptr->SetDirection(model::Direction::SOUTH);
    } else if (move_cmd.empty()) {
        dog_ptr->SetSpeed({0.0, 0.0});
        // Направление при остановке менять не требуется, оставляем старое
    } else {
        return false; // Некорректная команда
    }

    return true;
}

Application::RoadBounds Application::GetRoadBounds(const model::Road& road) const {
    constexpr double half_width = 0.4;
    double x0 = static_cast<double>(road.GetStart().x);
    double y0 = static_cast<double>(road.GetStart().y);
    double x1 = static_cast<double>(road.GetEnd().x);
    double y1 = static_cast<double>(road.GetEnd().y);

    return {
        .min_x = std::min(x0, x1) - half_width,
        .max_x = std::max(x0, x1) + half_width,
        .min_y = std::min(y0, y1) - half_width,
        .max_y = std::max(y0, y1) + half_width
    };
}

bool Application::IsPointOnRoad(const model::Point2D& pos, const RoadBounds& bounds) const {
    return pos.x >= bounds.min_x && pos.x <= bounds.max_x &&
           pos.y >= bounds.min_y && pos.y <= bounds.max_y;
}

void Application::Tick(double delta_time_seconds) {
    for (const auto& [id, player] : player_manager_.GetPlayers()) {
        auto dog_ptr = player->GetDog();
        if (!dog_ptr) continue;

        auto map_ptr = game_.FindMap(model::Map::Id{player->GetMapId()});
        if (!map_ptr) continue;

        // Вызываем приватный метод обновления физики
        UpdateDogPosition(*dog_ptr, *map_ptr, delta_time_seconds);
    }
}

void Application::UpdateDogPosition(model::Dog& dog, const model::Map& map, double delta_time_seconds) {
    model::Point2D p0 = dog.GetPosition();
    model::Speed2D v = dog.GetSpeed();

    if (v.ux == 0.0 && v.uy == 0.0) {
        return;
    }

    model::Point2D p_target = {
        p0.x + v.ux * delta_time_seconds,
        p0.y + v.uy * delta_time_seconds
    };

    const auto& all_roads = map.GetRoads();

    auto is_safe = [&](const model::Point2D& pos) {
        for (const auto& road : all_roads) {
            if (IsPointOnRoad(pos, GetRoadBounds(road))) {
                return true;
            }
        }
        return false;
    };

    if (is_safe(p_target)) {
        dog.SetPosition(p_target);
        return;
    }

    bool hit_boundary = false;

    //Итеративно расширяем границы по всей цепочке соединённых дорог
    if (v.ux > 0) { // Вправо (R)
        double max_x = p0.x;
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (const auto& road : all_roads) {
                auto bounds = GetRoadBounds(road);
                // Если дорога пересекается с нашим текущим отрезком по Y и расширяет доступный X
                if (p0.y >= bounds.min_y && p0.y <= bounds.max_y &&
                    max_x >= bounds.min_x && max_x <= bounds.max_x) {
                    if (bounds.max_x > max_x) {
                        max_x = bounds.max_x;
                        expanded = true;
                    }
                }
            }
        }
        if (p_target.x >= max_x) {
            p_target.x = max_x;
            hit_boundary = true;
        }
    }
    else if (v.ux < 0) { // Влево (L)
        double min_x = p0.x;
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (const auto& road : all_roads) {
                auto bounds = GetRoadBounds(road);
                if (p0.y >= bounds.min_y && p0.y <= bounds.max_y &&
                    min_x >= bounds.min_x && min_x <= bounds.max_x) {
                    if (bounds.min_x < min_x) {
                        min_x = bounds.min_x;
                        expanded = true;
                    }
                }
            }
        }
        if (p_target.x <= min_x) {
            p_target.x = min_x;
            hit_boundary = true;
        }
    }
    else if (v.uy > 0) { // Вниз (D)
        double max_y = p0.y;
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (const auto& road : all_roads) {
                auto bounds = GetRoadBounds(road);
                if (p0.x >= bounds.min_x && p0.x <= bounds.max_x &&
                    max_y >= bounds.min_y && max_y <= bounds.max_y) {
                    if (bounds.max_y > max_y) {
                        max_y = bounds.max_y;
                        expanded = true;
                    }
                }
            }
        }
        if (p_target.y >= max_y) {
            p_target.y = max_y;
            hit_boundary = true;
        }
    }
    else if (v.uy < 0) { // Вверх (U)
        double min_y = p0.y;
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (const auto& road : all_roads) {
                auto bounds = GetRoadBounds(road);
                if (p0.x >= bounds.min_x && p0.x <= bounds.max_x &&
                    min_y >= bounds.min_y && min_y <= bounds.max_y) {
                    if (bounds.min_y < min_y) {
                        min_y = bounds.min_y;
                        expanded = true;
                    }
                }
            }
        }
        if (p_target.y <= min_y) {
            p_target.y = min_y;
            hit_boundary = true;
        }
    }

    if (hit_boundary) {
        dog.SetSpeed({0.0, 0.0});
    }

    dog.SetPosition(p_target);
}

} // namespace app
