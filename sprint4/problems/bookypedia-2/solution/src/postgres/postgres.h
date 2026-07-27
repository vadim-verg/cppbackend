#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>
#include <optional>
#include <string>
#include <vector>
#include "../domain/author.h"
#include "../domain/book.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::work& work)
        : work_{work} {
    }

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetSortedAuthors() override;
    std::optional<domain::Author> FindByName(const std::string& name) override;
    bool Delete(const domain::AuthorId& author_id) override;
    bool Edit(const domain::AuthorId& author_id, const std::string& new_name) override;

private:
    pqxx::work& work_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::work& work)
        : work_{work} {
    }

    void Save(const domain::Book& book) override;
    std::vector<domain::BookWithAuthor> GetSortedBooksWithAuthors() override;
    std::vector<domain::BookDetailed> FindDetailedBooks(const std::string& title) override;
    std::vector<std::string> GetBookTags(const std::string& book_id) override;
    void AddTag(const std::string& book_id, const std::string& tag) override;
    bool DeleteBook(const std::string& book_id) override;
    bool EditBook(const std::string& book_id, const std::string& new_title, int new_year) override;
    void ClearBookTags(const std::string& book_id) override;

private:
    pqxx::work& work_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    pqxx::connection& GetConnection() {
        return connection_;
    }

private:
    pqxx::connection connection_;
};

}  // namespace postgres