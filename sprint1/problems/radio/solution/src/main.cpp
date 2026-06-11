#include "audio.h"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    // Карта инициализируется на формат Unsigned 8-bit, 1 канал (моно)
    Recorder recorder(ma_format_u8, 1);
    Player player(ma_format_u8, 1);

    while (true) {
        std::string str;

        std::cout << "Press Enter to record message..." << std::endl;
        std::getline(std::cin, str);

        // 65000 фреймов при частоте 44100 Гц — это ровно ~1.47 секунды чистой записи
        auto rec_result = recorder.Record(65000);
        std::cout << "Recording done. Frames captured: " << rec_result.frames << std::endl;

        // Воспроизводим ровно столько фреймов, сколько реально записали
        player.PlayBuffer(rec_result.data.data(), rec_result.frames);
        std::cout << "Playing done\n" << std::endl;
    }

    return 0;
}
