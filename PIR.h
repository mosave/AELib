#ifndef pir_h
#define pir_h

// All timeouts are in seconds
// Functions to directly work with dedicated PIRs.
// Leave mqttName empty to hide them from MQTT.
void pirRegister( byte pin, char* mqttName, bool inverted, bool pullUp, int timeout);
bool pirActive( byte pin );
bool pirEnabled( byte pin );
int pirTimeout( byte pin );
void pirSetEnabled( byte pin, bool enabled );
void pirSetTimeout( byte pin, int timeout );

// "Composite" PIR sensor:
void pirSetCompositeName( char* name );
bool pirActive();
bool pirEnabled();
int pirTimeout();
void pirSetEnabled( bool enabled );
void pirSetTimeout( int timeout);

void pirInit();

#endif
