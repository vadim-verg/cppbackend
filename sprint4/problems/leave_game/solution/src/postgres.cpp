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

    // ИСПРАВЛЕНО: Используем pqxx::work вместо устаревшего read_work
    pqxx::work tr(conn);

    auto rows = tr.exec_params(
        "SELECT name, score, play_time FROM retired_players ORDER BY score DESC, play_time ASC, name ASC LIMIT $1 OFFSET $2;",
        max_items, start
        );

    std::vector<model::RetiredDogInfo> result;
    result.reserve(rows.size());

    for (const auto& row : rows) {
        // ИСПРАВЛЕНО: Уходим от назначенного C++20 инициализатора к классическому созданию структуры
        model::RetiredDogInfo info;
        info.name = row[0].as<std::string>();
        info.score = row[1].as<int>();
        info.play_time = row[2].as<double>();

        result.push_back(std::move(info));
    }

    // Транзакции на чтение тоже коммитим, чтобы закрыть сессию в БД
    tr.commit();

    return result;
}

} // namespace database
