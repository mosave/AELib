#include <BMP280_DEV.h> // MartinL: https://github.com/MartinL1/BMP280_DEV

#include "AELib.h"
#include "Comms.h"
#include "Barometer.h"

static char* TOPIC_BaroTemperature PROGMEM = "Sensors/Temperature";
static char* TOPIC_BaroPressure PROGMEM = "Sensors/Pressure";
static char* TOPIC_BaroValid PROGMEM = "Sensors/BarometerValid";

#define baroAccuracy (float)0.6
#define baroTempAccuracy (float)0.25
#define baroValidityTimeout ((unsigned long)(30*1000))

BMP280_DEV barometer( I2C_SDA, I2C_SCL );

float baroPressure;
float baroTemperature;
unsigned long baroUpdatedOn = 0;

bool baroValid() {
  return ( (unsigned long)(millis() - baroUpdatedOn) < baroValidityTimeout );
}

void baroPublishStatus() {
  if ( !mqttConnected() ) return;
  if( baroUpdatedOn==0 ) return;
  
  static int _valid = -1;
  int valid = baroValid() ? 1 : 0;
  if ( valid != _valid ) {
    if ( mqttPublish( TOPIC_BaroValid, valid, true ) ) _valid = valid;
  }
  if ( valid == 0 ) return;

  char b[31];
  bool hindex = false;
  float delta;

  static float _baroPressure = -1000;
  delta = baroPressure - _baroPressure;  if (delta < 0) delta = -delta;
  if ( (delta > baroAccuracy) ) {
    dtostrf( baroPressure, 0, 0, b );
    if ( mqttPublish( TOPIC_BaroPressure, b, true ) ) _baroPressure = baroPressure;
  }

  static float _baroTemperature = -1000;
  delta = baroTemperature - _baroTemperature;  if (delta < 0) delta = -delta;
  if ( (delta > baroTempAccuracy) ) {
    dtostrf( baroTemperature, 0, 1, b );
    if ( mqttPublish( TOPIC_BaroTemperature, b, true ) ) _baroTemperature = baroTemperature;
  }

}


void barometerLoop() {
  float temperature, pressure, altitude;
  if( barometer.getMeasurements( temperature, pressure, altitude ) ) {
    baroTemperature = temperature;
    baroPressure = pressure / 1.33322;
    baroUpdatedOn = millis();
  }
  baroPublishStatus();
}

bool barometerValid() {
  return baroValid();
}

float barometerPressure() {
  return baroPressure;
}

float barometerTemperature() {
  return baroTemperature;
}

void barometerInit() {
  barometer.begin(BMP280_I2C_ALT_ADDR);
  barometer.setTimeStandby(TIME_STANDBY_2000MS);     // Set the standby time to 2 seconds
  barometer.startNormalConversion();
  aeRegisterLoop(barometerLoop);
}
