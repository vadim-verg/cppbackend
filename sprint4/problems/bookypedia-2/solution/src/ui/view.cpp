#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <set>
#include <optional>

#include "../app/use_cases.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << " by " << book.author_name << ", " << book.publication_year;
    return out;
}

}  // namespace detail

// Вспомогательная функция вывода векторов без точек
template <typename T>
void PrintVectorWithoutDot(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

// Алгоритм нормализации тегов книги
std::vector<std::string> ParseAndNormalizeTags(const std::string& input_tags) {
    std::vector<std::string> raw_tags;
    std::stringstream ss(input_tags);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        raw_tags.push_back(item);
    }

    std::set<std::string> unique_tags;
    std::vector<std::string> result;

    for (auto& tag : raw_tags) {
        boost::algorithm::trim(tag);
        if (tag.empty()) {
            continue;
        }

        std::string normalized_tag;
        std::stringstream words_stream(tag);
        std::string word;
        bool first = true;
        while (words_stream >> word) {
            if (!first) {
                normalized_tag += " ";
            }
            normalized_tag += word;
            first = false;
        }

        if (normalized_tag.length() <= 30 && unique_tags.insert(normalized_tag).second) {
            result.push_back(normalized_tag);
        }
    }
    return result;
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "[name]"s, "Edits author"s, std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Deletes author"s, std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s, std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowBook"s, "[title]"s, "Shows book info"s, std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "[title]"s, "Edits a book"s, std::bind(&View::EditBook, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "[title]"s, "Deletes a book"s, std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string current_name;
        std::getline(cmd_input, current_name);
        boost::algorithm::trim(current_name);

        std::string author_id_to_edit;
        bool use_name_scenario = !current_name.empty();

        if (use_name_scenario) {
            auto author = use_cases_.FindAuthorByName(current_name);
            if (!author) {
                output_ << "Failed to edit author" << std::endl;
                return true;
            }
        } else {
            output_ << "Select author:" << std::endl;
            auto authors = GetAuthors();
            if (authors.empty()) return true;

            PrintVectorWithoutDot(output_, authors);
            output_ << "Enter author # or empty line to cancel" << std::endl;

            std::string choice;
            if (!std::getline(input_, choice) || choice.empty()) return true;

            int author_idx = std::stoi(choice) - 1;
            if (author_idx < 0 || author_idx >= static_cast<int>(authors.size())) {
                output_ << "Failed to edit author" << std::endl;
                return true;
            }
            author_id_to_edit = authors[author_idx].id;
        }

        output_ << "Enter new name:" << std::endl;
        std::string new_name;
        if (!std::getline(input_, new_name)) return true;
        boost::algorithm::trim(new_name);

        bool success = use_name_scenario 
            ? use_cases_.EditAuthorByName(current_name, new_name)
            : use_cases_.EditAuthorById(author_id_to_edit, new_name);

        if (!success) {
            output_ << "Failed to edit author" << std::endl;
        }
    } catch (...) {
        output_ << "Failed to edit author" << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        if (!name.empty()) {
            if (!use_cases_.DeleteAuthorByName(name)) {
                output_ << "Failed to delete author" << std::endl;
            }
        } else {
            output_ << "Select author:" << std::endl;
            auto authors = GetAuthors();
            if (authors.empty()) return true;
            
            PrintVectorWithoutDot(output_, authors);
            output_ << "Enter author # or empty line to cancel" << std::endl;

            std::string str;
            if (!std::getline(input_, str) || str.empty()) return true;

            int author_idx = std::stoi(str) - 1;
            if (author_idx < 0 || author_idx >= static_cast<int>(authors.size())) {
                output_ << "Failed to delete author" << std::endl;
                return true;
            }

            if (!use_cases_.DeleteAuthorById(authors[author_idx].id)) {
                output_ << "Failed to delete author" << std::endl;
            }
        }
    } catch (...) {
        output_ << "Failed to delete author" << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        int publication_year;
        if (!(cmd_input >> publication_year)) {
            output_ << "Failed to add book" << std::endl;
            return true;
        }
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        if (title.empty()) {
            output_ << "Failed to add book" << std::endl;
            return true;
        }

        output_ << "Enter author name or empty line to select from list:" << std::endl;
        std::string author_name;
        if (!std::getline(input_, author_name)) return true;
        boost::algorithm::trim(author_name);

        std::optional<std::string> author_id;

        if (author_name.empty()) {
            author_id = SelectAuthor();
            if (!author_id) {
                output_ << "Failed to add book" << std::endl;
                return true;
            }
        } else {
            auto author = use_cases_.FindAuthorByName(author_name);
            if (!author) {
                output_ << "No author found. Do you want to add " << author_name << " (y/n)?" << std::endl;
                std::string answer;
                if (!std::getline(input_, answer)) return true;
                boost::algorithm::trim(answer);
                
                if (answer != "Y" && answer != "y") {
                    output_ << "Failed to add book" << std::endl;
                    return true;
                }
            } else {
                author_id = author->GetId().ToString();
            }
        }

        output_ << "Enter tags (comma separated):" << std::endl;
        std::string raw_tags;
        if (!std::getline(input_, raw_tags)) return true;
        std::vector<std::string> tags = ParseAndNormalizeTags(raw_tags);

        if (author_id) {
            use_cases_.AddBookWithExistingAuthor(title, *author_id, publication_year, tags);
        } else {
            use_cases_.AddBookWithNewAuthor(title, author_name, publication_year, tags);
        }
    } catch (...) {
        output_ << "Failed to add book" << std::endl;
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        auto found_books = use_cases_.FindDetailedBooks(title);
        if (found_books.empty()) return true;

        auto target_book = SelectBookFromList(found_books);
        if (!target_book) return true;

        output_ << "Title: " << target_book->title << std::endl;
        output_ << "Author: " << target_book->author_name << std::endl;
        output_ << "Publication year: " << target_book->publication_year << std::endl;

        auto tags = use_cases_.GetBookTags(target_book->id);
        if (!tags.empty()) {
            output_ << "Tags: ";
            for (size_t i = 0; i < tags.size(); ++i) {
                output_ << tags[i];
                if (i + 1 < tags.size()) output_ << ", ";
            }
            output_ << std::endl;
        }
    } catch (...) {}
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        auto found_books = use_cases_.FindDetailedBooks(title);
        if (!title.empty() && found_books.empty()) {
        output_ << "Book not found" << std::endl;
        return true;
    }

    auto target_book = SelectBookFromList(found_books);
    if (!target_book) return true;

    output_ << "Enter new title or empty line to use the current one (" << target_book->title << "):" << std::endl;
    std::string new_title;
    if (!std::getline(input_, new_title)) return true;
    boost::algorithm::trim(new_title);
    if (new_title.empty()) new_title = target_book->title;

    output_ << "Enter publication year or empty line to use the current one (" << target_book->publication_year << "):" << std::endl;
    std::string new_year_str;
    if (!std::getline(input_, new_year_str)) return true;
    boost::algorithm::trim(new_year_str);
    int new_year = new_year_str.empty() ? target_book->publication_year : std::stoi(new_year_str);

    auto current_tags = use_cases_.GetBookTags(target_book->id);
    output_ << "Enter tags (current tags: ";
    for (size_t i = 0; i < current_tags.size(); ++i) {
        output_ << current_tags[i];
        if (i + 1 < current_tags.size()) output_ << ", ";
    }
    output_ << "):" << std::endl;

    std::string raw_tags;
    if (!std::getline(input_, raw_tags)) return true;
    std::vector<std::string> tags = ParseAndNormalizeTags(raw_tags);

    if (!use_cases_.UpdateBookFull(target_book->id, new_title, new_year, tags)) {
        output_ << "Book not found" << std::endl;
    }
} catch (...) {
    output_ << "Book not found" << std::endl;
}
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        
        auto found_books = use_cases_.FindDetailedBooks(title);
        if (found_books.empty()) {
            output_ << "Failed to delete book" << std::endl;
            return true;
        }
        
        auto target_book = SelectBookFromList(found_books);
        if (!target_book) return true;
        
        if (!use_cases_.DeleteBookById(target_book->id)) {
            output_ << "Failed to delete book" << std::endl;
        }
    } catch (...) {
        output_ << "Failed to delete book" << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    PrintVectorWithoutDot(output_, GetAuthors());
    return true;
}

