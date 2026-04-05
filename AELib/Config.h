#ifndef config_h
#define config_h

/////////////////////////////////////////////////////////////////////
///      General IoT device configuration: WiFi, MQTT etc
/////////////////////////////////////////////////////////////////////

/// EEPROM snapshot version byte. Default in AELib.cpp is 0x01.
/// Bump this value when the storage layout becomes incompatible with previously saved data.
// #define STORAGE_Version 0x02

/// Firmware version number to display in device info topic: 
/// "<MQTT Root>/DeviceInfo"
/// Leave undefined if not required
// #define VERSION "1.0"

/// SSID and Password are mandatory.
// #define WIFI_SSID "SSID"
// #define WIFI_Password  "WiFi password"

/// Device host name and MQTT ClientId, default is "ESP_%s"
/// "%s" will be replaced with device' MAC address
/// "SetName" MQTT command will not be available if hostname defined
// #define WIFI_HostName "NightLight"

/// MQTT Broker configuration.
/// Device will search Broker via mDNS service if not defined:
// #define MQTT_Address "MQTT Broker address"
// #define MQTT_Port 1883

/// Log on anonymously if not defined
// #define MQTT_User "MQTTUserName"
// #define MQTT_Password "12345"

/// Top level of device' MQTT topic tree, default is "new/%s" if not defined
/// Defining root path disables SetRoot MQTT command
/// "%s" will be replaced with device host name (client Id)
/// Examples:
/// * "Bath/%s"
/// * "SecondFloor/Bedroom/%s"
/// * "%s"
// #define MQTT_Root "test/%s/"

/// Re-define PubSubClient maximum data packet size if required:
// #define MQTT_MAX_PACKET_SIZE 1024


/// Synchronize device time to TIMEZONE is set and NTP server is available.
/// Check "tz.h" for timezone constants
#define TIMEZONE TZ_Europe_Moscow

/// Re-define default NTP servers if required:
// #define NTP_SERVER1 "time.google.com"
// #define NTP_SERVER2 "time.nist.gov"


// Redefine these functions to disable serial port debug output across all AELib modules:
// #define aePrint( ... )
// #define aePrintf( ... )
// #define aePrintln( ... )


/////////////////////////////////////////////////////////////////////
///                   LED.h module configuration
/////////////////////////////////////////////////////////////////////
/// Define if your LED pin is not 2
// #define LED_Pin 2

/// Define this to invert LED power
// #define LED_Inverted

/////////////////////////////////////////////////////////////////////
///                 Buttons.h module configuration
/////////////////////////////////////////////////////////////////////

/// If BUTTONS_EASY_MODE is defined:
/// * btnVeryLongPressed() will not be available
/// * btnLongPressed() will trigger BEFORE button release
// #define BUTTONS_EASY_MODE

/////////////////////////////////////////////////////////////////////
///                 Dimmer.h module configuration
/////////////////////////////////////////////////////////////////////

/// Dimmer control channel output pin(s).
/// Configure second channel to enable WCWW and 2-channel modes
// #define DIMMER_CH1 2
// #define DIMMER_CH2 -1

/// Fixed dimmer mode (compile-time lock). If not defined, mode is taken from EEPROM / MQTT Dimmer/SetMode.
/// 0: Single channel, 1: Two channels, 2: White Cold/Warm
// #define DIMMER_FIX_MODE 0

/// PWM frequency for analogWrite. Default in Dimmer.cpp is 120 Hz.
// #define DIMMER_PWM_FREQ 120

/////////////////////////////////////////////////////////////////////
///                            Other
/////////////////////////////////////////////////////////////////////

/// I2C pins for sensors (e.g. Barometer, HTU21D)
// #define I2C_SDA 12
// #define I2C_SCL 13

/// DHT22 sensor pin
// #define DHT_Pin 13


/// Config.AE.h contains Developer' private configuration.
/// Please define valid WIFI_SSID and WIFI_Password above :)
#ifndef WIFI_SSID
#include "Config.AE.h"
#endif

#endif
