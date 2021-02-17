#include <Arduino.h>
#include <EEPROM.h>
#include "Config.h"

int freq = 2000;
int freqStep = +100;
unsigned long freqUpdated = 0;

bool buzzerOn = false;

void buzzer( bool turnOn ) {
  if( BUZZER_Pin>0) {
    buzzerOn = turnOn;
    if( !turnOn ) noTone(BUZZER_Pin);
  }
}

void buzzerLoop() {
  if( buzzerOn && ((unsigned long)(millis() - freqUpdated) > 10 ) ) {
    freqUpdated = millis();
    freq += freqStep;
    if( (freq>3500) || (freq<1500) ) {
      freqStep = -freqStep;
    }
    tone(BUZZER_Pin, freq);
  }
}

void buzzerInit() {
  if( BUZZER_Pin>0 )
  pinMode(BUZZER_Pin, OUTPUT);
  registerLoop(buzzerLoop);
}
