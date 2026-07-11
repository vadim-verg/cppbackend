#include "model.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
    // Индексируем дорогу для быстрого поиска
    size_t road_index = roads_.size() - 1;
    if (road.IsHorizontal()) {
        coord_to_horizontal_roads_[road.GetStart().y].push_back(road_index);
    } else {
        coord_to_vertical_roads_[road.GetStart().x].push_back(road_index);
    }
}

const std::vector<size_t>& Map::GetHorizontalRoadsByY(int y) const {
    static const std::vector<size_t> empty;
    if (auto it = coord_to_horizontal_roads_.find(y); it != coord_to_horizontal_roads_.end()) {
        return it->second;
    }
    return empty;
}

const std::vector<size_t>& Map::GetVerticalRoadsByX(int x) const {
    static const std::vector<size_t> empty;
    if (auto it = coord_to_vertical_roads_.find(x); it != coord_to_vertical_roads_.end()) {
        return it->second;
    }
    return empty;
}

}  // namespace model
