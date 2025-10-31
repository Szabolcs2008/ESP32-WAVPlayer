#pragma once
#include <Arduino.h>
#include <SD.h>



// #define VERBOSE

#define TAG "WAVPlayer"

#define PWM_CHANNEL 0
#define PWM_FREQ 75000
#define PWM_RESOLUTION 8
#define PWM_OUTPUT_PIN 2
#define DATA_BUFFER_SIZE 4096

typedef struct {
    char riff[4];
    unsigned long fileSize;
    char wave[4];
    char fmtmarker[4];
    unsigned long fmtLength;
    unsigned short fmtType;  // 1: PCM 
    unsigned short channelCount;
    unsigned long sampleRate; 
    unsigned long bytesPerSecond;
    unsigned short blockAlign;
    unsigned short bitsPerSample;
    char datamarker[4];
    unsigned short dataSize;
    unsigned long dataStartOffset;
} WavHeader; 

bool read_header(WavHeader& header_struct, File& file);

namespace WAVPlayer {
    bool loop();
    bool play(String path, bool isGUI);
    bool stop();
    bool pause();
    bool unpause();
    void volume(uint8_t v);
}
