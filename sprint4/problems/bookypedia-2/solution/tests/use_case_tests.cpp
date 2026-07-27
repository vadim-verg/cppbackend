#include <catch2/catch_test_macros.hpp>
#include <memory>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

// 1. Мок для репозитория авторов
struct MockAuthorRepository : domain::AuthorRepository {
    void Save(const domain::Author& author) override {
        saved_authors.push_back(author);
    }
    std::vector<domain::Author> GetSortedAuthors() override {
        return saved_authors;
    }
    std::optional<domain::Author> FindByName(const std::string& name) override {
        for (const auto& author : saved_authors) {
            if (author.GetName() == name) return author;
        }
        return std::nullopt;
    }
    bool Delete(const domain::AuthorId&) override { return true; }
    bool Edit(const domain::AuthorId&, const std::string&) override { return true; }

    std::vector<domain::Author> saved_authors;
};

// 2. Мок для репозитория книг
struct MockBookRepository : domain::BookRepository {
    void Save(const domain::Book& book) override {
        saved_books.push_back(book);
    }
    std::vector<domain::BookWithAuthor> GetSortedBooksWithAuthors() override { return {}; }
    std::vector<domain::BookDetailed> FindDetailedBooks(const std::string&) override { return {}; }
    std::vector<std::string> GetBookTags(const std::string&) override { return {}; }
    void AddTag(const std::string&, const std::string&) override {}
    bool DeleteBook(const std::string&) override { return true; }
    bool EditBook(const std::string&, const std::string&, int) override { return true; }
    void ClearBookTags(const std::string&) override {}

    std::vector<domain::Book> saved_books;
};

// 3. Мок для самого Unit of Work
class MockUnitOfWork : public app::UnitOfWork {
public:
    MockUnitOfWork(MockAuthorRepository& authors, MockBookRepository& books)
        : authors_(authors), books_(books) {}

    domain::AuthorRepository& GetAuthors() override { return authors_; }
    domain::BookRepository& GetBooks() override { return books_; }
    void Commit() override {}

private:
    MockAuthorRepository& authors_;
    MockBookRepository& books_;
};

// 4. Мок для фабрики Unit of Work
class MockUnitOfWorkFactory : public app::UnitOfWorkFactory {
public:
    MockUnitOfWorkFactory(MockAuthorRepository& authors, MockBookRepository& books)
        : authors_(authors), books_(books) {}

    std::unique_ptr<app::UnitOfWork> MakeUnitOfWork() override {
        return std::make_unique<MockUnitOfWork>(authors_, books_);
    }

private:
    MockAuthorRepository& authors_;
    MockBookRepository& books_;
};

// Фикстура для тестов Catch2
struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books;
    MockUnitOfWorkFactory uow_factory{authors, books};
    app::UseCasesImpl use_cases{uow_factory}; // Передаем фабрику, как требует новая архитектура
};

}  // namespace

TEST_CASE_METHOD(Fixture, "UseCases") {
    SECTION("AddAuthor") {
        CHECK(authors.saved_authors.empty());

        use_cases.AddAuthor("Joanne Rowling");

        REQUIRE(authors.saved_authors.size() == 1);
        CHECK(authors.saved_authors.at(0).GetName() == "Joanne Rowling");
        CHECK_FALSE(authors.saved_authors.at(0).GetId().ToString().empty());
    }
}