#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <vector>
#include <string>
#include <functional>

class AudioRecorder {
public:
    static AudioRecorder* instance;

    AudioRecorder(int sampleRate = 44100, int channels = 1,
                 int bitsPerSample = 16, int recordSeconds = 10);

    void start();
    void stop();
    double getLatestLevel();
    bool hasNewLevel();
    void clearLevels();
    bool isRunning() const;

    void startRecording();
    void stopRecordingNow();

    template<typename T>
    bool getNextLevel(T& level, int timeoutMs = 100);

private:
    struct AudioContext {
        HWAVEIN hWaveIn;
        WAVEFORMATEX wfx;
        std::vector<std::vector<char>> buffers;
        std::vector<WAVEHDR> headers;

        AudioContext();
    };

    void run();
    void monitorMicLevel();
    void recordingThread();
    static void CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg,
                                   DWORD_PTR dwInstance, DWORD_PTR dwParam1,
                                   DWORD_PTR dwParam2);
    static void signalHandlerStatic(int signal);
    void signalHandler(int signal);
    std::string getCurrentDateTimeString();
    void saveToWav(const std::string& filename, short* data, int dataSize);

    // Параметры записи
    int sampleRate;
    int channels;
    int bitsPerSample;
    int recordSeconds;
    int NUMPTS;

    // Буферы и состояние
    short* waveInBuffer;
    std::atomic<bool> isRecording;
    std::atomic<bool> stopRecording;
    std::atomic<bool> isRecordStart;
    std::atomic<bool> running;

    // Для уровней звука
    std::atomic<double> latestLevel;
    std::queue<double> levelQueue;
    std::mutex levelMutex;
    std::condition_variable levelCV;

    // Потоки
    std::thread workerThread;
    std::thread recordThread;  // ← ДОБАВЬТЕ ЭТУ СТРОКУ
};

#endif // AUDIORECORDER_H