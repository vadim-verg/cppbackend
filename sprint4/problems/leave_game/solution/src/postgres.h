#pragma once
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "model.h"

namespace database {

class PostgresDatabase {
public:
    explicit PostgresDatabase(const std::string& db_url)
        : connection_string_(db_url) {}

    // Убираем сложный пул на старте, открываем соединение безопасно по требованию
    void SaveRecord(const model::RetiredDogInfo& record);
    std::vector<model::RetiredDogInfo> GetRecords(size_t start, size_t max_items);

private:
    std::string connection_string_;
    std::mutex db_mutex_; // Защищаем доступ к СУБД от параллельных потоков io_context

    // Метод автоматического создания таблицы и индексов при каждом запросе
    void EnsureTableExists(pqxx::work& tr);
};

} // namespace database
