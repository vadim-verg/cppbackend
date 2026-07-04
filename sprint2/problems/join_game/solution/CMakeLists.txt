cmake_minimum_required(VERSION 3.15)
project(game_server CXX)

# Задаем версию Windows 10 для MSVC/Boost только если сборка идет под Windows
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
    add_compile_definitions(BOOST_LOG_USE_STATIC_LIBS)
    add_compile_definitions(BOOST_ALL_NO_LIB)
endif()

# Устанавливаем современный стандарт C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройки флагов компилятора под разные ОС
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    # Флаги для Linux (GCC/Clang): строгие предупреждения как ошибки
    add_compile_options(-Wall -Wextra)
endif()

# Подключаем конфигурацию зависимостей от Conan
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
elseif(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    # На Linux Практикум часто генерирует обычный conanbuildinfo.cmake вместо multi
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
else()
    message(FATAL_ERROR "Conan build info not found! Run 'conan install' first.")
endif()

conan_basic_setup()

# Описываем исполняемый файл и все исходники в папке src
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

# Линковка библиотек в зависимости от платформы
if(WIN32)
    # На Windows Conan управляет всем списком через ${CONAN_LIBS}
    target_link_libraries(game_server PRIVATE ${CONAN_LIBS} ws2_32 mswsock)
else()
    # ИСПРАВЛЕНИЕ ДЛЯ LINUX: Явно указываем линкеру подключить модули логирования и потоков,
    # так как одиночный генератор cmake в Conan на Linux иногда не включает их в общий список автоматически.
    target_link_libraries(game_server PRIVATE 
        ${CONAN_LIBS}
        boost_log
        boost_log_setup
        boost_thread
        boost_system
        boost_filesystem
        pthread
    )
endif()

# Указываем, что папка src является корневой для заголовочных файлов
target_include_directories(game_server PRIVATE src)

# Задаем директорию вывода готового бинарника
set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)