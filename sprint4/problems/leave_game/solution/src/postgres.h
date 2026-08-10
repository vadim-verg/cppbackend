#pragma once
#include <pqxx/pqxx>
#include <string>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace database {

// Умная обертка RAII для автоматического возврата соединения в пул
class ConnectionPool;

struct PlayerRecord {
    std::string name;
    int score = 0;
    double play_time = 0.0;
};

class ConnectionPtr {
public:
    ConnectionPtr(std::shared_ptr<pqxx::connection> conn, ConnectionPool& pool)
        : conn_(std::move(conn)), pool_(pool) {}

    ConnectionPtr(const ConnectionPtr&) = delete;
    ConnectionPtr& operator=(const ConnectionPtr&) = delete;

    ConnectionPtr(ConnectionPtr&&) = default;
    ConnectionPtr& operator=(ConnectionPtr&&) = default;

    ~ConnectionPtr();

    pqxx::connection& operator*() const noexcept { return *conn_; }
    pqxx::connection* operator->() const noexcept { return conn_.get(); }

private:
    std::shared_ptr<pqxx::connection> conn_;
    ConnectionPool& pool_;
};

// Потокобезопасный пул соединений
class ConnectionPool {
public:
    using PoolType = std::queue<std::shared_ptr<pqxx::connection>>;

    ConnectionPool(size_t capacity, std::string db_url);

    // Получить свободное соединение (блокирует поток, если все заняты)
    ConnectionPtr GetConnection();

    // Возвращает соединение обратно в очередь пула
    void ReturnConnection(std::shared_ptr<pqxx::connection> conn);

private:
    size_t capacity_;
    std::string db_url_;
    PoolType pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

// Класс управления базой данных
class Database {
public:
    explicit Database(const std::string& db_url, size_t pool_capacity);

    // Метод инициализации структуры (создание таблицы)
    void InitializeStructure();

    // Доступ к пулу соединений
    ConnectionPool& GetPool() { return pool_; }

    // Получить отсортированный список рекордов с пагинацией
    std::vector<PlayerRecord> GetRecords(int start, int max_items);

private:
    std::string db_url_;
    ConnectionPool pool_;
};

} // namespace database
