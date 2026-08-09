#include "postgres.h"

namespace database {

void PostgresDatabase::SaveRecord(const model::RetiredDogInfo& record) {
    pqxx::connection conn(connection_string_);
    pqxx::work tr(conn);

    tr.exec_params(
        "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3);",
        record.name, record.score, record.play_time
        );

    tr.commit();
}

std::vector<model::RetiredDogInfo> PostgresDatabase::GetRecords(size_t start, size_t max_items) {
    pqxx::connection conn(connection_string_);
    pqxx::read_work tr(conn);

    // Сортировка выполняется на стороне БД, обеспечивая максимальное быстродействие
    auto rows = tr.exec_params(
        "SELECT name, score, play_time FROM retired_players ORDER BY score DESC, play_time ASC, name ASC LIMIT $1 OFFSET $2;",
        max_items, start
        );

    std::vector<model::RetiredDogInfo> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.push_back(model::RetiredDogInfo{
            .name = row[0].as<std::string>(),
            .score = row[1].as<int>(),
            .play_time = row[2].as<double>()
        });
    }
    return result;
}

} // namespace database
