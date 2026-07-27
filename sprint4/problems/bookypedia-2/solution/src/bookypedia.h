#pragma once
#include <pqxx/pqxx>

#include "app/use_cases_impl.h"
#include "postgres/postgres.h"
#include "postgres/postgres_unit_of_work.h" // Подключаем созданный Unit of Work

namespace bookypedia {

struct AppConfig {
    std::string db_url;
};

class Application {
public:
    explicit Application(const AppConfig& config);

    void Run();

private:
    postgres::Database db_;
    postgres::UnitOfWorkFactoryImpl uow_factory_{db_.GetConnection()};
    app::UseCasesImpl use_cases_{uow_factory_};
};

}  // namespace bookypedia