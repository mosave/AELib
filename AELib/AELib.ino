#include <Device.h>

#include "AELib.h"
#include "Comms.h"
#include "LED.h"
#include "Buttons.h"
#include "Relays.h"

#define BTN0 16
#define BTN1 5
#define BTN2 4

#define RELAY1 14
#define RELAY2 12
#define RELAY3 13

void mqttConnect() {
  aePrintln("Subscribing other topics");
}

bool mqttCallback(char* topic, byte* payload, unsigned int length) {
    aePrintf("mqttCallback(\"%s\", %u, %u )\r\n", topic, payload, length);
    return false;
}

void setup() {
  ledInit( On );
  Serial.begin(115200);
  delay(500); 
  aePrintln();  aePrintln("Initializing");

  aeInit();
  commsInit();

  relayInit();
  relayRegister( RELAY1, NULL, false );
  relayRegister( RELAY2, NULL, false );
  relayRegister( RELAY3, NULL, false );


  btnInit();

  btnRegister( BTN0, false, true );
  btnDefaultFunction( BTN0, BDF_FactoryReset );
  
  btnRegister( BTN1, false, true );
  
  btnRegister( BTN2, false, true );
  
  mqttRegisterCallbacks( mqttCallback, mqttConnect );

  ledMode(Off);
}

void loop() {
  aeLoop();

  if (!haControlled()) {
    if( btnPressed( BTN1, BTN2 ) ) relaySwitch( RELAY3 ); 
    if( btnPressed( BTN1 ) ) relaySwitch( RELAY1 );
    if( btnPressed( BTN2 ) ) relaySwitch( RELAY2 );
  } else {
    btnPublishKeypressEvent(true, false);
  }

  if( btnState( BTN1 ) || btnState( BTN2 ) ) {
    ledMode( On );
  } else if ( !commsEnabled() ) {
    ledMode( Off ) ;
  } else if ( mqttConnected() ) {
    ledMode( Standby ) ;
  } else {
    ledMode( BlinkFast ) ;
  }
  
  delay(10);
}
