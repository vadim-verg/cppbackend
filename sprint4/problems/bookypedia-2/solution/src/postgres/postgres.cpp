#include "postgres.h"
#include <pqxx/pqxx>
#include <pqxx/zview.hxx>
#include <boost/algorithm/string/trim.hpp>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

// --- AuthorRepositoryImpl ---

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    work_.exec_params(
        "INSERT INTO authors (id, name) VALUES ($1, $2) ON CONFLICT (id) DO UPDATE SET name=$2;"_zv,
        author.GetId().ToString(), author.GetName());
}

std::vector<domain::Author> AuthorRepositoryImpl::GetSortedAuthors() {
    auto rows = work_.exec("SELECT id, name FROM authors ORDER BY name;"_zv);
    std::vector<domain::Author> result;
    for (const auto& row : rows) {
        result.emplace_back(
            domain::AuthorId::FromString(row["id"].as<std::string>()),
            row["name"].as<std::string>()
        );
    }
    return result;
}

std::optional<domain::Author> AuthorRepositoryImpl::FindByName(const std::string& name) {
    auto rows = work_.exec_params("SELECT id, name FROM authors WHERE name = $1;"_zv, name);
    if (rows.empty()) {
        return std::nullopt;
    }
    return domain::Author{
        domain::AuthorId::FromString(rows[0]["id"].as<std::string>()),
        rows[0]["name"].as<std::string>()
    };
}

bool AuthorRepositoryImpl::Delete(const domain::AuthorId& author_id) {
    std::string id_str = author_id.ToString();

    work_.exec_params(R"(
        DELETE FROM book_tags 
        WHERE book_id IN (SELECT id FROM books WHERE author_id = $1);
    )"_zv, id_str);

    work_.exec_params("DELETE FROM books WHERE author_id = $1;"_zv, id_str);

    auto result = work_.exec_params("DELETE FROM authors WHERE id = $1;"_zv, id_str);
    
    return result.affected_rows() > 0;
}

bool AuthorRepositoryImpl::Edit(const domain::AuthorId& author_id, const std::string& new_name) {
    if (new_name.empty()) return false;
    auto result = work_.exec_params("UPDATE authors SET name = $1 WHERE id = $2;"_zv, new_name, author_id.ToString());
    return result.affected_rows() > 0;
}

// --- BookRepositoryImpl ---

void BookRepositoryImpl::Save(const domain::Book& book) {
    work_.exec_params(
        "INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);"_zv,
        book.GetId().ToString(),
        book.GetAuthorId().ToString(),
        book.GetTitle(),
        book.GetPublicationYear()
    );
}

std::vector<domain::BookWithAuthor> BookRepositoryImpl::GetSortedBooksWithAuthors() {
    auto rows = work_.exec(R"(
        SELECT b.title, a.name AS author_name, b.publication_year 
        FROM books b
        INNER JOIN authors a ON b.author_id = a.id
        ORDER BY b.title ASC, a.name ASC, b.publication_year ASC;
    )"_zv);

    std::vector<domain::BookWithAuthor> result;
    for (const auto& row : rows) {
        result.push_back({
            row["title"].as<std::string>(),
            row["author_name"].as<std::string>(),
            row["publication_year"].as<int>()
        });
    }
    return result;
}

std::vector<domain::BookDetailed> BookRepositoryImpl::FindDetailedBooks(const std::string& title) {
    pqxx::result rows;
    
    // Очищаем входную строку от возможных пробелов по краям перед запросом
    std::string clean_title = title;
    boost::algorithm::trim(clean_title);

    if (clean_title.empty()) {
        rows = work_.exec(R"(
            SELECT b.id, b.title, a.name AS author_name, b.publication_year 
            FROM books b INNER JOIN authors a ON b.author_id = a.id
            ORDER BY b.title ASC, a.name ASC, b.publication_year ASC;
        )"_zv);
    } else {
        rows = work_.exec_params(R"(
            SELECT b.id, b.title, a.name AS author_name, b.publication_year 
            FROM books b INNER JOIN authors a ON b.author_id = a.id
            WHERE b.title = $1
            ORDER BY b.title ASC, a.name ASC, b.publication_year ASC;
        )"_zv, clean_title);
    }

    std::vector<domain::BookDetailed> result;
    for (const auto& row : rows) {
        result.push_back({
            row["id"].as<std::string>(),
            row["title"].as<std::string>(),
            row["author_name"].as<std::string>(),
            row["publication_year"].as<int>()
        });
    }
    return result;
}

std::vector<std::string> BookRepositoryImpl::GetBookTags(const std::string& book_id) {
    // ORDER BY tag ASC обязателен для прохождения тестов на сравнение списков тегов!
    auto rows = work_.exec_params("SELECT tag FROM book_tags WHERE book_id = $1 ORDER BY tag ASC;"_zv, book_id);
    std::vector<std::string> tags;
    for (const auto& row : rows) {
        tags.push_back(row["tag"].as<std::string>());
    }
    return tags;
}

void BookRepositoryImpl::AddTag(const std::string& book_id, const std::string& tag) {
    work_.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2) ON CONFLICT DO NOTHING;"_zv, book_id, tag);
}

bool BookRepositoryImpl::DeleteBook(const std::string& book_id) {
    auto result = work_.exec_params("DELETE FROM books WHERE id = $1;"_zv, book_id);
    return result.affected_rows() > 0;
}

bool BookRepositoryImpl::EditBook(const std::string& book_id, const std::string& new_title, int new_year) {
    auto result = work_.exec_params("UPDATE books SET title = $1, publication_year = $2 WHERE id = $3;"_zv, new_title, new_year, book_id);
    return result.affected_rows() > 0;
}

void BookRepositoryImpl::ClearBookTags(const std::string& book_id) {
    work_.exec_params("DELETE FROM book_tags WHERE book_id = $1;"_zv, book_id);
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
    author_id UUID NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    title varchar(100) NOT NULL,
    publication_year INT
);
)"_zv);

    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    tag varchar(30) NOT NULL,
    PRIMARY KEY (book_id, tag)
);
)"_zv);

    work.commit();
}

}  // namespace postgres