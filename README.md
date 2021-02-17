
1. Please include AELib.cpp in all your projects. Basic module initialization and loop support engine

2. Comms MQTT топики:
   * Version = "version" : retained value of VERSION const if defined in Config.h
   * Address = "IP Address" : .
   * Online = "1" | "0" : retained, "1" if device still online. last will is "0". Online=1 status re-sendended every 10 minutes.
   * Activity = "1" | "0" : Human activity detected. Triggered if button is pressed or motion detected
   Configuration commands (if not configured with #define):
   * SetName( "new hostname" )
   * SetRoot( "MQTT root path" )
   * EnableOTA - allow On The Air updates. Protected with WiFi password.
   * Reset device

3. Relays MQTT topics:
   * <RelayName>/State = "0" | "1" // current relay state
   Команды
   * <RelayName>/SetName( "NewName" ) // Rename relay, if not configured with call to relayRegister()
   * <RelayName>/State( "0"|"1" ) // Set new relay state
   * <RelayName>/Switch    // Trigger relay state


4. Button MQTT topics:
   * <ButtonName>/State = "0" | "1" // current button state

