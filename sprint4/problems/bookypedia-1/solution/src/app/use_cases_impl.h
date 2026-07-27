#pragma once
#include "../domain/author_fwd.h"
#include "../domain/book.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : authors_{authors}
        , books_{books} {
    }

    void AddAuthor(const std::string& name) override;
    std::vector<domain::Author> GetAuthors() override;
    
    void AddBook(const std::string& title, const std::string& author_id, int publication_year) override;
    std::vector<domain::Book> GetBooks() override;
    std::vector<domain::Book> GetAuthorBooks(const std::string& author_id) override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app