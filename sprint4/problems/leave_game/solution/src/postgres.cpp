#include "postgres.h"
#include <iostream>

namespace database {

PostgresDatabase::PostgresDatabase(const std::string& db_url)
    : connection_string_(db_url) {
}

std::unique_ptr<pqxx::connection> PostgresDatabase::GetConnection() {
    std::unique_lock<std::mutex> lock(pool_mutex_);

    if (pool_.empty() && created_connections_ < pool_capacity_) {
        try {
            auto conn = std::make_unique<pqxx::connection>(connection_string_);
            ++created_connections_;
            return conn;
        } catch (const std::exception& e) {
            std::cerr << "[DB Pool] Failed to create lazy connection: " << e.what() << std::endl;
        }
    }

    pool_cv_.wait(lock, [this] { return !pool_.empty(); });

    auto conn = std::move(pool_.front());
    pool_.pop();
    return conn;
}

void PostgresDatabase::ReturnConnection(std::unique_ptr<pqxx::connection> conn) {
    if (!conn) return;
    std::lock_guard<std::mutex> lock(pool_mutex_);
    pool_.push(std::move(conn));
    pool_cv_.notify_one();
}

void PostgresDatabase::EnsureTableExists(pqxx::work& tr) {
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

        auto rows = tr.exec_params(
            "SELECT name, score, play_time FROM retired_players ORDER BY score DESC, play_time ASC, name ASC LIMIT $1 OFFSET $2;",
            max_items, start
        );

        result.reserve(rows.size());
        for (const auto& row : rows) {
            model::RetiredDogInfo info;

            // Безопасное извлечение по индексам колонок
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
