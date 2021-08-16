#include <errno.h>
#include "Config.h"
#include "Comms.h"
#include "Storage.h"
#include "BH1750.h" // Christopher Laws BH1750: https://github.com/claws/BH1750

static char* TOPIC_LMLevel PROGMEM = "Sensors/LightLevel";
static char* TOPIC_LMValid PROGMEM = "Sensors/LightLevelValid";

static char* TOPIC_LMSunriseLevel  PROGMEM = "Sensors/SunriseLevel";
static char* TOPIC_LMSetSunriseLevel  PROGMEM = "Sensors/SetSunriseLevel";
static char* TOPIC_LMSunsetLevel  PROGMEM = "Sensors/SunsetLevel";
static char* TOPIC_LMSetSunsetLevel  PROGMEM = "Sensors/SetSunsetLevel";

static char* TOPIC_LMSunrise PROGMEM = "Sensors/Sunrise";
static char* TOPIC_LMSunset  PROGMEM = "Sensors/Sunset";
static char* TOPIC_LMPhase  PROGMEM = "Sensors/DayPhase";


// debug mode
//#define Debug

#define lmValidityTimeout ((unsigned long)(30*1000))
// Normal sunset detection timeframe (minutes)
#define LMSS_TIMEFRAME 30
// Minimum sunset detection timeframe (minutes) (wait after device start)
#define LMSS_TIMEFRAME_MIN 10

// Default sunrise/sunset trigger light levels:
#define LMSS_SUNRISE_LEVEL 10.0
#define LMSS_SUNSET_LEVEL 5.0

#define LMSS_UNKNOWN 0
#define LMSS_DAY 1
#define LMSS_NIGHT 2

struct LmConfig {
    float sunriseLevel;
    float sunsetLevel;
} lmConfig;

struct LmssDataPoint {
    float l;
    unsigned long t;
    //    bool f;
};


BH1750 lightMeter;

float lmLevel;
bool lmDetected;

bool lmssEnabled;
LmssDataPoint lmssData[LMSS_TIMEFRAME];
float lmssLevelSum;
int lmssLevelCnt;

int lmssDayPhase = LMSS_UNKNOWN;


byte lmMTReg = BH1750_DEFAULT_MTREG;
unsigned long lmUpdatedOn = 0;

float lmAccuracy(float lightLevel) {
    if (lightLevel < 10) {
        return 0.6;
    } else if (lightLevel < 50) {
        return 3;
    } else if (lightLevel < 100) {
        return 5;
    } else {
        return lightLevel / 20;
    }
}

bool lmValid() {
    return (lmDetected && ((unsigned long)(millis() - lmUpdatedOn) < lmValidityTimeout));
}

#pragma region MQTT publishing

void lmPublishSettings() {
  char b[31];
  dtostrf( lmConfig.sunriseLevel, 0, 1, b);
  mqttPublish(TOPIC_LMSunriseLevel, b, true);
  dtostrf( lmConfig.sunsetLevel, 0, 1, b);
  mqttPublish(TOPIC_LMSunsetLevel, b, true);
}

void lmPublishStatus() {
    if (!mqttConnected()) return;
    static int _valid = -1;
    int valid = lmValid() ? 1 : 0;
    if (valid != _valid) {
        if (mqttPublish(TOPIC_LMValid, valid, true)) _valid = valid;
    }
    if (valid == 0) return;

    char b[31];
    bool hindex = false;
    float delta;

    static float _lmLevel = -1000;
    delta = lmLevel - _lmLevel;  if (delta < 0) delta = -delta;
    if (delta > lmAccuracy(lmLevel)) {
        unsigned long t = millis();
        static unsigned long publishedOn;
        if ((delta > lmLevel / 3) || ((unsigned long)(t - publishedOn) > (unsigned long)60000)) {
            dtostrf(lmLevel, 0, 1, b);
            if (mqttPublish(TOPIC_LMLevel, b, true)) {
                _lmLevel = lmLevel;
                publishedOn = t;
            }
        }
    }
    if( lmssEnabled ) {
#ifdef TIMEZONE      
        time_t currentTime = time(nullptr);
        if( currentTime > 10000 ) {
#endif
            static int _dayPhase = -1;
            if (lmssDayPhase != _dayPhase) {
                if (lmssDayPhase == LMSS_DAY) {
                    mqttPublish(TOPIC_LMPhase, "Day", true);
                } else if (lmssDayPhase == LMSS_NIGHT) {
                    mqttPublish(TOPIC_LMPhase, "Night", true);
                } else {
                    mqttPublish(TOPIC_LMPhase, "Unknown", true);
                }

#ifdef TIMEZONE
                if (_dayPhase >= 0) {
                    tm* lt = localtime(&currentTime);
                    sprintf(b, "%02d:%02d", lt->tm_hour, lt->tm_min);

                    if ((_dayPhase == LMSS_DAY) && (lmssDayPhase == LMSS_NIGHT)) {
                        mqttPublish(TOPIC_LMSunset, b, true);
                    } else if ((_dayPhase == LMSS_NIGHT) && (lmssDayPhase == LMSS_DAY)) {
                        mqttPublish(TOPIC_LMSunrise, b, true);
                    }
                }
#endif
                _dayPhase = lmssDayPhase;
            }
#ifdef TIMEZONE
        }
#endif
    }
}
#pragma endregion

