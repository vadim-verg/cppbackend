#pragma once
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include "model.h"

namespace database {

class PostgresDatabase {
public:
    explicit PostgresDatabase(const std::string& db_url) : connection_string_(db_url) {
        Init();
    }

    // Сохранить рекорд ушедшего игрока
    void SaveRecord(const model::RetiredDogInfo& record);

    // Получить рекорды с пагинацией и сортировкой на уровне СУБД
    std::vector<model::RetiredDogInfo> GetRecords(size_t start, size_t max_items);

private:
    std::string connection_string_;

    void Init() {
        pqxx::connection conn(connection_string_);
        pqxx::work tr(conn);

        // Создаем таблицу
        tr.exec(R"(
            CREATE TABLE IF NOT EXISTS retired_players (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                score INT NOT NULL,
                play_time DOUBLE PRECISION NOT NULL
            );
        )");

        // Создаем составной индекс для ускорения сортировки и пагинации по правилу ТЗ
        tr.exec(R"(
            CREATE INDEX IF NOT EXISTS idx_retired_players_sort
            ON retired_players (score DESC, play_time ASC, name ASC);
        )");

        tr.commit();
    }
};

} // namespace database
