#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WAVPlayer.h>

/*
  To play a file on the SD card type the file path into the serial terminal and press enter.
  You can pause the playback with 'p', stop the playback with 'q' or CTRL + C
  Reboot the ESP by pressing CTRL + C while the player is inactive
*/


void printDir(String path) {
    File root = SD.open(path);

    File next = root.openNextFile();

    while (next) {
        if (next.isDirectory()) {
            printDir(next.path());
        } else {
            Serial.println(next.path());
        }
        next = root.openNextFile();
    }
}

void setup() {
    SPI.begin(4, 5, 6, 3);
    if (!SD.begin(3, SPI, 1000000)) {
        Serial.println("Failed to mount SD card.");
        while (1);
    }
    Serial.begin(115200);
    
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PWM_OUTPUT_PIN, PWM_CHANNEL);
    
    delay(1000);

    Serial.println("Contents of SDCARD:");
    printDir("/");

}


String filePath = ""; 
bool inputComplete = false;

void loop() {
    if (!inputComplete) {
        while (Serial.available() > 0) {
            char c = Serial.read();
            
            if (c == 0x03) {
                ESP.restart();
            }

            if (c == '\n' || c == '\r') {
                inputComplete = true;
                Serial.println();
                Serial.print("File path received: ");
                Serial.println(filePath);
                break;
            }

            else if (c == 8 || c == 127) {
                if (filePath.length() > 0) {
                    filePath.remove(filePath.length() - 1);
                    Serial.print("\b \b");
                }
            }

            else {
                filePath += c;
                Serial.print(c);
            }
        }
    } else {
        Serial.println("Playing " + filePath);
        WAVPlayer::volume(128);
        WAVPlayer::play(filePath, true);
        while(WAVPlayer::loop()) {
            if (Serial.available()) {
                if (Serial.peek() == 'q' || Serial.peek() == 0x03) {
                    Serial.print("\nStopped by user.");
                    WAVPlayer::stop();
                } else if (Serial.peek() == 'p') {
                    if (!WAVPlayer::pause()) WAVPlayer::unpause();
                }
                Serial.read();
            }
        }
        Serial.println();
        inputComplete = false;
        filePath = "";
    }
}