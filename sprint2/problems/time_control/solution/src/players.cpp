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
    std::vector<RoadBounds> current_roads;

    // Округляем координаты до целых чисел, чтобы найти осевые линии дорог
    int approx_x = static_cast<int>(std::round(p0.x));
    int approx_y = static_cast<int>(std::round(p0.y));

    // Быстро выбираем только потенциально подходящие горизонтальные дороги
    for (size_t idx : map.GetHorizontalRoadsByY(approx_y)) {
        auto bounds = GetRoadBounds(all_roads[idx]);
        if (IsPointOnRoad(p0, bounds)) {
            current_roads.push_back(bounds);
        }
    }

    // Быстро выбираем только потенциально подходящие вертикальные дороги
    for (size_t idx : map.GetVerticalRoadsByX(approx_x)) {
        auto bounds = GetRoadBounds(all_roads[idx]);
        if (IsPointOnRoad(p0, bounds)) {
            current_roads.push_back(bounds);
        }
    }

    // Если пёс из-за погрешности double не попал в округление, делаем резервный быстрый поиск по соседним осям
    if (current_roads.empty()) {
        for (int offset : {-1, 1}) {
            for (size_t idx : map.GetHorizontalRoadsByY(approx_y + offset)) {
                auto bounds = GetRoadBounds(all_roads[idx]);
                if (IsPointOnRoad(p0, bounds)) current_roads.push_back(bounds);
            }
            for (size_t idx : map.GetVerticalRoadsByX(approx_x + offset)) {
                auto bounds = GetRoadBounds(all_roads[idx]);
                if (IsPointOnRoad(p0, bounds)) current_roads.push_back(bounds);
            }
        }
    }

    // Проверяем безопасность целевой точки P_target на основе найденных локальных дорог
    bool target_is_safe = false;
    for (const auto& bounds : current_roads) {
        if (IsPointOnRoad(p_target, bounds)) {
            target_is_safe = true;
            break;
        }
    }

    if (target_is_safe) {
        dog.SetPosition(p_target);
    } else {
        if (v.ux > 0) {
            double best_max_x = p0.x;
            for (const auto& bounds : current_roads) best_max_x = std::max(best_max_x, bounds.max_x);
            p_target.x = std::min(p_target.x, best_max_x);
            dog.SetSpeed({0.0, 0.0});
        } else if (v.ux < 0) {
            double best_min_x = p0.x;
            for (const auto& bounds : current_roads) best_min_x = std::min(best_min_x, bounds.min_x);
            p_target.x = std::max(p_target.x, best_min_x);
            dog.SetSpeed({0.0, 0.0});
        } else if (v.uy > 0) {
            double best_max_y = p0.y;
            for (const auto& bounds : current_roads) best_max_y = std::max(best_max_y, bounds.max_y);
            p_target.y = std::min(p_target.y, best_max_y);
            dog.SetSpeed({0.0, 0.0});
        } else if (v.uy < 0) {
            double best_min_y = p0.y;
            for (const auto& bounds : current_roads) best_min_y = std::min(best_min_y, bounds.min_y);
            p_target.y = std::max(p_target.y, best_min_y);
            dog.SetSpeed({0.0, 0.0});
        }
        dog.SetPosition(p_target);
    }
}

} // namespace app
