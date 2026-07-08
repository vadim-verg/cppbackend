#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <random>
#include <optional>
#include "model.h"

namespace app {

class Player {
public:
    Player(uint32_t id, const std::string& name, const std::string& map_id, std::shared_ptr<model::Dog> dog = nullptr)
        : id_(id)
        , name_(name)
        , map_id_(map_id)
        , dog_(std::move(dog)) {}

    uint32_t GetId() const {return id_;}
    const std::string& GetName() const {return name_;}
    const std::string& GetMapId() const {return map_id_;}

    void SetDog(std::shared_ptr<model::Dog> dog) { dog_ = std::move(dog); }
    std::shared_ptr<model::Dog> GetDog() const { return dog_; }

private:
    uint32_t id_;
    std::string name_;
    std::string map_id_;
    std::shared_ptr<model::Dog> dog_;
};

class PlayerTokens {
public:
    // Генерация псевдослучайного токена (32 hex-символа)
    std::string GenerateToken();

    // Зарегистрировать игрока и выдать ему токен
    std::string AddPlayer(std::shared_ptr<Player> player);

    // Найти игрока по токену (возвращает nullptr, если токен не найден)
    std::shared_ptr<Player> FindPlayerByToken(const std::string& token) const;

private:
    std::random_device rd_;
    std::mt19937_64 generator_{rd_()};
    std::unordered_map<std::string, std::shared_ptr<Player>> token_to_player_;
};

class PlayerManager {
public:
    // Создать нового игрока на карте с присваиванием ID
    std::shared_ptr<Player> CreatePlayer(const std::string& name, const std::string& map_id) {
        uint32_t id = next_id_++;
        auto player = std::make_shared<Player>(id, name, map_id);
        players_[id] = player;
        return player;
    }

    const std::unordered_map<uint32_t, std::shared_ptr<Player>>& GetPlayers() const {
        return players_;
    }

private:
    uint32_t next_id_ = 0;
    std::unordered_map<uint32_t, std::shared_ptr<Player>> players_;
};

// Результат входа в игру
struct JoinGameResult {
    std::string token;
    uint32_t player_id;
};

class Application {
public:
    explicit Application(model::Game& game)
        : game_(game) {}

    // Модуляция игрового времени (игровой цикл)
    void Tick(double delta_time_seconds);

    // Возвращает токен и ID или nullopt, если карта не найдена
    std::optional<JoinGameResult> JoinGame(const std::string& user_name, const std::string& map_id);

    // Возвращает список игроков или nullopt, если токен невалидный
    std::optional<std::vector<std::shared_ptr<Player>>> GetPlayersInSession(const std::string& token) const;

    bool MovePlayer(const std::string& token, const std::string& move_cmd);

    void SetSpawnRandomize(bool randomize) {
        randomize_spawn_ = randomize;
    }

    void SetAutomaticTicking(bool auto_tick) {
        auto_ticking_ = auto_tick;
    }

    bool IsAutomaticTicking() const {
        return auto_ticking_;
    }

private:
    // Вспомогательная структура для границ дороги
    struct RoadBounds {
        double min_x;
        double max_x;
        double min_y;
        double max_y;
    };

    RoadBounds GetRoadBounds(const model::Road& road) const;
    bool IsPointOnRoad(const model::Point2D& pos, const RoadBounds& bounds) const;
    void UpdateDogPosition(model::Dog& dog, const model::Map& map, double delta_time_seconds);

    model::Game& game_;
    PlayerManager player_manager_;
    PlayerTokens player_tokens_;

    bool randomize_spawn_ = false;
    bool auto_ticking_ = false;
};

} // namespace app
