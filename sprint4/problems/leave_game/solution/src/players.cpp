#include "players.h"
#include "collision_detector.h"
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <unordered_map>

namespace app {

std::string PlayerTokens::GenerateToken() {
    std::stringstream ss;

    ss << std::hex << std::setfill('0')
       << std::setw(16) << generator_()
       << std::setw(16) << generator_();
    return ss.str();
}

std::string PlayerTokens::AddPlayer(std::shared_ptr<Player> player) {
    std::string token = GenerateToken();
    token_to_player_[token] = std::move(player);
    return token;
}

std::shared_ptr<Player> PlayerTokens::FindPlayerByToken(const std::string& token) const {
    if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
        return it->second;
    }
    return nullptr;
}

std::optional<JoinGameResult> Application::JoinGame(const std::string& user_name, const std::string& map_id) {
    auto map_ptr = game_.FindMap(model::Map::Id{map_id});
    if (!map_ptr) {
        return std::nullopt;
    }

    model::Point2D start_pos;
    if (randomize_spawn_) {
        // Если рандом включен — используем генерацию случайной позиции
        start_pos = model::CalculateRandomDogPosition(*map_ptr);
    } else {
        // Если рандом выключен — берем координаты начала (x0, y0) первой дороги
        const auto& roads = map_ptr->GetRoads();
        if (!roads.empty()) {
            auto start_road = roads.at(0).GetStart();
            start_pos = {static_cast<double>(start_road.x), static_cast<double>(start_road.y)};
        } else {
            start_pos = {0.0, 0.0}; // если дорог на карте нет
        }
    }

    auto player = player_manager_.CreatePlayer(user_name, map_id);

    model::Dog::Id dog_id{player->GetId()};
    auto dog = std::make_shared<model::Dog>(dog_id, user_name, start_pos);

    // === ВСТАВЛЯЕМ СЮДА: Новая собака НЕ бездействует на старте ===
    dog->SetIdleStarted(false);
    // =============================================================

    player->SetDog(dog);

    std::string token = player_tokens_.AddPlayer(player);

    return JoinGameResult{token, player->GetId()};
}

std::optional<std::vector<std::shared_ptr<Player>>> Application::GetPlayersInSession(const std::string& token) const {
    auto current_player = player_tokens_.FindPlayerByToken(token);
    if (!current_player) {
        return std::nullopt;
    }

    std::vector<std::shared_ptr<Player>> session_players;
    const std::string& target_map = current_player->GetMapId();

    for (const auto& [id, player] : player_manager_.GetPlayers()) {
        if (player->GetMapId() == target_map) {
            session_players.push_back(player);
        }
    }

    std::sort(session_players.begin(), session_players.end(), [](const auto& a, const auto& b) {
        return a->GetId() < b->GetId();
    });

    return session_players;
}

