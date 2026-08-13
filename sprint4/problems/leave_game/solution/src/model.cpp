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

Point2D CalculateRandomDogPosition(const Map& map) {
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        return {0.0, 0.0};
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    const auto& road = roads[road_dist(gen)];

    Point2D pos;
    if (road.IsHorizontal()) {
        // На горизонтальной дороге Y фиксирован, X — случайный
        double start_x = std::min(road.GetStart().x, road.GetEnd().x);
        double end_x = std::max(road.GetStart().x, road.GetEnd().x);
        std::uniform_real_distribution<double> x_dist(start_x, end_x);

        pos.x = x_dist(gen);
        pos.y = static_cast<double>(road.GetStart().y);
    } else {
        // На вертикальной дороге X фиксирован, Y — случайный
        double start_y = std::min(road.GetStart().y, road.GetEnd().y);
        double end_y = std::max(road.GetStart().y, road.GetEnd().y);
        std::uniform_real_distribution<double> y_dist(start_y, end_y);

        pos.x = static_cast<double>(road.GetStart().x);
        pos.y = y_dist(gen);
    }

    return pos;
}

void Game::Tick(std::chrono::milliseconds delta_time) {
    for (const auto& map : maps_) {
        const auto& map_id = map.GetId();

        if (!map_generators_.contains(map_id)) {
            auto rand_fn = []() {
                return 1.0;
            };
            map_generators_.emplace(
                map_id,
                loot_gen::LootGenerator(loot_period_, loot_probability_, rand_fn)
                );
        }

        unsigned loot_count = static_cast<unsigned>(map_loot_[map_id].size());
        unsigned looter_count = static_cast<unsigned>(GetDogCount(map_id));

        unsigned count_to_generate = map_generators_.at(map_id).Generate(delta_time, loot_count, looter_count);

        if (count_to_generate > 0) {
            GenerateLootForMap(map, count_to_generate);
        }
    }
}

void Game::GenerateLootForMap(const Map& map, unsigned count) {

    if (map.GetRoads().empty()) {
        return;
    }

    const auto& map_id = map.GetId();
    size_t types_count = map.GetLootTypesCount();

    if (types_count == 0) {
        return;
    }

    std::uniform_int_distribution<unsigned> type_dist(0, static_cast<unsigned>(types_count - 1));

    for (unsigned i = 0; i < count; ++i) {
        LostObject obj;
        obj.id = next_loot_id_[map_id]++;
        obj.type = type_dist(random_engine_);
        obj.pos = CalculateRandomDogPosition(map);

        map_loot_[map_id][obj.id] = obj;
    }
}

}  // namespace model
