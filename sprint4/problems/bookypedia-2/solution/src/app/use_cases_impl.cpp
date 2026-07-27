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
    auto uow = uow_factory_.MakeUnitOfWork();
    uow->GetAuthors().Save({AuthorId::New(), name});
    uow->Commit();
}

std::vector<Author> UseCasesImpl::GetAuthors() {
    auto uow = uow_factory_.MakeUnitOfWork();
    auto result = uow->GetAuthors().GetSortedAuthors();
    uow->Commit();
    return result;
}

std::optional<Author> UseCasesImpl::FindAuthorByName(const std::string& name) {
    auto uow = uow_factory_.MakeUnitOfWork();
    auto result = uow->GetAuthors().FindByName(name);
    uow->Commit();
    return result;
}

bool UseCasesImpl::DeleteAuthorById(const std::string& author_id) {
    auto uow = uow_factory_.MakeUnitOfWork();
    bool res = uow->GetAuthors().Delete(AuthorId::FromString(author_id));
    uow->Commit();
    return res;
}

bool UseCasesImpl::DeleteAuthorByName(const std::string& name) {
    auto uow = uow_factory_.MakeUnitOfWork();
    auto author = uow->GetAuthors().FindByName(name);
    if (!author) return false;
    bool res = uow->GetAuthors().Delete(author->GetId());
    uow->Commit();
    return res;
}

bool UseCasesImpl::EditAuthorById(const std::string& author_id, const std::string& new_name) {
    auto uow = uow_factory_.MakeUnitOfWork();
    bool res = uow->GetAuthors().Edit(AuthorId::FromString(author_id), new_name);
    uow->Commit();
    return res;
}

bool UseCasesImpl::EditAuthorByName(const std::string& current_name, const std::string& new_name) {
    auto uow = uow_factory_.MakeUnitOfWork();
    auto author = uow->GetAuthors().FindByName(current_name);
    if (!author) return false;
    bool res = uow->GetAuthors().Edit(author->GetId(), new_name);
    uow->Commit();
    return res;
}

void UseCasesImpl::AddBookWithExistingAuthor(const std::string& title, const std::string& author_id, int publication_year, const std::vector<std::string>& tags) {
    auto uow = uow_factory_.MakeUnitOfWork();
    auto book_id = BookId::New();
    uow->GetBooks().Save({book_id, AuthorId::FromString(author_id), title, publication_year});
    for (const auto& tag : tags) {
        uow->GetBooks().AddTag(book_id.ToString(), tag);
    }
    uow->Commit();
}

void UseCasesImpl::AddBookWithNewAuthor(const std::string& title, const std::string& author_name, int publication_year, const std::vector<std::string>& tags) {
    auto uow = uow_factory_.MakeUnitOfWork();
    auto author_id = AuthorId::New();
    uow->GetAuthors().Save({author_id, author_name});
    auto book_id = BookId::New();
    uow->GetBooks().Save({book_id, author_id, title, publication_year});
    for (const auto& tag : tags) {
        uow->GetBooks().AddTag(book_id.ToString(), tag);
    }
    uow->Commit();
}

std::vector<BookWithAuthor> UseCasesImpl::GetBooksWithAuthors() {
    auto uow = uow_factory_.MakeUnitOfWork();
    auto result = uow->GetBooks().GetSortedBooksWithAuthors();
    uow->Commit();
    return result;
}

std::vector<BookDetailed> UseCasesImpl::FindDetailedBooks(const std::string& title) {
    auto uow = uow_factory_.MakeUnitOfWork();
    auto result = uow->GetBooks().FindDetailedBooks(title);
    uow->Commit();
    return result;
}

std::vector<std::string> UseCasesImpl::GetBookTags(const std::string& book_id) {
    auto uow = uow_factory_.MakeUnitOfWork();
    auto result = uow->GetBooks().GetBookTags(book_id);
    uow->Commit();
    return result;
}

bool UseCasesImpl::DeleteBookById(const std::string& book_id) {
    auto uow = uow_factory_.MakeUnitOfWork();
    bool res = uow->GetBooks().DeleteBook(book_id);
    uow->Commit();
    return res;
}

bool UseCasesImpl::UpdateBookFull(const std::string& book_id, const std::string& title, int year, const std::vector<std::string>& tags) {
    auto uow = uow_factory_.MakeUnitOfWork();
    if (!uow->GetBooks().EditBook(book_id, title, year)) {
        return false;
    }
    uow->GetBooks().ClearBookTags(book_id);
    for (const auto& tag : tags) {
        uow->GetBooks().AddTag(book_id, tag);
    }
    uow->Commit();
    return true;
}

}  // namespace app