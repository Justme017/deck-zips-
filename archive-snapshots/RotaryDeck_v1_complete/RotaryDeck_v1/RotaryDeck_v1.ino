#include "DeckFirmware.h"

RotaryDeck deck;
volatile int32_t encoderTransitions=0;
volatile uint8_t previousAB=0;
portMUX_TYPE encoderMux=portMUX_INITIALIZER_UNLOCKED;
// RAM-resident lookup table for interrupt decoding.
DRAM_ATTR int8_t transitionTable[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
void IRAM_ATTR encoderISR(){
 uint8_t ab=(digitalRead(RotaryDeck::A)<<1)|digitalRead(RotaryDeck::B);
 portENTER_CRITICAL_ISR(&encoderMux);
 encoderTransitions+=transitionTable[(previousAB<<2)|ab];previousAB=ab;
 portEXIT_CRITICAL_ISR(&encoderMux);
}
void setup(){
 Serial.begin(115200);deck.begin();
 previousAB=(digitalRead(RotaryDeck::A)<<1)|digitalRead(RotaryDeck::B);
 attachInterrupt(digitalPinToInterrupt(RotaryDeck::A),encoderISR,CHANGE);
 attachInterrupt(digitalPinToInterrupt(RotaryDeck::B),encoderISR,CHANGE);
}
void loop(){
 static int remainder=0,lastThreshold=4;
 portENTER_CRITICAL(&encoderMux);
 int32_t count=encoderTransitions;encoderTransitions=0;
 portEXIT_CRITICAL(&encoderMux);
 int threshold=deck.detentsPerStep();
 if(threshold!=lastThreshold){remainder=0;lastThreshold=threshold;}
 remainder+=count;int detents=remainder/threshold;remainder-=detents*threshold;
 if(detents)deck.turn(detents);
 deck.pollButton();deck.tick();delay(1);
}
