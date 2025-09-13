#include <iostream>
#include <windows.h>
#include <mmsystem.h>
#include <fstream>
#include <conio.h>
#include <thread>
#include <atomic>
#include "VolumeScaner.hpp"

#pragma comment(lib,"winmm.lib")

using namespace std;

const int NUMPTS = 44100 * 2 * 5; // 5 секунд записи
int sampleRate = 44100;
short int* waveIn = nullptr;
atomic<bool> isRecording(false);
atomic<bool> stopRecording(false);

void saveToWav(const char* filename, short int* data, int dataSize) {
    ofstream file(filename, ios::binary);

    // WAV file header
    file << "RIFF";  // ChunkID
    int chunkSize = 36 + dataSize;
    file.write((char*)&chunkSize, 4); // ChunkSize
    file << "WAVE";  // Format

    file << "fmt ";  // Subchunk1ID
    int subchunk1Size = 16; // for PCM
    file.write((char*)&subchunk1Size, 4);
    short int audioFormat = 1; // PCM
    file.write((char*)&audioFormat, 2);
    short int numChannels = 2; // Stereo
    file.write((char*)&numChannels, 2);
    file.write((char*)&sampleRate, 4);
    int byteRate = sampleRate * numChannels * 16 / 8;
    file.write((char*)&byteRate, 4);
    short int blockAlign = numChannels * 16 / 8;
    file.write((char*)&blockAlign, 2);
    short int bitsPerSample = 16;
    file.write((char*)&bitsPerSample, 2);

    file << "data";  // Subchunk2ID
    file.write((char*)&dataSize, 4); // Subchunk2Size
    file.write((char*)data, dataSize); // Write audio data

    file.close();
}

void recordingThread() {
    WAVEFORMATEX pFormat;
    pFormat.wFormatTag = WAVE_FORMAT_PCM;
    pFormat.nChannels = 2;
    pFormat.wBitsPerSample = 16;
    pFormat.nSamplesPerSec = sampleRate;
    pFormat.nAvgBytesPerSec = sampleRate * pFormat.nChannels * pFormat.wBitsPerSample / 8;
    pFormat.nBlockAlign = pFormat.nChannels * pFormat.wBitsPerSample / 8;
    pFormat.cbSize = 0;

    HWAVEIN hWaveIn;
    WAVEHDR waveInHdr;

    // Выделяем память для записи
    waveIn = new short int[NUMPTS];
    ZeroMemory(waveIn, NUMPTS * sizeof(short int));

    // Открываем устройство записи
    if (waveInOpen(&hWaveIn, WAVE_MAPPER, &pFormat, 0L, 0L, WAVE_FORMAT_DIRECT) != MMSYSERR_NOERROR) {
        cerr << "Ошибка открытия устройства записи!" << endl;
        isRecording = false;
        return;
    }

    // Подготавливаем заголовок
    waveInHdr.lpData = (LPSTR)waveIn;
    waveInHdr.dwBufferLength = NUMPTS * sizeof(short int);
    waveInHdr.dwBytesRecorded = 0;
    waveInHdr.dwUser = 0L;
    waveInHdr.dwFlags = 0L;
    waveInHdr.dwLoops = 0L;

    waveInPrepareHeader(hWaveIn, &waveInHdr, sizeof(WAVEHDR));
    waveInAddBuffer(hWaveIn, &waveInHdr, sizeof(WAVEHDR));

    // Начинаем запись
    waveInStart(hWaveIn);
    cout << "Запись началась... Нажмите 'S' для остановки" << endl;

    // Ждем остановки записи
    while (!stopRecording) {
        Sleep(100); // Небольшая задержка для снижения нагрузки на CPU
    }

    // Останавливаем запись
    waveInStop(hWaveIn);
    waveInReset(hWaveIn);

    // Освобождаем ресурсы
    waveInUnprepareHeader(hWaveIn, &waveInHdr, sizeof(WAVEHDR));
    waveInClose(hWaveIn);

    // Сохраняем записанные данные
    saveToWav("output.wav", waveIn, waveInHdr.dwBytesRecorded);
    cout << "Запись сохранена в output.wav" << endl;

    // Освобождаем память
    delete[] waveIn;
    waveIn = nullptr;
    isRecording = false;
    stopRecording = false;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    cout << "Программа записи звука" << endl;
    cout << "Нажмите 'R' для начала записи" << endl;
    cout << "Нажмите 'S' для остановки записи" << endl;
    cout << "Нажмите 'Q' для выхода" << endl;

    while (true) {
        if (_kbhit()) {
            char key = _getch();
            key = toupper(key);

            if (key == 'R' && !isRecording) {
                isRecording = true;
                stopRecording = false;
                thread recordThread(recordingThread);
                recordThread.detach(); // Запускаем поток записи в фоне
            }
            else if (key == 'S' && isRecording) {
                stopRecording = true;
                cout << "Остановка записи..." << endl;
            }
            else if (key == 'Q') {
                if (isRecording) {
                    stopRecording = true;
                    cout << "Останавливаем запись и выходим..." << endl;
                    Sleep(1000); // Даем время для завершения записи
                }
                break;
            }
        }
        Sleep(100); // Небольшая задержка для снижения нагрузки на CPU
    }

    return 0;
}