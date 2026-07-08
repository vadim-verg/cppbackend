#pragma once

#include <filesystem>

#include "model.h"

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path);

std::string_view DirectionToString(model::Direction dir);

}  // namespace json_loader
