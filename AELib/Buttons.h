#ifndef buttons_h
#define buttons_h
#include "Config.h"


enum BtnDefaultFunction {
	BDF_None = 0, // none
	BDF_EnableOTA, // enable OTA on 10th fast click
	BDF_SwitchWIFI, // enable/disable WIFI and MQTT support and restart
	BDF_FactoryReset, // Reset storage on 10th click
	BDF_Reset // Reset device on 10th click
};

enum BtnMqttBehavior {
	AlwaysNotify = 0,		// Always send button updates and events to MQTT
	NeverNotify = 1,		// Do not notify MQTT on button events
	IfControlledByHA = 2	// Send MQTT updates if device is in "haControlled()" state only
};

void btnRegister(byte btnPin, char* mqttName, bool btnInverted, bool pullUp);
void btnDefaultFunction(byte btnPin, BtnDefaultFunction bdf);

char* btnName(byte btnPin);

bool btnState(byte btnPin);
bool btnPressed(byte btnPin);
bool btnReleased(byte btnPin);
bool btnShortPressed(byte btnPin);
bool btnLongPressed(byte btnPin);
#ifndef ButtonsEasyMode
bool btnVeryLongPressed(byte btnPin);
#endif

bool btnState(byte btnPin, byte btnPin2);
bool btnPressed(byte btnPin, byte btnPin2);
bool btnReleased(byte btnPin, byte btnPin2);
bool btnShortPressed(byte btnPin, byte btnPin2);
bool btnLongPressed(byte btnPin, byte btnPin2);
#ifndef ButtonsEasyMode
bool btnVeryLongPressed(byte btnPin, byte btnPin2);
#endif

void btnSetMqttBehavior(BtnMqttBehavior bmb);

void btnInit();
#endif
