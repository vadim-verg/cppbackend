cmake_minimum_required(VERSION 3.15)
project(game_server CXX)

# Включаем макросы статического логгера для ВСЕХ платформ (и Windows, и Linux),
# так как Conan в Docker-контейнере Практикума поставляет статический Boost.
add_compile_definitions(BOOST_LOG_USE_STATIC_LIBS)
add_compile_definitions(BOOST_ALL_NO_LIB)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

# Стандарт C++20, необходимый для Яндекс Практикума
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Строгие флаги компиляции (убрали -Werror для Linux)
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Интеграция с генераторами Conan (подключаем базовые конфигураторы мульти-сборки)
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
elseif(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
else()
    message(FATAL_ERROR "Conan build info not found! Run 'conan install' first.")
endif()

# Вызываем conan_basic_setup без параметров, 
# чтобы Конан сам корректно разложил зависимости для Release/Debug конфигураций 
# и инициализировал макрос conan_find_apple_frameworks
conan_basic_setup()

# ИСПРАВЛЕНИЕ ДЛЯ LINUX: Явно находим системный пакет потоков, 
# чтобы таргет Threads::Threads стал доступен линкеру
find_package(Threads REQUIRED)

# Таргет сборки (все исходники задания)
add_executable(game_server
    src/api_handler.cpp
    src/api_handler.h
    src/boost_json.cpp
    src/http_server.cpp
    src/http_server.h
    src/json_loader.cpp
    src/json_loader.h
    src/logger.cpp
    src/logger.h
    src/main.cpp
    src/model.cpp
    src/model.h
    src/players.cpp
    src/players.h
    src/request_handler.cpp
    src/request_handler.h
    src/sdk.h
    src/tagged.h
)

# Линковка библиотек
if(WIN32)
    target_link_libraries(game_server PRIVATE ${CONAN_LIBS} ws2_32 mswsock)
else()
    # На Linux связываем все либы Конана и найденные системные потоки
    target_link_libraries(game_server PRIVATE ${CONAN_LIBS} Threads::Threads)
endif()

# Пути поиска заголовочных файлов внутри src
target_include_directories(game_server PRIVATE src)

# Папка вывода готового скомпилированного файла
set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)