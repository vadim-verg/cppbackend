#include "postgres.h"
#include <iostream>

namespace database {

// ИСПРАВЛЕНО: Сигнатура теперь строго совпадает с объявлением в заголовочном файле
PostgresDatabase::PostgresDatabase(const std::string& db_url)
    : connection_string_(db_url)
    , pool_(10, db_url) {
    Init();
}

void PostgresDatabase::Init() {
    try {
        auto conn = pool_.GetConnection();
        pqxx::work tr(*conn);

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

        tr.commit();
        pool_.ReturnConnection(conn);
    } catch (const std::exception& e) {
        std::cerr << "Database initialization failed: " << e.what() << std::endl;
        throw;
    }
}

void PostgresDatabase::SaveRecord(const model::RetiredDogInfo& record) {
    auto conn = pool_.GetConnection();
    try {
        pqxx::work tr(*conn);
        tr.exec_params(
            "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3);",
            record.name, record.score, record.play_time
            );
        tr.commit();
    } catch (const std::exception& e) {
        std::cerr << "Failed to save record: " << e.what() << std::endl;
    }
    pool_.ReturnConnection(conn);
}

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
