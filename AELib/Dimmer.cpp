#include <arduino.h>
#include <stdarg.h>
#include <errno.h>

#include "AELib.h"
#include "Comms.h"
#include "Dimmer.h"

#ifndef DIMMER_CH1
#define DIMMER_CH1 2
#endif

#ifndef DIMMER_CH2
#define DIMMER_CH2 -1
#endif

#if DIMMER_CH2<0
#define DIMMER_NCHANNELS 1
#ifdef DIMMER_FIX_MODE
#undef DIMMER_FIX_MODE
#endif
#define DIMMER_FIX_MODE 0
#else
#define DIMMER_NCHANNELS 2
#endif


#ifndef DIMMER_PWM_FREQ
#define DIMMER_PWM_FREQ 400
#endif

static char* TOPIC_Mode PROGMEM = "Dimmer/Mode";
static char* TOPIC_SetMode PROGMEM = "Dimmer/SetMode";

static char* MODE0_Name PROGMEM = "0: Single Channel";
static char* MODE1_Name PROGMEM = "1: Two Channels";
static char* MODE2_Name PROGMEM = "2: White Cold/Warm";

static char* TOPIC_Range PROGMEM = "Dimmer/WorkRange";
static char* TOPIC_SetRange PROGMEM = "Dimmer/SetWorkRange";

static char* TOPIC_Transition PROGMEM = "Dimmer/Transition";
static char* TOPIC_SetTransition PROGMEM = "Dimmer/SetTransition";

static char* TOPIC_State PROGMEM = "Dimmer/State";
static char* TOPIC_SetState PROGMEM = "Dimmer/SetState";

static char* TOPIC_State2 PROGMEM = "Dimmer/State2";
static char* TOPIC_SetState2 PROGMEM = "Dimmer/SetState2";

static char* TOPIC_Switch PROGMEM = "Dimmer/Switch";
static char* TOPIC_Switch2 PROGMEM = "Dimmer/Switch2";

static char* TOPIC_SaveDefaults PROGMEM = "Dimmer/SaveDefaults";

static char* TOPIC_Brightness PROGMEM = "Dimmer/Brightness";
static char* TOPIC_SetBrightness PROGMEM = "Dimmer/SetBrightness";

static char* TOPIC_Brightness2 PROGMEM = "Dimmer/Brightness2";
static char* TOPIC_SetBrightness2 PROGMEM = "Dimmer/SetBrightness2";

static char* TOPIC_Temperature PROGMEM = "Dimmer/Temperature";
static char* TOPIC_SetTemperature PROGMEM = "Dimmer/SetTemperature";

static char* TOPIC_MiredsRange PROGMEM = "Dimmer/MiredsRange";
static char* TOPIC_SetMiredsRange PROGMEM = "Dimmer/SetMiredsRange";

static char* TOPIC_Mireds PROGMEM = "Dimmer/Mireds";
static char* TOPIC_SetMireds PROGMEM = "Dimmer/SetMireds";


struct DimmerConfig {
    byte mode;
    byte dimmerBrightness;
    byte dimmerBrightness2;
    byte dimmerTemperature;
    int rangeMin;
    int rangeMax;
    int dimmerTransition;
    int miredsMin;
    int miredsMax;
} dimmerConfig;

struct DimmerChannel {
    int pin;
    int level;
    int targetLevel;
    float a;
} channels[DIMMER_NCHANNELS] = {
    { DIMMER_CH1, 0, 0 },
#if DIMMER_NCHANNELS>1
    { DIMMER_CH2, 0, 0 },
#endif
};

unsigned long dimmerTransitionStarted = 0;

bool dimmerState = false;
bool dimmerState2 = false;
int dimmerBrightness = 1;
int dimmerBrightness2 = 1;
int dimmerTemperature = 0;
#define dimmerMireds ((int)map(dimmerTemperature, 0, 255, dimmerConfig.miredsMax, dimmerConfig.miredsMin))
int dimmerTransition = 300;

bool dimmerGlowUp = false;
int dimmerGlowDelta = 10;
unsigned long dimmerGlowDT = 0;
unsigned long dimmerGlowTimeout = 100;

#pragma region Transitions support

