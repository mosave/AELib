#ifndef config_h
#define config_h

#include <Arduino.h>

// "<MQTT_Root>/Version" topic value
// Leave undefined to disable version
// #define VERSION "1.0"


//#define WIFI_SSID "SSID"
//#define WIFI_Password  "WiFi password"

// Device host name and MQTT ClientId, default is "ESP_%s"
// "%s" is to be replaced with device' MAC address
// Disables SetName MQTT command
//#define WIFI_HostName "NightLight"

//#define MQTT_Address "MQTT Broker address"
//#define MQTT_Port 1883

// If NO MQTT address or port defined then client will search for 
// mDNS advertisement with service type="mqtt" and protocol="tcp" 



#ifndef WIFI_SSID
    #include "Config.AE.h"
#endif


// Top level of device' MQTT topic tree, default is "new/%s" if not defined
// Defining root path disables SetRoot MQTT command
// "%s" will be replaced with device host name (client Id)
// Examples:
// * "Bath/%s"
// * "SecondFloor/Bedroom/%s"
// * "%s"
//#define MQTT_Root "test/%s/"

// Define if I2S connected devices used e.g. Barometer
//#define I2C_SDA 12
//#define I2C_SCL 13

// Define pin where DHT22 sensor connected to
//#define DHT_Pin 13

// Define this to allow serial port output
#define aePrintf( ... ) Serial.printf( __VA_ARGS__ )
#define aePrint( ... ) Serial.print( __VA_ARGS__ )
#define aePrintln( ... ) Serial.println( __VA_ARGS__ )

// or this to disable serial port output
//#define aePrint( ... )
//#define aePrintf( ... )
//#define aePrintln( ... )


#define LED_Pin 2


//#define ShowLoopTimes
#define LOOP std::function<void()>

void registerLoop( LOOP loop );
void Loop();


#endif