#pragma region Sunset/Sunrise detection code

void lmMqttConnect() {
  aePrintln("Subscribing other topics");
  lmPublishSettings();
  mqttSubscribeTopic( TOPIC_LMSetSunriseLevel );
  mqttSubscribeTopic( TOPIC_LMSetSunsetLevel );
}

bool lmMqttCallback(char* topic, byte* payload, unsigned int length) {
    //aePrintf("mqttCallback(\"%s\", %u, %u )\r\n", topic, payload, length);
        
    int cmd = 0;
    if( lmssEnabled && mqttIsTopic( topic, TOPIC_LMSetSunriseLevel ) ) {
      cmd = 1;
    } else if( lmssEnabled && mqttIsTopic( topic, TOPIC_LMSetSunsetLevel ) ) {
      cmd = 2;
    }
    
    if( cmd>0 ) {
      if( (payload != NULL) && (length > 0) && (length<31) ) {
        char s[31];
        memset( s, 0, sizeof(s) );
        strncpy( s, ((char*)payload), length );
        
        errno = 0;
        float v = ((int)(strtof(s,NULL) * 10.0)) / 10.0 ;
        
        if ( (errno==0) && (v>=0.1) && (v<1000) ) {
          if( cmd=1 ) {
            lmConfig.sunriseLevel = v;
            mqttPublish(TOPIC_LMSetSunriseLevel,(char*)NULL, false);
          } else {
            lmConfig.sunsetLevel = v;
            mqttPublish(TOPIC_LMSetSunsetLevel,(char*)NULL, false);
          }
          storageSave();
          lmPublishSettings();
        }
      }
      return true;
    }
    return false;
}


bool lmssApproximate(double* a, double* b, bool filter) {
    unsigned long t0 = millis();
    double sL = 0;
    double sT = 0;
    double sT2 = 0;
    double sTL = 0;
    int n = 0;

    for (int i = 0; (i < LMSS_TIMEFRAME) && (lmssData[i].t); i++) {
        double l = lmssData[i].l;
        double t = (t0 - lmssData[i].t)/1000.0;
        if ( t <= LMSS_TIMEFRAME*60+5 ) {
            double l2 = l * l;
            // Take this point if filtering is off or point is "close" to graph
            if (!filter || (abs((*a) * t + (*b) - l) < 100)) {
                sT += t;
                sL += l;
                sT2 += t * t;
                sTL += t * l;
                n++;
            }
        }
    }
    if (n < 5) return false;

    
    *a = ((n * sTL) - (sT * sL)) / (n * sT2 - sT * sT);
    *b = (sL - (*a) * sT) / n;
#ifdef Debug
    aePrint("L0="); aePrint(lmssData[0].l); 
    aePrint("n="); aePrint(n); aePrint(" sT="); aePrint(sT); aePrint(" sT2="); aePrint(sT2); aePrint(" sL="); aePrint(sL); 
    aePrint(" sTL="); aePrint(sTL);aePrint(" a="); aePrint(*a); aePrint(" b="); aePrintln(*b);
#endif
    return true;
}

