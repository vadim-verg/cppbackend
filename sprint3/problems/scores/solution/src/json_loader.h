#pragma once

#include <filesystem>
#include "model.h"
#include "loot_provider.h"

namespace json_loader {

// Структура для одновременного возврата игры и данных о луте
struct ParsedGameData {
    model::Game game;
    app::LootInfoProvider loot_info;
};

ParsedGameData LoadGame(const std::filesystem::path& json_path);

std::string_view DirectionToString(model::Direction dir);

}  // namespace json_loader