bool Application::MovePlayer(const std::string& token, const std::string& move_cmd) {

    auto player = player_tokens_.FindPlayerByToken(token);
    if (!player) {
        return false;
    }

    auto map_ptr = game_.FindMap(model::Map::Id{player->GetMapId()});
    if (!map_ptr) {
        return false;
    }
    double s = map_ptr->GetDogSpeed();

    auto dog_ptr = player->GetDog();
    if (!dog_ptr) {
        return false;
    }

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
        dog_ptr->SetIdleStarted(true);
    } else {
        dog_ptr->SetIdleStarted(false);
        return false;
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

// Вспомогательные маркеры для типов объектов в провайдере коллизий
enum class ProviderItemType {
    LOST_OBJECT,
    OFFICE
};

struct ProviderItemInfo {
    ProviderItemType type;
    unsigned id;
    size_t office_idx;
};

class GameItemGathererProvider : public collision_detector::ItemGathererProvider {
public:
    std::vector<collision_detector::Item> items;
    std::vector<collision_detector::Gatherer> gatherers;
    std::vector<ProviderItemInfo> item_infos;
    std::vector<std::shared_ptr<model::Dog>> dog_ptrs;

    size_t ItemsCount() const override { return items.size(); }
    collision_detector::Item GetItem(size_t idx) const override { return items.at(idx); }

    size_t GatherersCount() const override { return gatherers.size(); }
    collision_detector::Gatherer GetGatherer(size_t idx) const override { return gatherers.at(idx); }
};

void Application::Tick(double delta_time_seconds) {
    auto& mutable_game = game_;

    std::unordered_map<std::string, std::vector<std::shared_ptr<model::Dog>>> map_to_dogs;
    std::unordered_map<uint32_t, model::Point2D> dog_start_positions;

    // Вектор для сбора ID игроков, чьи собаки превысили таймаут бездействия
    std::vector<uint32_t> players_to_retire;

    double retirement_timeout = game_.GetDogRetirementTime();

    for (const auto& [id, player] : player_manager_.GetPlayers()) {
        auto dog_ptr = player->GetDog();
        if (!dog_ptr) continue;

        auto map_ptr = game_.FindMap(model::Map::Id{player->GetMapId()});
        if (!map_ptr) continue;

        // Обновляем игровое время собаки
        dog_ptr->UpdateTime(delta_time_seconds);

        // === ИСПРАВЛЕНО: Проверяем таймаут, только если он включен (больше 0) ===
        if (retirement_timeout > 0.0 && dog_ptr->GetIdleTime() >= retirement_timeout) {
            players_to_retire.push_back(id);
            continue;
        }
        // ======================================================================

        dog_start_positions[dog_ptr->GetId().operator*()] = dog_ptr->GetPosition();
        UpdateDogPosition(*dog_ptr, *map_ptr, delta_time_seconds);
        map_to_dogs[player->GetMapId()].push_back(dog_ptr);
    }

    // Сбор коллизий отдельно для каждой карты
    for (const auto& [map_id_str, dogs] : map_to_dogs) {
        model::Map::Id map_id{map_id_str};
        auto map_ptr = game_.FindMap(map_id);
        if (!map_ptr) continue;

        GameItemGathererProvider provider;

        for (const auto& dog_ptr : dogs) {
            model::Point2D start_pos = dog_start_positions[dog_ptr->GetId().operator*()];
            model::Point2D end_pos = dog_ptr->GetPosition();

            provider.gatherers.push_back(collision_detector::Gatherer{
                .start_pos = {start_pos.x, start_pos.y},
                .end_pos = {end_pos.x, end_pos.y},
                .width = 0.3 // ПОЛОВИНА ШИРИНЫ ИГРОКА (0.6 / 2)
            });
            provider.dog_ptrs.push_back(dog_ptr);
        }

        const auto& lost_objects = game_.GetLostObjects(map_id);
        for (const auto& [obj_id, obj] : lost_objects) {
            provider.items.push_back(collision_detector::Item{
                .position = {obj.pos.x, obj.pos.y},
                .width = 0.0 // ШИРИНA ПРЕДМЕТА
            });
            provider.item_infos.push_back(ProviderItemInfo{
                .type = ProviderItemType::LOST_OBJECT,
                .id = obj_id,
                .office_idx = 0
            });
        }

        const auto& offices = map_ptr->GetOffices();
        for (size_t idx = 0; idx < offices.size(); ++idx) {
            auto office_pos = offices[idx].GetPosition();
            provider.items.push_back(collision_detector::Item{
                .position = {static_cast<double>(office_pos.x), static_cast<double>(office_pos.y)},
                .width = 0.25 // ПОЛОВИНА ШИРИНЫ БАЗЫ (0.5 / 2)
            });
            provider.item_infos.push_back(ProviderItemInfo{
                .type = ProviderItemType::OFFICE,
                .id = 0,
                .office_idx = idx
            });
        }

        if (!provider.gatherers.empty() && !provider.items.empty()) {
            auto events = collision_detector::FindGatherEvents(provider);

            std::unordered_set<unsigned> collected_in_tick;

            for (const auto& event : events) {
                auto dog_ptr = provider.dog_ptrs.at(event.gatherer_id);
                const auto& item_info = provider.item_infos.at(event.item_id);

                if (item_info.type == ProviderItemType::LOST_OBJECT) {
                    if (collected_in_tick.contains(item_info.id) || dog_ptr->IsBagFull()) {
                        continue;
                    }

                    unsigned obj_id = item_info.id;
                    if (lost_objects.contains(obj_id)) {
                        unsigned obj_type = lost_objects.at(obj_id).type;

                        if (dog_ptr->AddToBag(model::BagItem{.id = obj_id, .type = obj_type})) {
                            collected_in_tick.insert(obj_id);
                            mutable_game.RemoveLostObject(map_id, obj_id);
                        }
                    }
                }
                else if (item_info.type == ProviderItemType::OFFICE) {
                    const auto& bag = dog_ptr->GetBag();
                    if (!bag.empty()) {
                        for (const auto& bag_item : bag) {
                            int item_points = map_ptr->GetLootValue(bag_item.type);
                            dog_ptr->AddScore(item_points);
                        }
                        dog_ptr->ClearBag();
                    }
                }
            }
        }
    }

    auto delta_ms = std::chrono::milliseconds(static_cast<long long>(delta_time_seconds * 1000.0));
    mutable_game.Tick(delta_ms);

    // 3. Отправляем неактивных собак на заслуженный отдых и удаляем их из игры
    for (uint32_t player_id : players_to_retire) {
        const auto& players = player_manager_.GetPlayers();
        if (auto it = players.find(player_id); it != players.end()) {
            auto player = it->second;
            auto dog = player->GetDog();

            if (dog) {
                // Вызываем сигнал рекордов для базы данных
                dog_retired_signal(dog->GetName(), dog->GetScore(), dog->GetPlayTime());

                // === СИНХРОНИЗАЦИЯ С МОДЕЛЬЮ GAME ===
                model::Map::Id map_id{player->GetMapId()};
                size_t current_count = mutable_game.GetDogCount(map_id);
                if (current_count > 0) {
                    mutable_game.SetDogCount(map_id, current_count - 1);
                }
                // ===================================
            }

            // Аннулируем авторизационный токен
            player_tokens_.RemovePlayerToken(player);

            // Удаляем игрока из менеджера игроков
            player_manager_.RemovePlayer(player_id);
        }
    }

    // Сбрасываем флаг перед проверкой
    should_save_state_ = false;

    if (state_file_ && save_state_period_) {
        time_since_last_save_ += delta_ms;
        if (time_since_last_save_ >= *save_state_period_) {
            should_save_state_ = true;
            time_since_last_save_ = std::chrono::milliseconds(0);
        }
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
    bool hit_boundary = false;

    if (v.ux > 0) { // Вправо (R)
        double max_x = p0.x;
        // Собираем границы дорог, на которых стоим
        for (const auto& road : all_roads) {
            auto bounds = GetRoadBounds(road);
            if (p0.y >= bounds.min_y && p0.y <= bounds.max_y && p0.x >= bounds.min_x && p0.x <= bounds.max_x) {
                max_x = std::max(max_x, bounds.max_x);
            }
        }
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (const auto& road : all_roads) {
                auto bounds = GetRoadBounds(road);
                if (p0.y >= bounds.min_y && p0.y <= bounds.max_y && bounds.min_x <= max_x && bounds.max_x > max_x) {
                    max_x = bounds.max_x;
                    expanded = true;
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
        for (const auto& road : all_roads) {
            auto bounds = GetRoadBounds(road);
            if (p0.y >= bounds.min_y && p0.y <= bounds.max_y && p0.x >= bounds.min_x && p0.x <= bounds.max_x) {
                min_x = std::min(min_x, bounds.min_x);
            }
        }
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (const auto& road : all_roads) {
                auto bounds = GetRoadBounds(road);
                if (p0.y >= bounds.min_y && p0.y <= bounds.max_y && bounds.max_x >= min_x && bounds.min_x < min_x) {
                    min_x = bounds.min_x;
                    expanded = true;
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
        for (const auto& road : all_roads) {
            auto bounds = GetRoadBounds(road);
            if (p0.x >= bounds.min_x && p0.x <= bounds.max_x && p0.y >= bounds.min_y && p0.y <= bounds.max_y) {
                max_y = std::max(max_y, bounds.max_y);
            }
        }
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (const auto& road : all_roads) {
                auto bounds = GetRoadBounds(road);
                if (p0.x >= bounds.min_x && p0.x <= bounds.max_x && bounds.min_y <= max_y && bounds.max_y > max_y) {
                    max_y = bounds.max_y;
                    expanded = true;
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
        for (const auto& road : all_roads) {
            auto bounds = GetRoadBounds(road);
            if (p0.x >= bounds.min_x && p0.x <= bounds.max_x && p0.y >= bounds.min_y && p0.y <= bounds.max_y) {
                min_y = std::min(min_y, bounds.min_y);
            }
        }
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (const auto& road : all_roads) {
                auto bounds = GetRoadBounds(road);
                if (p0.x >= bounds.min_x && p0.x <= bounds.max_x && bounds.max_y >= min_y && bounds.min_y < min_y) {
                    min_y = bounds.min_y;
                    expanded = true;
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
        dog.SetIdleStarted(true);
    }

    dog.SetPosition(p_target);
}

} // namespace app