void transitionStart() {
    //aePrintln("Starting Transition");
    float f0 = -1, f1 = -1; //[0..1]
    switch (dimmerConfig.mode) {
    case 0: // Single channel, synchronize
        f1 = f0 = (dimmerState ? dimmerBrightness / 255.0 : -1);
        break;
    case 1:
        f0 = (dimmerState ? dimmerBrightness / 255.0 : -1);
        f1 = (dimmerState2 ? dimmerBrightness2 / 255.0 : -1);
        break;
    case 2:
        if (dimmerState) {
            f0 = (dimmerTemperature > 1) ? ((dimmerTemperature - 1) / 254.0) : 0.0;
            f1 = (dimmerTemperature < 255) ? ((254 - dimmerTemperature) / 254.0) : 1.0;
            float scale = (1 / max(f0, f1)) * (dimmerBrightness / 255.0);
            f0 = (dimmerTemperature > 1) ? (f0 * scale) : -1;
            f1 = (dimmerTemperature < 255) ? (f1 * scale) : -1;
        }
        break;
    }
    channels[0].targetLevel =
        (f0 >= 0) ? dimmerConfig.rangeMin + f0 * ((float)(dimmerConfig.rangeMax - dimmerConfig.rangeMin)) : 0;

    if (channels[0].targetLevel > dimmerConfig.rangeMax) {
        channels[0].targetLevel = dimmerConfig.rangeMax;
    }

    channels[1].targetLevel =
        (f1 >= 0) ? dimmerConfig.rangeMin + f1 * ((float)(dimmerConfig.rangeMax - dimmerConfig.rangeMin)) : 0;

    if (channels[1].targetLevel > dimmerConfig.rangeMax) {
        channels[1].targetLevel = dimmerConfig.rangeMax;
    }

#ifdef Debug    
    char sdbg[16];
    sprintf(sdbg, "%d,%d", channels[0].targetLevel, channels[1].targetLevel);
    mqttPublish("dbgLevels", sdbg, false);
#endif    
    unsigned long t = millis();
    dimmerTransitionStarted = 0;
    for (int i = 0; i < DIMMER_NCHANNELS; i++) {
        if (dimmerTransition > 50) {
            if (channels[i].targetLevel != channels[i].level) {
                channels[i].a = (float)(channels[i].targetLevel - channels[i].level) / ((float)dimmerTransition);
                dimmerTransitionStarted = t;
            } else {
                channels[i].a = 0;
            }
        }
    }
}

void transitionLoop() {
    static unsigned long _t = 0;
    unsigned long t = millis();
    if (!timedOut(t, _t, 30)) return;
    _t = t;

    long dt = (dimmerTransitionStarted != 0) ?
        dimmerTransition - (long)((unsigned long)(millis() - dimmerTransitionStarted)) :
        0;
    if ((dt <= 0) && (dimmerTransitionStarted)) {
        dimmerTransitionStarted = 0;
        //aePrintln("Stopping Transition");
    }
    for (int i = 0; i < DIMMER_NCHANNELS; i++) {
        int l = channels[i].level;
        if (channels[i].level != channels[i].targetLevel) {
            if (dt > 0) {
                channels[i].level = channels[i].targetLevel - (int)(channels[i].a * ((float)dt));
            } else {
                channels[i].level = channels[i].targetLevel;
            }
        }
        if (channels[i].level != l) {
            analogWrite(channels[i].pin, channels[i].level);
        }
    }
}
#pragma endregion 

