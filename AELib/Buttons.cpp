#include <Arduino.h>

#include "AELib.h"
#include "Comms.h"
#include "Buttons.h"

//#define Debug

// Maximum number of buttons supported
#define ButtonsSize 8
// Anticlush filtering timeout, ms
#define ClashTimeout ((unsigned long)75)

#ifndef LongPressTimeout
#define LongPressTimeout ((unsigned long)800)
#endif

#ifndef RepeatTimeout
#define RepeatTimeout ((unsigned long)1000)
#endif

#ifndef VeryLongPressTimeout
#define VeryLongPressTimeout ((unsigned long)10000)
#endif

// Normal button: 
// LOW == pressed
// HIGH == released

struct Btn {
    byte pin; // Pin number
    bool inverted; // false if button connects to ground
    bool pressed; // inverted ? (digitalRead(pin) == HIGH) : (digitalRead(pin) == LOW );
    BtnDefaultFunction bdf;
    unsigned long triggeredOn;
    unsigned long lastPressedOn;
    int fastCount;

    bool wasPressed;
    bool wasReleased;
    bool wasShortPressed;
    bool wasLongPressed;
    bool wasVeryLongPressed;
    // BUTTONS_EASY_MODE only - 
    bool wasLongPressedFlag;
    int reportedState;
};

byte btnCount = 0;
Btn buttons[ButtonsSize];

// Current button state collected via interrupts
byte btnStates = 0;
// Millis of last button change (clash detection)
unsigned long btnChangedOn = 0;

bool btnInterruptsSupported(byte btnPin) {
    return (btnPin != 16);
}

short int btnIndex(byte btnPin) {
    for (int i = 0; i < btnCount; i++) {
        if (buttons[i].pin == btnPin) return i;
    }
    return -1;
}

#ifdef Debug
void btnShowStatus(Btn* btn) {
    aePrint(F(" #")); aePrint(btn->pin); aePrint(": ");
    if (btn->wasPressed) aePrint(F("wasPressed "));
    if (btn->wasShortPressed) aePrint(F("wasShortPressed "));
    if (btn->wasLongPressed) aePrint(F("wasLongPressed "));
    if (btn->wasVeryLongPressed) aePrint(F("wasVeryLongPressed "));
    if (btn->wasReleased) aePrint(F("wasReleased "));
    aePrintln();
}
#endif


void btnInterrupt(int index) {
    bool pressed = (digitalRead(buttons[index].pin) == ((buttons[index].inverted) ? HIGH : LOW));
    byte bitmask = (1 << index);
    if (pressed != ((btnStates & bitmask)!=0) ) {
        if (pressed) {
            btnStates |= bitmask;
        } else {
            btnStates &= ~bitmask;
        }
        btnChangedOn = millis();
    }
}
void IRAM_ATTR btnInterrupt0() {
    btnInterrupt(0);
}
void IRAM_ATTR btnInterrupt1() {
    btnInterrupt(1);
}
void IRAM_ATTR btnInterrupt2() {
    btnInterrupt(2);
}
void IRAM_ATTR btnInterrupt3() {
    btnInterrupt(3);
}
void IRAM_ATTR btnInterrupt4() {
    btnInterrupt(4);
}
void IRAM_ATTR btnInterrupt5() {
    btnInterrupt(5);
}
void IRAM_ATTR btnInterrupt6() {
    btnInterrupt(6);
}
void IRAM_ATTR btnInterrupt7() {
    btnInterrupt(7);
}