void lmssUpdateStatus() {
    double a, b, sL2Filter, sL2Max;

    if (lmssApproximate(&a, &b, false) /*&& lmssApproximate(&a, &b, true)*/ ) {
        // b = current light level approximation
        // b0 = "(TIMEFRAME-1) minutes ago" light level approximation
        float b0 = b + a * (LMSS_TIMEFRAME-1)*60.0;
//        char s[31];
//        dtostrf(b0, 0, 1, s);
//        mqttPublish("Sensors/b0", s, false);
//        dtostrf(b, 0, 1, s);
//        mqttPublish("Sensors/b", s, false);
        
        if ( (lmssDayPhase != LMSS_DAY) && (b0 < lmConfig.sunriseLevel) && (b > lmConfig.sunriseLevel) ) {
            lmssDayPhase = LMSS_DAY;
        } else if ( (lmssDayPhase != LMSS_NIGHT) && (b0 > lmConfig.sunsetLevel) && (b < lmConfig.sunsetLevel) ) {
            lmssDayPhase = LMSS_NIGHT;
        }
    }
}

#pragma endregion

void lmLoop() {
    if (!lmDetected) return;

    unsigned long t = millis();
    static unsigned long checkedOn;

    if ((unsigned long)(t - checkedOn) > (unsigned long)1000) {
        checkedOn = t;
        lmLevel = lightMeter.readLightLevel();

        if (lmLevel >= 0) {
            lmUpdatedOn = t;
            byte mtreg = BH1750_DEFAULT_MTREG;
            if (lmLevel <= 10.0) { //very low light environment
                mtreg = 180;
            } else  if (lmLevel > 40000.0) { // reduce measurement time - needed in direct sun light
                mtreg = 32;
            }
            if (mtreg != lmMTReg) {
                lmMTReg = mtreg;
                lightMeter.setMTreg(mtreg);
            }
            if (lmssEnabled) {
                lmssLevelSum = lmssLevelSum + lmLevel;
                lmssLevelCnt++;
                if ((lmssData[0].t == 0) && (lmssLevelCnt > 10) ||
                    (lmssData[0].t > 0) && ((unsigned long)(t - lmssData[0].t) >= (unsigned long)60000)) {
                    for (int i = LMSS_TIMEFRAME-1; i > 0; i--) {
                        lmssData[i] = lmssData[i - 1];
                    }
                    lmssData[0].l = lmssLevelSum / lmssLevelCnt;
                    lmssData[0].t = t;
                    lmssUpdateStatus();
                    lmssLevelSum = 0; lmssLevelCnt = 0;
                }
            }
            lmPublishStatus();
        } else {
            //aePrintf("t=%f, h=%f hindex=%f\n", tahTemperature, tahHumidity, dht.computeHeatIndex(tahTemperature, tahHumidity, false));
            lmPublishStatus();
        }
    }
}
bool lightMeterValid() {
    return lmValid();
}
float lightMeterLevel() {
    return lmLevel;
}

void lightMeterInit(bool sunTracker) {
    lmssEnabled = sunTracker;
    lmDetected = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

    if (lmssEnabled) {
        memset(lmssData, 0, sizeof(lmssData));
        lmssLevelSum = 0;
        lmssLevelCnt = 0;
        storageRegisterBlock('L', &lmConfig, sizeof(lmConfig));
        if ((lmConfig.sunriseLevel == 0) || (lmConfig.sunsetLevel == 0)) {
            lmConfig.sunriseLevel = LMSS_SUNRISE_LEVEL;
            lmConfig.sunsetLevel = LMSS_SUNSET_LEVEL;
        }
#ifdef Debug
        randomSeed(micros());
        for (int i = LMSS_TIMEFRAME-1; i>=0; i--) {
            lmssData[i].t = millis() - ((unsigned long)i) * (unsigned long)60000L;
            lmssData[i].l = /*80 - */(i / (float)LMSS_TIMEFRAME)*80 + random(20);
            if (lmssData[i].l < 0) lmssData[i].l = 0.1;
            aePrint((int)(lmssData[i].l * 100) / 100.0); aePrint(" ");
        }
        aePrintln();
        double a, b;
        aePrintln(lmssApproximate(&a, &b, false));
        aePrintln(lmssApproximate(&a, &b, true));

        aePrintln("");
#endif
        mqttRegisterCallbacks( lmMqttCallback, lmMqttConnect );
    }

    if (lmDetected) {
        aePrintln(F("BH1750 found"));
        registerLoop(lmLoop);
    } else {
        aePrintln(F("Error initialising BH1750"));
    }
}

void lightMeterInit() {
    lightMeterInit(false);
}