#pragma region mqttPublish
void dimmerMqttPublish() {
    if (!mqttConnected()) return;
    unsigned long t = millis();

    static int _state = -1;
    if ((_state != dimmerState) && mqttPublish(TOPIC_State, dimmerState ? 1 : 0, true)) {
        _state = dimmerState;
    }

    static int _brightness = -1;
    if ((_brightness != dimmerBrightness) && mqttPublish(TOPIC_Brightness, dimmerBrightness, true)) {
        _brightness = dimmerBrightness;
    }

    if (dimmerConfig.mode == 1) {
        static int _state2 = -1;
        if ((_state2 != dimmerState2) && mqttPublish(TOPIC_State2, dimmerState2 ? 1 : 0, true)) {
            _state2 = dimmerState2;
        }

        static int _brightness2 = -1;
        if ((_brightness2 != dimmerBrightness2) && mqttPublish(TOPIC_Brightness2, dimmerBrightness2, true)) {
            _brightness2 = dimmerBrightness2;
        }
    }
    if (dimmerConfig.mode == 2) {
        static int _temperature = -1;
        if ((_temperature != dimmerTemperature) &&
            mqttPublish(TOPIC_Temperature, dimmerTemperature, true) &&
            mqttPublish(TOPIC_Mireds, dimmerMireds, true)
            ) {
            _temperature = dimmerTemperature;
        }

        static int _rangeMin = -1;
        static int _rangeMax = -1;
        if ((_rangeMin != dimmerConfig.rangeMin) || (_rangeMax != dimmerConfig.rangeMax)) {
            char s[16];
            sprintf(s, "%d,%d", dimmerConfig.rangeMin, dimmerConfig.rangeMax);
            if (mqttPublish(TOPIC_Range, s, true)) {
                _rangeMin = dimmerConfig.rangeMin;
                _rangeMax = dimmerConfig.rangeMax;
            }
        }

        static int _miredsMin = -1;
        static int _miredsMax = -1;
        if ((_miredsMin != dimmerConfig.miredsMin) || (_miredsMax != dimmerConfig.miredsMax)) {
            char s[16];
            sprintf(s, "%d,%d", dimmerConfig.miredsMin, dimmerConfig.miredsMax);
            if (mqttPublish(TOPIC_MiredsRange, s, true)) {
                _miredsMin = dimmerConfig.miredsMin;
                _miredsMax = dimmerConfig.miredsMax;
            }
        }
    }


    static long _transition = -1;
    if ((_transition != dimmerTransition) && mqttPublish(TOPIC_Transition, dimmerTransition, true)) {
        _transition = dimmerTransition;
    }
}

#pragma endregion 

#pragma region MQTT Connect & Callback

void dimmerMqttConnect() {
#ifndef DIMMER_FIX_MODE
    mqttSubscribeTopic(TOPIC_SetMode);
#endif
    mqttSubscribeTopic(TOPIC_SetRange);
    mqttSubscribeTopic(TOPIC_SetMiredsRange);
    mqttSubscribeTopic(TOPIC_SetTransition);
    mqttSubscribeTopic(TOPIC_Switch);
    mqttSubscribeTopic(TOPIC_Switch2);
    mqttSubscribeTopic(TOPIC_SetState);
    mqttSubscribeTopic(TOPIC_SetState2);

    mqttSubscribeTopic(TOPIC_SetBrightness);
    mqttSubscribeTopic(TOPIC_SetBrightness2);
    mqttSubscribeTopic(TOPIC_SetTemperature);
    mqttSubscribeTopic(TOPIC_SetMireds);

    mqttSubscribeTopic(TOPIC_SaveDefaults);

    mqttPublish(TOPIC_Mode, (dimmerConfig.mode == 0) ? MODE0_Name : (dimmerConfig.mode == 1) ? MODE1_Name : MODE2_Name, true);
}

long extractInt(byte* payload, unsigned int length, long vMin, long vMax) {
    if ((length > 0) && (length < 30)) {
        char s[32];
        memset(s, 0, sizeof(s));
        strncpy(s, ((char*)payload), length);

        errno = 0;
        long v = strtol(s, NULL, 10);

        if ((errno == 0) && (v >= vMin) && (v <= vMax)) {
            return v;
        }
    }
    return -999;
}