void btnLoop(Btn* btn, bool pressed, unsigned long t) {
    int state = pressed ? 1 : 0;
    if (pressed != btn->pressed) {
        btn->pressed = pressed;
        if (pressed) { // button pressed
            if ((unsigned long)(t - btn->lastPressedOn) < (unsigned long)1000) {
                btn->fastCount++;
                if (btn->fastCount >= 20) {
                    if (btn->bdf == BDF_Reset) commsRestart();
                    if (btn->bdf == BDF_FactoryReset) storageReset();
                    if (btn->bdf == BDF_EnableOTA) commsEnableOTA();
                    if (btn->bdf == BDF_SwitchWIFI) {
                        if (commsEnabled()) {
                            commsDisable();
                        } else {
                            commsEnable();
                        }
                        commsRestart();
                    }
                    btn->fastCount = -1000;
                }
            } else {
                btn->fastCount = 1;
            }
            btn->lastPressedOn = t;
            btn->wasPressed = true;
            btn->wasReleased = false;
            btn->wasShortPressed = false;
            btn->wasLongPressed = false;
            btn->wasLongPressedFlag = false;
            btn->wasVeryLongPressed = false;
        } else { // button released
            btn->wasPressed = false;
            btn->wasReleased = true;
#ifdef BUTTONS_EASY_MODE
            if (((unsigned long)(t - btn->triggeredOn) < LongPressTimeout) && (!btn->wasLongPressedFlag)) {
                btn->wasShortPressed = true;
            }
#else
            if ((unsigned long)(t - btn->triggeredOn) < LongPressTimeout) {
                btn->wasShortPressed = true;
            } else if ((unsigned long)(t - btn->triggeredOn) < VeryLongPressTimeout) {
                btn->wasLongPressed = true;
            } else {
                btn->wasVeryLongPressed = true;
            }
#endif        
        }
        btn->triggeredOn = t;
#ifdef Debug
        btnShowStatus(btn);
#endif        
    }
}

void btnRegister(byte btnPin, bool inverted, bool pullUp) {
    if (btnIndex(btnPin) >= 0) return;

    if (btnCount < ButtonsSize - 1) {

        buttons[btnCount].pin = btnPin;
        buttons[btnCount].inverted = inverted;
        buttons[btnCount].fastCount = 0;
        buttons[btnCount].bdf = BDF_None;
        buttons[btnCount].pressed = false;
        buttons[btnCount].reportedState = -1;

        pinMode(btnPin, pullUp ? INPUT_PULLUP : INPUT);
        if (btnInterruptsSupported(btnPin)) {
            switch (btnCount) {
            case 0: attachInterrupt(btnPin, btnInterrupt0, CHANGE); break;
            case 1: attachInterrupt(btnPin, btnInterrupt1, CHANGE); break;
            case 2: attachInterrupt(btnPin, btnInterrupt2, CHANGE); break;
            case 3: attachInterrupt(btnPin, btnInterrupt3, CHANGE); break;
            case 4: attachInterrupt(btnPin, btnInterrupt4, CHANGE); break;
            case 5: attachInterrupt(btnPin, btnInterrupt5, CHANGE); break;
            case 6: attachInterrupt(btnPin, btnInterrupt6, CHANGE); break;
            case 7: attachInterrupt(btnPin, btnInterrupt7, CHANGE); break;
            }
        }
        btnCount++;
        btnLoop(&buttons[btnCount - 1], false, 0);
    }
}
void btnDefaultFunction(byte btnPin, BtnDefaultFunction bdf) {
    short int index = btnIndex(btnPin);
    if (index >= 0) buttons[index].bdf = bdf;
}

bool btnState(byte btnPin) {
    short int index = btnIndex(btnPin);
    return ((index >= 0) && buttons[index].pressed);
}

bool btnState(byte btnPin, byte btnPin2) {
    short int index = btnIndex(btnPin);
    short int index2 = btnIndex(btnPin2);
    return ((index >= 0) && buttons[index].pressed && (index2 >= 0) && buttons[index2].pressed);
}

bool btnPressed(byte btnPin) {
    short int index = btnIndex(btnPin);
    if ((index < 0) || !buttons[index].wasPressed) return false;
    buttons[index].wasPressed = false;
    return true;
}

