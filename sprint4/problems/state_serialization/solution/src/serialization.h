#pragma once

#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <string>
#include <vector>
#include <unordered_map>

#include "model.h"

// 1. Внешние функции сериализации для базовых структур геометрии из model.h
namespace model {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar & point.x;
    ar & point.y;
}

template <typename Archive>
void serialize(Archive& ar, Speed2D& speed, [[maybe_unused]] const unsigned version) {
    ar & speed.ux;
    ar & speed.uy;
}

template <typename Archive>
void serialize(Archive& ar, BagItem& item, [[maybe_unused]] const unsigned version) {
    ar & item.id;
    ar & item.type;
}

}  // namespace model

namespace serialization {

// Снимок Собаки
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : id_(dog.GetId())
        , name_(dog.GetName())
        , pos_(dog.GetPosition())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , score_(dog.GetScore())
        , bag_capacity_(dog.GetBagCapacity())
        , bag_(dog.GetBag()) {}

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{id_, name_, pos_};
        dog.SetSpeed(speed_);
        dog.SetDirection(direction_);
        dog.SetBagCapacity(bag_capacity_);
        dog.AddScore(score_);
        for (const auto& item : bag_) {
            if (!dog.AddToBag(item)) {
                throw std::runtime_error("Failed to restore dog bag content");
            }
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & *id_;
        ar & name_;
        ar & pos_;
        ar & speed_;
        ar & direction_;
        ar & score_;
        ar & bag_capacity_;
        ar & bag_;
    }

private:
    model::Dog::Id id_ = model::Dog::Id{0u};
    std::string name_;
    model::Point2D pos_;
    model::Speed2D speed_;
    model::Direction direction_ = model::Direction::NORTH;
    int score_ = 0;
    size_t bag_capacity_ = 0;
    std::vector<model::BagItem> bag_;
};

// Снимок Потерянного предмета
class LostObjectRepr {
public:
    LostObjectRepr() = default;

    explicit LostObjectRepr(const model::LostObject& loot)
        : id_(loot.id)
        , type_(loot.type)
        , pos_(loot.pos) {}

    [[nodiscard]] model::LostObject Restore() const {
        return model::LostObject{id_, type_, pos_};
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & id_;
        ar & type_;
        ar & pos_;
    }

private:
    unsigned id_ = 0;
    unsigned type_ = 0;
    model::Point2D pos_;
};

// Снимок Игрока (включая его собаку)
struct PlayerRepr {
    uint32_t id;
    std::string name;
    std::string map_id;
    std::optional<DogRepr> dog; // Собака может отсутствовать, если игрок еще не заспавнился

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & id;
        ar & name;
        ar & map_id;
        ar & dog;
    }
};

// Полное сериализуемое состояние
struct GameStateRepr {
    // Потерянный лут по картам
    std::unordered_map<std::string, std::vector<LostObjectRepr>> map_loot;
    // Генераторные счетчики ID предметов для карт
    std::unordered_map<std::string, unsigned> next_loot_id;

    // Состояние менеджера игроков
    uint32_t next_player_id = 0;
    std::vector<PlayerRepr> players;

    // Токены авторизации: Токен -> ID игрока
    std::unordered_map<std::string, uint32_t> token_to_player_id;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & map_loot;
        ar & next_loot_id;
        ar & next_player_id;
        ar & players;
        ar & token_to_player_id;
    }
};

}  // namespace serialization
