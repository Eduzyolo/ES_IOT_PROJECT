#include <ESP8266WiFi.h>

#define R1 1.000                  //First Resistence Voltage divider --> to adjust
#define R2 1.000                  //Second Resistence Voltage divider --> to adjust
#define VBAT_MAX 3100       // milliVolts - 1.5V * 2 AA - fully charged batteries
#define VBAT_MIN 2800       // milliVolts - due to battery regulator, lower than this we consider the batteries discharged
#define ONE_HOUR 3600000000ULL 
#define TWO_HOUR 2 * ONE_HOUR
#define FOUR_HOUR 2 * TWO_HOUR
#define SIX_HOUR 3 * TWO_HOUR
#define TWELWE_HOUR 2 * SIX_HOUR
#define ONE_DAY 2 * TWELWE_HOUR

#define SLEEP_ON

uint64_t remainingSleepTime = TWO_HOUR;

float readVoltage() {                         //Function to compute the battery voltage
  int reading = analogRead(A0);               //Analog Read of the battery voltage
  float volt = map(reading, 0, 1023, 0, 330); //Mapping from AnalogRead to Volt
  volt = (((R1 + R2) * volt ) / R2) / 100.0;  //Compute the battery voltage
  return volt;
}

uint8_t getBatteryPercentage(){
  float voltage = readVoltage();
  uint8_t percentage_battery = 100.0 / (VBAT_MAX - VBAT_MIN) * 1.0 * (voltage*1000.0 - VBAT_MIN);
  if (percentage_battery > 100) percentage_battery = 100;
  if (percentage_battery < 0) percentage_battery = 0;
  return percentage_battery;
}

void setup() {
  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
#ifdef SLEEP_ON
  if (remainingSleepTime > ONE_HOUR) {
    ESP.deepSleep(ONE_HOUR);
    remainingSleepTime -= ONE_HOUR;
  } else {
    ESP.deepSleep(remainingSleepTime);
    remainingSleepTime = 0;
  }
  if (remainingSleepTime == 0){
#endif
    getBatteryPercentage();
#ifdef SLEEP_ON
  }
#endif
}
