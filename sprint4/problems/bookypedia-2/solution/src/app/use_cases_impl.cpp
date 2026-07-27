#include "use_cases_impl.h"
#include "../domain/author.h"
#include "../domain/book.h"
#include <stdexcept>

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(UnitOfWork& uow, const std::string& name) {
    if (name.empty()) throw std::runtime_error("Failed to add author");
    uow.GetAuthors().Save({AuthorId::New(), name});
}

std::vector<Author> UseCasesImpl::GetAuthors(UnitOfWork& uow) {
    return uow.GetAuthors().GetSortedAuthors();
}

std::optional<Author> UseCasesImpl::FindAuthorByName(UnitOfWork& uow, const std::string& name) {
    return uow.GetAuthors().FindByName(name);
}

bool UseCasesImpl::DeleteAuthorById(UnitOfWork& uow, const std::string& author_id) {
    return uow.GetAuthors().Delete(AuthorId::FromString(author_id));
}

bool UseCasesImpl::DeleteAuthorByName(UnitOfWork& uow, const std::string& name) {
    auto author = uow.GetAuthors().FindByName(name);
    if (!author) return false;
    return uow.GetAuthors().Delete(author->GetId());
}

bool UseCasesImpl::EditAuthorById(UnitOfWork& uow, const std::string& author_id, const std::string& new_name) {
    return uow.GetAuthors().Edit(AuthorId::FromString(author_id), new_name);
}

bool UseCasesImpl::EditAuthorByName(UnitOfWork& uow, const std::string& current_name, const std::string& new_name) {
    auto author = uow.GetAuthors().FindByName(current_name);
    if (!author) return false;
    return uow.GetAuthors().Edit(author->GetId(), new_name);
}

void UseCasesImpl::AddBookWithExistingAuthor(UnitOfWork& uow, const std::string& title, const std::string& author_id, int publication_year, const std::vector<std::string>& tags) {
    auto book_id = BookId::New();
    uow.GetBooks().Save({book_id, AuthorId::FromString(author_id), title, publication_year});
    for (const auto& tag : tags) {
        uow.GetBooks().AddTag(book_id.ToString(), tag);
    }
}

void UseCasesImpl::AddBookWithNewAuthor(UnitOfWork& uow, const std::string& title, const std::string& author_name, int publication_year, const std::vector<std::string>& tags) {
    auto author_id = AuthorId::New();
    uow.GetAuthors().Save({author_id, author_name});
    auto book_id = BookId::New();
    uow.GetBooks().Save({book_id, author_id, title, publication_year});
    for (const auto& tag : tags) {
        uow.GetBooks().AddTag(book_id.ToString(), tag);
    }
}

std::vector<BookWithAuthor> UseCasesImpl::GetBooksWithAuthors(UnitOfWork& uow) {
    return uow.GetBooks().GetSortedBooksWithAuthors();
}

std::vector<BookDetailed> UseCasesImpl::FindDetailedBooks(UnitOfWork& uow, const std::string& title) {
    return uow.GetBooks().FindDetailedBooks(title);
}

std::vector<std::string> UseCasesImpl::GetBookTags(UnitOfWork& uow, const std::string& book_id) {
    return uow.GetBooks().GetBookTags(book_id);
}

bool UseCasesImpl::DeleteBookById(UnitOfWork& uow, const std::string& book_id) {
    return uow.GetBooks().DeleteBook(book_id);
}

bool UseCasesImpl::UpdateBookFull(UnitOfWork& uow, const std::string& book_id, const std::string& title, int year, const std::vector<std::string>& tags) {
    if (!uow.GetBooks().EditBook(book_id, title, year)) return false;
    uow.GetBooks().ClearBookTags(book_id);
    for (const auto& tag : tags) {
        uow.GetBooks().AddTag(book_id, tag);
    }
    return true;
}

}  // namespace app