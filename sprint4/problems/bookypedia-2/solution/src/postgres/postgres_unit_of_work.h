#pragma once
#include "../app/unit_of_work.h"
#include "postgres.h"
#include <pqxx/pqxx>

namespace postgres {

class UnitOfWorkImpl : public app::UnitOfWork {
public:
    explicit UnitOfWorkImpl(pqxx::connection& connection)
        : tx_{connection}
        , authors_repo_{tx_}
        , books_repo_{tx_} {}

    domain::AuthorRepository& GetAuthors() override { return authors_repo_; }
    domain::BookRepository& GetBooks() override { return books_repo_; }
    void Commit() override { tx_.commit(); }

private:
    pqxx::work tx_;
    AuthorRepositoryImpl authors_repo_;
    BookRepositoryImpl books_repo_;
};

class UnitOfWorkFactoryImpl : public app::UnitOfWorkFactory {
public:
    explicit UnitOfWorkFactoryImpl(pqxx::connection& connection)
        : connection_{connection} {}

    std::unique_ptr<app::UnitOfWork> MakeUnitOfWork() override {
        return std::make_unique<UnitOfWorkImpl>(connection_);
    }

private:
    pqxx::connection& connection_;
};

}  // namespace postgres