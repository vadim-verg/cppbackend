#pragma once
#include "../domain/author_fwd.h"
#include "../domain/book.h"
#include "use_cases.h"
#include "unit_of_work.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(UnitOfWorkFactory& uow_factory)
        : uow_factory_{uow_factory} {
    }

    void AddAuthor(const std::string& name) override;
    std::vector<domain::Author> GetAuthors() override;
    std::optional<domain::Author> FindAuthorByName(const std::string& name) override;
    bool DeleteAuthorById(const std::string& author_id) override;
    bool DeleteAuthorByName(const std::string& name) override;
    bool EditAuthorById(const std::string& author_id, const std::string& new_name) override;
    bool EditAuthorByName(const std::string& current_name, const std::string& new_name) override;

    void AddBookWithExistingAuthor(const std::string& title, const std::string& author_id, int publication_year, const std::vector<std::string>& tags) override;
    void AddBookWithNewAuthor(const std::string& title, const std::string& author_name, int publication_year, const std::vector<std::string>& tags) override;
    std::vector<domain::BookWithAuthor> GetBooksWithAuthors() override;
    std::vector<domain::BookDetailed> FindDetailedBooks(const std::string& title) override;
    std::vector<std::string> GetBookTags(const std::string& book_id) override;
    bool DeleteBookById(const std::string& book_id) override;
    bool UpdateBookFull(const std::string& book_id, const std::string& title, int year, const std::vector<std::string>& tags) override;

private:
    UnitOfWorkFactory& uow_factory_;
};

}  // namespace app