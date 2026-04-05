#include <Arduino.h>

#include "AELib.h"
#include "Comms.h"
#include "BinarySensors.h"

//#define Debug

// Maximum number of buttons supported
#define BNS_MAX_SENSORS 8

// Anticlush filtering timeout, ms
#define BNS_CLASH_TIMEOUT ((unsigned long)75)

static char* TOPIC_SensorState PROGMEM = "Sensors/BinarySensor%s";

struct BinarySensor {
    byte pin; // Pin number
    bool inverted; // false if sensor value should be reversed
    bool state; // inverted ? (digitalRead(pin) == HIGH) : (digitalRead(pin) == LOW );
    unsigned int reportEverySeconds;
    unsigned long triggeredOn;
    unsigned long reportedOn;
    char reportedState;
};

byte bnsCount = 0;
BinarySensor bnsSensors[BNS_MAX_SENSORS];

void bnsPublish(bool force) {
    if (force || !mqttConnected() ) {
        for (int i = 0; i < bnsCount; i++) {
            bnsSensors[i].reportedState = 2;
        }
    }

    if (!mqttConnected()) {
        return;
    }

    long t = millis();;
    
    for (int i = 0; i < bnsCount; i++) {
        bool updated = (bnsSensors[i].state ? 1 : 0) != bnsSensors[i].reportedState;
        bool outdated =
            (bnsSensors[i].reportEverySeconds > 0)
            && bnsSensors[i].state 
            && timedOut(t, bnsSensors[i].reportedOn, 1000L * bnsSensors[i].reportEverySeconds);

        if (updated || outdated) {
            char ns[4];
            sprintf(ns, "%d", i + 1);
            if (mqttPublish(TOPIC_SensorState, ns, (int)(bnsSensors[i].state ? 1 : 0), true)) {
                bnsSensors[i].reportedState = bnsSensors[i].state ? 1 : 0;
                bnsSensors[i].reportedOn = t;
            }
        }
    }
}

void bnsRegister(byte pin, bool inverted, bool pullUp, unsigned int reportEverySeconds ) {
    if (bnsCount >= BNS_MAX_SENSORS - 1) {
        return;
    }

    bnsSensors[bnsCount].pin = pin;
    bnsSensors[bnsCount].inverted = inverted;
    bnsSensors[bnsCount].reportEverySeconds = reportEverySeconds;
    bnsSensors[bnsCount].state = false;
    bnsSensors[bnsCount].reportedState = 2;

    pinMode( pin, pullUp ? INPUT_PULLUP : INPUT);
    bnsCount++;
}

bool bnsState(byte pin) {
    for (int i = 0; i < bnsCount; i++) {
        if (bnsSensors[i].pin == pin) {
            return bnsSensors[i].state;
        }
    }
    return false;
}

void bnsLoop() {
    static unsigned long _tmSensorsUpdated = 0;
    unsigned long t = millis();
    bool triggered = false;
    if ((t < 1000L) || !timedOut(t, _tmSensorsUpdated, 50)) {
        return;
    }

    _tmSensorsUpdated = t;
    for (int i = 0; i < bnsCount; i++) {
        bool state = (digitalRead(bnsSensors[i].pin) == ((bnsSensors[i].inverted) ? LOW : HIGH));
        if ((state != bnsSensors[i].state)) {
            if (bnsSensors[i].triggeredOn == 0) {
                bnsSensors[i].triggeredOn = t;
            } else if (timedOut(t, bnsSensors[i].triggeredOn, BNS_CLASH_TIMEOUT)) {
                bnsSensors[i].state = state;
                bnsSensors[i].triggeredOn = 0;
            }
        } else {
            bnsSensors[i].triggeredOn = 0;
        }
    }

    bnsPublish(false);
}

void bnsInit() {
    aeRegisterLoop(bnsLoop);
}
