#pragma once
#include <Arduino.h>
#include <SD.h>


// Define VERBOSE for extra log output. 
// #define VERBOSE


// Define for the ability to play stereo files. 
// Keep in mind that stereo uses double the bandwidth. 
#define WAVPLAYER_CAN_PLAY_STEREO


// Async reader settings
#define WAVPLAYER_DATA_READER_PRIORITY 3  // FreeRTOS task priority. Set higher if the audio is choppy, lower if loop() gets blocked
#define WAVPLAYER_DATA_READER_STACK_SIZE 4096


#define TAG "WAVPlayer"

#define WAV_DATA_BUFFER_SIZE 4096 // Set this to a higher value if the audio clicks or repeats

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
    #ifdef WAVPLAYER_CAN_PLAY_STEREO 
        bool begin(const int pwm_channel_l, const int pwm_channel_r, const int pin_l, const int pin_r, const int pwm_frequency = 75000, const int pwm_resolution = 8);
    #else
        bool begin(const int pwm_channel, const int output_pin, const int pwm_frequency = 75000, const int pwm_resolution = 8);
    #endif
    // bool loop();
    bool play(String path, bool print_progress = false);
    bool stop();
    bool pause();
    bool unpause();
    void volume(uint8_t v);
    bool isBusy();
}
