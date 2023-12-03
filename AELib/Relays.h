#ifndef relays_h
#define relays_h

#define RELAYS_StorageId 'R'

// If name is null or empty then default "RelayN" will be used
void relayRegister(byte pin, char* name, bool inverted);
char* relayGetName(byte btnPin);
bool relayState(byte pin);
bool relaySetState(byte pin, bool state);
bool relaySwitch(byte pin);

void relayInit(bool mqtt);
void relayInit();
#endif
