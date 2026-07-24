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

# Явно задаем рабочую папку в корне системы
WORKDIR /

# Копируем готовый сервер из этапа сборки ПРЯМО в /app/game_server, как требует робот
COPY --from=build /app/build/bin/game_server /app/game_server

# Копируем обязательные папки с ресурсами внутрь папки /app
COPY data /app/data
COPY static /app/static

# Переключаемся на рабочую директорию /app, где лежат ресурсы
WORKDIR /app

# Переключаемся на созданного пользователя по требованию ТЗ безопасности
USER www

# Точка входа по умолчанию (когда запускаем вручную)
ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "data/config.json", "--www-root", "static"]