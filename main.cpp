#include <iostream>
#include <windows.h>
#include <mmsystem.h>
#include <fstream>

#pragma comment(lib,"winmm.lib")

using namespace std;

const int NUMPTS = 44100 * 2 * 5;
int sampleRate = 44100;
short int waveIn[NUMPTS];

void saveToWav(const char* filename, short int* data, int dataSize) {
    ofstream file(filename, ios::binary);

    // WAV file header
    file << "RIFF";  // ChunkID
    file.write((char*)&dataSize, 4); // ChunkSize
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

int main() {
    WAVEFORMATEX pFormat;
    pFormat.wFormatTag = WAVE_FORMAT_PCM;     // simple, uncompressed format
    pFormat.nChannels = 2;                    //  1=mono, 2=stereo
    pFormat.wBitsPerSample = 16;              //  16 for high quality, 8 for telephone-grade
    pFormat.nSamplesPerSec = sampleRate;
    pFormat.nAvgBytesPerSec = sampleRate * pFormat.nChannels * pFormat.wBitsPerSample / 8;
    pFormat.nBlockAlign = pFormat.nChannels * pFormat.wBitsPerSample / 8;
    pFormat.cbSize = 0;

    HWAVEIN hWaveIn;
    WAVEHDR waveInHdr;

    waveInOpen(&hWaveIn, WAVE_MAPPER, &pFormat, 0L, 0L, WAVE_FORMAT_DIRECT);

    waveInHdr.lpData = (LPSTR)waveIn;
    waveInHdr.dwBufferLength = NUMPTS * 2;
    waveInHdr.dwBytesRecorded = 0;
    waveInHdr.dwUser = 0L;
    waveInHdr.dwFlags = 0L;
    waveInHdr.dwLoops = 0L;

    waveInPrepareHeader(hWaveIn, &waveInHdr, sizeof(WAVEHDR));

    waveInAddBuffer(hWaveIn, &waveInHdr, sizeof(WAVEHDR));

    waveInStart(hWaveIn);

    // Wait until finished recording
    do {} while (waveInUnprepareHeader(hWaveIn, &waveInHdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING);

    waveInClose(hWaveIn);

    // Save the recorded audio to a file
    saveToWav("output.wav", waveIn, NUMPTS * sizeof(short int));

    return 0;
}