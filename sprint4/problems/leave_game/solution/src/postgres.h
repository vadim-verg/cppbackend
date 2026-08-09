#pragma once
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "model.h"

namespace database {

class ConnectionPool {
public:
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

    ConnectionPool(size_t capacity, const std::string& connection_string) {
        for (size_t i = 0; i < capacity; ++i) {
            pool_.emplace(std::make_shared<pqxx::connection>(connection_string));
        }
    }

    ConnectionPtr GetConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !pool_.empty(); });
        auto conn = pool_.front();
        pool_.pop();
        return conn;
    }

    void ReturnConnection(ConnectionPtr conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push(std::move(conn));
        cond_var_.notify_one();
    }

private:
    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::queue<ConnectionPtr> pool_;
};

class PostgresDatabase {
public:
    // Сигнатура строго с const std::string&
    explicit PostgresDatabase(const std::string& db_url);

    void SaveRecord(const model::RetiredDogInfo& record);
    std::vector<model::RetiredDogInfo> GetRecords(size_t start, size_t max_items);

private:
    std::string connection_string_;
    ConnectionPool pool_;

    void Init();
};

} // namespace database