bool dimmerMqttCallback(char* topic, byte* payload, unsigned int length) {

    if (mqttIsTopic(topic, TOPIC_Switch)) {
        dimmerState = !dimmerState;
        dimmerMqttPublish();
        transitionStart();
        return true;
    }

    if (mqttIsTopic(topic, TOPIC_Switch2)) {
        if (dimmerConfig.mode == 1) {
            dimmerState2 = !dimmerState2;
            dimmerMqttPublish();
            transitionStart();
        }
        return true;
    }

    if (mqttIsTopic(topic, TOPIC_SetState)) {
        if (length > 0) {
            if (length == 1) {
                dimmerState = ((*payload == '1') || (*payload == 1));
                dimmerMqttPublish();
                transitionStart();
            }
        }
        return true;
    }

    if (mqttIsTopic(topic, TOPIC_SetState2)) {
        if (length > 0) {
            if ((dimmerConfig.mode == 1) && (length == 1)) {
                dimmerState2 = ((*payload == '1') || (*payload == 1));
                dimmerMqttPublish();
                transitionStart();
            }
        }
        return true;
    }

    if (mqttIsTopic(topic, TOPIC_SaveDefaults)) {
        dimmerConfig.dimmerBrightness = dimmerBrightness;
        dimmerConfig.dimmerBrightness2 = dimmerBrightness2;
        dimmerConfig.dimmerTemperature = dimmerTemperature;
        dimmerConfig.dimmerTransition = dimmerTransition;
        storageSave();
        return true;
    }

#ifndef DIMMER_FIX_MODE
    if (mqttIsTopic(topic, TOPIC_SetMode)) {
        if (length > 0) {
            int m = extractInt(payload, length, 0, 2);
            if (m >= 0) {
                dimmerConfig.mode = m;
                storageSave();
                commsClearTopicAndRestart(TOPIC_SetMode);
            }
        }
        return true;
    }
#endif
    if (mqttIsTopic(topic, TOPIC_SetTransition)) {
        if (length > 0) {
            long t = extractInt(payload, length, 0, 30000);
            if (t >= 0) {
                dimmerTransition = t;
                transitionStart();
            }
        }
        return true;
    }

    if (mqttIsTopic(topic, TOPIC_SetRange)) {
        if (length > 0) {
            if ((length > 4) && (length < 16)) {
                char str[16];
                char* p = str;
                int min = -1, max = -1;

                memset(str, 0, sizeof(str));
                strncpy(str, ((char*)payload), length);

                errno = 0;
                min = (int)strtol(p, &p, 10);
                while (((*p) != 0) && (((*p) < '0') || ((*p) > '9'))) p++;

                if ((errno == 0) && ((*p) > 0)) {
                    max = (int)strtol(p, &p, 10);
                }
                if ((errno == 0) && (min > 0) && (min <= 1000) && (max > min + 10) && (max <= 1023)) {
                    dimmerConfig.rangeMax = max;
                    dimmerConfig.rangeMin = min;
                    storageSave();
                    transitionStart();
                }
            }
        }
        return true;
    }

    if (mqttIsTopic(topic, TOPIC_SetMiredsRange)) {
        if (length > 0) {
            if ((length > 4) && (length < 16)) {
                char str[16];
                char* p = str;
                int min = -1, max = -1;

                memset(str, 0, sizeof(str));
                strncpy(str, ((char*)payload), length);

                errno = 0;
                min = (int)strtol(p, &p, 10);
                while (((*p) != 0) && (((*p) < '0') || ((*p) > '9'))) p++;

                if ((errno == 0) && ((*p) > 0)) {
                    max = (int)strtol(p, &p, 10);
                }
                if ((errno == 0) && (min > 100) && (min <= 600) && (max > min + 10) && (max <= 600)) {
                    dimmerConfig.miredsMax = max;
                    dimmerConfig.miredsMin = min;
                    storageSave();
                    dimmerMqttPublish();
                    transitionStart();
                }
            }
        }
        return true;
    }

    if (mqttIsTopic(topic, TOPIC_SetBrightness)) {
        if (length > 0) {
            int l = extractInt(payload, length, 0, 255);
            if (l >= 0) {
                dimmerBrightness = l;
                dimmerMqttPublish();
                transitionStart();
            }
        }
        return true;
    }

    if (mqttIsTopic(topic, TOPIC_SetBrightness2)) {
        if (length > 0) {
            int l = extractInt(payload, length, 0, 255);
            if ((l >= 0) && (dimmerConfig.mode == 1)) {
                dimmerBrightness2 = l;
                dimmerMqttPublish();
                transitionStart();
            }
        }
        return true;
    }

    if (mqttIsTopic(topic, TOPIC_SetTemperature)) {
        if (length > 0) {
            int t = extractInt(payload, length, 0, 255);
            if (t >= 0) {
                dimmerTemperature = t;
                dimmerMqttPublish();
            }
        }
        transitionStart();
        return true;
    }

    if (mqttIsTopic(topic, TOPIC_SetMireds)) {
        if (length > 0) {
            int t = extractInt(payload, length, dimmerConfig.miredsMin, dimmerConfig.miredsMax);
            if (t >= 0) {
                dimmerTemperature = map(t, dimmerConfig.miredsMax, dimmerConfig.miredsMin, 0, 254);
                if (dimmerTemperature < 0) dimmerTemperature = 0;
                if (dimmerTemperature > 255) dimmerTemperature = 255;
                dimmerMqttPublish();
                transitionStart();
            }
        }
        return true;
    }

    return false;
}
#pragma endregion

