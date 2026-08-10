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

class PostgresDatabase {
public:
    // Умный указатель, который автоматически возвращает соединение в пул при уничтожении
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

    explicit PostgresDatabase(const std::string& db_url)
        : connection_string_(db_url) {}

    ~PostgresDatabase() = default;

    // Основные методы работы с рекордами
    void SaveRecord(const model::RetiredDogInfo& record);
    std::vector<model::RetiredDogInfo> GetRecords(size_t start, size_t max_items);

    // Метод получения безопасного соединения (вынесен в public для возможности использования в других слоях)
    ConnectionPtr GetConnection();

private:
    std::string connection_string_;

    std::mutex pool_mutex_;
    std::condition_variable pool_cv_;
    std::queue<std::unique_ptr<pqxx::connection>> pool_;
    size_t pool_capacity_ = 50; // Увеличено до 50 для выдерживания массовых тестов на 150 игроков
    size_t created_connections_ = 0;

    // Внутренний метод возврата сырого указателя в пул
    void ReturnConnection(std::unique_ptr<pqxx::connection> conn);

    // Метод инициализации схемы БД
    void EnsureTableExists(pqxx::work& tr);
};

} // namespace database
