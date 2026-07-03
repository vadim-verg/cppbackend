cmake_minimum_required(VERSION 3.15)
project(game_server CXX)

# Конфигурация макросов для Windows / MSVC
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
    add_compile_definitions(BOOST_LOG_USE_STATIC_LIBS)
    add_compile_definitions(BOOST_ALL_NO_LIB)
endif()

# Стандарт C++20, необходимый для Яндекс Практикума
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Строгие флаги компиляции
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    # Оставляем информативные предупреждения, но не прерываем сборку из-за багов Boost
    add_compile_options(-Wall -Wextra)
endif()

# Интеграция с ОДИНОЧНЫМ генератором Conan (conanbuildinfo.cmake)
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
else()
    message(FATAL_ERROR "Conan build info not found! Run 'conan install' first.")
endif()

conan_basic_setup()

# Таргет сборки (строго файлы из твоего списка)
add_executable(game_server
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
    src/request_handler.cpp
    src/request_handler.h
    src/sdk.h
    src/tagged.h
)

# Линковка библиотек
if(WIN32)
    target_link_libraries(game_server PRIVATE ${CONAN_LIBS} ws2_32 mswsock)
else()
    target_link_libraries(game_server PRIVATE ${CONAN_LIBS})
endif()

# Пути поиска заголовочных файлов внутри src
target_include_directories(game_server PRIVATE src)

# Папка вывода готового скомпилированного файла
set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)