#pragma region Dimmers API methods
void dimmerSwitch() {
    dimmerState = !dimmerState;
    if (dimmerConfig.mode == 1) {
        dimmerState2 = !dimmerState2;
    }
    transitionStart();
}
void dimmerStartGlowing() {
    dimmerState = true;
    dimmerGlowUp = true;
    dimmerGlowTimeout = 300;
    transitionStart();
}
void dimmerStopGlowing() {
    dimmerGlowUp = false;
}
void glowingLoop() {
    if (dimmerGlowUp) {
        unsigned long t = millis();
        if (timedOut(t, dimmerGlowDT, dimmerGlowTimeout)) {
            dimmerBrightness = dimmerBrightness + dimmerGlowDelta;
            if (dimmerBrightness >= 255) {
                dimmerBrightness = 255;
                dimmerGlowDelta = -dimmerGlowDelta;
                dimmerGlowTimeout = 3000;
            } else if (dimmerBrightness <= 1) {
                dimmerBrightness = 1;
                dimmerGlowDelta = -dimmerGlowDelta;
                dimmerGlowTimeout = 3000;
            } else {
                dimmerGlowTimeout = 300;
            }
            if (dimmerConfig.mode == 1) {
                dimmerBrightness2 = dimmerBrightness;
            }
            transitionStart();
            dimmerGlowDT = t;
        }
    }
}
#pragma endregion

#pragma region Loop, Init

void dimmerLoop() {
    dimmerMqttPublish();
    transitionLoop();
    glowingLoop();
}

void dimmerInit() {
    storageRegisterBlock(DIMMER_StorageId, &dimmerConfig, sizeof(dimmerConfig));

#ifdef DIMMER_FIX_MODE
    byte mode = DIMMER_FIX_MODE;
#else
    byte mode = dimmerConfig.mode;
#endif

    if ((dimmerConfig.mode != mode) ||
        (dimmerConfig.rangeMin < 1) || (dimmerConfig.rangeMin + 10 > dimmerConfig.rangeMax) || (dimmerConfig.rangeMax > 1023) ||
        (dimmerConfig.miredsMin < 100) || (dimmerConfig.miredsMin + 10 > dimmerConfig.miredsMax) || (dimmerConfig.miredsMax > 600)
        ) {

        dimmerConfig.mode = mode;
        dimmerConfig.dimmerBrightness = 255;
        dimmerConfig.dimmerBrightness2 = 255;
        dimmerConfig.rangeMin = 1;
        dimmerConfig.rangeMax = 1023;
        dimmerConfig.dimmerTemperature = 127;
        dimmerConfig.dimmerTransition = 200;
        dimmerConfig.miredsMin = 166;
        dimmerConfig.miredsMax = 333;
        storageSave();
    }

    dimmerBrightness = dimmerConfig.dimmerBrightness;
    dimmerBrightness2 = dimmerConfig.dimmerBrightness2;
    dimmerTemperature = dimmerConfig.dimmerTemperature;
    dimmerTransition = dimmerConfig.dimmerTransition;

    mqttRegisterCallbacks(dimmerMqttCallback, dimmerMqttConnect);

    analogWriteFreq(DIMMER_PWM_FREQ);

    for (int i = 0; i < DIMMER_NCHANNELS; i++) {
        pinMode(channels[i].pin, OUTPUT);
        digitalWrite(channels[i].pin, LOW);
    }
    aeRegisterLoop(dimmerLoop);
}
#pragma endregion
