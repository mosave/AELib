#include <Arduino.h>
#include "Buttons.h"
#include "Config.h"
#include "Comms.h"
#include "PIR.h"

//#define Debug

// Maximum number of PIR sensors supported
#define PIRsSize 8
#define PIRNameSize 16
#define PIRSensorTimeout 5

// "%s" will be PIR name
// Settings:
static char* TOPIC_Timeout PROGMEM = "%s/Timeout";
static char* TOPIC_Enabled PROGMEM = "%s/Enabled";
// State:
static char* TOPIC_Active PROGMEM = "%s/Active";

// Message to accept
static char* TOPIC_Enable PROGMEM = "%s/Enable";
static char* TOPIC_Disable PROGMEM = "%s/Disable";
static char* TOPIC_SetTimeout PROGMEM = "%s/SetTimeout";

struct PIR {
  byte pin; // Pin number
  bool inverted; 
  int timeout;
  bool enabled; 
  bool active; 
  char name[PIRNameSize];
  unsigned long activatedOn;
  // last reported states
  int _active;
  int _enabled;
  int _timeout;
};
char pirCompositeName[PIRNameSize];
byte pirCount = 0;

PIR pirs[PIRsSize];

short int pirIndex( byte pin ) {
  for( int i=0; i<pirCount; i++ ) {
    if(pirs[i].pin == pin ) return i;
  }
  return -1;
}

bool pirActive( PIR* pir ) {
  if( !pir->enabled ) return false;
  if( pir->active ) return true;
  return ( (pir->activatedOn>0) && ((unsigned long)(millis() - pir->activatedOn) < ((unsigned long)(pir->timeout-PIRSensorTimeout)) * ((unsigned long)1000))  );
}


void pirLoop( PIR* pir ) {
  unsigned long t = millis();
  bool active = (digitalRead(pir->pin) == LOW);
  char topic[64];
  if( pir->inverted ) active = !active;
  if( pir->active != active ) {
    pir->active = active;
  }
  if( active ) pir->activatedOn = t;

  if( (strlen(pir->name)>0) && mqttConnected() ) {
    int v = pir->enabled ? 1 : 0;
    if( (v != pir->_enabled) && mqttPublish( TOPIC_Enabled, pir->name, v, true  ) ) pir->_enabled = v;

    v = pir->timeout;
    if( (v != pir->_timeout) && mqttPublish( TOPIC_Timeout, pir->name, v, true  ) ) pir->_timeout = v;
  
    v = pirActive(pir) ? 1 : 0;
    if( (v != pir->_active) && mqttPublish( TOPIC_Active, pir->name, v, true  ) ) pir->_active = v;
  }
  
  
#ifdef Debug
  print( (strlen(pir->name)>0) ? pir->name : "PIR" );
  print(F(" #")); print(pir->pin); 
  print(F(": t=")); print(pir->timeout);
  print(F(", e=")); print( pir->enabled ? 1 : 0);
  print(F(", a=")); print( pir->active ? 1 : 0);
  print(F(", A=")); print( v );
  println();
#endif
}

//**************************************************************************
//                         MQTT support functions
//**************************************************************************

void pirsMQTTConnect() {
  char s[64];
  for(int i=0; i<pirCount; i++ ) if( strlen(pirs[i].name)>0 ) {
    mqttSubscribeTopic( TOPIC_Enable, pirs[i].name );
    mqttSubscribeTopic( TOPIC_Disable, pirs[i].name );
    mqttSubscribeTopic( TOPIC_SetTimeout, pirs[i].name );
  }

  if( strlen(pirCompositeName)>0 ) {
    mqttSubscribeTopic( TOPIC_Enable, pirCompositeName );
    mqttSubscribeTopic( TOPIC_Disable, pirCompositeName );
    mqttSubscribeTopic( TOPIC_SetTimeout, pirCompositeName );
  }

}

bool pirsCallback( PIR* pir, char* topic, byte* payload, unsigned int length ) {
  char s[63];
  if( pir != NULL ) {
    if( strlen(pir->name)<=0 ) return false;
    mqttTopic(s, "%s", pir->name );
  } else {
    if( strlen(pirCompositeName)<=0 ) return false;
    mqttTopic(s, "%s", pirCompositeName );
  }
  
  if( strncmp(topic, s, strlen(s) ) != 0 ) return false;
  
  topic += strlen(s);
  if( (*topic)='/' ) topic++;
  
  if( strcmp( topic, "SetTimeout" )==0 ) {
    if( (payload != NULL) && (length>0) ) {
      int t = 0; 
      for( int p=0; p<length; p++) {
        if( ((*(payload+p))>='0') && ((*(payload+p))<='9') ) t = t * 10 + ((*(payload+p))-'0');
      }
      if( pir != NULL ) {
        pirSetTimeout( pir->pin, t );
      } else {
        pirSetTimeout( t );
      }
    }
    return true;
  } else if( strcmp( topic, "Enable" )==0 ) {
      if( pir != NULL ) {
        pirSetEnabled(pir->pin, true );
      } else {
        pirSetEnabled( true );
      }
    return true;
  } else if( strcmp( topic, "Disable" )==0 ) {
      if( pir != NULL ) {
        pirSetEnabled(pir->pin, false);
      } else {
        pirSetEnabled( false );
      }
    return true;
  }
  return false;
}

