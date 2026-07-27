#pragma once
#include <string>
#include <vector>
#include "author.h"
#include "../util/tagged_uuid.h"

namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

struct BookWithAuthor {
    std::string title;
    std::string author_name;
    int publication_year;
};

struct BookDetailed {
    std::string id;
    std::string title;
    std::string author_name;
    int publication_year;
};

class Book {
public:
    Book(BookId id, AuthorId author_id, std::string title, int publication_year)
        : id_(std::move(id))
        , author_id_(std::move(author_id))
        , title_(std::move(title))
        , publication_year_(publication_year) {
    }

    const BookId& GetId() const noexcept {
        return id_;
    }

    const AuthorId& GetAuthorId() const noexcept {
        return author_id_;
    }

    const std::string& GetTitle() const noexcept {
        return title_;
    }

    int GetPublicationYear() const noexcept {
        return publication_year_;
    }

private:
    BookId id_;
    AuthorId author_id_;
    std::string title_;
    int publication_year_;
};

class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual std::vector<BookWithAuthor> GetSortedBooksWithAuthors() = 0;
    virtual std::vector<BookDetailed> FindDetailedBooks(const std::string& title) = 0;
    virtual std::vector<std::string> GetBookTags(const std::string& book_id) = 0;
    virtual void AddTag(const std::string& book_id, const std::string& tag) = 0;
    virtual bool DeleteBook(const std::string& book_id) = 0;
    virtual bool EditBook(const std::string& book_id, const std::string& new_title, int new_year) = 0;
    virtual void ClearBookTags(const std::string& book_id) = 0;

protected:
    ~BookRepository() = default;
};

}  // namespace domain