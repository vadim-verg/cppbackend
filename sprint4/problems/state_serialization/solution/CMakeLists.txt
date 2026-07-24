cmake_minimum_required(VERSION 3.15)
project(game_server CXX)

# Конфигурация макросов для Boost
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
    add_compile_definitions(BOOST_LOG_USE_STATIC_LIBS)
    add_compile_definitions(BOOST_ALL_NO_LIB)
else()
    # На Linux принудительно линкуем статический Boost.Log, как собрал Конан
    add_compile_definitions(BOOST_LOG_USE_STATIC_LIBS)
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

# Интеграция с Конаном (Работает и в Linux Docker, и на Windows)
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
else()
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
endif()

# Инициализация таргетов Конана
conan_basic_setup(TARGETS)

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
    src/ticker.h
    src/collision_detector.cpp
    src/collision_detector.h
    src/geom.h
    src/loot_generator.h
    src/loot_generator.cpp
    src/loot_provider.h
    src/serialization.h
    src/state_manager.h
)

# Линковка через таргеты Конана и системные библиотеки
if(WIN32)
    target_link_libraries(game_server PRIVATE 
        CONAN_PKG::boost 
        ws2_32 
        mswsock
    )
else()
    # Линкуем через прямые таргеты компонентов Конана, чтобы линкер строго соблюдал порядок
    target_link_libraries(game_server PRIVATE 
        CONAN_PKG::boost
        CONAN_LIBS
        Threads::Threads
    )
endif()

target_include_directories(game_server PRIVATE src)

set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)