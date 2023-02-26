#include <Arduino.h>
#include "Buttons.h"
#include "Config.h"
#include "Storage.h"
#include "Comms.h"
//#define Debug

// Maximum number of buttons supported
#define ButtonsSize 8
#define ButtonNameSize 16
// Anticlush filtering timeout, ms
#define ClashTimeout ((unsigned long)75)
#ifndef LongPressTimeout
#define LongPressTimeout ((unsigned long)800)
#endif
#ifndef VeryLongPressTimeout
#define VeryLongPressTimeout ((unsigned long)10000)
#endif

// "%s" will be button name
static char* TOPIC_State PROGMEM = "%s/State";
static char* TOPIC_ShortPressed PROGMEM = "%s/ShortPressed";
static char* TOPIC_LongPressed PROGMEM = "%s/LongPressed";
static char* TOPIC_VeryLongPressed PROGMEM = "%s/VeryLongPressed";

// Normal button: 
// LOW == pressed
// HIGH == released

struct Btn {
	byte pin; // Pin number
	bool inverted; // false if button connects to ground
	bool pressed; // inverted ? (digitalRead(pin) == HIGH) : (digitalRead(pin) == LOW );
	BtnDefaultFunction bdf;
	char name[ButtonNameSize];
	unsigned long triggeredOn;
	unsigned long lastPressedOn;
	int fastCount;

	bool wasPressed;
	bool wasReleased;
	bool wasShortPressed;
	bool wasLongPressed;
	bool wasVeryLongPressed;
	// ButtonsEasyMode only - 
	bool wasLongPressedFlag;
	int reportedState;
};

byte btnCount = 0;

Btn buttons[ButtonsSize];
BtnMqttBehavior btnMqttBehavior = BtnMqttBehavior::AlwaysNotify;


short int btnIndex(byte btnPin) {
	for (int i = 0; i < btnCount; i++) {
		if (buttons[i].pin == btnPin) return i;
	}
	return -1;
}

bool btnPublishEvent(Btn* btn, char* topic, char* value) {
	if (strlen(btn->name) == 0) {
		return false;
	}
	if (btnMqttBehavior == BtnMqttBehavior::NeverNotify) {
		return false;
	}
	if ((btnMqttBehavior == BtnMqttBehavior::IfControlledByHA) && !haControlled()) {
		return false;
	}
	return mqttPublish(topic, btn->name, value, false);
}

#ifdef Debug
void btnShowStatus(Btn* btn) {
	aePrint((strlen(btn->name) > 0) ? btn->name : "Button");
	aePrint(F(" #")); aePrint(btn->pin); aePrint(": ");
	if (btn->wasPressed) aePrint(F("wasPressed "));
	if (btn->wasShortPressed) aePrint(F("wasShortPressed "));
	if (btn->wasLongPressed) aePrint(F("wasLongPressed "));
	if (btn->wasVeryLongPressed) aePrint(F("wasVeryLongPressed "));
	if (btn->wasReleased) aePrint(F("wasReleased "));
	aePrintln();
}
#endif