bool btnPressed(byte btnPin, byte btnPin2) {
    short int index = btnIndex(btnPin);
    if ((index < 0) || !buttons[index].wasPressed) return false;

    short int index2 = btnIndex(btnPin2);
    if ((index2 < 0) || !buttons[index2].wasPressed) return false;

    buttons[index].wasPressed = false;
    buttons[index2].wasPressed = false;
    return true;
}

bool btnReleased(byte btnPin) {
    short int index = btnIndex(btnPin);
    if ((index < 0) || !buttons[index].wasReleased) return false;
    buttons[index].wasReleased = false;
    return true;
}

bool btnReleased(byte btnPin, byte btnPin2) {
    short int index = btnIndex(btnPin);
    if ((index < 0) || !buttons[index].wasReleased) return false;

    short int index2 = btnIndex(btnPin2);
    if ((index2 < 0) || !buttons[index2].wasReleased) return false;

    buttons[index].wasReleased = false;
    buttons[index2].wasReleased = false;
    return true;
}

bool btnShortPressed(byte btnPin) {
    short int index = btnIndex(btnPin);
    if ((index < 0) || !buttons[index].wasShortPressed) return false;

#ifdef Debug
    aePrintf("Shortpress check #%d TRUE\n", btnPin);
#endif  

    buttons[index].wasShortPressed = false;
    return true;
}

bool btnShortPressed(byte btnPin, byte btnPin2) {
    short int index = btnIndex(btnPin);
    if ((index < 0) || !buttons[index].wasShortPressed) return false;

    short int index2 = btnIndex(btnPin2);
    if ((index2 < 0) || !buttons[index2].wasShortPressed) return false;

#ifdef Debug
    aePrintf("Shortpress check #%d, #%d TRUE\n", btnPin, btnPin2);
#endif  

    buttons[index].wasShortPressed = false;
    buttons[index2].wasShortPressed = false;
    return true;
}

bool btnLongPressed(byte btnPin) {
    short int index = btnIndex(btnPin);
    if ((index < 0) || !buttons[index].wasLongPressed) return false;
    buttons[index].wasLongPressed = false;
    return true;
}

bool btnLongPressed(byte btnPin, byte btnPin2) {
    short int index = btnIndex(btnPin);
    if ((index < 0) || !buttons[index].wasLongPressed) return false;

    short int index2 = btnIndex(btnPin2);
    if ((index2 < 0) || !buttons[index2].wasLongPressed) return false;

    buttons[index].wasLongPressed = false;
    buttons[index2].wasLongPressed = false;
    return true;
}

#ifndef BUTTONS_EASY_MODE
bool btnVeryLongPressed(byte btnPin) {
    short int index = btnIndex(btnPin);
    if ((index < 0) || !buttons[index].wasVeryLongPressed) return false;
    buttons[index].wasVeryLongPressed = false;
    return true;
}

bool btnVeryLongPressed(byte btnPin, byte btnPin2) {
    short int index = btnIndex(btnPin);
    if ((index < 0) || !buttons[index].wasVeryLongPressed) return false;

    short int index2 = btnIndex(btnPin2);
    if ((index2 < 0) || !buttons[index2].wasVeryLongPressed) return false;

    buttons[index].wasVeryLongPressed = false;
    buttons[index2].wasVeryLongPressed = false;
    return true;
}
#endif

static int btnLastPressB1 = -1;
static int btnLastPressB2 = -1;
static unsigned long btnLastPressed = 0;


bool btnSingleButtonEvent(char* eventName, bool repeats) {
    for (int b1 = 0; b1 < btnCount; b1++) {
        if (btnShortPressed(buttons[b1].pin)) {
            if ((btnLastPressB1 != b1) || !repeats) {
                sprintf(eventName, "Btn%dPressed", b1 + 1);
            } else {
                sprintf(eventName, "Btn%dRepeated", b1 + 1);
            }
            btnLastPressB1 = b1;
            btnLastPressB2 = -1;
            return true;
        }
        if (btnLongPressed(buttons[b1].pin)) {
            sprintf(eventName, "Btn%dLongPressed", b1 + 1);
            btnLastPressB1 = -1;
            btnLastPressB2 = -1;
            return true;
        }
#ifndef BUTTONS_EASY_MODE
        if (btnVeryLongPressed(buttons[b1].pin)) {
            sprintf(eventName, "Btn%dVeryLongPressed", b1 + 1);
            btnLastPressB1 = -1;
            btnLastPressB2 = -1;
            return true;
        }
#endif
    }
    return false;
}