bool View::ShowBooks() const {
    PrintVectorWithoutDot(output_, GetBooks());
    return true;
}

std::optional<domain::BookDetailed> View::SelectBookFromList(const std::vector<domain::BookDetailed>& books) const {
    if (books.empty()) return std::nullopt;
    if (books.size() == 1) return books[0];
    
    std::vector<detail::BookSelectionInfo> sel_list;
    for (const auto& b : books) {
        sel_list.push_back({b.title, b.author_name, b.publication_year});
    }
    PrintVectorWithoutDot(output_, sel_list);
    
    output_ << "Enter the book # or empty line to cancel:" << std::endl;
    std::string str;
    if (!std::getline(input_, str) || str.empty()) return std::nullopt;
    
    int idx = std::stoi(str) - 1;
    if (idx < 0 || idx >= static_cast<int>(books.size())) {
        throw std::runtime_error("Invalid book selection");
    }
    return books[idx];
}

std::optional<std::string> View::SelectAuthor() const {
    output_ << "Select author:" << std::endl;
    auto authors = GetAuthors();
    PrintVectorWithoutDot(output_, authors);
    
    output_ << "Enter author # or empty line to cancel" << std::endl;
    std::string str;
    if (!std::getline(input_, str) || str.empty()) return std::nullopt;
    
    int author_idx = std::stoi(str) - 1;
    if (author_idx < 0 || author_idx >= static_cast<int>(authors.size())) {
        throw std::runtime_error("Invalid author num");
    }
    return authors[author_idx].id;
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    std::vector<detail::AuthorInfo> dst_authors;
    for (const auto& author : use_cases_.GetAuthors()) {
        dst_authors.push_back({author.GetId().ToString(), author.GetName()});
    }
    return dst_authors;
}

std::vector<detail::BookInfo> View::GetBooks() const {
    std::vector<detail::BookInfo> dst_books;
    for (const auto& book : use_cases_.GetBooksWithAuthors()) {
        dst_books.push_back({book.title, book.author_name, book.publication_year});
    }
    return dst_books;
}

}  // namespace ui