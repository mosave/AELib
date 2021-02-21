#ifndef barometer_h
#define barometer_h

#ifndef I2C_SDA
  #define I2C_SDA 0
#endif

#ifndef I2C_SCL
  #define I2C_SCL 0
#endif


bool barometerValid();
float barometerPressure();
float barometerTemperature();
void barometerInit();

#endif
