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

Point2D Map::CalculateRandomDogPosition(const Map& map) {
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        return {0.0, 0.0};
    }

    // Настраиваем генератор случайных чисел
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

}  // namespace model
