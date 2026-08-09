#include "postgres.h"
#include <iostream>

namespace database {

void PostgresDatabase::EnsureTableExists(pqxx::work& tr) {
    // Выполняем создание идемпотентно. СУБД мгновенно пропустит это, если таблица уже есть
    tr.exec(R"(
        CREATE TABLE IF NOT EXISTS retired_players (
            id SERIAL PRIMARY KEY,
            name VARCHAR(100) NOT NULL,
            score INT NOT NULL,
            play_time DOUBLE PRECISION NOT NULL
        );
    )");

    tr.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_retired_players_sort
        ON retired_players (score DESC, play_time ASC, name ASC);
    )");
}

void PostgresDatabase::SaveRecord(const model::RetiredDogInfo& record) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        pqxx::connection conn(connection_string_);
        pqxx::work tr(conn);

        EnsureTableExists(tr);

        tr.exec_params(
            "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3);",
            record.name, record.score, record.play_time
            );
        tr.commit();
    } catch (const std::exception& e) {
        // Если база данных временно «моргнула», сервер НЕ упадет, а просто залогирует ошибку
        std::cerr << "[DB Error] Failed to save record: " << e.what() << std::endl;
    }
}

std::vector<model::RetiredDogInfo> PostgresDatabase::GetRecords(size_t start, size_t max_items) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<model::RetiredDogInfo> result;
    try {
        pqxx::connection conn(connection_string_);
        pqxx::work tr(conn);

        EnsureTableExists(tr);

        auto rows = tr.exec_params(
            "SELECT name, score, play_time FROM retired_players ORDER BY score DESC, play_time ASC, name ASC LIMIT $1 OFFSET $2;",
            max_items, start
            );

        result.reserve(rows.size());
        for (const auto& row : rows) {
            model::RetiredDogInfo info;
            info.name = row.as<std::string>();
            info.score = row.as<int>();
            info.play_time = row.as<double>();
            result.push_back(std::move(info));
        }
        tr.commit();
    } catch (const std::exception& e) {
        std::cerr << "[DB Error] Failed to get records: " << e.what() << std::endl;
    }
    return result;
}

} // namespace database
