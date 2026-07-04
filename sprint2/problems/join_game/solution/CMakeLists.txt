cmake_minimum_required(VERSION 3.15)
project(game_server CXX)

# Конфигурация макросов
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
    add_compile_definitions(BOOST_LOG_USE_STATIC_LIBS)
    add_compile_definitions(BOOST_ALL_NO_LIB)
else()
    # ИСПРАВЛЕНИЕ ДЛЯ LINUX: Запрещаем автолинковку внутри заголовков Boost
    add_compile_definitions(BOOST_LOG_NO_LIB)
endif()

# Стандарт C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Предупреждения компилятора
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Интеграция с Conan
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
elseif(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
else()
    message(FATAL_ERROR "Conan build info not found! Run 'conan install' first.")
endif()

conan_basic_setup()

# Находим системные потоки Linux
find_package(Threads REQUIRED)

# Цель сборки
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

# Линковка
if(WIN32)
    target_link_libraries(game_server PRIVATE ${CONAN_LIBS} ws2_32 mswsock)
else()
    # ИСПРАВЛЕНИЕ: Передаем чистые имена библиотек Boost.
    # Поскольку conan_basic_setup() уже добавил папки Конана в пути поиска линкера,
    # он без труда найдет файлы libboost_log.a и libboost_thread.a на сервере.
    target_link_libraries(game_server PRIVATE 
        ${CONAN_LIBS}
        boost_log
        boost_thread
        Threads::Threads
    )
endif()

target_include_directories(game_server PRIVATE src)

set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)