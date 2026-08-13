#include "postgres.h"
#include <iostream>

namespace database {

ConnectionPtr::~ConnectionPtr() {
    if (conn_) {
        pool_.ReturnConnection(std::move(conn_));
    }
}

ConnectionPool::ConnectionPool(size_t capacity, std::string db_url)
    : capacity_(capacity)
{
    if (db_url.find("postgres://") == 0) {
        db_url.insert(8, "ql");
    }
    db_url_ = std::move(db_url);
}

ConnectionPtr ConnectionPool::GetConnection() {
    std::unique_lock<std::mutex> lock(mutex_);

    // Ожидаем, пока либо появится готовое соединение, либо у нас будет квота на создание нового
    cv_.wait(lock, [this] {
        return !pool_.empty() || created_connections_ < capacity_;
    });

    // Если очередь пуста, но мы не превысили лимит — создаем новое соединение лениво
    if (pool_.empty()) {
        try {
            auto new_conn = std::make_shared<pqxx::connection>(db_url_);
            ++created_connections_; // Запоминаем, что создали еще одно соединение в рамках лимита
            return ConnectionPtr(std::move(new_conn), *this);
        } catch (...) {
            // Если упало при подключении, не блокируем пул для других потоков
            cv_.notify_one();
            throw;
        }
    }

    // Если в очереди уже было готовое соединение, забираем его
    auto conn = pool_.front();
    pool_.pop();
    return ConnectionPtr(std::move(conn), *this);
}

void ConnectionPool::ReturnConnection(std::shared_ptr<pqxx::connection> conn) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push(std::move(conn));
    }
    cv_.notify_one();
}

Database::Database(const std::string& db_url, size_t pool_capacity)
    : db_url_(db_url), pool_(pool_capacity, db_url)
{
}

void Database::InitializeStructure() {
    for (int attempt = 0; attempt < 20; ++attempt) {
        try {
            auto conn_ptr = pool_.GetConnection();
            pqxx::work tx(*conn_ptr);
            tx.exec(R"(
                CREATE TABLE IF NOT EXISTS retired_players (
                    id SERIAL PRIMARY KEY,
                    name VARCHAR(100) NOT NULL,
                    score INT NOT NULL,
                    play_time DOUBLE PRECISION NOT NULL
                );
            )");

            tx.exec(R"(
                CREATE INDEX IF NOT EXISTS idx_retired_players_score_time_name
                ON retired_players (score DESC, play_time ASC, name ASC);
            )");

            tx.commit();
            return;
        } catch (const std::exception& ex) {
            if (attempt == 19) {
                throw;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
}

std::vector<PlayerRecord> Database::GetRecords(int start, int max_items) {

    try {
        auto conn_ptr = pool_.GetConnection();
        pqxx::read_transaction tx(*conn_ptr);

        std::string query = "SELECT name, score, play_time FROM retired_players ORDER BY score DESC, play_time ASC, name ASC LIMIT "
                            + tx.quote(max_items) + " OFFSET " + tx.quote(start) + ";";

        pqxx::result res = tx.exec(query);

        std::vector<PlayerRecord> records;
        records.reserve(res.size());

        for (const auto& row : res) {
            records.push_back({
                row["name"].as<std::string>(),
                row["score"].as<int>(),
                row["play_time"].as<double>()
            });
        }

        return records;
    } catch (const std::exception& ex) {
        std::cerr << "[DB Error] Failed to fetch records: " << ex.what() << std::endl;
        return {};
    }
}

} // namespace database
