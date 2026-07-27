#pragma once
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    UseCasesImpl() = default; // Больше не хранит фабрику внутри

    void AddAuthor(UnitOfWork& uow, const std::string& name) override;
    std::vector<domain::Author> GetAuthors(UnitOfWork& uow) override;
    std::optional<domain::Author> FindAuthorByName(UnitOfWork& uow, const std::string& name) override;
    bool DeleteAuthorById(UnitOfWork& uow, const std::string& author_id) override;
    bool DeleteAuthorByName(UnitOfWork& uow, const std::string& name) override;
    bool EditAuthorById(UnitOfWork& uow, const std::string& author_id, const std::string& new_name) override;
    bool EditAuthorByName(UnitOfWork& uow, const std::string& current_name, const std::string& new_name) override;

    void AddBookWithExistingAuthor(UnitOfWork& uow, const std::string& title, const std::string& author_id, int publication_year, const std::vector<std::string>& tags) override;
    void AddBookWithNewAuthor(UnitOfWork& uow, const std::string& title, const std::string& author_name, int publication_year, const std::vector<std::string>& tags) override;
    std::vector<domain::BookWithAuthor> GetBooksWithAuthors(UnitOfWork& uow) override;
    std::vector<domain::BookDetailed> FindDetailedBooks(UnitOfWork& uow, const std::string& title) override;
    std::vector<std::string> GetBookTags(UnitOfWork& uow, const std::string& book_id) override;
    bool DeleteBookById(UnitOfWork& uow, const std::string& book_id) override;
    bool UpdateBookFull(UnitOfWork& uow, const std::string& book_id, const std::string& title, int year, const std::vector<std::string>& tags) override;
};

}  // namespace app