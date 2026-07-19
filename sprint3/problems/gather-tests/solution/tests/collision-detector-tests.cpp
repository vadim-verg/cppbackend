#define _USE_MATH_DEFINES

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>

#include "../src/collision-detector.h"

using namespace collision_detector;

// Требуемая абсолютная погрешность 10^-10
constexpr double EPSILON = 1e-10;

namespace Catch {
template<>
struct StringMaker<GatheringEvent> {
    static std::string convert(GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(" << value.gatherer_id << "," << value.item_id << "," << value.sq_distance << "," << value.time << ")";
        return tmp.str();
    }
};
}  // namespace Catch

// Тестовый провайдер для передачи данных в FindGatherEvents
class MockItemGathererProvider : public ItemGathererProvider {
public:
    std::vector<Item> items;
    std::vector<Gatherer> gatherers;

    size_t ItemsCount() const override { return items.size(); }
    Item GetItem(size_t idx) const override { return items.at(idx); }

    size_t GatherersCount() const override { return gatherers.size(); }
    Gatherer GetGatherer(size_t idx) const override { return gatherers.at(idx); }
};

// Кастомный матчер Catch2 для посимвольного сравнения векторов событий с учётом погрешности
class GatheringEventsMatcher : public Catch::Matchers::MatcherGenericBase {
public:
    explicit GatheringEventsMatcher(std::vector<GatheringEvent> expected)
        : expected_(std::move(expected)) {}

    bool match(const std::vector<GatheringEvent>& actual) const {
        if (actual.size() != expected_.size()) {
            return false;
        }
        for (size_t i = 0; i < actual.size(); ++i) {
            if (actual[i].gatherer_id != expected_[i].gatherer_id ||
                actual[i].item_id != expected_[i].item_id ||
                std::abs(actual[i].sq_distance - expected_[i].sq_distance) > EPSILON ||
                std::abs(actual[i].time - expected_[i].time) > EPSILON) {
                return false;
            }
        }
        return true;
    }

    std::string describe() const override {
        std::ostringstream ss;
        ss << "Equals to expected events vector: {";
        for (const auto& ev : expected_) {
            ss << Catch::StringMaker<GatheringEvent>::convert(ev) << " ";
        }
        ss << "}";
        return ss.str();
    }

private:
    std::vector<GatheringEvent> expected_;
};

inline GatheringEventsMatcher EqualsEvents(std::vector<GatheringEvent> expected) {
    return GatheringEventsMatcher(std::move(expected));
}


TEST_CASE("Collision Detector Tests", "[collision_detector]") {
    
    SECTION("Empty provider returns no events") {
        MockItemGathererProvider provider;
        auto result = FindGatherEvents(provider);
        CHECK(result.empty());
    }

    SECTION("Gatherer doesn't move -> no collisions even if standing on item") {
        MockItemGathererProvider provider;
        provider.items = {
            Item{{0.0, 0.0}, 1.0}
        };
        provider.gatherers = {
            Gatherer{{0.0, 0.0}, {0.0, 0.0}, 1.0} // start_pos == end_pos
        };
        
        auto result = FindGatherEvents(provider);
        CHECK(result.empty());
    }

    SECTION("Gatherer moves and standard collision occurs with precise data") {
        MockItemGathererProvider provider;
        provider.items = {
            Item{{5.0, 0.0}, 1.0}
        };
        provider.gatherers = {
            Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}
        };

        auto result = FindGatherEvents(provider);
        
        std::vector<GatheringEvent> expected = {
            GatheringEvent{0, 0, 0.0, 0.5}
        };
        CHECK_THAT(result, EqualsEvents(expected));
    }

    SECTION("Events must be strictly sorted by chronological time") {
        MockItemGathererProvider provider;
        provider.items = {
            Item{{8.0, 0.0}, 1.0}, // Соберется вторым (time = 0.8)
            Item{{2.0, 0.0}, 1.0}  // Соберется первым (time = 0.2)
        };
        provider.gatherers = {
            Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}
        };

        auto result = FindGatherEvents(provider);
        
        std::vector<GatheringEvent> expected = {
            GatheringEvent{1, 0, 0.0, 0.2},
            GatheringEvent{0, 0, 0.0, 0.8}
        };
        CHECK_THAT(result, EqualsEvents(expected));
    }

    SECTION("Item is not deleted after collision (multiple gatherers gather same item)") {
        MockItemGathererProvider provider;
        provider.items = {
            Item{{5.0, 0.0}, 1.0}
        };
        provider.gatherers = {
            Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0},
            Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}
        };

        auto result = FindGatherEvents(provider);
        CHECK(result.size() == 2);
    }

    SECTION("Item is outside the movement segment boundaries (projection test)") {
        MockItemGathererProvider provider;
        provider.items = {
            Item{{-2.0, 0.0}, 1.0}, // Позади старта (time < 0)
            Item{{12.0, 0.0}, 1.0}  // Впереди финиша (time > 1)
        };
        provider.gatherers = {
            Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}
        };

        auto result = FindGatherEvents(provider);
        CHECK(result.empty());
    }

    SECTION("Edge case: precise touch on radius boundary") {
        MockItemGathererProvider provider;
        provider.items = {
            Item{{5.0, 2.0}, 1.0}
        };
        provider.gatherers = {
            Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}
        };

        auto result = FindGatherEvents(provider);
        
        std::vector<GatheringEvent> expected = {
            GatheringEvent{0, 0, 4.0, 0.5}
        };
        CHECK_THAT(result, EqualsEvents(expected));
    }

    SECTION("No collision when item is just outside the radius boundary") {
        MockItemGathererProvider provider;
        provider.items = {
            Item{{5.0, 2.00001}, 1.0}
        };
        provider.gatherers = {
            Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}
        };

        auto result = FindGatherEvents(provider);
        CHECK(result.empty());
    }
}