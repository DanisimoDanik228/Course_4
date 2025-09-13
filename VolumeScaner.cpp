// mic_level_4hz.cpp
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <atomic>
#include <vector>
#include <algorithm>
#include <csignal>
#include <chrono>
#include <thread>

#pragma comment(lib, "winmm.lib")

static std::atomic<bool> g_running{ true };
const int TIME_SPAN = 250; //ms

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nПолучен Ctrl+C. Завершение..." << std::endl;
        g_running = false;
    }
}

struct AudioContext {
    HWAVEIN hWaveIn = nullptr;
    WAVEFORMATEX wfx{};
    std::vector<WAVEHDR> headers;
    std::vector<std::vector<BYTE>> buffers;
};

void PrintPrecent(float pr)
{
    pr /= 3;

    while (pr > 0)
    {
        std::cout << "#";
        pr--;
    }

    std::cout << std::endl;
}

void CALLBACK WaveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR /*dwParam2*/) {
    if (uMsg != WIM_DATA) return;

    AudioContext* ctx = reinterpret_cast<AudioContext*>(dwInstance);
    WAVEHDR* hdr = reinterpret_cast<WAVEHDR*>(dwParam1);

    // Расчёт пикового уровня для 16-бит моно
    const int16_t* samples = reinterpret_cast<const int16_t*>(hdr->lpData);
    const size_t sampleCount = hdr->dwBytesRecorded / sizeof(int16_t);
    int peak = 0;
    for (size_t i = 0; i < sampleCount; ++i) {
        int v = std::abs((int)samples[i]);
        if (v > peak) peak = v;
    }
    // Перевод в проценты (0..100)
    double level = std::min(100.0, (peak / 32767.0) * 100.0);

    std::cout << "Уровень речи: " << (int)(level + 0.5) << "%\n";
    //PrintPrecent(level);
    std::cout.flush();

    // Переиспользуем буфер
    waveInAddBuffer(ctx->hWaveIn, hdr, sizeof(WAVEHDR));
}

void externalCode() {
   std::cout << "Захват микрофона (4 раза/сек). Нажмите Ctrl+C для выхода.\n";

    AudioContext ctx;

    // Формат: 16-bit PCM, mono, 44100 Гц (широко поддерживается)
    ctx.wfx.wFormatTag = WAVE_FORMAT_PCM;
    ctx.wfx.nChannels = 1;
    ctx.wfx.nSamplesPerSec = 44100;
    ctx.wfx.wBitsPerSample = 16;
    ctx.wfx.nBlockAlign = (ctx.wfx.nChannels * ctx.wfx.wBitsPerSample) / 8;
    ctx.wfx.nAvgBytesPerSec = ctx.wfx.nSamplesPerSec * ctx.wfx.nBlockAlign;
    ctx.wfx.cbSize = 0;

    MMRESULT mmr = waveInOpen(&ctx.hWaveIn, WAVE_MAPPER, &ctx.wfx,
                              (DWORD_PTR)WaveInProc, (DWORD_PTR)&ctx,
                              CALLBACK_FUNCTION);
    if (mmr != MMSYSERR_NOERROR) {
        std::cerr << "Не удалось открыть устройство записи (waveInOpen), код: " << mmr << "\n";
        return;
    }

    // Длительность буфера = 250 мс (4 раза в секунду)
    const DWORD bufferMs = TIME_SPAN;
    const DWORD bufferBytes = (ctx.wfx.nAvgBytesPerSec * bufferMs) / 1000;

    // Двойная буферизация
    const int bufferCount = 2;
    ctx.buffers.resize(bufferCount);
    ctx.headers.resize(bufferCount);

    for (int i = 0; i < bufferCount; ++i) {
        ctx.buffers[i].resize(bufferBytes);
        ZeroMemory(&ctx.headers[i], sizeof(WAVEHDR));
        ctx.headers[i].lpData = reinterpret_cast<LPSTR>(ctx.buffers[i].data());
        ctx.headers[i].dwBufferLength = bufferBytes;

        mmr = waveInPrepareHeader(ctx.hWaveIn, &ctx.headers[i], sizeof(WAVEHDR));
        if (mmr != MMSYSERR_NOERROR) {
            std::cerr << "waveInPrepareHeader ошибка, код: " << mmr << "\n";
            g_running = false;
            break;
        }

        mmr = waveInAddBuffer(ctx.hWaveIn, &ctx.headers[i], sizeof(WAVEHDR));
        if (mmr != MMSYSERR_NOERROR) {
            std::cerr << "waveInAddBuffer ошибка, код: " << mmr << "\n";
            g_running = false;
            break;
        }
    }

    if (g_running) {
        mmr = waveInStart(ctx.hWaveIn);
        if (mmr != MMSYSERR_NOERROR) {
            std::cerr << "waveInStart ошибка, код: " << mmr << "\n";
            g_running = false;
        }
    }

    // Главный цикл
    while (g_running) {
        Sleep(50);
    }

    // Остановка и очистка
    //waveInStop(ctx.hWaveIn);
    //waveInReset(ctx.hWaveIn);

    for (int i = 0; i < bufferCount; ++i) {
        if (ctx.headers[i].dwFlags & WHDR_PREPARED) {
            waveInUnprepareHeader(ctx.hWaveIn, &ctx.headers[i], sizeof(WAVEHDR));
        }
    }

    waveInClose(ctx.hWaveIn);
}

int start() {

    SetConsoleOutputCP(CP_UTF8);
    // Установка обработчика сигнала
    std::signal(SIGINT, signalHandler);

    std::cout << "Программа запущена. Нажмите Ctrl+C для завершения" << std::endl;

    // Запуск стороннего кода
    externalCode();

    return 0;
}
