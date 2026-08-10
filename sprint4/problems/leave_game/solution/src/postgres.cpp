#include "postgres.h"
#include <iostream>

namespace database {

// --- ConnectionPtr Реализация ---
ConnectionPtr::~ConnectionPtr() {
    if (conn_) {
        pool_.ReturnConnection(std::move(conn_));
    }
}

// --- ConnectionPool Реализация ---
ConnectionPool::ConnectionPool(size_t capacity, std::string db_url)
    : capacity_(capacity)
{
    std::string final_url = std::move(db_url);
    if (final_url.find("postgres://") == 0) {
        final_url.insert(8, "ql");
    }
    this->db_url_ = final_url;

    // Пытаемся заполнить пул. Если база «лежит», не падаем, а попробуем открывать лениво
    try {
        for (size_t i = 0; i < capacity_; ++i) {
            pool_.push(std::make_shared<pqxx::connection>(this->db_url_));
        }
    } catch (const std::exception& e) {
        std::cerr << "[Pool Warning] Could not pre-warm DB connections: " << e.what() << std::endl;
        // Оставляем пул пустым, он наполнится лениво при вызовах, если база оживет
    }
}

ConnectionPtr ConnectionPool::GetConnection() {
    std::unique_lock<std::mutex> lock(mutex_);

    // Если на старте база лежала и пул пуст — пробуем создать коннект прямо сейчас
    if (pool_.empty()) {
        try {
            return ConnectionPtr(std::make_shared<pqxx::connection>(db_url_), *this);
        } catch (...) {
            // Если и сейчас не вышло — ждем по cv, как обычно
        }
    }

    cv.wait(lock, [this] { return !pool_.empty(); });

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

// --- Database Реализация ---
Database::Database(const std::string& db_url, size_t pool_capacity)
    : db_url_(db_url), pool_(pool_capacity, db_url)
{
}

void Database::InitializeStructure() {
    // Получаем безопасное соединение из только что созданного пула
    auto conn_ptr = pool_.GetConnection();
    pqxx::work tx(*conn_ptr);

    // Создаем таблицу
    tx.exec(R"(
        CREATE TABLE IF NOT EXISTS retired_players (
            id SERIAL PRIMARY KEY,
            name VARCHAR(100) NOT NULL,
            score INT NOT NULL,
            play_time DOUBLE PRECISION NOT NULL
        );
    )");

    // Создаем составной индекс
    tx.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_retired_players_score_time_name
        ON retired_players (score DESC, play_time ASC, name ASC);
    )");

    tx.commit();
}

std::vector<PlayerRecord> Database::GetRecords(int start, int max_items) {
    auto conn_ptr = pool_.GetConnection();
    pqxx::read_transaction tx(*conn_ptr);

    // SQL-запрос с сортировкой по ТЗ и лимитами
    std::string query = R"(
        SELECT name, score, play_time
        FROM retired_players
        ORDER BY score DESC, play_time ASC, name ASC
        LIMIT )" + tx.quote(max_items) + " OFFSET " + tx.quote(start) + ";";

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
}

} // namespace database
