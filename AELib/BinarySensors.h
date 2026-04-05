#ifndef binary_sensors_h
#define binary_sensors_h

/// <summary>Register a debounced binary sensor; state is published under Sensors/BinarySensor with one-based index</summary>
/// <param name="pin">GPIO pin number</param>
/// <param name="inverted">When true, invert active level relative to digitalRead</param>
/// <param name="pullUp">Use INPUT_PULLUP when true</param>
/// <param name="reportEverySeconds">If greater than zero, republish while active every N seconds; zero means publish only on change</param>
void bnsRegister(byte pin, bool inverted, bool pullUp, unsigned int reportEverySeconds);

/// <summary>Return current debounced state for the sensor registered on the given pin</summary>
/// <param name="btnPin">GPIO pin number</param>
/// <returns>True if the sensor is in the active state, false otherwise or if the pin is not registered</returns>
bool bnsState(byte btnPin);

/// <summary>Initialize the binary sensors module (registers the polling loop)</summary>
void bnsInit();
#endif