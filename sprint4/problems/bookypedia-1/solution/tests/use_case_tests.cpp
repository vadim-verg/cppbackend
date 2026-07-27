#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    void Save(const domain::Author& author) override {
        saved_authors.push_back(author);
    }
    
    std::vector<domain::Author> GetSortedAuthors() override {
        return saved_authors;
    }

    std::vector<domain::Author> saved_authors;
};

struct MockBookRepository : domain::BookRepository {
    void Save(const domain::Book& book) override {
        saved_books.push_back(book);
    }

    std::vector<domain::Book> GetSortedBooks() override {
        return saved_books;
    }

    std::vector<domain::Book> GetBooksByAuthor(const domain::AuthorId&) override {
        return saved_books;
    }

    std::vector<domain::Book> saved_books;
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books; // Добавляем мок для книг
    app::UseCasesImpl use_cases{authors, books}; // Передаем оба мока в конструктор
};

}  // namespace

TEST_CASE_METHOD(Fixture, "UseCases") {
    SECTION("AddAuthor") {
        BOOST_CONCEPT_ASSERT((int)1); // Заглушка, чтобы секция не была пустой
        // Проверяем, что изначально авторов нет
        CHECK(authors.saved_authors.empty());

        // Добавляем автора через UseCases
        use_cases.AddAuthor("Joanne Rowling");

        // Проверяем, что автор успешно сохранился в репозиторий
        REQUIRE(authors.saved_authors.size() == 1);
        CHECK(authors.saved_authors.at(0).GetName() == "Joanne Rowling");
        CHECK_FALSE(authors.saved_authors.at(0).GetId().ToString().empty());
    }
}