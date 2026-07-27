#include "postgres.h"
#include <pqxx/pqxx> // Подключаем полный пакет libpqxx вместо отдельных модулей
#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

// --- AuthorRepositoryImpl ---

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetSortedAuthors() {
    pqxx::work work{connection_};
    auto rows = work.exec("SELECT id, name FROM authors ORDER BY name;"_zv);
    work.commit();

    std::vector<domain::Author> result;
    for (const auto& row : rows) {
        result.emplace_back(
            domain::AuthorId::FromString(row["id"].as<std::string>()),
            row["name"].as<std::string>()
        );
    }
    return result;
}

// --- BookRepositoryImpl ---

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);
)"_zv,
        book.GetId().ToString(),
        book.GetAuthorId().ToString(),
        book.GetTitle(),
        book.GetPublicationYear()
    );
    work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetSortedBooks() {
    pqxx::work work{connection_};
    auto rows = work.exec("SELECT id, author_id, title, publication_year FROM books ORDER BY title ASC;"_zv);
    work.commit();

    std::vector<domain::Book> result;
    for (const auto& row : rows) {
        result.emplace_back(
            domain::BookId::FromString(row["id"].as<std::string>()),
            domain::AuthorId::FromString(row["author_id"].as<std::string>()),
            row["title"].as<std::string>(),
            row["publication_year"].as<int>()
        );
    }
    return result;
}

std::vector<domain::Book> BookRepositoryImpl::GetBooksByAuthor(const domain::AuthorId& author_id) {
    pqxx::work work{connection_};
    auto rows = work.exec_params(
        "SELECT id, title, publication_year FROM books WHERE author_id = $1 ORDER BY publication_year ASC, title ASC;"_zv,
        author_id.ToString()
    );
    work.commit();

    std::vector<domain::Book> result;
    for (const auto& row : rows) {
        result.emplace_back(
            domain::BookId::FromString(row["id"].as<std::string>()),
            author_id,
            row["title"].as<std::string>(),
            row["publication_year"].as<int>()
        );
    }
    return result;
}

// --- Database ---

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);

    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL REFERENCES authors(id),
    title varchar(100) NOT NULL,
    publication_year INT
);
)"_zv);

    work.commit();
}

}  // namespace postgres