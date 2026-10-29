#include <Arduino.h>
#include <U8g2lib.h>
#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <Preferences.h>
#include "DeckCore.h"
#include "DisplayUI.h"
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Select XIAO_ESP32S3. The wiring/wake settings are board-specific."
#endif
DeckCore deck;DisplayUI display;
void setup(){Serial.begin(115200);display.begin();deck.begin();}
void loop(){deck.update();display.render(deck.state(),millis());if(deck.sleepRequested()){display.blankForSleep();deck.enterSleep();}if(deck.restartRequested()){Serial.flush();ESP.restart();}delay(1);}
