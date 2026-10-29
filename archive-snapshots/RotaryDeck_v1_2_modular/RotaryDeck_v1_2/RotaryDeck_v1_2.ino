// Rotary Deck V1.2: three-file modular firmware.
// Keep library includes explicit so Arduino discovers the dependencies.
#include <Arduino.h>
#include <U8g2lib.h>
#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <Preferences.h>
#include "DeckCore.h"
#include "DisplayUI.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This pin mapping and wake configuration require XIAO ESP32-S3."
#endif

DeckCore deck;
DisplayUI display;

void setup(){
 Serial.begin(115200);
 display.begin();
 deck.begin();
}
void loop(){
 deck.update();
 display.render(deck.state(),millis());
 if(deck.sleepRequested()){
  display.blankForSleep();
  deck.enterSleep();
 }
 if(deck.restartRequested()){
  Serial.flush();ESP.restart();
 }
 delay(1);
}
