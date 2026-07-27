#pragma once

#include <string>
#include <vector>
#include <optional>
#include "../domain/author_fwd.h"
#include "../domain/book.h"

namespace app {

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual std::vector<domain::Author> GetAuthors() = 0;
    virtual std::optional<domain::Author> FindAuthorByName(const std::string& name) = 0;
    virtual bool DeleteAuthorById(const std::string& author_id) = 0;
    virtual bool DeleteAuthorByName(const std::string& name) = 0;
    virtual bool EditAuthorById(const std::string& author_id, const std::string& new_name) = 0;
    virtual bool EditAuthorByName(const std::string& current_name, const std::string& new_name) = 0;

    virtual void AddBookWithExistingAuthor(const std::string& title, const std::string& author_id, int publication_year, const std::vector<std::string>& tags) = 0;
    virtual void AddBookWithNewAuthor(const std::string& title, const std::string& author_name, int publication_year, const std::vector<std::string>& tags) = 0;
    virtual std::vector<domain::BookWithAuthor> GetBooksWithAuthors() = 0;
    virtual std::vector<domain::BookDetailed> FindDetailedBooks(const std::string& title) = 0;
    virtual std::vector<std::string> GetBookTags(const std::string& book_id) = 0;
    virtual bool DeleteBookById(const std::string& book_id) = 0;
    virtual bool UpdateBookFull(const std::string& book_id, const std::string& title, int year, const std::vector<std::string>& tags) = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app