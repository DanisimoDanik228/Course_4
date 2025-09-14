#include "AudioRecorder.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <format>
#include <algorithm>
#include <csignal>

AudioRecorder* AudioRecorder::instance = nullptr;

AudioRecorder::AudioRecorder(int sampleRate, int channels, int bitsPerSample, int recordSeconds)
    : sampleRate(sampleRate), channels(channels), bitsPerSample(bitsPerSample),
      recordSeconds(recordSeconds), waveInBuffer(nullptr),
      isRecording(false), stopRecording(false), isRecordStart(false), running(false) {
    NUMPTS = sampleRate * channels * recordSeconds;
    latestLevel.store(0.0);
}

AudioRecorder::AudioContext::AudioContext() : hWaveIn(nullptr) {
    ZeroMemory(&wfx, sizeof(WAVEFORMATEX));
}

void AudioRecorder::start() {
    if (running) {
        std::cout << "AudioRecorder already running\n";
        return;
    }

    running = true;
    workerThread = std::thread(&AudioRecorder::run, this);
    std::cout << "AudioRecorder started in separate thread\n";
}

void AudioRecorder::stop() {
    if (!running) {
        return;
    }

    running = false;
    levelCV.notify_all();

    // Останавливаем запись если она активна
    stopRecordingNow();

    if (workerThread.joinable()) {
        workerThread.join();
    }

    std::cout << "AudioRecorder stopped\n";
}

void AudioRecorder::run() {
    std::signal(SIGINT, signalHandlerStatic);
    instance = this;

    std::cout << "Audio monitoring started in thread: " << std::this_thread::get_id() << "\n";
    monitorMicLevel();
    std::cout << "Audio monitoring finished\n";
}

void AudioRecorder::signalHandlerStatic(int signal) {
    if (instance) instance->signalHandler(signal);
}

void AudioRecorder::signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nCtrl+C received. Exiting...\n";
        running = false;
    }
}

std::string AudioRecorder::getCurrentDateTimeString() {
    auto now = std::chrono::system_clock::now();
    return std::format("{:%Y-%m-%d_%H-%M-%S}", now);
}

void AudioRecorder::saveToWav(const std::string& filename, short* data, int dataSize) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    // RIFF chunk
    file.write("RIFF", 4);
    int chunkSize = 36 + dataSize;
    file.write(reinterpret_cast<const char*>(&chunkSize), 4);
    file.write("WAVE", 4);

    // fmt subchunk
    file.write("fmt ", 4);
    int subchunk1Size = 16;
    file.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    short audioFormat = 1; // PCM
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    int byteRate = sampleRate * channels * bitsPerSample / 8;
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    short blockAlign = channels * bitsPerSample / 8;
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data subchunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
    file.write(reinterpret_cast<const char*>(data), dataSize);

    file.close();
    std::cout << "Recording saved to " << filename << " (" << dataSize << " bytes)" << std::endl;
}

