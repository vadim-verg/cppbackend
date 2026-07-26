cmake_minimum_required(VERSION 3.11)

project(book_manager CXX)
set(CMAKE_CXX_STANDARD 20)

# Подключение Конана для libpqxx (Оригинальные строки прекода)
include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Ищем системный Boost, установленный в Docker-образе Практикума
find_package(Boost REQUIRED)

add_executable(book_manager main.cpp)

# Линкуем оригинальный CONAN_PKG::libpqxx и найденные системные заголовочные файлы Boost
target_link_libraries(book_manager PRIVATE 
    CONAN_PKG::libpqxx
    Boost::boost
    Threads::Threads
)