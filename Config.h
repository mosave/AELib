#ifndef config_h
#define config_h

#include <Arduino.h>

// "<MQTT_Root>/Version" topic value
// Leave undefined to disable version
// #define VERSION "1.0"

#define WIFI_SSID "AE Home"
#define WIFI_Password  "chernayazhopa"

// Device host name and MQTT ClientId, default is "ESP_%s" if not defined
// Defining host name disables SetName MQTT command
// "%s" will be replaced with device' MAC address
//#define WIFI_HostName "NightLight"

#define MQTT_Address "1.1.1.33"
#define MQTT_Port 1883

// Top level of device' MQTT topic tree, default is "new/%s" if not defined
// Defining root path disables SetRoot MQTT command
// "%s" will be replaced with device host name (client Id)
// Examples:
// * "Bath/%s"
// * "SecondFloor/Bedroom/%s"
// * "%s"
//#define MQTT_Root "test/%s/"


#define LED_Pin 2


//#define ShowLoopTimes
#define LOOP std::function<void()>

void registerLoop( LOOP loop );
void Loop();


#endif