bool btnTwoButtonsEvent(char* eventName, bool repeats) {
    for (int b1 = 0; b1 < btnCount - 1; b1++) {
        for (int b2 = b1 + 1; b2 < btnCount; b2++) {
            if (btnShortPressed(buttons[b1].pin, buttons[b2].pin)) {
                if ((btnLastPressB1 != b1) || (btnLastPressB2 != b2) || !repeats) {
                    sprintf(eventName, "Btn%d%dPressed", b1 + 1, b2 + 1);
                } else {
                    sprintf(eventName, "Btn%d%dRepeated", b1 + 1, b2 + 1);
                }
                btnLastPressB1 = b1;
                btnLastPressB2 = b2;
                return true;
            }
            if (btnLongPressed(buttons[b1].pin, buttons[b2].pin)) {
                sprintf(eventName, "Btn%d%dLongPressed", b1 + 1, b2 + 1);
                btnLastPressB1 = -1;
                btnLastPressB2 = -1;
                return true;
            }
#ifndef BUTTONS_EASY_MODE
            if (btnVeryLongPressed(buttons[b1].pin, buttons[b2].pin)) {
                sprintf(eventName, "Btn%d%dVeryLongPressed", b1 + 1, b2 + 1);
                btnLastPressB1 = -1;
                btnLastPressB2 = -1;
                return true;
            }
#endif
        }
    }
    return false;
}

bool btnPublishKeypressEvent(bool combinations, bool repeats) {
    if (!mqttConnected()) return false;
    unsigned long t = millis();
    if ((btnLastPressed > 0) && timedOut(t, btnLastPressed, RepeatTimeout)) {
        btnLastPressB1 = -1;
        btnLastPressB2 = -1;
        btnLastPressed = 0;
    }
    char eventName[24] = { 0 };

    if ((combinations && btnTwoButtonsEvent(eventName, repeats)) || btnSingleButtonEvent(eventName, repeats)) {
        btnLastPressed = t;
#ifdef Debug
        aePrintln(eventName);
#endif
        mqttPublish(eventName, (char*)NULL, false);
        return true;
    }
    return false;
}

void btnsLoop() {
    static unsigned long _tmInterruptsEnabledButtonsUpdated = 0;
    unsigned long t = millis();
    bool triggered = false;

    if (timedOut(t, _tmInterruptsEnabledButtonsUpdated, 20)) {
        _tmInterruptsEnabledButtonsUpdated = t;
        for (int i = 0; i < btnCount; i++) {
            if (!btnInterruptsSupported(buttons[i].pin)) {
                btnInterrupt(i);
            }
        }
    }

    // Limit button scan frequency
    if ((btnChangedOn > 0) && timedOut(t, btnChangedOn, ClashTimeout)) {
        btnChangedOn = 0;

        triggerActivity();
        // Process states;
        for (int i = 0; i < btnCount; i++) {
            btnLoop(&buttons[i], ((btnStates & (1 << i)) != 0), t);
        }
    }
#ifdef BUTTONS_EASY_MODE
    for (int i = 0; i < btnCount; i++) {
        if (timedOut(t, buttons[i].triggeredOn, LongPressTimeout) && buttons[i].pressed && !buttons[i].wasLongPressedFlag) {
            buttons[i].wasLongPressedFlag = true;
            buttons[i].wasLongPressed = true;
        }
    }
#endif        

}

void btnInit() {
    aeRegisterLoop(btnsLoop);
}