void btnLoop(Btn* btn, bool pressed, unsigned long t) {
	int state = pressed ? 1 : 0;
	char state_s[4];
	if (pressed) {
		strcpy(state_s, "1");
	} else {
		strcpy(state_s, "0");
	}

	if ((state != btn->reportedState) && btnPublishEvent(btn, TOPIC_State, state_s )) {
		btn->reportedState = state;
	} else {
		btn->reportedState = -1;
	}

	if (pressed != btn->pressed) {
		btn->pressed = pressed;
		if (pressed) { // button pressed
			if ((unsigned long)(t - btn->lastPressedOn) < (unsigned long)1000) {
				btn->fastCount++;
				if (btn->fastCount >= 10) {
					if (btn->bdf == BDF_Reset) commsRestart();
					if (btn->bdf == BDF_FactoryReset) storageReset();
					if (btn->bdf == BDF_EnableOTA) commsEnableOTA();
					if (btn->bdf == BDF_SwitchWIFI) {
						if (wifiEnabled()) {
							wifiDisable();
						} else {
							wifiEnable();
						}
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
#ifdef ButtonsEasyMode
			if (((unsigned long)(t - btn->triggeredOn) < LongPressTimeout) && (!btn->wasLongPressedFlag)) {
				btn->wasShortPressed = true;
				btnPublishEvent(btn, TOPIC_ShortPressed, "");
			}
#else
			if ((unsigned long)(t - btn->triggeredOn) < LongPressTimeout) {
				btn->wasShortPressed = true;
				btnPublishEvent(btn, TOPIC_ShortPressed, "");
			} else if ((unsigned long)(t - btn->triggeredOn) < VeryLongPressTimeout) {
				btn->wasLongPressed = true;
				btnPublishEvent(btn, TOPIC_LongPressed, "");
		} else {
				btn->wasVeryLongPressed = true;
				btnPublishEvent(btn, TOPIC_VeryLongPressed, "");
			}
#endif        
			}
		btn->triggeredOn = t;
#ifdef Debug
		btnShowStatus(btn);
#endif        
	}
}

void btnRegister(byte btnPin, char* mqttName, bool inverted, bool pullUp) {
	if (btnCount < ButtonsSize - 1) {
		pinMode(btnPin, pullUp ? INPUT_PULLUP : INPUT);
		buttons[btnCount].pin = btnPin;
		if ((mqttName != NULL) && (strlen(mqttName) > 0)) {
			strncpy(buttons[btnCount].name, mqttName, ButtonNameSize);
		}
		buttons[btnCount].inverted = inverted;
		buttons[btnCount].fastCount = 0;
		buttons[btnCount].bdf = BDF_None;
		buttons[btnCount].pressed = false;
		buttons[btnCount].reportedState = -1;
		btnCount++;
		btnLoop(&buttons[btnCount - 1], false, 0);
	}
}
void btnDefaultFunction(byte btnPin, BtnDefaultFunction bdf) {
	short int index = btnIndex(btnPin);
	if (index >= 0) buttons[index].bdf = bdf;
}

char* btnName(byte btnPin) {
	short int index = btnIndex(btnPin);
	return (index >= 0) ? buttons[index].name : NULL;
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
#ifndef ButtonsEasyMode
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

void btnsLoop() {
	static byte _states = 0b1111111;
	static unsigned long changedOn = 0;
	byte states = 0;
	unsigned long t = millis();
	bool triggered = false;

	for (int i = 0; i < btnCount; i++) {
		bool pressed = (digitalRead(buttons[i].pin) == LOW);
		if (buttons[i].inverted) pressed = !pressed;
		states |= (pressed ? (1 << i) : 0);
	}

	if (states != _states) { // button states changed
		_states = states;
		changedOn = t;
} else {
		if ((changedOn > 0) && (unsigned long)(t - changedOn) > ClashTimeout) {
#ifdef Debug      
			aePrintf("Button states: %d\n", states);
#endif      
			triggerActivity();
			changedOn = 0;
			// Process states;
			for (int i = 0; i < btnCount; i++) {
				btnLoop(&buttons[i], ((states & (1 << i)) != 0), t);
			}
		}
#ifdef ButtonsEasyMode
		if ((unsigned long)(t - changedOn) > ClashTimeout) {
			for (int i = 0; i < btnCount; i++) {
				if (buttons[i].pressed && (!buttons[i].wasLongPressedFlag) && (unsigned long)(t - buttons[i].triggeredOn) >= LongPressTimeout) {
					buttons[i].wasLongPressedFlag = true;
					buttons[i].wasLongPressed = true;
					btnPublishEvent( &buttons[i], TOPIC_LongPressed, "");
				}
			}
		}
#endif
	}
}

void btnSetMqttBehavior(BtnMqttBehavior bmb) {
	btnMqttBehavior = bmb;
}


void btnInit() {
	registerLoop(btnsLoop);
}
