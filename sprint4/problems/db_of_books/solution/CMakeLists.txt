cmake_minimum_required(VERSION 3.11)

project(book_manager CXX)
set(CMAKE_CXX_STANDARD 20)

# Подключаем Конан (оригинальные строки Практикума)
include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(book_manager main.cpp)

# КРИТИЧЕСКИЙ ФИКС: Используем CONAN_LIBS. Сюда Конан автоматически 
# пропишет и libpqxx, и boost_json, соблюдая правильный порядок для линкера.
target_link_libraries(book_manager PRIVATE
    ${CONAN_LIBS}
    Threads::Threads
)