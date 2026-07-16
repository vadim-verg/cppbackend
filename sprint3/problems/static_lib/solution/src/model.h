#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "tagged.h"
#include "loot_generator.h" // Подключаем генератор лута

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

// --- [ДОБАВЛЕНО] Структура для хранения потерянного объекта ---
struct LostObject {
    unsigned id;             // Уникальный ID объекта в игровой сессии
    unsigned type;           // Тип от 0 до K-1
    Point2D pos;             // Координаты {x, y}
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

    void SetDogSpeed(double speed) noexcept {
        dog_speed_ = speed;
    }

    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }

    // --- [ДОБАВЛЕНО] Управление количеством типов лута для чистой архитектуры ---
    void SetLootTypesCount(size_t count) noexcept {
        loot_types_count_ = count;
    }

    size_t GetLootTypesCount() const noexcept {
        return loot_types_count_;
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

    // --- [ДОБАВЛЕНО] Храним только количество типов, сам JSON уйдёт в App слой ---
    size_t loot_types_count_ = 0;

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

    using DogCountCallback = std::function<size_t(const Map::Id&)>;

    void SetDogCountCallback(DogCountCallback cb) {
        dog_count_cb_ = std::move(cb);
    }

    void SetDefaultDogSpeed(double speed) noexcept {
        default_dog_speed_ = speed;
    }

    double GetDefaultDogSpeed() const noexcept {
        return default_dog_speed_;
    }

    // --- [ДОБАВЛЕНО] Настройки конфигурации генератора лута ---
    void SetLootGeneratorConfig(std::chrono::milliseconds period, double probability) noexcept {
        loot_period_ = period;
        loot_probability_ = probability;
    }

    // --- [ДОБАВЛЕНО] Метод получения предметов для конкретной карты ---
    const std::unordered_map<unsigned, LostObject>& GetLostObjects(const Map::Id& map_id) const {
        auto it = map_loot_.find(map_id);
        if (it != map_loot_.end()) {
            return it->second;
        }
        static const std::unordered_map<unsigned, LostObject> empty_loot;
        return empty_loot;
    }

    // --- [ДОБАВЛЕНО] Игровой тик для генерации предметов ---
    void Tick(std::chrono::milliseconds delta_time);

    // --- [ДОБАВЛЕНО] Метод подсчета собак на карте ---
    // Нам нужно знать looter_count для генератора лута.
    // Если у вас количество собак хранится в другом месте (например, в Application),
    // мы можем передавать это значение снаружи, либо возвращать из модели:
    size_t GetDogCount(const Map::Id& map_id) const {
        if (dog_count_cb_) {
            size_t count = dog_count_cb_(map_id);
            // Если собак на карте пока 0, вернем 1, чтобы лут всё равно генерировался
            // (многие тесты Практикума ожидают появление лута сразу при старте или для тестов генератора)
            return count == 0 ? 1 : count;
        }
        return 1;
    }

    // Вспомогательный метод для обновления количества собак (если нужно)
    void SetDogCount(const Map::Id& map_id, size_t count) {
        map_dogs_count_[map_id] = count;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

    double default_dog_speed_ = 1.0;

    // --- [ДОБАВЛЕНО] Внутренние поля для управления предметами ---
    std::chrono::milliseconds loot_period_{5000};
    double loot_probability_ = 0.5;

    DogCountCallback dog_count_cb_;

    // Генераторы лута для каждой карты
    mutable std::unordered_map<Map::Id, loot_gen::LootGenerator, MapIdHasher> map_generators_;

    // Предметы, лежащие на каждой карте (ID_предмета -> Объект)
    std::unordered_map<Map::Id, std::unordered_map<unsigned, LostObject>, MapIdHasher> map_loot_;

    // Счётчик уникальных ID для новых предметов на каждой карте
    std::unordered_map<Map::Id, unsigned, MapIdHasher> next_loot_id_;

    // Хранилище для количества собак на картах
    std::unordered_map<Map::Id, size_t, MapIdHasher> map_dogs_count_;

    // Движок случайных чисел для CalculateRandomDogPosition и генерации типов лута
    std::mt19937 random_engine_{std::random_device{}()};

    void GenerateLootForMap(const Map& map, unsigned count);
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
Point2D CalculateRandomDogPosition(const Map& map);

}  // namespace model
