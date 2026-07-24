# ------------------------------------------------------------------------------
# Этап 1: Сборка приложения
# ------------------------------------------------------------------------------
FROM ubuntu:22.04 AS build

# Устанавливаем системные зависимости
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Устанавливаем Conan 1.x для сборки зависимостей
RUN pip3 install "conan<2.0"

# Копируем проект
COPY . /app
WORKDIR /app

# Настраиваем профиль и собираем через одиночный генератор cmake
RUN conan profile new default --detect && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

RUN mkdir build && cd build && \
    conan install .. --build=missing -s build_type=Release

# Конфигурируем и собираем бинарник в режиме Release
RUN cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build .

# ------------------------------------------------------------------------------
# Этап 2: Финальный минимальный образ для запуска
# ------------------------------------------------------------------------------
FROM ubuntu:22.04 AS run

# КРИТИЧНО ДЛЯ ТЕСТОВ ПРАКТИКУМА: Создаем системного пользователя www
RUN groupadd -r www && useradd -r -g www www

# Копируем готовый сервер из этапа сборки
COPY --from=build /app/build/bin/game_server /app/game_server

# Копируем обязательные папки с картами и симулятором фронтенда
COPY data /app/data
COPY static /app/static

# Переключаемся на созданного пользователя по требованию ТЗ
USER www
WORKDIR /app

# ИСПРАВЛЕНО: Разделяем точку входа и дефолтные параметры, чтобы тесты могли передавать свои флаги
ENTRYPOINT ["./game_server"]
CMD ["-c", "data/config.json", "-w", "static"]