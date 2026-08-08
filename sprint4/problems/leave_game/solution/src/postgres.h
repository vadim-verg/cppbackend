#pragma once
#include <pqxx/pqxx>
#include <mutex>
#include <queue>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

namespace postgres {

class ConnectionPool {
public:
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

    ConnectionPool(size_t capacity, std::string conn_str)
        : conn_str_(std::move(conn_str))
    {
        for (size_t i = 0; i < capacity; ++i) {
            try {
                pool_.emplace(std::make_shared<pqxx::connection>(conn_str_));
            } catch (...) {
                // Если база не успела подняться, создадим соединение на лету в GetConnection
            }
        }
    }

    ConnectionPtr GetConnection() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pool_.empty()) {
            auto conn = pool_.front();
            pool_.pop();
            return conn;
        }
        try {
            return std::make_shared<pqxx::connection>(conn_str_);
        } catch (...) {
            return nullptr;
        }
    }

    void ReturnConnection(ConnectionPtr conn) {
        if (!conn) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.size() < 20) { // Ограничиваем пул сверху во избежание утечки дескрипторов
            pool_.push(conn);
        }
    }

private:
    std::string conn_str_;
    std::mutex mutex_;
    std::queue<ConnectionPtr> pool_;
};

struct Record {
    std::string name;
    int score;
    double play_time;
};

class RecordsRepository {
public:
    explicit RecordsRepository(std::shared_ptr<ConnectionPool> pool) : pool_(pool) {
        auto conn = pool_->GetConnection();
        if (!conn) {
            throw std::runtime_error("Failed to connect to DB during repository init");
        }

        try {
            // Транзакция 1: Создаем таблицу
            {
                pqxx::work w(*conn);
                w.exec(R"(
                    CREATE TABLE IF NOT EXISTS retired_players (
                        id SERIAL PRIMARY KEY,
                        name VARCHAR(100) NOT NULL,
                        score INT NOT NULL,
                        play_time DOUBLE PRECISION NOT NULL
                    );
                )");
                w.commit();
            }

            // Транзакция 2: Отдельно создаем индекс, чтобы избежать дедлоков каталога СУБД при автотестах
            {
                pqxx::work w(*conn);
                w.exec(R"(
                    CREATE INDEX IF NOT EXISTS idx_retired_players_sort
                    ON retired_players (score DESC, play_time ASC, name ASC);
                )");
                w.commit();
            }
        } catch (...) {
            pool_->ReturnConnection(conn);
            throw;
        }
        pool_->ReturnConnection(conn);
    }

    void SaveRecord(const std::string& name, int score, double play_time) {
        auto conn = pool_->GetConnection();
        if (!conn) return;
        try {
            pqxx::work w(*conn);
            w.exec_params(
                "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3);",
                name, score, play_time
                );
            w.commit();
        } catch (...) {}
        pool_->ReturnConnection(conn);
    }

    std::vector<Record> GetRecords(size_t start, size_t max_items) {
        std::vector<Record> res;
        auto conn = pool_->GetConnection();
        if (!conn) return res;
        try {
            pqxx::work w(*conn);
            // СТРОГОЕ ИСПРАВЛЕНИЕ ТИПОВ: приводим size_t к int,
            // иначе под Linux exec_params ломает биндинг LIMIT/OFFSET
            pqxx::result rows = w.exec_params(
                "SELECT name, score, play_time FROM retired_players "
                "ORDER BY score DESC, play_time ASC, name ASC LIMIT $1 OFFSET $2;",
                static_cast<int>(max_items), static_cast<int>(start)
                );
            w.commit();
            for (const auto& row : rows) {
                std::string name = row["name"].as<std::string>();
                int score = row["score"].as<int>();
                double play_time = row["play_time"].as<double>();
                res.push_back({name, score, play_time});
            }
        } catch (...) {}
        pool_->ReturnConnection(conn);
        return res;
    }

private:
    std::shared_ptr<ConnectionPool> pool_;
};

} // namespace postgres
