#pragma once
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <iostream>
#include "model.h"

namespace database {

class PostgresDatabase {
public:
    // Конструктор пишем прямо внутри заголовочного файла (inline), чтобы сбросить кэш
    explicit PostgresDatabase(const std::string& db_url)
        : connection_string_(db_url) {}

    ~PostgresDatabase() = default;

    void SaveRecord(const model::RetiredDogInfo& record);
    std::vector<model::RetiredDogInfo> GetRecords(size_t start, size_t max_items);

private:
    std::string connection_string_;

    // Пул соединений полностью инкапсулирован внутри хедера
    std::mutex pool_mutex_;
    std::condition_variable pool_cv_;
    std::queue<std::unique_ptr<pqxx::connection>> pool_;
    size_t pool_capacity_ = 10;
    size_t created_connections_ = 0;

    // Методы пула делаем инлайн, чтобы компилятор собрал их мгновенно без рассинхронизации
    std::unique_ptr<pqxx::connection> GetConnection() {
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

    void ReturnConnection(std::unique_ptr<pqxx::connection> conn) {
        if (!conn) return;
        std::lock_guard<std::mutex> lock(pool_mutex_);
        pool_.push(std::move(conn));
        pool_cv_.notify_one();
    }

    void EnsureTableExists(pqxx::work& tr) {
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
};

} // namespace database
