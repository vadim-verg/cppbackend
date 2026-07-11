#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "tagged.h"

namespace model {



using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

// Направление движения в пространстве
enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

// Структура для вещественных координат
struct Point2D {
    double x = 0.0; // метры
    double y = 0.0;
};

// Структура для скорости
struct Speed2D {
    double ux = 0.0; // единица карты/сек.
    double uy = 0.0;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road);

    // Получение индексов дорог, проходящих через координату Y (для горизонтальных)
    const std::vector<size_t>& GetHorizontalRoadsByY(int y) const;

    // Получение индексов дорог, проходящих через координату X (для вертикальных)
    const std::vector<size_t>& GetVerticalRoadsByX(int x) const;

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);


//    Point2D CalculateRandomDogPosition(const Map& map);

    void SetDogSpeed(double speed) noexcept {
        dog_speed_ = speed;
    }

    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    double dog_speed_ = 1.0;

    std::unordered_map<int, std::vector<size_t>> coord_to_horizontal_roads_;
    std::unordered_map<int, std::vector<size_t>> coord_to_vertical_roads_;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    void SetDefaultDogSpeed(double speed) noexcept {
        default_dog_speed_ = speed;
    }

    double GetDefaultDogSpeed() const noexcept {
        return default_dog_speed_;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

    double default_dog_speed_ = 1.0;
};

class Dog {
public:
    using Id = util::Tagged<uint32_t, Dog>;

    Dog(Id id, std::string name, Point2D position)
        : id_(std::move(id))
        , name_(std::move(name))
        , position_(position)
    {}

    const Id& GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }

    const Point2D& GetPosition() const noexcept { return position_; }
    void SetPosition(Point2D pos) noexcept { position_ = pos; }

    const Speed2D& GetSpeed() const noexcept { return speed_; }
    void SetSpeed(Speed2D speed) noexcept { speed_ = speed; }

    Direction GetDirection() const noexcept { return direction_; }
    void SetDirection(Direction dir) noexcept { direction_ = dir; }

private:
    Id id_;
    std::string name_;
    Point2D position_;
    Speed2D speed_ = {0.0, 0.0};                     // По умолчанию скорость 0
    Direction direction_ = Direction::NORTH;         // По умолчанию направление на север
};

// Получение случайной координаты на случайной дороге карты
Point2D CalculateRandomDogPosition(const Map& map) {
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

}  // namespace model
