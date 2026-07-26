#include <iostream>
#include <string>
#include <optional>
#include <pqxx/pqxx>
#include <boost/json.hpp>

using namespace std::literals;

int main(int argc, const char* argv[]) {
    try {
        if (argc == 1) {
            std::cout << "Usage: book_manager <conn-string>\n"sv;
            return EXIT_SUCCESS;
        } else if (argc != 2) {
            std::cerr << "Invalid command line\n"sv;
            return EXIT_FAILURE;
        }

        // Подключаемся к БД, указывая её параметры в качестве аргумента
        pqxx::connection conn{argv[1]};

        // Шаг 1. Создаём таблицу в выбранной базе данных
        {
            pqxx::work w(conn);
            w.exec(
                "CREATE TABLE IF NOT EXISTS books ("
                "id SERIAL PRIMARY KEY, "
                "title varchar(100) NOT NULL, "
                "author varchar(100) NOT NULL, "
                "year integer NOT NULL, "
                "ISBN char(13) UNIQUE);"sv);
            w.commit(); // Сразу сохраняем структуру таблицы
        }

        std::string line;

        // Цикл обработки построчных JSON-запросов
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            try {
                auto json_obj = boost::json::parse(line).as_object();
                std::string action = std::string(json_obj.at("action").as_string());

                // Обработка выхода из программы
                if (action == "exit") {
                    break;
                }

                // Обработка добавления книги
                if (action == "add_book") {
                    auto payload = json_obj.at("payload").as_object();

                    std::string title = std::string(payload.at("title").as_string());
                    std::string author = std::string(payload.at("author").as_string());
                    int year = payload.at("year").as_int64();

                    std::optional<std::string> isbn = std::nullopt;
                    if (payload.contains("ISBN") && !payload.at("ISBN").is_null()) {
                        isbn = std::string(payload.at("ISBN").as_string());
                    }

                    // Сохраняем книгу в базу данных
                    try {
                        pqxx::work w(conn);
                        // Параметризованный запрос безопасно обработает std::optional (превратит в NULL, если пусто)
                        w.exec_params(
                            "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4);"sv,
                            title, author, year, isbn
                            );
                        w.commit();

                        // Выводим ответ об успехе по ТЗ
                        std::cout << "{\"result\":true}" << std::endl;
                    } catch (const std::exception& e) {
                        // Если нарушена уникальность ISBN или другая ошибка БД — выводим false
                        std::cout << "{\"result\":false}" << std::endl;
                    }
                }
                // Обработка вывода всех книг
                else if (action == "all_books") {
                    pqxx::read_work w(conn);

                    // ИСПРАВЛЕНО: Сортировка строго по ТЗ (year DESC, остальное ASC)
                    pqxx::result res = w.exec(
                        "SELECT id, title, author, year, ISBN FROM books "
                        "ORDER BY year DESC, title ASC, author ASC, ISBN ASC;"sv
                        );

                    boost::json::array json_arr;
                    for (const auto& row : res) {
                        boost::json::object book_obj;
                        book_obj["id"] = row["id"].as<int>();
                        book_obj["title"] = row["title"].as<std::string>();
                        book_obj["author"] = row["author"].as<std::string>();
                        book_obj["year"] = row["year"].as<int>();

                        if (row["ISBN"].is_null()) {
                            book_obj["ISBN"] = nullptr; // В JSON запишется null
                        } else {
                            book_obj["ISBN"] = row["ISBN"].as<std::string>();
                        }
                        json_arr.push_back(book_obj);
                    }
                    std::cout << boost::json::serialize(json_arr) << std::endl;
                }

            } catch (const std::exception& e) {
                std::cerr << "JSON parse error: " << e.what() << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
