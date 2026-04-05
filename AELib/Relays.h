#ifndef relays_h
#define relays_h

void relayRegister(byte pin, bool inverted, bool state);
void relayRegister(byte pin, bool inverted);
bool relayState(byte pin);
bool relaySetState(byte pin, bool state);
bool relaySwitch(byte pin);

void relayInit(bool mqtt);
void relayInit();

#endif
