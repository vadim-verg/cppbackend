#include "request_handler.h"
#include "loot_provider.h"
#include <filesystem>

namespace http_handler {

//вспомогательные функции для декомпоновки и возможности удобного расширения в дальнейшем

boost::json::array SerializeRoads(const model::Map& map) {
    boost::json::array json_roads;
    for (const auto& road : map.GetRoads()) {
        boost::json::object json_road;
        auto start = road.GetStart();
        json_road["x0"] = start.x;
        json_road["y0"] = start.y;
        if (road.IsHorizontal()) {
            json_road["x1"] = road.GetEnd().x;
        } else {
            json_road["y1"] = road.GetEnd().y;
        }
        json_roads.push_back(std::move(json_road));
    }
    return json_roads;
}

boost::json::array SerializeBuildings(const model::Map& map) {
    boost::json::array json_buildings;
    for (const auto& building : map.GetBuildings()) {
        boost::json::object json_build;
        auto bounds = building.GetBounds();
        json_build["x"] = bounds.position.x;
        json_build["y"] = bounds.position.y;
        json_build["w"] = bounds.size.width;
        json_build["h"] = bounds.size.height;
        json_buildings.push_back(std::move(json_build));
    }
    return json_buildings;
}

boost::json::array SerializeOffices(const model::Map& map) {
    boost::json::array json_offices;
    for (const auto& office : map.GetOffices()) {
        boost::json::object json_office;
        json_office["id"] = *office.GetId();
        json_office["x"] = office.GetPosition().x;
        json_office["y"] = office.GetPosition().y;
        json_office["offsetX"] = office.GetOffset().dx;
        json_office["offsetY"] = office.GetOffset().dy;
        json_offices.push_back(std::move(json_office));
    }
    return json_offices;
}

boost::json::object SerializeMap(const model::Map& map, const app::LootInfoProvider& loot_info_provider) {
    boost::json::object json_map;
    json_map["id"] = *map.GetId();
    json_map["name"] = map.GetName();

    json_map["roads"] = SerializeRoads(map);
    json_map["buildings"] = SerializeBuildings(map);
    json_map["offices"] = SerializeOffices(map);

    if (auto const* loot_types = loot_info_provider.GetLootTypesForMap(*map.GetId())) {
        json_map["lootTypes"] = *loot_types;
    } else {
        // Если вдруг в конфигурации пусто, возвращаем пустой массив
        json_map["lootTypes"] = boost::json::array{};
    }

    return json_map;
}

boost::json::object SerializeLostObjects(const model::Map::Id& map_id, const model::Game& game) {
    boost::json::object json_lost_objects;

    // Получаем список потерянных предметов для конкретной карты из модели
    const auto& lost_objects = game.GetLostObjects(map_id);

    for (const auto& [obj_id, obj] : lost_objects) {
        boost::json::object json_obj;
        json_obj["type"] = obj.type;

        boost::json::array json_pos;
        json_pos.push_back(obj.pos.x);
        json_pos.push_back(obj.pos.y);
        json_obj["pos"] = std::move(json_pos);

        json_lost_objects[std::to_string(obj_id)] = std::move(json_obj);
    }

    return json_lost_objects;
}

//функция URL-декодирования
std::string UrlDecode(std::string_view src) {
    std::string res;
    res.reserve(src.size());

    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '+') {
            res += ' '; // Плюс превращается в пробел
        } else if (src[i] == '%' && i + 2 < src.size()) {
            // Читаем два шестнадцатеричных символа после '%'
            std::string hex_str(src.substr(i + 1, 2));
            char chr = static_cast<char>(std::stol(hex_str, nullptr, 16));
            res += chr;
            i += 2; // Пропускаем обработанные символы
        } else {
            res += src[i];
        }
    }
    return res;
}

//функция определения MIME-типа (Content-Type)
std::string_view GetMimeType(const std::filesystem::path& path) {
    using namespace std::literals;

    // Получаем расширение (например, ".html" или ".PNG")
    std::string ext = path.extension().string();
    // Переводим в нижний регистр
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (ext == ".htm" || ext == ".html") return "text/html"sv;
    if (ext == ".css")                  return "text/css"sv;
    if (ext == ".txt")                  return "text/plain"sv;
    if (ext == ".js")                   return "text/javascript"sv;
    if (ext == ".json")                 return "application/json"sv;
    if (ext == ".xml")                  return "application/xml"sv;
    if (ext == ".png")                  return "image/png"sv;
    if (ext == ".jpg" || ext == ".jpe" || ext == ".jpeg") return "image/jpeg"sv;
    if (ext == ".gif")                  return "image/gif"sv;
    if (ext == ".bmp")                  return "image/bmp"sv;
    if (ext == ".ico")                  return "image/vnd.microsoft.icon"sv;
    if (ext == ".tiff" || ext == ".tif") return "image/tiff"sv;
    if (ext == ".svg" || ext == ".svgz") return "image/svg+xml"sv;
    if (ext == ".mp3")                  return "audio/mpeg"sv;

    return "application/octet-stream"sv; // Для неизвестных типов
}

//Защита от выхода за пределы директории
bool IsSubpath(std::filesystem::path path, std::filesystem::path base) {
    path = std::filesystem::weakly_canonical(path);
    base = std::filesystem::weakly_canonical(base);

    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

std::optional<std::string> RequestHandler::TryExtractToken(const auto& req) const {
    auto auth_it = req.find(boost::beast::http::field::authorization);
    if (auth_it == req.end()) {
        return std::nullopt;
    }

    std::string_view auth_header = auth_it->value();
    std::string_view prefix = "Bearer ";
    if (!auth_header.starts_with(prefix)) {
        return std::nullopt;
    }

    auth_header.remove_prefix(prefix.size());
    // Обрезаем пробелы
    while (!auth_header.empty() && auth_header.front() == ' ') auth_header.remove_prefix(1);
    while (!auth_header.empty() && auth_header.back() == ' ') auth_header.remove_suffix(1);

    if (auth_header.size() != 32) {
        return std::nullopt;
    }
    return std::string(auth_header);
}

}  // namespace http_handler
