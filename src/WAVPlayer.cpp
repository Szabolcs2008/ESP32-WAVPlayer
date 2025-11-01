#include "WAVPlayer.h"
#include <SD.h>
#include "esp_log.h"

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
hw_timer_t *timer = NULL;
File soundFile;
WavHeader header;
uint64_t filePosition = 0;
volatile int bufferPosition = 0;
uint8_t buffer0[DATA_BUFFER_SIZE];
uint8_t buffer1[DATA_BUFFER_SIZE];
volatile int refillInactiveBuffer = 0;
uint8_t *activeBuffer = buffer0;
uint8_t *inactiveBuffer = buffer1;
bool GUImode = false;
bool stopped = true;
bool paused = false;
float soundVolume = 1.0f;
bool initialized = false;


uint32_t readUInt32(File& file) {
    unsigned long product = 0;
    for (int i = 0; i < 4; i++) {
        product |= ((uint32_t)file.read() << (8*i)); 
    }
    return product;
}

uint16_t readUInt16(File& file) {
    unsigned long product = 0;
    for (int i = 0; i < 2; i++) {
        product |= ((uint32_t)file.read() << (8*i)); 
    }
    return product;
}

bool read_header(WavHeader& header_struct, File& file) {
    file.seek(0);
    if (file.size() < 44) {
        return false;
    }
    file.readBytes(header_struct.riff, 4);
    header_struct.fileSize = readUInt32(file);
    file.readBytes(header_struct.wave, 4);
    if (memcmp(header_struct.riff, "RIFF", 4) != 0 || memcmp(header_struct.wave, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "WAV headers didn't match");
        return false;
    }
    file.readBytes(header_struct.fmtmarker, 4);
    header_struct.fmtLength = readUInt32(file);
    header_struct.fmtType = readUInt16(file);
    header_struct.channelCount = readUInt16(file);
    header_struct.sampleRate = readUInt32(file);
    header_struct.bytesPerSecond = readUInt32(file);
    header_struct.blockAlign = readUInt16(file);
    header_struct.bitsPerSample = readUInt16(file);

    char header[4] = {0, 0, 0, 0};

    while (true) {
        if (file.readBytes(header, 4) != 4) {
            ESP_LOGE(TAG, "Couldn't find data section.");
            return false;
        }
        uint32_t chunkSize = readUInt32(file);

        if (memcmp(header, "data", 4) == 0) {
            memcpy(&header_struct.datamarker, header, 4);
            header_struct.dataSize = chunkSize;
            header_struct.dataStartOffset = file.position();
            break;
        } else {
            file.seek(file.position() + chunkSize);
            if (chunkSize & 1) {
                file.seek(file.position() + 1);
            }
        }
    }
    return true;
}

bool WAVPlayer::begin(const int pwm_channel, const int output_pin, const int pwm_frequency = 75000, const int pwm_resolution = 8) {
    if (initialized) return false;

    ledcSetup(pwm_channel, pwm_frequency, pwm_resolution);
    ledcAttachPin(output_pin, pwm_channel);

    return true;
}

bool WAVPlayer::stop() {
    if (!stopped) {
        timerAlarmDisable(timer);
        timerDetachInterrupt(timer);
        timerEnd(timer);
        stopped = true;
        return true;
    }
    return false;
}

void player_isr() {
    portENTER_CRITICAL_ISR(&timerMux);

    if (paused) {
        
        // Do this every frame if the playback is paused

        portEXIT_CRITICAL_ISR(&timerMux);
        return;
    }
    uint8_t value = (uint8_t)std::max(std::min(((((float)activeBuffer[bufferPosition]-128.0f)*soundVolume)+128.0f), 255.0f), 0.0f);
    ledcWrite(PWM_CHANNEL, value);
    bufferPosition++;
    if (bufferPosition >= DATA_BUFFER_SIZE) {
        bufferPosition = 0;
        uint8_t *tmp = activeBuffer;
        
        activeBuffer = inactiveBuffer;
        inactiveBuffer = tmp;

        refillInactiveBuffer = 1;
    }

    portEXIT_CRITICAL_ISR(&timerMux);
}

bool WAVPlayer::loop() {
    
    if (refillInactiveBuffer) {
        // Serial.printf("Inactive buffer: %p\r\n", inactiveBuffer);
        
        if (!soundFile) {
            ESP_LOGE(TAG, "The opened file is invalid.");
            // DPRINTLN("File handle is invalid. Something went horribly wrong.");
            WAVPlayer::stop();
        }
        if (GUImode) {
            Serial.printf("File position: %d/%d (%.02fs)\r", soundFile.position(), soundFile.size(), ((float)(soundFile.position()-header.dataStartOffset)/(float)header.sampleRate));
        }
        if (soundFile.position() != filePosition) {
            soundFile.seek(filePosition);
        }
        if (!soundFile.available() || soundFile.position() > soundFile.size()) {
            WAVPlayer::stop();
        }
        soundFile.read(inactiveBuffer, DATA_BUFFER_SIZE);
        filePosition += DATA_BUFFER_SIZE;

        portENTER_CRITICAL(&timerMux);
        refillInactiveBuffer = 0;
        portEXIT_CRITICAL(&timerMux);
    }
    return (!stopped);
}

void setup_timer(uint32_t sampleRate) {
    timer = timerBegin(0, 80, true);
    
    uint64_t alarmValue = 1000000 / sampleRate;

    timerAlarmWrite(timer, alarmValue, true);
    timerAlarmEnable(timer);

    timerAttachInterrupt(timer, &player_isr, true);
    ESP_LOGI(TAG, "Timers set up.");
}

bool WAVPlayer::play(String path, bool isGUI) {
    if (!stopped) {
        WAVPlayer::stop();
    }
    GUImode = isGUI;
    #ifdef VERBOSE
    Serial.println("Setting up...");
    Serial.println("File is "+path);
    #endif
    
    soundFile = SD.open(path);
    if (!read_header(header, soundFile)) {
        ESP_LOGE(TAG, "Failed to read the WAV header. Is it a valid WAV file?");
        return false;;
    }

    if (soundFile) {
        #ifdef VERBOSE
        Serial.println("Opened file. Setting up timers.");
        #endif
    } else {
        ESP_LOGE(TAG, "Failed to open file.");
        return false;
    }
    
    bool shouldExit = false;
    if (header.bitsPerSample != 8 || header.fmtType != 1 || header.channelCount != 1) {
        ESP_LOGE(TAG, "I can't play this file (Unsupported format). Only mono unsigned 8 bit PCM is supported");
        shouldExit = true;
    }
    
    if (header.sampleRate < 8000 || header.sampleRate > 48000) {
        ESP_LOGE(TAG, "I can't play this file (Unsupported sample rate). Sample rates 8000Hz to 48000Hz are supported.");
        shouldExit = true;
    }
    
    if (shouldExit) {
        return false;
    }

    for (size_t i = 0; i < DATA_BUFFER_SIZE; i++) {
        buffer0[i] = 0x80;
        buffer1[i] = 0x80;
    }
    activeBuffer = buffer0;
    inactiveBuffer = buffer1;
    soundFile.seek(header.dataStartOffset);
    filePosition = header.dataStartOffset;
    // stop_playback();
    stopped = false;
    paused = false;
    setup_timer(header.sampleRate);
    return true;
}

bool WAVPlayer::pause() {
    if (paused) return false;
    paused = true;
    return true;
}

bool WAVPlayer::unpause() {
    if (!paused) return false;
    paused = false;
    return true;
}

void WAVPlayer::volume(uint8_t v) {
    soundVolume = v / 255.0f;
}