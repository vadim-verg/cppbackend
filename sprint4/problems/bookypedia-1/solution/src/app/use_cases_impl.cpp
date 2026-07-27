#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"
#include <stdexcept>

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    if (name.empty()) {
        throw std::runtime_error("Failed to add author");
    }
    authors_.Save({AuthorId::New(), name});
}

std::vector<Author> UseCasesImpl::GetAuthors() {
    return authors_.GetSortedAuthors();
}

void UseCasesImpl::AddBook(const std::string& title, const std::string& author_id, int publication_year) {
    books_.Save({
        BookId::New(),
        AuthorId::FromString(author_id),
        title,
        publication_year
    });
}

std::vector<Book> UseCasesImpl::GetBooks() {
    return books_.GetSortedBooks();
}

std::vector<Book> UseCasesImpl::GetAuthorBooks(const std::string& author_id) {
    return books_.GetBooksByAuthor(AuthorId::FromString(author_id));
}

}  // namespace app