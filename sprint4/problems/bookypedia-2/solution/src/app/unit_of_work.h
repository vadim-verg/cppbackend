#pragma once
#include <memory>
#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

class UnitOfWork {
public:
    virtual ~UnitOfWork() = default;
    virtual domain::AuthorRepository& GetAuthors() = 0;
    virtual domain::BookRepository& GetBooks() = 0;
    virtual void Commit() = 0;
};

class UnitOfWorkFactory {
public:
    virtual ~UnitOfWorkFactory() = default;
    virtual std::unique_ptr<UnitOfWork> MakeUnitOfWork() = 0;
};

}  // namespace app