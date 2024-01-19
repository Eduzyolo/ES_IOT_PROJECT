#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Servo.h>
#include <EEPROM.h>

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
#define OPEN_PIN(p1, p2) { digitalWrite(p1, HIGH); digitalWrite(p2, LOW); }
#define CLOSE_PIN(p1, p2) { digitalWrite(p1, LOW); digitalWrite(p2, HIGH); }
#define READ_PHOTO_PERCENT(pin) (100.0 * analogRead(pin) / 1023.0)
#define SET_LED_BRIGHTNESS(ledPin, percent) analogWrite(ledPin, (1023 * percent) / 100)

// #define HANDLE_MESSAGES 10
#define TELEGRAM_DEBUG

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
Servo myServo;
const int servoPin = D2;
const int ledPin = D1;
unsigned long lastUpdateId = 0;

// #define SLEEP_ON

void adjustBrightness(int photoPin, int ledPin) {
    float lightPercent = READ_PHOTO_PERCENT(photoPin);
    static int currentBrightness = 0;

    if (lightPercent < 50.0) {
        // Gradually increase brightness
        if (currentBrightness < 100) {
            currentBrightness++;
            SET_LED_BRIGHTNESS(ledPin, currentBrightness);
            delay(100); // Delay to slow down brightness increase
        }
    } else {
        // Reset brightness
        currentBrightness = 0;
        SET_LED_BRIGHTNESS(ledPin, currentBrightness);
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


void controlWatering(bool enable) {
  if (enable) {
    // Logic to control servo for watering
    myServo.write(90); // Example position, adjust as needed
  } else {
    myServo.write(0); // Reset position
  }
}

void controlLEDs(bool enable) {
  digitalWrite(ledPin, enable ? HIGH : LOW);
}

void handleNewMessage(telegramMessage &message) {
  Serial.print(message.date);
  Serial.print(" ");
  Serial.print(message.message_id);
  Serial.print(" ");
  Serial.print(message.update_id);
  Serial.print(" ");
  Serial.println(message.text);
   String chat_id = message.chat_id;
    String text = message.text;

  if (text == "/status") {
    // You need to implement the logic to get the status from your sensors
    String statusMessage = "Plant Status: [Sensor Readings]";
    bot.sendMessage(chat_id, statusMessage, "", 0);
  } else if (text == "/water_now") {
    controlWatering(true);
    bot.sendMessage(chat_id, "Watering now", "", 0);
  } else if (text == "/stop_watering") {
    controlWatering(false);
    bot.sendMessage(chat_id, "Watering stopped", "", 0);
  } else if (text == "/set_schedule") {
    // Implement scheduling logic here
    bot.sendMessage(chat_id, "Schedule set. [Details]", "", 0);
  } else if (text == "/cancel_schedule") {
    // Implement cancel scheduling logic here
    bot.sendMessage(chat_id, "Watering schedule canceled", "", 0);
  } else if (text == "/adjust_water_amount") {
    // Implement logic to adjust water amount
    bot.sendMessage(chat_id, "Water amount adjusted", "", 0);
  } else if (text == "/light_status") {
    // Implement logic to report LED status
    bot.sendMessage(chat_id, "LED Status: [On/Off]", "", 0);
  } else if (text == "/toggle_lights") {
    static bool lightStatus = false;
    lightStatus = !lightStatus;
    controlLEDs(lightStatus);
    bot.sendMessage(chat_id, "Toggled lights", "", 0);
  } else if (text == "/set_light_schedule") {
    // Implement light scheduling logic here
    bot.sendMessage(chat_id, "Light schedule set", "", 0);
  } else if (text == "/get_config") {
    // Implement logic to get current configuration
    bot.sendMessage(chat_id, "Current Config: [Details]", "", 0);
  } else if (text == "/help" || text == "/commands") {
    String helpMessage = "Available Commands:\n";
    helpMessage += "/status - Get system status\n";
    helpMessage += "/water_now - Trigger immediate watering\n";
    helpMessage += "/stop_watering - Stop watering\n";
    helpMessage += "/set_schedule - Set watering schedule\n";
    helpMessage += "/cancel_schedule - Cancel watering schedule\n";
    helpMessage += "/adjust_water_amount - Adjust water amount\n";
    helpMessage += "/light_status - Get LED status\n";
    helpMessage += "/toggle_lights - Toggle LEDs on/off\n";
    helpMessage += "/set_light_schedule - Set light schedule\n";
    helpMessage += "/get_config - Get current configuration\n";
    helpMessage += "/help - Show this help message\n";
    bot.sendMessage(chat_id, helpMessage, "", 0);
    } else {
    // Response for unknown command
    bot.sendMessage(chat_id, "Unknown command. Use /help to see available commands.", "",0);
    }
  bot.last_message_received = message.update_id;
}




void setup() {
  Serial.begin(115200);
  Serial.println();

 // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
  // myServo.attach(servoPin);
  // pinMode(ledPin, OUTPUT);
  
}

void loop() {
  int numNewMessages = bot.getUpdates(bot.last_message_received +1);

    for (int i = 0; i < numNewMessages; i++) {
        // Process each message
        handleNewMessage(bot.messages[i]);
    }
}
