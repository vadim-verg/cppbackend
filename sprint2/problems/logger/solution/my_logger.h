#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>
#include <filesystem>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    auto GetTime() const {
        // manual_ts_ читается под мьютексом в вызывающей функции,
        // поэтому здесь дополнительный lock не нужен, если вызовы согласованы.
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    std::string GetTimeStamp() const {
        const auto now = GetTime();
        auto t_c = std::chrono::system_clock::to_time_t(now);

        // 1. Защита от отрицательного времени (до 1970 года)
        if (t_c < 0) {
            t_c = 0;
        }

        // 2. Используем потокобезопасный и нативный для Windows/Linux localtime
        std::tm local_time;
#if defined(_WIN32) || defined(_WIN64)
        if (localtime_s(&local_time, &t_c) != 0) {
            return "1970-01-01 00:00:00";
        }
#else
        auto* pt = std::localtime(&t_c);
        if (!pt) return "1970-01-01 00:00:00";
        local_time = *pt;
#endif

        std::stringstream ss;
        // Заменили %F на %Y-%m-%d, а %T на %H:%M:%S
        ss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // Форматирует дату как "YYYY_MM_DD" для имени файла
    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        auto t_c = std::chrono::system_clock::to_time_t(now);

        if (t_c < 0) {
            t_c = 0;
        }

        std::tm local_time;
#if defined(_WIN32) || defined(_WIN64)
        if (localtime_s(&local_time, &t_c) != 0) {
            return "1970_01_01";
        }
#else
        auto* pt = std::localtime(&t_c);
        if (!pt) return "1970_01_01";
        local_time = *pt;
#endif

        std::stringstream ss;
        ss << std::put_time(&local_time, "%Y_%m_%d");
        return ss.str();
    }

    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    // Метод Log принимает произвольное количество аргументов любого типа
    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 1. Проверяем текущую дату, чтобы понять, нужно ли открыть/сменить файл
        std::string current_date = GetFileTimeStamp();
        if (current_date != last_date_ || !file_stream_.is_open()) {
            last_date_ = current_date;
            if (file_stream_.is_open()) {
                file_stream_.close();
            }
            // Формируем путь: /var/log/sample_log_YYYY_MM_DD.log
            std::string file_path = "/var/log/sample_log_" + current_date + ".log";

            // В Windows путей /var/log может не существовать, поэтому создаем директорию
            std::filesystem::path p(file_path);
            if (p.has_parent_path()) {
                std::filesystem::create_directories(p.parent_path());
            }

            file_stream_.open(file_path, std::ios::app);
        }

        // 2. Выводим временную метку
        file_stream_ << GetTimeStamp() << ": "sv;

        // 3. Распаковываем variadic template с помощью C++17 Fold Expression
        ((file_stream_ << args), ...);

        // 4. Завершаем строку и делаем эффективный сброс на диск
        file_stream_ << '\n';
        file_stream_.flush();
    }

    // Потокобезопасная установка кастомного времени
    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(mutex_);
        manual_ts_ = ts;
    }

private:
    mutable std::mutex mutex_;
    std::optional<std::chrono::system_clock::time_point> manual_ts_;

    std::ofstream file_stream_;
    std::string last_date_;
};
