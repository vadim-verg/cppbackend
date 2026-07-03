# ------------------------------------------------------------------------------
# Этап 1: Сборка приложения
# ------------------------------------------------------------------------------
FROM ubuntu:22.04 AS build

# Устанавливаем системные зависимости для сборки C++
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Устанавливаем правильную версию Conan, как требует ТЗ
RUN pip3 install "conan<2.0"

# Копируем исходный код в контейнер
COPY . /app
WORKDIR /app

# Настраиваем профиль Conan по умолчанию под Linux
RUN conan profile new default --detect && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

# Собираем зависимости через Conan (одиночный cmake генератор)
RUN mkdir build && cd build && \
    conan install .. --build=missing -s build_type=Release

# Конфигурируем и компилируем проект через CMake в режиме Release
RUN cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build .

# ------------------------------------------------------------------------------
# Этап 2: Финальный минимальный образ для запуска
# ------------------------------------------------------------------------------
FROM ubuntu:22.04 AS run

# Копируем только готовый скомпилированный исполняемый файл из этапа сборки
COPY --from=build /app/build/bin/game_server /app/game_server

# Копируем папки со статикой и конфигурационными данными карт
COPY data /app/data
COPY static /app/static

WORKDIR /app

# Инструкция запуска сервера при старте контейнера
ENTRYPOINT ["./game_server", "data/config.json", "static"]