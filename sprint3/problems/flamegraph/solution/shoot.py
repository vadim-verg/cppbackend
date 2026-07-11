import argparse
import subprocess
import time
import random
import shlex
import os
import signal

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

# Константы строго из условий задачи
SHOOT_COUNT = 100
COOLDOWN = 0.1


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def run(command, output=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process


def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


# 1. Запуск сервера с помощью переданного аргумента командной строки
server_command = start_server()
server = run(server_command)

# Даем серверу время на инициализацию сетевого порта
time.sleep(1.0)

# 2. Запуск процесса perf record с явным указанием файла вывода (-o)
# Трассируем конкретный PID сервера (-p) и собираем стек вызовов (-g)
perf_data_file = "perf.data"
perf_command = f"perf record -g -F 999 -p {server.pid} -o {perf_data_file}"
perf_process = run(perf_command)

# КРИТИЧЕСКИЙ ШАГ ДЛЯ CI/CD: Ждем 3 секунды, чтобы утилита perf успела полноценно
# инициализироваться в контейнере и начать перехват до того, как прилетит первый curl
time.sleep(3.0)

# 3. Выполнение обстрела сервера запросами (длится около 10 секунд)
make_shots()

# Даем perf зафиксировать финальные системные тики после окончания стрельбы
time.sleep(1.0)

# 4. Корректное завершение работы perf record
# Посылаем сигнал SIGINT (аналог Ctrl+C), чтобы perf успел записать заголовки и таблицы символов
if perf_process.poll() is None:
    perf_process.send_signal(signal.SIGINT)
    perf_process.wait()  # Ждем окончания финализации файла данных на диске

# Остановка сервера
stop(server)
time.sleep(1.0)
print('Job done')

# 5. Построение флеймграфа через двойной пайп
# Определяем пути к Perl-скриптам из каталога FlameGraph, расположенного рядом со скриптом
current_dir = os.path.dirname(os.path.abspath(__file__))
stackcollapse_path = os.path.join(current_dir, "FlameGraph", "stackcollapse-perf.pl")
flamegraph_path = os.path.join(current_dir, "FlameGraph", "flamegraph.pl")

try:
    # Запуск perf script с явным указанием входного файла (-i)
    perf_script_proc = subprocess.Popen(
        shlex.split(f"perf script -i {perf_data_file}"),
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )
    
    # Первый пайп: Схлопывание одинаковых стеков вызовов
    collapse_proc = subprocess.Popen(
        [stackcollapse_path],
        stdin=perf_script_proc.stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )
    # Важно закрыть поток чтения в родительском процессе, чтобы collapse_proc корректно получил SIGPIPE
    perf_script_proc.stdout.close()
    
    # Второй пайп: Считывание схлопнутых стеков и генерация финального graph.svg
    with open("graph.svg", "w") as svg_file:
        flame_proc = subprocess.Popen(
            [flamegraph_path],
            stdin=collapse_proc.stdout,
            stdout=svg_file,
            stderr=subprocess.DEVNULL
        )
        collapse_proc.stdout.close()  # Закрываем поток для корректного завершения
        flame_proc.wait()             # Дожидаемся окончания записи файла graph.svg
        
    print("Flamegraph generated successfully inside graph.svg")

except Exception as e:
    print(f"Error during FlameGraph generation: {e}")