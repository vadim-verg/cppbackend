cmake_minimum_required(VERSION 3.11)

project(LootServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# ПОДКЛЮЧЕНИЕ CONAN ДЛЯ MULTI-CONFIGURATION
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
else()
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake OPTIONAL)
endif()

conan_basic_setup()

# [ОБНОВЛЕНО] Добавляем глобальные макросы компиляции для Windows (MSVC)
# 1. BOOST_ALL_NO_LIB — отключаем встроенную автолинковку VS
# 2. _WIN32_WINNT=0x0601 — убираем предупреждение сети (целимся в Windows 7+)
add_compile_definitions(
    BOOST_ALL_NO_LIB
    _WIN32_WINNT=0x0601
)

# ---------------------------------------------------------------------------
# 1. СОЗДАЁМ СТАТИЧЕСКУЮ БИБЛИОТЕКУ МОДЕЛИ
# ---------------------------------------------------------------------------
add_library(GameModel STATIC
    src/model.cpp
    src/model.h
    src/loot_generator.cpp
    src/loot_generator.h
    src/loot_provider.h
    src/json_loader.cpp
    src/json_loader.h
    src/players.cpp
    src/players.h
    src/tagged.h
)

target_include_directories(GameModel PUBLIC src ${CONAN_INCLUDE_DIRS})

# ---------------------------------------------------------------------------
# 2. ДОБАВЛЯЕМ ЦЕЛИ И ИХ ЗАВИСИМОСТИ
# ---------------------------------------------------------------------------

# Исполняемый файл сервера
add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/request_handler.cpp
    src/request_handler.h
    src/api_handler.cpp
    src/api_handler.h
    src/logger.cpp
    src/logger.h
    src/ticker.h
)

target_include_directories(game_server PRIVATE ${CONAN_INCLUDE_DIRS})

# Используем простой стиль линковки
target_link_libraries(game_server GameModel)
conan_target_link_libraries(game_server)

# Исполняемый файл тестов
add_executable(game_tests 
    tests/loot_generator_tests.cpp
)

target_include_directories(game_tests PRIVATE ${CONAN_INCLUDE_DIRS})
target_link_libraries(game_tests GameModel)
conan_target_link_libraries(game_tests)