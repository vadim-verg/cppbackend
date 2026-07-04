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

} // namespace app
