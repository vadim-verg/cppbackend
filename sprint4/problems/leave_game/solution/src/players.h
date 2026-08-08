#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <random>
#include <optional>
#include "model.h"
#include "postgres.h"


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
    std::string GenerateToken();

    std::string AddPlayer(std::shared_ptr<Player> player) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::string token = GenerateToken();
        token_to_player_[token] = std::move(player);
        return token;
    }

    std::shared_ptr<Player> FindPlayerByToken(const std::string& token) const {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Для сохранения состояния (возвращает константную ссылку под lock)
    const std::unordered_map<std::string, std::shared_ptr<Player>>& GetTokenMap() const {
        return token_to_player_;
    }

    // Для безопасного итерирования в параллельных потоках
    std::unordered_map<std::string, std::shared_ptr<Player>> GetTokenMapCopy() const {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        return token_to_player_;
    }

    void RemoveToken(const std::string& token) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        token_to_player_.erase(token);
    }

    void RestoreToken(const std::string& token, std::shared_ptr<Player> player) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        token_to_player_[token] = std::move(player);
    }

private:
    std::random_device rd_;
    std::mt19937_64 generator_{rd_()};
    std::unordered_map<std::string, std::shared_ptr<Player>> token_to_player_;
    mutable std::recursive_mutex mutex_;
};

class PlayerManager {
public:
    std::shared_ptr<Player> CreatePlayer(const std::string& name, const std::string& map_id) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        uint32_t id = next_id_++;
        auto player = std::make_shared<Player>(id, name, map_id);
        players_[id] = player;
        return player;
    }

    // ВОЗВРАЩАЕМ СТАРЫЙ МЕТОД ДЛЯ СОВМЕСТИМОСТИ С СЕРИАЛИЗАЦИЕЙ И MAIN
    const std::unordered_map<uint32_t, std::shared_ptr<Player>>& GetPlayers() const {
        return players_;
    }

    // Метод для безопасных циклов
    std::unordered_map<uint32_t, std::shared_ptr<Player>> GetPlayersCopy() const {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        return players_;
    }

    void RemovePlayer(uint32_t id) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        players_.erase(id);
    }

    void RestorePlayer(std::shared_ptr<Player> player) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        players_[player->GetId()] = player;
    }

    void SetNextId(uint32_t id) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        next_id_ = id;
    }

    uint32_t GetNextIdInternal() const {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        return next_id_;
    }

private:
    uint32_t next_id_ = 0;
    std::unordered_map<uint32_t, std::shared_ptr<Player>> players_;
    mutable std::recursive_mutex mutex_;
};

// Результат входа в игру
struct JoinGameResult {
    std::string token;
    uint32_t player_id;
};

class Application {
public:
    Application(model::Game& game, std::shared_ptr<postgres::RecordsRepository> db_repo, double retirement_time)
        : game_(game)
        , db_repo_(db_repo)
        , dog_retirement_time_(retirement_time) {}

    std::shared_ptr<postgres::RecordsRepository> GetDBRepository() const { return db_repo_; }

    // Модуляция игрового времени
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

    const model::Game& GetGame() const noexcept {
        return game_;
    }

    size_t GetDogCountOnMap(const std::string& map_id) const {
        size_t count = 0;
        for (const auto& [id, player] : player_manager_.GetPlayers()) {
            if (player->GetMapId() == map_id) {
                count++;
            }
        }
        return count;
    }

    const PlayerManager& GetPlayerManager() const { return player_manager_; }

    PlayerManager& GetPlayerManagerMutable() { return player_manager_; }

    const PlayerTokens& GetPlayerTokens() const { return player_tokens_; }

    PlayerTokens& GetPlayerTokensMutable() { return player_tokens_; }

    bool PopShouldSaveState() const {
        bool res = should_save_state_;
        should_save_state_ = false;
        return res;
    }

    // Также добавьте простой метод для инициализации параметров времени:
    void SetupSaveParameters(std::optional<std::string> state_file, std::optional<uint32_t> period_ms) {
        state_file_ = std::move(state_file);
        if (period_ms.has_value()) {
            save_state_period_ = std::chrono::milliseconds(period_ms.value());
        } else {
            save_state_period_ = std::nullopt;
        }
    }

private:
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
    mutable bool should_save_state_ = false;

    std::optional<std::string> state_file_;
    std::optional<std::chrono::milliseconds> save_state_period_;
    std::chrono::milliseconds time_since_last_save_{0};

    std::shared_ptr<postgres::RecordsRepository> db_repo_;
    double dog_retirement_time_ = 60.0;

};

} // namespace app
