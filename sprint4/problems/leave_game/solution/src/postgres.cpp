#include "postgres.h"
#include <iostream>

namespace database {

void PostgresDatabase::SaveRecord(const model::RetiredDogInfo& record) {
    auto conn = GetConnection();
    if (!conn) return;

    try {
        pqxx::work tr(*conn);
        EnsureTableExists(tr);

        tr.exec_params(
            "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3);",
            record.name, record.score, record.play_time
            );
        tr.commit();
    } catch (const std::exception& e) {
        std::cerr << "[DB Error] SaveRecord failed: " << e.what() << std::endl;
    }

    ReturnConnection(std::move(conn));
}

std::vector<model::RetiredDogInfo> PostgresDatabase::GetRecords(size_t start, size_t max_items) {
    std::vector<model::RetiredDogInfo> result;
    auto conn = GetConnection();
    if (!conn) return result;

    try {
        pqxx::work tr(*conn);
        EnsureTableExists(tr);

        // Извлекаем поля: name, score, play_time
        auto rows = tr.exec_params(
            "SELECT name, score, play_time FROM retired_players ORDER BY score DESC, play_time ASC, name ASC LIMIT $1 OFFSET $2;",
            max_items, start
            );

        result.reserve(rows.size());
        for (const auto& row : rows) {
            model::RetiredDogInfo info;
            info.name = row[0].as<std::string>();
            info.score = row[1].as<int>();
            info.play_time = row[2].as<double>();
            result.push_back(std::move(info));
        }
        tr.commit();
    } catch (const std::exception& e) {
        std::cerr << "[DB Error] GetRecords failed: " << e.what() << std::endl;
    }

    ReturnConnection(std::move(conn));
    return result;
}

} // namespace database
