#include "Config.h"
#include "Comms.h"
#include "BH1750.h" // Christopher Laws BH1750: https://github.com/claws/BH1750

static char* TOPIC_LMLevel PROGMEM = "Sensors/LightMeter";
static char* TOPIC_LMValid PROGMEM = "Sensors/LightMeterValid";

#define lmValidityTimeout ((unsigned long)(30*1000))

BH1750 lightMeter;

float lmLevel;
bool lmDetected;
byte lmMTReg = BH1750_DEFAULT_MTREG;
unsigned long lmUpdatedOn = 0;

float lmAccuracy( float lightLevel ) {
  if(lightLevel<10) {
    return 0.6;
  } else if( lightLevel < 50) {
    return 3;
  } else if( lightLevel < 100) {
    return 5;
  } else {
    return lightLevel / 20;
  }
}
bool lmValid() {
  return ( lmDetected && ((unsigned long)(millis() - lmUpdatedOn) < lmValidityTimeout ) );
}

void lmPublishStatus() {
  if( !mqttConnected() ) return;
  static int _valid = -1;
  int valid = lmValid() ? 1 : 0;
  if( valid != _valid ) {
    if( mqttPublish( TOPIC_LMValid, valid, true ) ) _valid = valid;
  }
  if( valid==0 ) return;
  
  char b[31];
  bool hindex = false;
  float delta;
  
  static float _lmLevel = -1000;
  delta = lmLevel - _lmLevel;  if(delta<0) delta = -delta;
  if( delta > lmAccuracy(lmLevel) ){
    unsigned long t = millis();
    static unsigned long publishedOn;
    if( (delta > lmLevel/3) || ((unsigned long)(t - publishedOn) > (unsigned long)60000 ) ) {
      dtostrf( lmLevel, 0, 1, b );
      if( mqttPublish( TOPIC_LMLevel, b, true ) ) {
        _lmLevel = lmLevel;
        publishedOn = t;
      }
    }
  }
}


void lmLoop() {
  if( !lmDetected ) return;
  
  unsigned long t = millis();
  static unsigned long checkedOn;
  
  if( (unsigned long)(t - checkedOn) > (unsigned long)1000 ) {
    checkedOn = t;
    lmLevel = lightMeter.readLightLevel();

    if( lmLevel >= 0 ) {
      lmUpdatedOn = t;
      byte mtreg = BH1750_DEFAULT_MTREG;
      if (lmLevel <= 10.0) { //very low light environment
        mtreg = 180;
      } else  if (lmLevel > 40000.0) { // reduce measurement time - needed in direct sun light
        mtreg = 32;
      }
      if( mtreg != lmMTReg ) {
        lmMTReg = mtreg;
        lightMeter.setMTreg(mtreg);
      }
      
      lmPublishStatus();
    } else {
      //aePrintf("t=%f, h=%f hindex=%f\n", tahTemperature, tahHumidity, dht.computeHeatIndex(tahTemperature, tahHumidity, false));
      lmPublishStatus();
    }
  }
}
bool lightMeterValid(){
  return lmValid();
}
float lightMeterLevel(){
  return lmLevel;
}

void lightMeterInit() {
  lmDetected = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  
  if( lmDetected ) {
    aePrintln(F("BH1750 found"));
    registerLoop( lmLoop );
  } else {
    aePrintln(F("Error initialising BH1750"));
  }  
}
