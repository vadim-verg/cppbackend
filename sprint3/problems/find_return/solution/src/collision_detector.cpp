#include "collision_detector.h"
#include <cassert>
#include <algorithm>

namespace collision_detector {

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    // Проверим, что перемещение ненулевое.
    assert(b.x != a.x || b.y != a.y);
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult{sq_distance, proj_ratio};
}

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> events;

    const size_t gatherers_count = provider.GatherersCount();
    const size_t items_count = provider.ItemsCount();

    if (gatherers_count == 0 || items_count == 0) {
        return events;
    }

    for (size_t g_idx = 0; g_idx < gatherers_count; ++g_idx) {
        const Gatherer gatherer = provider.GetGatherer(g_idx);

        // Если собиратель не переместился, он не совершает столкновений (согласно ТЗ)
        if (gatherer.start_pos.x == gatherer.end_pos.x && gatherer.start_pos.y == gatherer.end_pos.y) {
            continue;
        }

        const double collect_radius = gatherer.width + 0.0; // Инициализация базового радиуса

        for (size_t i_idx = 0; i_idx < items_count; ++i_idx) {
            const Item item = provider.GetItem(i_idx);

            // Вычисляем геометрию столкновения
            CollectionResult res = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);

            // Радиус сбора — это сумма радиусов собирателя и предмета
            double total_radius = gatherer.width + item.width;

            // Проверяем, произошел ли сбор объекта
            if (res.IsCollected(total_radius)) {
                events.push_back(GatheringEvent{
                    .item_id = i_idx,
                    .gatherer_id = g_idx,
                    .sq_distance = res.sq_distance,
                    .time = res.proj_ratio
                });
            }
        }
    }

    // Хронологическая сортировка по времени столкновения (time)
    std::sort(events.begin(), events.end(), [](const GatheringEvent& lhs, const GatheringEvent& rhs) {
        return lhs.time < rhs.time;
    });

    return events;
}

}  // namespace collision_detector