bool pirsMQTTCallback( char* topic, byte* payload, unsigned int length ) {
  bool result = false;
//  print("Payload="); 
//  if( payload != NULL ) { println( (char*)payload ); } else { println( "empty" );}
  for(int i=0; i<pirCount; i++ ) {
    if( pirsCallback( &pirs[i], topic, payload, length ) ) result = true;
  }
  if( pirsCallback( NULL, topic, payload, length ) ) result = true;
  return result;
}

//**************************************************************************
//                         Interface functions implementation
//**************************************************************************
void pirRegister( byte pin, char* name, bool inverted, bool pullUp, int timeout) {
  if(pirCount < PIRsSize-1) {
    pinMode( pin, pullUp ? INPUT_PULLUP : INPUT);
    pirs[pirCount].pin = pin;
    if( (name != NULL) && (strlen(name)>0) ) {
      strncpy( pirs[pirCount].name, name, PIRNameSize );
    }
    if( timeout < PIRSensorTimeout ) timeout = PIRSensorTimeout;
    pirs[pirCount].inverted = inverted;
    pirs[pirCount].timeout = timeout;
    pirs[pirCount].active = false;
    pirs[pirCount].activatedOn = 0;
    pirs[pirCount].enabled = true;
    pirs[pirCount]._active = -1;
    pirs[pirCount]._enabled = -1;
    pirs[pirCount]._timeout = -1;
    pirCount++;
    pirLoop(&pirs[pirCount-1]);
  }
}

bool pirActive( byte pin ) {
  short int index = pirIndex(pin);
  return ((index>=0) && pirActive( &pirs[index] ) );
}

bool pirEnabled( byte pin ) {
  short int index = pirIndex(pin);
  return ((index>=0) && pirs[index].enabled );
}

int pirTimeout( byte pin ) {
  short int index = pirIndex(pin);
  if( index<0 ) return 0;
  return pirs[index].timeout;
}

void pirSetEnabled( byte pin, bool enabled ) {
  short int index = pirIndex(pin);
  if(index>=0) pirs[index].enabled = enabled;
}

void pirSetTimeout( byte pin, int timeout ) {
  short int index = pirIndex(pin);

  if( timeout < PIRSensorTimeout ) timeout = PIRSensorTimeout;
  if(index>=0) pirs[index].timeout = timeout;
}

void pirSetCompositeName( char* mqttName ) {
  strncpy( pirCompositeName, mqttName, PIRNameSize );
}
bool pirActive() {
  for(int i=0; i<pirCount; i++){
    if( pirActive( &pirs[i] ) ) return true;
  }
  return false;
}

bool pirEnabled() {
  for(int i=0; i<pirCount; i++){
    if( pirs[i].enabled ) return true;
  }
  return false;
}
int pirTimeout(){
  int timeout = 0;
  for(int i=0; i<pirCount; i++){
    if( pirs[i].timeout > timeout ) timeout = pirs[i].timeout;
  }
  return timeout;
}
void pirSetEnabled( bool enabled ){
  for(int i=0; i<pirCount; i++){
    pirs[i].enabled = enabled;
  }
}
void pirSetTimeout( int timeout){
  for(int i=0; i<pirCount; i++){
    pirs[i].timeout = timeout;
  }
}


void pirsLoop() {
  int timeout = 0;
  int enabled = 0;
  int active = 0;
  
  for(int i=0; i<pirCount; i++ ) {
    if( pirs[i].enabled ) enabled = 1;
    if( pirs[i].timeout > timeout ) timeout = pirs[i].timeout;
    if( pirActive( &pirs[i] ) ) active = 1;
    pirLoop( &pirs[i] );
  }

  if( (strlen(pirCompositeName)>0) && mqttConnected() ) {
    static int _enabled = -1;
    static int _timeout = -1;
    static int _active = -1;
    if( (_enabled != enabled) && mqttPublish( TOPIC_Enabled, pirCompositeName, enabled, true  ) ) _enabled = enabled;
    if( (_timeout != timeout) && mqttPublish( TOPIC_Timeout, pirCompositeName, timeout, true  ) ) _timeout = timeout;
    if( (_active != active) && mqttPublish( TOPIC_Active, pirCompositeName, active, true  ) ) _active = active;
  }
}

void pirInit() {
  mqttRegisterCallbacks( pirsMQTTCallback, pirsMQTTConnect );
  registerLoop(pirsLoop);
}