void AudioRecorder::recordingThread() {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = channels;
    format.wBitsPerSample = bitsPerSample;
    format.nSamplesPerSec = sampleRate;
    format.nAvgBytesPerSec = sampleRate * channels * bitsPerSample / 8;
    format.nBlockAlign = channels * bitsPerSample / 8;
    format.cbSize = 0;

    HWAVEIN hWaveIn;
    WAVEHDR waveInHdr;

    waveInBuffer = new short[NUMPTS];
    ZeroMemory(waveInBuffer, NUMPTS * sizeof(short));

    MMRESULT result = waveInOpen(&hWaveIn, WAVE_MAPPER, &format,
                               0L, 0L, WAVE_FORMAT_DIRECT);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Error opening recording device! Error code: " << result << std::endl;
        delete[] waveInBuffer;
        waveInBuffer = nullptr;
        isRecording = false;
        return;
    }

    // Подготавливаем заголовок
    ZeroMemory(&waveInHdr, sizeof(WAVEHDR));
    waveInHdr.lpData = (LPSTR)waveInBuffer;
    waveInHdr.dwBufferLength = NUMPTS * sizeof(short);
    waveInHdr.dwFlags = 0;

    result = waveInPrepareHeader(hWaveIn, &waveInHdr, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Error preparing header! Error code: " << result << std::endl;
        waveInClose(hWaveIn);
        delete[] waveInBuffer;
        waveInBuffer = nullptr;
        isRecording = false;
        return;
    }

    result = waveInAddBuffer(hWaveIn, &waveInHdr, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Error adding buffer! Error code: " << result << std::endl;
        waveInUnprepareHeader(hWaveIn, &waveInHdr, sizeof(WAVEHDR));
        waveInClose(hWaveIn);
        delete[] waveInBuffer;
        waveInBuffer = nullptr;
        isRecording = false;
        return;
    }

    result = waveInStart(hWaveIn);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Error starting recording! Error code: " << result << std::endl;
        waveInUnprepareHeader(hWaveIn, &waveInHdr, sizeof(WAVEHDR));
        waveInClose(hWaveIn);
        delete[] waveInBuffer;
        waveInBuffer = nullptr;
        isRecording = false;
        return;
    }

    std::cout << "Recording started...\n";

    // Ждем пока буфер заполнится или пока не получим команду остановки
    while (!stopRecording) {
        if (waveInHdr.dwFlags & WHDR_DONE) {
            break;
        }
        Sleep(100);
    }

    // Останавливаем запись
    waveInStop(hWaveIn);
    waveInReset(hWaveIn);
    waveInUnprepareHeader(hWaveIn, &waveInHdr, sizeof(WAVEHDR));
    waveInClose(hWaveIn);

    // Сохраняем только если есть данные
    if (waveInHdr.dwBytesRecorded > 0) {
        std::string filename = "output_" + getCurrentDateTimeString() + ".wav";
        saveToWav(filename, waveInBuffer, waveInHdr.dwBytesRecorded);
    } else {
        std::cout << "No audio data recorded\n";
    }

    delete[] waveInBuffer;
    waveInBuffer = nullptr;
    isRecording = false;
    stopRecording = false;
}

void AudioRecorder::startRecording() {
    if (!isRecording) {
        isRecording = true;
        stopRecording = false;
        recordThread = std::thread(&AudioRecorder::recordingThread, this);
        recordThread.detach();
    }
}

void AudioRecorder::stopRecordingNow() {
    if (isRecording) {
        stopRecording = true;
        // Ждем завершения потока записи
        if (recordThread.joinable()) {
            recordThread.join();
        }
        isRecording = false;
    }
}

void CALLBACK AudioRecorder::waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (uMsg != WIM_DATA) return;

    auto* ctx = reinterpret_cast<AudioContext*>(dwInstance);
    auto* hdr = reinterpret_cast<WAVEHDR*>(dwParam1);

    const int16_t* samples = reinterpret_cast<const int16_t*>(hdr->lpData);
    size_t sampleCount = hdr->dwBytesRecorded / sizeof(int16_t);

    int peak = 0;
    for (size_t i = 0; i < sampleCount; ++i) {
        peak = std::max(peak, std::abs((int)samples[i]));
    }
    double level = std::min(100.0, (peak / 32767.0) * 100.0);

    if (instance) {
        instance->latestLevel.store(level);

        {
            std::lock_guard<std::mutex> lock(instance->levelMutex);
            instance->levelQueue.push(level);
            if (instance->levelQueue.size() > 100) {
                instance->levelQueue.pop();
            }
        }
        instance->levelCV.notify_one();

        // Логика автоматической записи
        if (instance->isRecordStart) {
            if (level <= 10.0) {
                instance->isRecordStart = false;
                instance->stopRecordingNow();
            }
        } else {
            if (level > 10.0) {
                instance->isRecordStart = true;
                instance->startRecording();
            }
        }
    }

    // Переиспользуем буфер
    waveInAddBuffer(ctx->hWaveIn, hdr, sizeof(WAVEHDR));
}

