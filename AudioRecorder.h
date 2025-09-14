//
// Created by Werty on 9/14/2025.
//

#ifndef COURSE_AUDIORECORDER_H
#define COURSE_AUDIORECORDER_H

#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <string>
#include <vector>
#include <csignal>

#pragma comment(lib,"winmm.lib")

class AudioRecorder {
public:
    AudioRecorder(int sampleRate = 44100, int channels = 2, int bitsPerSample = 16, int recordSeconds = 5);
    void run();

private:
    int sampleRate;
    int channels;
    int bitsPerSample;
    int recordSeconds;
    int NUMPTS;

    short* waveInBuffer;
    std::atomic<bool> isRecording;
    std::atomic<bool> stopRecording;
    std::atomic<bool> isRecordStart;
    std::atomic<bool> running;

    struct AudioContext {
        HWAVEIN hWaveIn;
        WAVEFORMATEX wfx;
        std::vector<WAVEHDR> headers;
        std::vector<std::vector<BYTE>> buffers;

        AudioContext();
    };

    static AudioRecorder* instance;

    static void signalHandlerStatic(int signal);
    void signalHandler(int signal);
    static std::string getCurrentDateTimeString();
    void saveToWav(const std::string& filename, short* data, int dataSize);
    void recordingThread();
    void startRecording();
    void stopRecordingNow();
    static void CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
    void monitorMicLevel();
};

#endif // AUDIORECORDER_H

#endif //COURSE_AUDIORECORDER_H