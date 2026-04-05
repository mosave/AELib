#include <Arduino.h>

#include "AELib.h"
#include "Comms.h"
#include "Relays.h"

// Turn on debug output
//#define Debug

// Maximum number of relays supported
#define RelaysSize 8

// first %s is relay name
#define TOPIC_State "Relay%s/State"
#define TOPIC_SetState "Relay%s/SetState"
#define TOPIC_Switch "Relay%s/Switch"

//Mininum relay switch timeout
#define TriggerDelay ((unsigned long)(300))
#define LoopDelay ((unsigned long)75)

// Normal relay
// LOW == OFF
// HIGH == ON

struct Relay {
    byte pin; // Pin number
    bool inverted;
    bool state; // inverted ? (digitalRead(pin) == LOW) : (digitalRead(pin) == HIGH );

    unsigned long triggeredOn;
    int reportedState;
};

byte relayCount = 0;

bool relayEnableMQTT = false;
Relay relays[RelaysSize];

#ifdef Debug
void relayShowStatus(Relay* relay) {
    aePrint(F("Relay #")); aePrint(relay->pin); aePrint(": ");
    aePrint(relay->state ? "ON" : "Off");
    aePrint(", reported "); aePrint(relay->reportedState);
    aePrintln();
}
#endif

//**************************************************************************
//                    Helper functions
//**************************************************************************

Relay* relayFind(byte pin) {
    for (int i = 0; i < relayCount; i++) {
        if (relays[i].pin == pin) return &relays[i];
    }
    return NULL;
}

bool relayState(byte pin) {
    Relay* relay = relayFind(pin);
    return ((relay != NULL) && relay->state);
}

bool relaySetState(byte pin, bool state) {
    Relay* relay = relayFind(pin);
    if (relay == NULL) return false;
    relay->state = state;
    return relay->state;
}

bool relaySwitch(byte pin) {
    return relaySetState(pin, !relayState(pin));
}

void relayRegister(byte pin, bool inverted, bool state) {
    if (relayCount < RelaysSize) {
        pinMode(pin, OUTPUT);
        digitalWrite( pin, state != inverted ? HIGH : LOW);

        relays[relayCount].pin = pin;
        relays[relayCount].inverted = inverted;
        relays[relayCount].reportedState = -1;
        relays[relayCount].state = state;
        relayCount++;
    }
}
void relayRegister(byte pin, bool inverted) {
    relayRegister(pin, inverted, false);
}
//**************************************************************************
//                         MQTT support functions
//**************************************************************************

void relaysMQTTConnect() {
    if (relayEnableMQTT) {
        for (int i = 0; i < relayCount; i++) {
            char rns[4];
            sprintf(rns, "%d", i + 1);
            mqttSubscribeTopic(TOPIC_SetState, rns);
            mqttSubscribeTopic(TOPIC_Switch, rns);
        }
    }
}

bool relaysMQTTCallback(char* topic, byte* payload, unsigned int length) {
    char payloadString[32];
    memset(payloadString, 0, sizeof(payloadString));
    strncpy(payloadString, (char*)payload, (length < 31) ? length : 31);

    //aePrint("Payload=");
    for (int i = 0; i < relayCount; i++) {
        char rns[4];
        sprintf(rns, "%d", i + 1);
        if (mqttIsTopic(topic, TOPIC_SetState, rns)) {
            bool b;
            if (parseBool(payloadString, &b)) {
                relaySetState(relays[i].pin, b);
            }
            return true;
        }
        if (mqttIsTopic(topic, TOPIC_Switch, rns)) {
            relaySwitch(relays[i].pin);
            return true;
        }
    }
    return false;
}

//**************************************************************************
//                            Relays engine
//**************************************************************************

bool relayLoop(Relay* relay) {
    unsigned long t = millis();

    if ((unsigned long)(t - relay->triggeredOn) > TriggerDelay) {
        bool currentState = (digitalRead(relay->pin) == HIGH);
        if (relay->inverted) currentState = !currentState;

        if (relay->state != currentState) {
            bool s = relay->state;
            if (relay->inverted) s = !s;
            digitalWrite(relay->pin, s ? HIGH : LOW);
            relay->triggeredOn = t;
            return true;
        }
    }
    return false;
}

unsigned long lastCheck = 0;

void relaysLoop() {
    unsigned long t = millis();
    if ((unsigned long)(t - lastCheck) < LoopDelay) return;
    lastCheck = t;

    for (int i = 0; i < relayCount; i++) {
        // Switch one relay per loop iteration to spread inrush currents over time:
        if (relayLoop(&relays[i])) break;
    }

    if (relayEnableMQTT && mqttConnected()) {
        for (int i = 0; i < relayCount; i++) {
            int state = (relays[i].state ? 1 : 0);
            char rns[4];
            sprintf(rns, "%d", i + 1);
            if ((state != relays[i].reportedState) && 
                mqttPublish(TOPIC_State, rns, state, true)) {
                relays[i].reportedState = state;
            }
        }
    }
}

void relayInit(bool enableMQTT) {
    relayEnableMQTT = enableMQTT;
    if (relayEnableMQTT) {
        mqttRegisterCallbacks(relaysMQTTCallback, relaysMQTTConnect);
    }
    aeRegisterLoop(relaysLoop);
}

void relayInit() {
    relayInit(true);
}
