#ifndef comms_h
#define comms_h

#include <time.h>

#define COMMS_StorageId 'C'

#define MQTT_CALLBACK std::function<bool(char*, uint8_t*, unsigned int)> callback
#define MQTT_CONNECT std::function<void()> connect
#define MQTT_MAX_TOPIC_LEN 128

// Exported functions:
// WiFi
char* wifiHostName();

bool wifiConnected();
bool mqttConnected();

void commsConnect();
void commsDisconnect();

bool commsEnabled();
void commsEnable();
void commsDisable();

// Home Assistant
bool haConnected();
bool haControlled();

// All these functions treat TOPIC_Name as template and complete it with MQTT_Root, mqttClientId and optional variables (if passed)
// mqttTopic(...) function will be used to transform TOPIC_Name

char* mqttTopic(char* buffer, char* TOPIC_Name);
char* mqttTopic(char* buffer, char* TOPIC_Name, char* topicVar);
char* mqttTopic(char* buffer, char* TOPIC_Name, char* topicVar1, char* topicVar2);

bool mqttIsTopic(char* topic, char* TOPIC_Name);
bool mqttIsTopic(char* topic, char* TOPIC_Name, char* topicVar);
bool mqttIsTopic(char* topic, char* TOPIC_Name, char* topicVar1, char* topicVar2);

void mqttSubscribeTopic(char* TOPIC_Name);
void mqttSubscribeTopic(char* TOPIC_Name, char* topicVar);
void mqttSubscribeTopic(char* TOPIC_Name, char* topicVar1, char* topicVar2);

bool mqttPublish(char* TOPIC_Name, long value, bool retained);
bool mqttPublish(char* TOPIC_Name, char* topicVar, long value, bool retained);
bool mqttPublish(char* TOPIC_Name, char* topicVar1, char* topicVar2, long value, bool retained);

bool mqttPublish(char* TOPIC_Name, char* value, bool retained);
bool mqttPublish(char* TOPIC_Name, char* topicVar, char* value, bool retained);
bool mqttPublish(char* TOPIC_Name, char* topicVar1, char* topicVar2, char* value, bool retained);

// RAW topic names (no templating)
void mqttSubscribeTopicRaw(char* topic);
bool mqttPublishRaw(char* topic, long value, bool retained);
bool mqttPublishRaw(char* topic, char* value, bool retained);

// Parsers
/// <summary>Check if string is "boolean": 
/// "1"/"0", "on"/"off", "true"/"false","yes"/"no"
/// </summary>
/// <param name="str">String to parse</param>
/// <param name="value">Variable to return either true or false</param>
/// <returns>True if string passed contain one of the "boolean" values so "value" is valid</returns>
bool parseBool(char* str, bool* value);

/// <summary>
/// Parse string expecting floating point number validating it with range specified
/// </summary>
/// <param name="str">String to be parsed</param>
/// <param name="min">Minimum allowed number</param>
/// <param name="max">Maximum allowed number</param>
/// <param name="max">Parse result (if successful</param>
/// <returns>True if string passed contain floating value and it fall into the [min..max] range so "value" is valid</returns>
bool parseFloat(char* str, float min, float max, float* value);

// Human activity
void triggerActivity();

void mqttRegisterCallbacks(MQTT_CALLBACK, MQTT_CONNECT);

bool commsOTAEnabled();
void commsEnableOTA();

// Be sure TIMEZONE is defined in config.h for time functions to work
// TRUE if local time is synchronized 
bool commsTimeIsValid();

// Returns pointer to structure containing local time or NULL if local time is not yet synchronized.
tm* commsGetTime();

void commsRestart();
void commsClearTopicAndRestart(char* topic);
void commsClearTopicAndRestart(char* topic, char* topicVar1);
void commsClearTopicAndRestart(char* topic, char* topicVar1, char* topicVar2);

// Comms engine
void commsInit(bool isTimeCritical);
void commsInit();

#endif
