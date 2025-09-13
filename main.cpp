#include <iostream>
#include <windows.h>
#include <mmsystem.h>
#include <fstream>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>
#include <format>
#include <vector>
#include <csignal>
#include <algorithm>

#pragma comment(lib,"winmm.lib")

class AudioRecorder {
public:
    AudioRecorder(int sampleRate = 44100, int channels = 2, int bitsPerSample = 16, int recordSeconds = 5)
        : sampleRate(sampleRate), channels(channels), bitsPerSample(bitsPerSample),
          recordSeconds(recordSeconds), isRecording(false), stopRecording(false), isRecordStart(false) {
        NUMPTS = sampleRate * channels * recordSeconds;
    }

    void run() {
        std::signal(SIGINT, signalHandlerStatic);
        instance = this;

        std::cout << "Program started. Press Ctrl+C to exit\n";
        monitorMicLevel();
    }

private:
    int sampleRate;
    int channels;
    int bitsPerSample;
    int recordSeconds;
    int NUMPTS;

    short* waveInBuffer = nullptr;
    std::atomic<bool> isRecording;
    std::atomic<bool> stopRecording;
    std::atomic<bool> isRecordStart;
    std::atomic<bool> running{ true };

    struct AudioContext {
        HWAVEIN hWaveIn = nullptr;
        WAVEFORMATEX wfx{};
        std::vector<WAVEHDR> headers;
        std::vector<std::vector<BYTE>> buffers;
    };

    static AudioRecorder* instance;

    static void signalHandlerStatic(int signal) {
        if (instance) instance->signalHandler(signal);
    }

    void signalHandler(int signal) {
        if (signal == SIGINT) {
            std::cout << "\nCtrl+C received. Exiting...\n";
            running = false;
        }
    }

    static std::string getCurrentDateTimeString() {
        auto now = std::chrono::system_clock::now();
        return std::format("{:%Y-%m-%d_%H-%M-%S}", now);
    }

    void saveToWav(const std::string& filename, short* data, int dataSize) {
        std::ofstream file(filename, std::ios::binary);

        file << "RIFF";
        int chunkSize = 36 + dataSize;
        file.write((char*)&chunkSize, 4);
        file << "WAVE";

        file << "fmt ";
        int subchunk1Size = 16;
        file.write((char*)&subchunk1Size, 4);
        short audioFormat = 1;
        file.write((char*)&audioFormat, 2);
        file.write((char*)&channels, 2);
        file.write((char*)&sampleRate, 4);
        int byteRate = sampleRate * channels * bitsPerSample / 8;
        file.write((char*)&byteRate, 4);
        short blockAlign = channels * bitsPerSample / 8;
        file.write((char*)&blockAlign, 2);
        file.write((char*)&bitsPerSample, 2);

        file << "data";
        file.write((char*)&dataSize, 4);
        file.write((char*)data, dataSize);
    }

    void recordingThread() {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = channels;
        format.wBitsPerSample = bitsPerSample;
        format.nSamplesPerSec = sampleRate;
        format.nAvgBytesPerSec = sampleRate * channels * bitsPerSample / 8;
        format.nBlockAlign = channels * bitsPerSample / 8;

        HWAVEIN hWaveIn;
        WAVEHDR waveInHdr;

        waveInBuffer = new short[NUMPTS];
        ZeroMemory(waveInBuffer, NUMPTS * sizeof(short));

        if (waveInOpen(&hWaveIn, WAVE_MAPPER, &format, 0L, 0L, WAVE_FORMAT_DIRECT) != MMSYSERR_NOERROR) {
            std::cerr << "Error opening recording device!\n";
            isRecording = false;
            return;
        }

        waveInHdr.lpData = (LPSTR)waveInBuffer;
        waveInHdr.dwBufferLength = NUMPTS * sizeof(short);
        waveInHdr.dwFlags = 0;

        waveInPrepareHeader(hWaveIn, &waveInHdr, sizeof(WAVEHDR));
        waveInAddBuffer(hWaveIn, &waveInHdr, sizeof(WAVEHDR));

        waveInStart(hWaveIn);
        std::cout << "Recording started...\n";

        while (!stopRecording) {
            Sleep(100);
        }

        waveInStop(hWaveIn);
        waveInReset(hWaveIn);
        waveInUnprepareHeader(hWaveIn, &waveInHdr, sizeof(WAVEHDR));
        waveInClose(hWaveIn);

        std::string filename = "output_" + getCurrentDateTimeString() + ".wav";
        saveToWav(filename, waveInBuffer, waveInHdr.dwBytesRecorded);
        std::cout << "Recording saved to " << filename << "\n";

        delete[] waveInBuffer;
        waveInBuffer = nullptr;
        isRecording = false;
        stopRecording = false;
    }

    void startRecording() {
        if (!isRecording) {
            isRecording = true;
            stopRecording = false;
            std::thread(&AudioRecorder::recordingThread, this).detach();
        }
    }

    void stopRecordingNow() {
        if (isRecording) {
            stopRecording = true;
        }
    }

    static void CALLBACK waveInProc(HWAVEIN, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR) {
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

        std::cout << "Speech level: " << level << "%\n";

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

        waveInAddBuffer(ctx->hWaveIn, hdr, sizeof(WAVEHDR));
    }

    void monitorMicLevel() {
        AudioContext ctx;
        ctx.wfx.wFormatTag = WAVE_FORMAT_PCM;
        ctx.wfx.nChannels = 1;
        ctx.wfx.nSamplesPerSec = 44100;
        ctx.wfx.wBitsPerSample = 16;
        ctx.wfx.nBlockAlign = (ctx.wfx.nChannels * ctx.wfx.wBitsPerSample) / 8;
        ctx.wfx.nAvgBytesPerSec = ctx.wfx.nSamplesPerSec * ctx.wfx.nBlockAlign;

        if (waveInOpen(&ctx.hWaveIn, WAVE_MAPPER, &ctx.wfx, (DWORD_PTR)waveInProc, (DWORD_PTR)&ctx, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
            std::cerr << "Failed to open recording device\n";
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

            waveInPrepareHeader(ctx.hWaveIn, &ctx.headers[i], sizeof(WAVEHDR));
            waveInAddBuffer(ctx.hWaveIn, &ctx.headers[i], sizeof(WAVEHDR));
        }

        waveInStart(ctx.hWaveIn);

        while (running) {
            Sleep(50);
        }

        waveInClose(ctx.hWaveIn);
    }
};

AudioRecorder* AudioRecorder::instance = nullptr;

int main() {
    SetConsoleOutputCP(CP_UTF8);
    AudioRecorder recorder;
    recorder.run();
    return 0;
}