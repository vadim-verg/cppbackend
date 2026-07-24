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

# СТРОГО ПО ИНСТРУКЦИИ: Устанавливаем Conan конкретной версии 1.52.0
RUN pip3 install conan==1.52.0

# Копируем проект
COPY . /app
WORKDIR /app

# Настраиваем профиль Conan
RUN conan profile new default --detect && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

# СТРОГО ПО ИНСТРУКЦИИ: Создаем build папку и ставим пакеты с жесткими флагами
RUN mkdir build && cd build && \
    conan install .. --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11

# Конфигурируем и собираем бинарник в режиме Release
RUN cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -- -j$(nproc)

# ------------------------------------------------------------------------------
# Этап 2: Финальный минимальный образ для запуска
# ------------------------------------------------------------------------------
FROM ubuntu:22.04 AS run

# Создаем системного пользователя www для безопасности
RUN groupadd -r www && useradd -r -g www www

WORKDIR /

# Копируем готовый сервер строго по пути /app/game_server, как ожидает робот
COPY --from=build /app/build/bin/game_server /app/game_server

# Копируем обязательные папки с картами и фронтендом
COPY data /app/data
COPY static /app/static

# Переключаемся в рабочую папку с ресурсами и на пользователя www
WORKDIR /app
USER www

# Точка входа по умолчанию
ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "data/config.json", "--www-root", "static"]