#pragma once
#include <pqxx/pqxx>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <string>
#include <vector>

namespace postgres {

class ConnectionPool {
public:
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

    ConnectionPool(size_t capacity, std::string conn_str) {
        for (size_t i = 0; i < capacity; ++i) {
            pool_.emplace(std::make_shared<pqxx::connection>(conn_str));
        }
    }

    ConnectionPtr GetConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !pool_.empty(); });
        auto conn = pool_.front();
        pool_.pop();
        return conn;
    }

    void ReturnConnection(ConnectionPtr conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push(conn);
        cond_.notify_one();
    }

private:
    std::mutex mutex_;
    std::condition_variable cond_;
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
        pqxx::work w(*conn);
        w.exec(R"(
            CREATE TABLE IF NOT EXISTS retired_players (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                score INT NOT NULL,
                play_time DOUBLE PRECISION NOT NULL
            );
        )");
        w.exec(R"(
            CREATE INDEX IF NOT EXISTS idx_retired_players_sort
            ON retired_players (score DESC, play_time ASC, name ASC);
        )");
        w.commit();
        pool_->ReturnConnection(conn);
    }

    void SaveRecord(const std::string& name, int score, double play_time) {
        auto conn = pool_->GetConnection();
        pqxx::work w(*conn);
        w.exec_params(
            "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3);",
            name, score, play_time
            );
        w.commit();
        pool_->ReturnConnection(conn);
    }

    std::vector<Record> GetRecords(size_t start, size_t max_items) {
        auto conn = pool_->GetConnection();
        pqxx::read_work w(*conn);
        auto rows = w.exec_params(
            "SELECT name, score, play_time FROM retired_players "
            "ORDER BY score DESC, play_time ASC, name ASC LIMIT $1 OFFSET $2;",
            max_items, start
            );
        pool_->ReturnConnection(conn);

        std::vector<Record> res;
        for (const auto& row : rows) {
            res.push_back({
                row[0].as<std::string>(),
                row[1].as<int>(),
                row[2].as<double>()
            });
        }
        return res;
    }

private:
    std::shared_ptr<ConnectionPool> pool_;
};

} // namespace postgres
