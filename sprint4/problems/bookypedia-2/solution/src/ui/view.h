#pragma once
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>
#include "../domain/book.h"
#include "../app/unit_of_work.h" // Подключаем интерфейс UOW

namespace menu {
class Menu;
}

namespace app {
class UseCases;
}

namespace ui {
namespace detail {

struct AddBookParams {
    std::string title;
    std::string author_id;
    int publication_year = 0;
};

struct AuthorInfo {
    std::string id;
    std::string name;
};

struct BookInfo {
    std::string title;
    std::string author_name;
    int publication_year;
};

}  // namespace detail

class View {
public:
    // Конструктор теперь дополнительно принимает фабрику UnitOfWorkFactory
    View(menu::Menu& menu, app::UseCases& use_cases, app::UnitOfWorkFactory& uow_factory, std::istream& input, std::ostream& output);

private:
    bool AddAuthor(std::istream& cmd_input) const;
    bool EditAuthor(std::istream& cmd_input) const;
    bool DeleteAuthor(std::istream& cmd_input) const;
    
    bool AddBook(std::istream& cmd_input) const;
    bool ShowBook(std::istream& cmd_input) const;
    bool EditBook(std::istream& cmd_input) const;
    bool DeleteBook(std::istream& cmd_input) const;
    
    bool ShowAuthors() const;
    bool ShowBooks() const;

    std::optional<std::string> SelectAuthor(app::UnitOfWork& uow) const;
    std::optional<domain::BookDetailed> SelectBookFromList(app::UnitOfWork& uow, const std::vector<domain::BookDetailed>& books) const;
    
    std::vector<detail::AuthorInfo> GetAuthors(app::UnitOfWork& uow) const;
    std::vector<detail::BookInfo> GetBooks(app::UnitOfWork& uow) const;

    menu::Menu& menu_;
    app::UseCases& use_cases_;
    app::UnitOfWorkFactory& uow_factory_; // Сохраняем фабрику
    std::istream& input_;
    std::ostream& output_;
};

}  // namespace ui