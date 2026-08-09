#include "postgres.h"
#include <iostream>

namespace database {

// ... методы PostgresDatabase::PostgresDatabase, Init и SaveRecord остаются без изменений ...

std::vector<model::RetiredDogInfo> PostgresDatabase::GetRecords(size_t start, size_t max_items) {
    auto conn = pool_.GetConnection();
    std::vector<model::RetiredDogInfo> result;
    try {
        pqxx::work tr(*conn);
        auto rows = tr.exec_params(
            "SELECT name, score, play_time FROM retired_players ORDER BY score DESC, play_time ASC, name ASC LIMIT $1 OFFSET $2;",
            max_items, start
            );

        result.reserve(rows.size());
        for (const auto& row : rows) {
            model::RetiredDogInfo info;

            // ИСПРАВЛЕНО: Безопасно извлекаем значения из каждой ячейки строки по индексу
            info.name = row[0].as<std::string>();
            info.score = row[1].as<int>();
            info.play_time = row[2].as<double>();

            result.push_back(std::move(info));
        }
        tr.commit();
    } catch (const std::exception& e) {
        std::cerr << "Failed to get records: " << e.what() << std::endl;
    }
    pool_.ReturnConnection(conn);
    return result;
}

} // namespace database
