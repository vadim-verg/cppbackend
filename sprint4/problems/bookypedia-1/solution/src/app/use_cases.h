#pragma once

#include <string>
#include <vector>
#include "../domain/author_fwd.h"
#include "../domain/book.h"

namespace app {

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual std::vector<domain::Author> GetAuthors() = 0;
    
    virtual void AddBook(const std::string& title, const std::string& author_id, int publication_year) = 0;
    virtual std::vector<domain::Book> GetBooks() = 0;
    virtual std::vector<domain::Book> GetAuthorBooks(const std::string& author_id) = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app