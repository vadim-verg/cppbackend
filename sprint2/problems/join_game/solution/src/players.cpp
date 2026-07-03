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
    // 1. Проверяем существование карты в модели
    auto map_tagged_id = model::Map::Id{map_id};
    if (!game_.FindMap(map_tagged_id)) {
        return std::nullopt; // Карта не найдена — бизнес-ошибка
    }

    // 2. Создаем игрока и генерируем токен
    auto player = player_manager_.CreatePlayer(user_name, map_id);
    std::string token = player_tokens_.AddPlayer(player);

    return JoinGameResult{token, player->GetId()};
}

std::optional<std::vector<std::shared_ptr<Player>>> Application::GetPlayersInSession(const std::string& token) const {
    // 1. Ищем игрока по токену
    auto current_player = player_tokens_.FindPlayerByToken(token);
    if (!current_player) {
        return std::nullopt; // Токен не найден
    }

    // 2. Фильтруем игроков, находящихся на той же карте
    std::string current_map_id = current_player->GetMapId();
    std::vector<std::shared_ptr<Player>> result;

    for (const auto& [id, player] : player_manager_.GetPlayers()) {
        if (player->GetMapId() == current_map_id) {
            result.push_back(player);
        }
    }

    return result;
}

} // namespace app
