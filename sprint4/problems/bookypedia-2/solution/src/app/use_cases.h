#pragma once
#include <string>
#include <vector>
#include <optional>
#include "../domain/author_fwd.h"
#include "../domain/book.h"
#include "unit_of_work.h"

namespace app {

class UseCases {
public:
    virtual void AddAuthor(UnitOfWork& uow, const std::string& name) = 0;
    virtual std::vector<domain::Author> GetAuthors(UnitOfWork& uow) = 0;
    virtual std::optional<domain::Author> FindAuthorByName(UnitOfWork& uow, const std::string& name) = 0;
    virtual bool DeleteAuthorById(UnitOfWork& uow, const std::string& author_id) = 0;
    virtual bool DeleteAuthorByName(UnitOfWork& uow, const std::string& name) = 0;
    virtual bool EditAuthorById(UnitOfWork& uow, const std::string& author_id, const std::string& new_name) = 0;
    virtual bool EditAuthorByName(UnitOfWork& uow, const std::string& current_name, const std::string& new_name) = 0;

    virtual void AddBookWithExistingAuthor(UnitOfWork& uow, const std::string& title, const std::string& author_id, int publication_year, const std::vector<std::string>& tags) = 0;
    virtual void AddBookWithNewAuthor(UnitOfWork& uow, const std::string& title, const std::string& author_name, int publication_year, const std::vector<std::string>& tags) = 0;
    virtual std::vector<domain::BookWithAuthor> GetBooksWithAuthors(UnitOfWork& uow) = 0;
    virtual std::vector<domain::BookDetailed> FindDetailedBooks(UnitOfWork& uow, const std::string& title) = 0;
    virtual std::vector<std::string> GetBookTags(UnitOfWork& uow, const std::string& book_id) = 0;
    virtual bool DeleteBookById(UnitOfWork& uow, const std::string& book_id) = 0;
    virtual bool UpdateBookFull(UnitOfWork& uow, const std::string& book_id, const std::string& title, int year, const std::vector<std::string>& tags) = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app