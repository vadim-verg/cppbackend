#pragma once

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

class Recorder {
    static void Callback(ma_device* pDevice, void* pOutput, const void* pInput,
                         ma_uint32 frameCount) {
        Recorder* recorder = reinterpret_cast<Recorder*>(pDevice->pUserData);
        recorder->SaveBuffer(pInput, frameCount);
        (void)pOutput;
    }

    void SaveBuffer(const void* pInput, ma_uint32 frameCount) {
        // Вычисляем, сколько байт физически осталось до конца буфера
        size_t remaining_bytes = buffer_.size() - current_off_;
        size_t requested_bytes = static_cast<size_t>(frameCount * frame_size_);
        size_t bytes_to_copy = std::min(requested_bytes, remaining_bytes);

        if (bytes_to_copy > 0) {
            std::copy_n(reinterpret_cast<const char*>(pInput), bytes_to_copy, buffer_.data() + current_off_);
            current_off_ += bytes_to_copy;
        }

        // Если буфер заполнен до конца, сигнализируем главному потоку об остановке
        if (current_off_ >= buffer_.size()) {
            is_finished_.store(true);
        }
    }

public:
    Recorder(ma_format format, int channels) {
        ma_device_config device_config = ma_device_config_init(ma_device_type_capture);
        device_config.capture.pDeviceID = NULL;
        device_config.capture.format = format;
        device_config.capture.channels = channels;
        device_config.sampleRate = 44100;
        device_config.dataCallback = Callback;
        device_config.pUserData = this;

        frame_size_ = ma_get_bytes_per_frame(format, channels);
        init_result_ = ma_device_init(NULL, &device_config, &device_);
    }

    ~Recorder() {
        ma_device_uninit(&device_);
    }

    struct RecordingResult {
        std::vector<char> data;
        size_t frames;
    };

    // Параметр dur больше не нужен, размер буфера жестко задает длительность!
    // При 44100 Гц, 65000 фреймов — это ровно ~1.47 секунды записи.
    RecordingResult Record(size_t max_frames) {
        current_off_ = 0;
        is_finished_.store(false);
        buffer_.resize(max_frames * frame_size_);

        ma_device_start(&device_);
        
        // Активное ожидание завершения записи в Callback (без сонного зависания намертво)
        while (!is_finished_.load()) {
            std::this_thread::yield(); 
        }
        
        ma_device_stop(&device_);

        return {std::move(buffer_), current_off_ / frame_size_};
    }

private:
    ma_device device_;
    ma_result init_result_;
    int frame_size_;
    std::vector<char> buffer_;
    size_t current_off_;
    std::atomic<bool> is_finished_; // Флаг для синхронизации потоков
};

class Player {
    static void Callback(ma_device* pDevice, void* pOutput, const void* pInput,
                         ma_uint32 frameCount) {
        Player* player = reinterpret_cast<Player*>(pDevice->pUserData);
        player->FillBuffer(pOutput, frameCount);
        (void)pInput;
    }

    void FillBuffer(void* pOutput, ma_uint32 frameCount) {
        size_t total_bytes = max_frame_ * frame_size_;
        size_t remaining_bytes = total_bytes - current_off_;
        size_t requested_bytes = static_cast<size_t>(frameCount * frame_size_);
        size_t bytes_to_copy = std::min(requested_bytes, remaining_bytes);

        if (bytes_to_copy > 0) {
            std::copy_n(current_buffer_ + current_off_, bytes_to_copy, reinterpret_cast<char*>(pOutput));
            current_off_ += bytes_to_copy;
        }

        // Если скопировали меньше, чем просил miniaudio (аудио кончилось), забиваем остаток тишиной
        if (bytes_to_copy < requested_bytes) {
            std::fill_n(reinterpret_cast<char*>(pOutput) + bytes_to_copy, requested_bytes - bytes_to_copy, 0);
        }

        // Если проиграли весь буфер, сигнализируем об окончании
        if (current_off_ >= total_bytes) {
            is_finished_.store(true);
        }
    }

public:
    Player(ma_format format, int channels) {
        ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
        device_config.playback.pDeviceID = NULL;
        device_config.playback.format = format;
        device_config.playback.channels = channels;
        device_config.sampleRate = 44100;
        device_config.dataCallback = Callback;
        device_config.pUserData = this;

        frame_size_ = ma_get_bytes_per_frame(format, channels);
        init_result_ = ma_device_init(NULL, &device_config, &device_);
    }

    ~Player() {
        ma_device_uninit(&device_);
    }

    void PlayBuffer(const char* data, size_t frames) {
        current_buffer_ = data;
        current_off_ = 0;
        max_frame_ = frames;
        is_finished_.store(false);

        ma_device_start(&device_);
        
        // Ждем, пока Callback физически не воспроизведет все фреймы
        while (!is_finished_.load()) {
            std::this_thread::yield();
        }
        
        ma_device_stop(&device_);
    }

private:
    ma_device device_;
    ma_result init_result_;
    int frame_size_;
    const char* current_buffer_;
    size_t current_off_;
    size_t max_frame_;
    std::atomic<bool> is_finished_; // Флаг для синхронизации потоков
};
