#include <Arduino.h>
#include "Config.h"
#include "Comms.h"
#include "Storage.h"

// Turn on debug output
#define Debug

// Relays storage block Id
#define RELAYS_StorageId 'R'

// Maximum number of relays supported
#define RelaysSize 8
#define RelayNameSize 16

// first %s is relay name
#define TOPIC_State "%s/State"

//Mininum relay switch timeout
#define TriggerDelay ((unsigned long)(300))
#define LoopDelay ((unsigned long)50)

// Normal relay
// LOW == OFF
// HIGH == ON

struct Relay {
  byte pin; // Pin number
  bool inverted; // false if button connects to ground
  bool state; // inverted ? (digitalRead(pin) == LOW) : (digitalRead(pin) == HIGH );
  char name[RelayNameSize];

  unsigned long triggeredOn;
  int reportedState;
};

struct RelayConfig {
  char name[RelayNameSize];
} relayConfig;

byte relayCount = 0;

void relayLoop( Relay* relay, bool publish );  //Forward definition

bool relayEnableMQTT = false;
Relay relays[RelaysSize];
RelayConfig relaysConfig[RelaysSize];

#ifdef Debug
void relayShowStatus( Relay* relay ) {
  Serial.print( (strlen(relay->name) > 0) ? relay->name : "Relay" );
  Serial.print(F(" #")); Serial.print(relay->pin); Serial.print(": ");
  Serial.print( relay->state ? "ON" : "Off" );
  Serial.print( ", reported " ); Serial.print( relay->reportedState );
  //  if( btn->wasPressed ) Serial.print(F("wasPressed "));
  //  if( btn->wasShortPressed ) Serial.print(F("wasShortPressed "));
  //  if( btn->wasLongPressed ) Serial.print(F("wasLongPressed "));
  //  if( btn->wasVeryLongPressed ) Serial.print(F("wasVeryLongPressed "));
  //  if( btn->wasReleased ) Serial.print(F("wasReleased "));
  Serial.println();
}
#endif

//**************************************************************************
//                    Helper functions
//**************************************************************************

Relay* relayFind( byte pin ) {
  for ( int i = 0; i < relayCount; i++ ) {
    if (relays[i].pin == pin ) return &relays[i];
  }
  return NULL;
}

void relayRegister( byte pin, char* name, bool inverted = false ) {
  if ( relayCount < RelaysSize - 1 ) {
    pinMode( pin, OUTPUT );
    relays[relayCount].pin = pin;
    relays[relayCount].inverted = inverted;
    if ( (name != NULL) && (strlen(name) > 0) ) {
      strncpy( relays[relayCount].name, name, RelayNameSize );
      strncpy( relaysConfig[relayCount].name, name, RelayNameSize );
    } else {
      if( strlen(relaysConfig[relayCount].name) <=0 ) {
        char _name[RelayNameSize];
        sprintf( relaysConfig[relayCount].name, "Relay%u", (relayCount+1) );
      }
      strncpy( relays[relayCount].name, relaysConfig[relayCount].name, RelayNameSize );
    }
    relays[relayCount].reportedState = -1;
    relayLoop( &relays[relayCount], false );

    relayCount++;
  }
}

char* relayGetName( byte pin ) {
  Relay* relay = relayFind(pin);
  return ( (relay != NULL) ? relay->name : NULL );
}

bool relayState( byte pin ) {
  Relay* relay = relayFind( pin );
  return ( (relay != NULL) && relay->state );
}

bool relaySetState( byte pin, bool state ) {
  Relay* relay = relayFind( pin );
  if ( relay == NULL ) return false;
  if ( relay->state != state ) {
    relay->state = state;
  }
  return relay->state;
}

bool relaySwitch( byte pin ) {
  return relaySetState( pin, !relayState(pin) );
}

//**************************************************************************
//                         MQTT support functions
//**************************************************************************

void relaysMQTTConnect() {
  if( relayEnableMQTT ) {
    for (int i = 0; i < relayCount; i++ ) if ( strlen(relays[i].name) > 0 ) {
      char s[64];
      mqttSubscribeTopic( "%s/SetName", relays[i].name );
      mqttSubscribeTopic( "%s/State", relays[i].name );
      mqttSubscribeTopic( "%s/Switch", relays[i].name );
    }
  }
}

bool relaysMQTTCallback( char* topic, byte* payload, unsigned int length ) {
  char s[63];
  //Serial.print("Payload=");
  //if( payload != NULL ) { Serial.println( (char*)payload ); } else { Serial.println( "empty" );}
  for (int i = 0; i < relayCount; i++ ) if ( strlen(relays[i].name) > 0 ) {
      mqttTopic(s, "%s", relays[i].name );
      if ( strncmp(topic, s, strlen(s) ) == 0 ) {
        topic += strlen(s);
        if ( (*topic) = '/' ) topic++;
        if ( strcmp( topic, "SetName" ) == 0 ) {
          if( (payload != NULL) && (length > 1) && (length<32) ) {
            memset( relaysConfig[i].name, 0, sizeof(relaysConfig[i].name) );
            strncpy( relaysConfig[i].name, ((char*)payload), length );
            Serial.print(F("Button name set to ")); Serial.println(relaysConfig[i].name);
            commsRestart();
          }
          return true;
        } else if ( strcmp( topic, "State" ) == 0 ) {
          if( (payload != NULL) && (length == 1) ) {
            relaySetState( relays[i].pin,  (*payload == '1') || (*payload == 1) );
          }
          return true;
        } else if ( strcmp( topic, "Switch" ) == 0 ) {
          relaySwitch(relays[i].pin);
          return true;
        }
      }
    }
  return false;
}

//**************************************************************************
//                            Relays engine
//**************************************************************************

void relayLoop( Relay* relay, bool publish ) {
  unsigned long t = millis();

  if ( (unsigned long)(t - relay->triggeredOn) > TriggerDelay ) {
    bool currentState = (digitalRead( relay->pin ) == HIGH);
    if ( relay->inverted ) currentState = !currentState;

    if ( relay->state != currentState ) {
      bool s = relay->state;
      if ( relay->inverted ) s = !s;
      digitalWrite( relay->pin,  s ? HIGH : LOW );
      relay->triggeredOn = t;
    }
  }

  if ( relayEnableMQTT && publish && (strlen(relay->name) > 0) ) {
    int state = (relay->state ? 1 : 0);
    if ( (state != relay->reportedState) && mqttPublish( TOPIC_State, relay->name, state, true  ) ) relay->reportedState = state;
  }
}

unsigned long lastCheck = 0;

void relaysLoop() {
  unsigned long t = millis();
  if ( (unsigned long)(t - lastCheck) < LoopDelay ) return;
  lastCheck = t;

  bool publish = (relayEnableMQTT && mqttConnected());

  for (int i = 0; i < relayCount; i++ ) {
    relayLoop( &relays[i], publish );
  }
}


void relayInit(bool enableMQTT) {
  relayEnableMQTT = enableMQTT;
  storageRegisterBlock( RELAYS_StorageId, &relaysConfig, sizeof(relaysConfig) );
  if( relayEnableMQTT ) {
    mqttRegisterCallbacks( relaysMQTTCallback, relaysMQTTConnect );
  }
  registerLoop( relaysLoop );
}

void relayInit() {
  relayInit(true);
}
