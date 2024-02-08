#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Servo.h>
#include <EEPROM.h>

#define DEBUG
#define PHOTO_PIN D0     // Pin for the photoresistor (analog input)
#define VALVE_O_PIN D1   // Pin for the open valve (digital output)
#define VALVE_C_PIN D2   // Pin for the close valve (digital output)
#define LED_PIN D3       // Pin for the LED (digital output)
#define BATT_PIN A0      // Pin for the Battery (analog input)

#define FULL_LIGHT 100.0
#define ZERO_LIGHT 0.0
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
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define BOT_TOKEN ""
#define OPEN_VALVE() { digitalWrite(VALVE_O_PIN, HIGH); digitalWrite(VALVE_C_PIN, LOW); }
#define CLOSE_VALVE() { digitalWrite(VALVE_O_PIN, LOW); digitalWrite(VALVE_C_PIN, HIGH); }
#define READ_BRIGHTNESS() (100.0 * analogRead(PHOTO_PIN) / 1023.0)
#define SET_LED_BRIGHTNESS(percent) analogWrite(LED_PIN, (1023 * percent) / 100)

// #define HANDLE_MESSAGES 10
#define TELEGRAM_DEBUG

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
Servo myServo;
const int servoPin = D2;
const int ledPin = D1;
unsigned long lastUpdateId = 0;


static int LED_brightness = 0;
static int Light_brightness = 0;
static int battery_percentage = 0;
static int time_to_sleep = 0;
static bool valve_status = false; // false--> closed, true --> opened
static int setted_LED_brightness = ZERO_LIGHT;
// #define SLEEP_ON

// PID Constants
#define KP 0.5
#define KI 0.2
#define KD 0.1

// Function to compute PID output
int computePID(int lightPercentage, int targetPercentage) {
    static int integral = 0;
    static int lastError = 0;
    int error = targetPercentage - lightPercentage;

    // Proportional term
    int P = KP * error;

    // Integral term
    integral += error;
    int I = KI * integral;

    // Derivative term
    int derivative = error - lastError;
    int D = KD * derivative;

    int output = P + I + D;

    // Update last error
    lastError = error;

    return output;
}

void adjustBrightness(float target) {
  float lightPercent = READ_BRIGHTNESS();
  while (lightPercent < target){
    // bad thing can append here if the LED is faulted
    // good is to add a timer
    Light_brightness = computePID(lightPercent, target);
    SET_LED_BRIGHTNESS(Light_brightness);
    delay(100); // Adjust delay according to your requirements
  }
}

void saveConfigToEEPROM(String jsonConfig) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, jsonConfig);

  EEPROM.begin(512);
  for (int i = 0; i < jsonConfig.length(); ++i) {
      EEPROM.write(i, jsonConfig[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}

void loadConfigFromEEPROM() {
  EEPROM.begin(512);
  String jsonConfig;
  for (int i = 0; EEPROM.read(i) != 0 && i < 512; ++i) {
      jsonConfig += (char)EEPROM.read(i);
  }
  EEPROM.end();

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, jsonConfig);
  // Use the doc as needed
}


float readVoltage() {                         //Function to compute the battery voltage
  int reading = analogRead(BATT_PIN);               //Analog Read of the battery voltage
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


void controlWatering(bool enable) {
  if (enable) {
    OPEN_VALVE();
  } else {
    CLOSE_VALVE();
  }
}

void handleNewMessage(telegramMessage &message) {
#ifdef DEBUG
  Serial.print(message.date);
  Serial.print(" ");
  Serial.print(message.message_id);
  Serial.print(" ");
  Serial.print(message.update_id);
  Serial.print(" ");
  Serial.println(message.text);
#endif
   String chat_id = message.chat_id;
    String text = message.text;
    String msg = "";

  if (text == "/status") {
    // You need to implement the logic to get the status from your sensors
    msg = "Plant Status: \n";
    msg += "LED Brightness: ";
    msg += String(LED_brightness) + "\n";

    msg += "Light Brightness: ";
    msg += String(Light_brightness) + "\n";

    msg += "Battery Percentage: ";
    msg += String(battery_percentage) + "\n";

    msg += "Time to Sleep: ";
    msg += String(time_to_sleep) + "\n";

    msg += "Valve Status: ";
    msg += valve_status ? "Opened" : "Closed";
    msg += "\n";
  } else if (text == "/water_now") {
    msg = "Watering now";
    controlWatering(true);
  } else if (text == "/stop_watering") {
    controlWatering(false);
    msg = "Watering stopped";
  } else if (text == "/set_schedule") {
    // Implement scheduling logic here
    msg = "Schedule set";
  } else if (text == "/cancel_schedule") {
    // Implement cancel scheduling logic here
    msg = "Watering schedule canceled";
  }  else if (text == "/light_status") {
    // Implement logic to report LED status
    msg = "LED Status:";
    msg += String(LED_brightness) + "\n";
  } else if (text == "/lights_on") {
    adjustBrightness(FULL_LIGHT);
    msg = "Turning lights ON!";
  } else if (text == "/lights_off") {
    adjustBrightness(ZERO_LIGHT);
    msg = "Turning lights OFF!";
  } else if (text == "/set_light_schedule") {
    // Implement light scheduling logic here
    msg = "Light schedule set";
  } else if (text == "/get_config") {
    // Implement logic to get current configuration
    msg = "Current Config: [Details]";
  } else if (text == "/help" || text == "/commands") {
    msg = "Available Commands:\n";
    msg += "/status - Get system status\n";
    msg += "/water_now - Trigger immediate watering\n";
    msg += "/stop_watering - Stop watering\n";
    msg += "/set_schedule - Set watering schedule\n";
    msg += "/cancel_schedule - Cancel watering schedule\n";
    msg += "/adjust_water_amount - Adjust water amount\n";
    msg += "/light_status - Get LED status\n";
    msg += "/toggle_lights - Toggle LEDs on/off\n";
    msg += "/set_light_schedule - Set light schedule\n";
    msg += "/get_config - Get current configuration\n";
    msg += "/help - Show this help message\n";
    } else {
    // Response for unknown command
    msg = "Unknown command. Use /help to see available commands.";
    }
#if DEBUG
  Serial.println(msg);
#endif
  bot.sendMessage(chat_id, msg, "", 0);
  bot.last_message_received = message.update_id;
}

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println();

 // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
#ifdef DEBUG
    Serial.print(".");
#endif
    delay(500);
  }
#ifdef DEBUG
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
#endif
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
#ifdef DEBUG
    Serial.print(".");
#endif
    delay(100);
    now = time(nullptr);
  }
#ifdef DEBUG
    Serial.print(now);
#endif
  
}

void loop() {
  int numNewMessages = bot.getUpdates(bot.last_message_received +1);

  for (int i = 0; i < numNewMessages; i++) {
      // Process each message
      handleNewMessage(bot.messages[i]);
  }
}
