#include "postgres.h"
#include <iostream>

namespace database {

PostgresDatabase::ConnectionPtr PostgresDatabase::GetConnection() {
    std::unique_lock<std::mutex> lock(pool_mutex_);

    // Исправлено условие ожидания: поток засыпает только если пул пуст И лимит соединений уже исчерпан
    pool_cv_.wait(lock, [this] {
        return !pool_.empty() || created_connections_ < pool_capacity_;
    });

    std::unique_ptr<pqxx::connection> conn;

    if (!pool_.empty()) {
        conn = std::move(pool_.front());
        pool_.pop();
    } else {
        try {
            conn = std::make_unique<pqxx::connection>(connection_string_);
            ++created_connections_;
        } catch (const std::exception& e) {
            std::cerr << "[DB Connection Error]: " << e.what() << std::endl;
            throw; // Пробрасываем ошибку, чтобы сервер выдал 500, а не завис
        }
    }

    // Возвращаем shared_ptr. Лямбда-делетер перехватывает управление при уничтожении
    return ConnectionPtr(conn.release(), [this](pqxx::connection* p) {
        std::unique_ptr<pqxx::connection> to_return(p);
        ReturnConnection(std::move(to_return));
    });
}

void PostgresDatabase::ReturnConnection(std::unique_ptr<pqxx::connection> conn) {
    if (!conn) return;
    std::lock_guard<std::mutex> lock(pool_mutex_);
    pool_.push(std::move(conn));
    pool_cv_.notify_one(); // Пробуждаем один из ждущих потоков
}

void PostgresDatabase::SaveRecord(const model::RetiredDogInfo& record) {
    // Автоматическое получение соединения
    auto conn = GetConnection();

    try {
        pqxx::work tr(*conn);
        EnsureTableExists(tr);

        tr.exec_params(
            "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3);",
            record.name, record.score, record.play_time
            );
        tr.commit();
    } catch (const std::exception& e) {
        std::cerr << "[DB Error] SaveRecord failed: " << e.what() << std::endl;
    }
    // Коннект автоматически возвращается в пул при выходе из области видимости функции
}

std::vector<model::RetiredDogInfo> PostgresDatabase::GetRecords(size_t start, size_t max_items) {
    std::vector<model::RetiredDogInfo> result;
    auto conn = GetConnection();

    try {
        pqxx::work tr(*conn);
        EnsureTableExists(tr);

        // Используем явное приведение параметров string для exec_params
        auto rows = tr.exec_params(
            "SELECT name, score, play_time FROM retired_players ORDER BY score DESC, play_time ASC, name ASC LIMIT $1 OFFSET $2;",
            std::to_string(max_items), std::to_string(start)
            );

        result.reserve(rows.size());
        for (const auto& row : rows) {
            model::RetiredDogInfo info;
            info.name = row[0].as<std::string>();
            info.score = row[1].as<int>();
            info.play_time = row[2].as<double>();
            result.push_back(std::move(info));
        }
        tr.commit();
    } catch (const std::exception& e) {
        std::cerr << "[DB Error] GetRecords failed: " << e.what() << std::endl;
    }

    return result;
}

void PostgresDatabase::EnsureTableExists(pqxx::work& tr) {
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

} // namespace database