double AudioRecorder::getLatestLevel() {
    return latestLevel.load();
}

bool AudioRecorder::hasNewLevel() {
    std::lock_guard<std::mutex> lock(levelMutex);
    return !levelQueue.empty();
}

void AudioRecorder::clearLevels() {
    std::lock_guard<std::mutex> lock(levelMutex);
    while (!levelQueue.empty()) {
        levelQueue.pop();
    }
}

bool AudioRecorder::isRunning() const {
    return running.load();
}

template<typename T>
bool AudioRecorder::getNextLevel(T& level, int timeoutMs) {
    std::unique_lock<std::mutex> lock(levelMutex);

    if (levelCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
        [this]() { return !levelQueue.empty() || !running; })) {

        if (!levelQueue.empty()) {
            level = levelQueue.front();
            levelQueue.pop();
            return true;
        }
    }
    return false;
}

void AudioRecorder::monitorMicLevel() {
    AudioContext ctx;
    ctx.wfx.wFormatTag = WAVE_FORMAT_PCM;
    ctx.wfx.nChannels = 1;
    ctx.wfx.nSamplesPerSec = 44100;
    ctx.wfx.wBitsPerSample = 16;
    ctx.wfx.nBlockAlign = (ctx.wfx.nChannels * ctx.wfx.wBitsPerSample) / 8;
    ctx.wfx.nAvgBytesPerSec = ctx.wfx.nSamplesPerSec * ctx.wfx.nBlockAlign;

    MMRESULT result = waveInOpen(&ctx.hWaveIn, WAVE_MAPPER, &ctx.wfx,
                               (DWORD_PTR)waveInProc, (DWORD_PTR)&ctx,
                               CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to open recording device. Error code: " << result << std::endl;
        return;
    }

    const DWORD bufferMs = 250;
    const DWORD bufferBytes = (ctx.wfx.nAvgBytesPerSec * bufferMs) / 1000;
    const int bufferCount = 2;

    ctx.buffers.resize(bufferCount);
    ctx.headers.resize(bufferCount);

    for (int i = 0; i < bufferCount; ++i) {
        ctx.buffers[i].resize(bufferBytes);
        ZeroMemory(&ctx.headers[i], sizeof(WAVEHDR));
        ctx.headers[i].lpData = reinterpret_cast<LPSTR>(ctx.buffers[i].data());
        ctx.headers[i].dwBufferLength = bufferBytes;

        result = waveInPrepareHeader(ctx.hWaveIn, &ctx.headers[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            std::cerr << "Error preparing header " << i << ". Error code: " << result << std::endl;
            waveInClose(ctx.hWaveIn);
            return;
        }

        result = waveInAddBuffer(ctx.hWaveIn, &ctx.headers[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            std::cerr << "Error adding buffer " << i << ". Error code: " << result << std::endl;
            waveInUnprepareHeader(ctx.hWaveIn, &ctx.headers[i], sizeof(WAVEHDR));
            waveInClose(ctx.hWaveIn);
            return;
        }
    }

    result = waveInStart(ctx.hWaveIn);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Error starting monitoring. Error code: " << result << std::endl;
        for (int i = 0; i < bufferCount; ++i) {
            waveInUnprepareHeader(ctx.hWaveIn, &ctx.headers[i], sizeof(WAVEHDR));
        }
        waveInClose(ctx.hWaveIn);
        return;
    }

    std::cout << "Audio monitoring started successfully\n";

    while (running) {
        Sleep(50);
    }

    waveInStop(ctx.hWaveIn);
    waveInReset(ctx.hWaveIn);

    for (int i = 0; i < bufferCount; ++i) {
        waveInUnprepareHeader(ctx.hWaveIn, &ctx.headers[i], sizeof(WAVEHDR));
    }

    waveInClose(ctx.hWaveIn);
    std::cout << "Audio monitoring stopped\n";
}

// Явная инстанциация шаблона
template bool AudioRecorder::getNextLevel<double>(double& level, int timeoutMs);