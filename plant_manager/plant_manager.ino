#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h>

#define DEBUG
#define PHOTO_PIN A0     // Pin for the photoresistor (analog input)
#define VALVE_O_PIN D1   // Pin for the open valve (digital output)
#define VALVE_C_PIN D2   // Pin for the close valve (digital output)
#define LED_PIN D4       // Pin for the LED (digital output)
#define BATT_PIN D5      // Pin for the Battery (analog input)

#define FULL_LIGHT 100.0
#define ZERO_LIGHT 0.0
#define R1 1.000                  //First Resistence Voltage divider --> to adjust
#define R2 1.000                  //Second Resistence Voltage divider --> to adjust
#define VBAT_MAX 3100       // milliVolts - 1.5V * 2 AA - fully charged batteries
#define VBAT_MIN 2800       // milliVolts - due to battery regulator, lower than this we consider the batteries discharged
const int FIVE_SECONDS = 5 / 60; // Less than a minute, rounded down to 0
const int ONE_MINUTE = 1;
const int FIVE_MINUTES = 5;
const int QUARTER_HOUR = 15;
const int HALF_HOUR = 30;
const int ONE_HOUR = 60;
const int TWO_HOUR = 120;
const int FOUR_HOUR = 240;
const int SIX_HOUR = 360;
const int TWELVE_HOUR = 720;
const int ONE_DAY = 1440; // 24 hours * 60 minutes
const byte signature[] = {0xAB, 0xCD, 0xEF}; // Example signature
const int signatureLength = sizeof(signature);


#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define BOT_TOKEN ""


enum stat_enum {ON, OFF};
enum batt_status_enum {LEVEL_OK, LEVEL_LOW};

// Define the struct to hold configuration data
struct Configuration {
  struct Light {
    int startHour;
    int duration;
    int repeatInterval;
    int brightness;
  } light;

  struct Water {
    int startHour;
    int duration;
    int repeatInterval;
  } water;
};

struct Status {
  stat_enum LED_status = OFF;
  int Light_brightness = 0;
  batt_status_enum battery_status = LEVEL_OK;
  stat_enum valve_status = OFF; 
  int time_to_sleep = 0;
};


// #define HANDLE_MESSAGES 10
#define TELEGRAM_DEBUG

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long lastUpdateId = 0;
bool sleep = false;




Configuration config;
Status status;
// #define SLEEP_ON

// PID Constants
#define KP 0.5
#define KI 0.2
#define KD 0.1

// Function to compute PID output scaled to a value between 0 and 100
int computePID(int lightPercentage, int targetPercentage) {
    int integral = 0;
    int lastError = 0;
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

    // Scale the output to a value between 0 and 100
    output = constrain(output, 0, 100); // Ensure output is within range
    output = map(output, -100, 100, 0, 100); // Map output from -100 to 100 to 0 to 100 range

    // Update last error
    lastError = error;

    return output;
}

void adjustBrightness(float target) {
  status.Light_brightness = 100.0 * analogRead(PHOTO_PIN) / 1023.0;
  // target = target > 90 ? 85 : target;  
  // target = target < 50 ? target : 55;
  if (target < 50 && status.Light_brightness > 50 ){
    status.LED_status = OFF;
    digitalWrite(LED_PIN, LOW);
  }else{
    status.LED_status = ON;
    digitalWrite(LED_PIN,HIGH);
//     // adjusting to a reachable percentage
//     target = target > 90 ? 85 : target; 
//   // brightness < 50 --> dark, brightness > 80 --> full light 
//     while (Light_brightness < target){
//       // bad thing can append here if the LED is faulted
//       // good is to add a timer
//       LED_brightness = computePID(Light_brightness, target);
//       analogWrite(LED_PIN, (1023 * LED_brightness) / 100);
// #ifdef DEBUG
//     Serial.print(target);
//     Serial.print(" ");
//     Serial.println(LED_brightness);
// #endif
//       delay(10); // Adjust delay according to your requirements
//       Light_brightness = 100.0 * analogRead(PHOTO_PIN) / 1023.0;
//     }
//     analogWrite(LED_PIN, (1023 * LED_brightness) / 100);
  }
}

// Method to save configuration to EEPROM
void saveConfigToEEPROM() {
  EEPROM.begin(512);

  // Serialize the configuration to JSON
  DynamicJsonDocument doc(1024);
  JsonObject root = doc.to<JsonObject>();
  JsonObject lightObj = root.createNestedObject("light");
  lightObj["startHour"] = config.light.startHour;
  lightObj["duration"] = config.light.duration;
  lightObj["repeatInterval"] = config.light.repeatInterval;
  lightObj["brightness"] = config.light.brightness;
  JsonObject waterObj = root.createNestedObject("water");
  waterObj["startHour"] = config.water.startHour;
  waterObj["duration"] = config.water.duration;
  waterObj["repeatInterval"] = config.water.repeatInterval;

  // Serialize the JSON to a string
  String jsonConfig;
  serializeJson(root, jsonConfig);
  // First, write the signature
  for (int i = 0; i < signatureLength; ++i) {
    EEPROM.write(i, signature[i]);
  }
  // Write the JSON string to EEPROM
  for (int i = 0; i < jsonConfig.length(); ++i) {
    EEPROM.write(signatureLength + i, jsonConfig[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}

// Method to load configuration from EEPROM
void loadConfigFromEEPROM() {
  EEPROM.begin(512);

  // Read the JSON string from EEPROM
  String jsonConfig;
  for (int i = signatureLength; EEPROM.read(i) != 0 && i < 512; ++i) {
    jsonConfig += (char)EEPROM.read(i);
  }
  EEPROM.end();

  // Deserialize the JSON string
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, jsonConfig);
  JsonObject root = doc.as<JsonObject>();
  JsonObject lightObj = root["light"];
  JsonObject waterObj = root["water"];

  // Populate the configuration struct
  config.light.startHour = lightObj["startHour"];
  config.light.duration = lightObj["duration"];
  config.light.repeatInterval = lightObj["repeatInterval"];
  config.light.brightness = lightObj["brightness"];
  config.water.startHour = waterObj["startHour"];
  config.water.duration = waterObj["duration"];
  config.water.repeatInterval = waterObj["repeatInterval"];
}

bool isValidConfigInEEPROM() {
  EEPROM.begin(512);

  for (int i = 0; i < signatureLength; ++i) {
    if (EEPROM.read(i) != signature[i]) {
      EEPROM.end();
      return false; // Signature doesn't match
    }
  }

  EEPROM.end();
  return true; // Signature matches, valid configuration is likely present
}

// Function to convert an integer representing time (in minutes since midnight) to HH:MM format
String convertIntToTimeString(int minutes) {
  int hours = minutes / 60;
  int mins = minutes % 60;
  return String(hours) + ":" + (mins < 10 ? "0" : "") + String(mins);
}

String convertMillisToDurationString(unsigned long durationMinutes) {
  String durationStr = "";
  if (durationMinutes >= 1440) { // More than or equal to a day
    unsigned long days = durationMinutes / 1440;
    durationStr += String(days) + " days";
  } else if (durationMinutes >= 60) { // More than or equal to an hour
    unsigned long hours = durationMinutes / 60;
    durationStr += String(hours) + " hours";
  } else {
    durationStr += String(durationMinutes) + " minutes";
  }
  return durationStr;
}



// Function to convert time string (HH:MM) to integer
int convertTimeStringToInt(String timeStr) {
  int hours = timeStr.substring(0, 2).toInt();
  int minutes = timeStr.substring(3).toInt();
  return hours * 60 + minutes; // Convert to minutes
}

unsigned long convertDurationStringToMillis(String durationStr) {
  unsigned long duration = 0;
  int value;
  if (durationStr.indexOf(" day") != -1) {
    value = durationStr.substring(0, durationStr.indexOf(" day")).toInt();
    duration += value * 1440; // Days to minutes
  }
  if (durationStr.indexOf(" hour") != -1) {
    value = durationStr.substring(0, durationStr.indexOf(" hour")).toInt();
    duration += value * 60; // Hours to minutes
  }
  if (durationStr.indexOf(" minute") != -1) {
    value = durationStr.substring(0, durationStr.indexOf(" minute")).toInt();
    duration += value; // Minutes remain as minutes
  }
  return duration;
}


String getConfigString() {
  String configStr;
  
  // Light configuration
  configStr += "Light Configuration:\n";
  configStr += "  Start Hour: " + convertIntToTimeString(config.light.startHour) + "\n";
  configStr += "  Duration: " + convertMillisToDurationString(config.light.duration) + "\n";
  configStr += "  Repeat Interval: " + convertMillisToDurationString(config.light.repeatInterval) + "\n";
  configStr += "  Brightness: " + String(config.light.brightness) + "\n";
  
  // Water configuration
  configStr += "Water Configuration:\n";
  configStr += "  Start Hour: " + convertIntToTimeString(config.water.startHour) + "\n";
  configStr += "  Duration: " + convertMillisToDurationString(config.water.duration) + "\n";
  configStr += "  Repeat Interval: " + convertMillisToDurationString(config.water.repeatInterval) + "\n";

  return configStr;
}

void setConfigFromString(String configString) {
  // Light configuration
  int index = configString.indexOf("Light Configuration:");
  if (index != -1) {
    // Parse start hour
    index = configString.indexOf("Start Hour: ") + 12;
    int endIndex = configString.indexOf('\n', index);
    String startHourStr = configString.substring(index, endIndex);
    config.light.startHour = convertTimeStringToInt(startHourStr);
    // Parse duration
    index = configString.indexOf("Duration: ") + 10;
    endIndex = configString.indexOf('\n', index);
    String durationStr = configString.substring(index, endIndex);
    durationStr.trim(); // Correct use of trim() - modifies the string in place
    config.light.duration = convertDurationStringToMillis(durationStr);

    // Parse repeat interval
    index = configString.indexOf("Repeat Interval: ") + 17;
    endIndex = configString.indexOf('\n', index);
    String repeatIntervalStr = configString.substring(index, endIndex);
    repeatIntervalStr.trim(); // Correct use of trim() - modifies the string in place
    config.light.repeatInterval = convertDurationStringToMillis(repeatIntervalStr);

    // Parse brightness
    index = configString.indexOf("Brightness: ") + 12;
    endIndex = configString.indexOf('\n', index);
    String brightnessStr = configString.substring(index, endIndex);
    brightnessStr.trim(); // Apply trim() here as well, if needed
    config.light.brightness = brightnessStr.toInt();
  }

  // Water configuration
  index = configString.indexOf("Water Configuration:");
  if (index != -1) {
      // Parse start hour
      index = configString.lastIndexOf("Start Hour: ") + 12;
      int endIndex = configString.indexOf('\n', index);
      String startHourStr = configString.substring(index, endIndex);
      startHourStr.trim(); // Trim the string in place
      config.water.startHour = convertTimeStringToInt(startHourStr); // Convert start hour to integer (minutes since midnight)

      // Parse duration
      index = configString.lastIndexOf("Duration: ") + 10;
      endIndex = configString.indexOf('\n', index);
      String durationStr = configString.substring(index, endIndex);
      durationStr.trim(); // Trim the string in place
      config.water.duration = convertDurationStringToMillis(durationStr); // Convert duration string to minutes

      // Parse repeat interval
      index = configString.lastIndexOf("Repeat Interval: ") + 17;
      endIndex = configString.indexOf('\n', index);
      String repeatIntervalStr = configString.substring(index, endIndex);
      repeatIntervalStr.trim(); // Trim the string in place
      config.water.repeatInterval = convertDurationStringToMillis(repeatIntervalStr); // Convert repeat interval string to minutes
  }

}

void initDefaultConfig() {
  // Light configuration
  config.light.startHour = 0; // Start hour remains unchanged
  config.light.duration = FIVE_MINUTES; // Default duration (5 minutes)
  config.light.repeatInterval = HALF_HOUR; // Default repeat interval (30 minutes)
  config.light.brightness = 50; // Brightness remains unchanged

  // Water configuration
  config.water.startHour = 0;
  config.water.duration = FIVE_MINUTES; // Default duration (5 minutes)
  config.water.repeatInterval = HALF_HOUR; // Default repeat interval (30 minutes)
}

float readVoltage() { 
  int reading = analogRead(BATT_PIN);               //Analog Read of the battery voltage
  float volt = map(reading, 0, 1023, 0, 330); //Mapping from AnalogRead to Volt
  volt = (((R1 + R2) * volt ) / R2) / 100.0;  //Compute the battery voltage
  return volt;
}

void setBatteryPercentage(){
  float voltage = readVoltage();
  uint8_t percentage_battery = 100.0 / (VBAT_MAX - VBAT_MIN) * 1.0 * (voltage*1000.0 - VBAT_MIN);
  if (percentage_battery > 50) status.battery_status = LEVEL_OK;
  if (percentage_battery < 0) status.battery_status = LEVEL_LOW;
}

void controlWatering(bool enable) {
  if (enable) {

    digitalWrite(VALVE_C_PIN, HIGH); 
    digitalWrite(VALVE_O_PIN, LOW);
  } else {
    digitalWrite(VALVE_O_PIN, HIGH); 
    digitalWrite(VALVE_C_PIN, LOW);
  }
}

void powerOffWiFi() {
  WiFi.disconnect(); // Disconnect from WiFi network
  WiFi.mode(WIFI_OFF); // Turn off WiFi
}

// void powerOnWiFi() {
// #ifdef DEBUG
//   Serial.begin(115200);
//   Serial.println();

//  // attempt to connect to Wifi network:
//   Serial.print("Connecting to Wifi SSID ");
//   Serial.print(WIFI_SSID);
// #endif
//   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
//   secured_client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
//   while (WiFi.status() != WL_CONNECTED)
//   {
// #ifdef DEBUG
//     Serial.print(".");
// #endif
//     delay(500);
//   }
// #ifdef DEBUG
//   Serial.print("\nWiFi connected. IP address: ");
//   Serial.println(WiFi.localIP());

//   Serial.print("Retrieving time: ");
// #endif
//   configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
//   time_t now = time(nullptr);
//   while (now < 24 * 3600)
//   {
// #ifdef DEBUG
//     Serial.print(".");
// #endif
//     delay(100);
//     now = time(nullptr);
//   }
// #ifdef DEBUG
//     Serial.print(now);
// #endif
// }


// // Function to send inline keyboard for light configuration
// void sendLightConfigurationKeyboard(String chat_id) {
//   String keyboard = "[[{\"text\":\"Set Start Hour\",\"callback_data\":\"set_start_hour_light\"},{\"text\":\"Set Duration\",\"callback_data\":\"set_duration_light\"}],"
//                     "[{\"text\":\"Set Repeat Interval\",\"callback_data\":\"set_repeat_interval_light\"},{\"text\":\"Set Brightness\",\"callback_data\":\"set_brightness_light\"}]]";
//   bot.sendMessageWithInlineKeyboard(chat_id, "Configure Light Settings:", keyboard, "");
// }

// // Function to send inline keyboard for water configuration
// void sendWaterConfigurationKeyboard(String chat_id) {
//   String keyboard = "[[{\"text\":\"Set Start Hour\",\"callback_data\":\"set_start_hour_water\"},{\"text\":\"Set Duration\",\"callback_data\":\"set_duration_water\"}],"
//                     "[{\"text\":\"Set Repeat Interval\",\"callback_data\":\"set_repeat_interval_water\"}]]";
//   bot.sendMessageWithInlineKeyboard(chat_id, "Configure Water Settings:", keyboard, "");
// }

// // Function to send inline keyboard for water configuration
// void sendConfigurationKeyboard(String chat_id) {
//   String keyboard = "[[{\"text\":\"Set Water\",\"callback_data\":\"set_water\"},{\"text\":\"Set Light\",\"callback_data\":\"set_light\"}]]";
//   bot.sendMessageWithInlineKeyboard(chat_id, "Configure Settings:", keyboard, "");
// }

// // Function to handle inline keyboard button presses
// void handleCallbackQueries(telegramMessage &message) {
//     String chat_id = message.chat_id;
//     String callback_data = message.text;
//     String msg = "";
//   if (callback_data == "set_start_hour_light") {
//     // Handle setting start hour for light
//   } else if (callback_data == "set_duration_light") {
//     // Handle setting duration for light
//   } else if (callback_data == "set_repeat_interval_light") {
//     // Handle setting repeat interval for light
//   } else if (callback_data == "set_brightness_light") {
//     // Handle setting brightness for light
//   } else if (callback_data == "set_start_hour_water") {
//     // Handle setting start hour for water
//   } else if (callback_data == "set_duration_water") {
//     // Handle setting duration for water
//   } else if (callback_data == "set_repeat_interval_water") {
//     // Handle setting repeat interval for water
//   }
//   bot.sendMessage(chat_id, msg, "", 0);
//   bot.last_message_received = message.update_id;
// }

#ifdef DEBUG
void print_on_serial_config(){
  Serial.println("Initialized Configuration:");
  Serial.print("Light Start Hour: ");
  Serial.println(config.light.startHour);
  Serial.print("Light Duration: ");
  Serial.println(config.light.duration);
  Serial.print("Light Repeat Interval: ");
  Serial.println(config.light.repeatInterval);
  Serial.print("Light Brightness: ");
  Serial.println(config.light.brightness);

  Serial.print("Water Start Hour: ");
  Serial.println(config.water.startHour);
  Serial.print("Water Duration: ");
  Serial.println(config.water.duration);
  Serial.print("Water Repeat Interval: ");
  Serial.println(config.water.repeatInterval);

}
#endif

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
    status.Light_brightness = 100.0 - (100.0 * analogRead(PHOTO_PIN) / 1023.0);
    setBatteryPercentage();
    msg = "Plant Status: \n";
    msg += "LED Status: ";
    msg +=  status.LED_status == ON ? "ON" : "OFF";
    msg += "\n";

    msg += "Light Brightness: ";
    msg += String(status.Light_brightness) + "%\n";

    msg += "Battery Status: ";
    msg +=  status.battery_status == ON ? "OK" : "Low battery";
    msg += "\n";

    msg += "Time to Sleep: ";
    msg += String(status.time_to_sleep) + "\n";

    msg += "Valve Status: ";
    msg += status.valve_status == ON ? "Opened" : "Closed";
    msg += "\n";
  } else if (text == "/water_now") {
    status.valve_status = ON;
    controlWatering(true);
    msg = "Watering now";
  } else if (text == "/stop_watering") {
    status.valve_status = OFF;
    controlWatering(false);
    msg = "Watering stopped";
  } else if (text == "/set_schedule") {
    // Implement scheduling logic here
    // sendConfigurationKeyboard(chat_id);
    msg = "Please send back a configuration message similar to the one below:\n\n";
    msg += "Light Configuration:\n";
    msg += "  Start Hour: 18:00\n";
    msg += "  Duration: 2 hours\n";
    msg += "  Repeat Interval: 1 days\n";
    msg += "  Brightness: 80\n";
    msg += "Water Configuration:\n";
    msg += "  Start Hour: 07:30\n";
    msg += "  Duration: 30 minutes\n";
    msg += "  Repeat Interval: 2 hours\n";
  } else if (text == "/cancel_schedule") {
    // Implement cancel scheduling logic here
    initDefaultConfig();
    msg = "Watering schedule canceled";
  } else if (text == "/lights_on") {
    status.LED_status = ON;
    adjustBrightness(FULL_LIGHT);
    msg = "Turning lights ON!";
  } else if (text == "/lights_off") {
    status.LED_status = OFF;
    adjustBrightness(ZERO_LIGHT);
    msg = "Turning lights OFF!";
  } else if (text == "/set_light_schedule") {
    // Implement light scheduling logic here
    msg = "Light schedule set";
  } else if (text == "/start_config") {
    // Implement logic to get current configuration
    msg = "Going to sleep\n";
    saveConfigToEEPROM();
    sleep = true;
#ifdef DEBUG
  Serial.println("Configuration saved to EEPROM!");
  Serial.println("Pijama time!!");
#endif   
  } else if (text == "/get_config") {
    // Implement logic to get current configuration
    msg = getConfigString();
#ifdef DEBUG
  print_on_serial_config();
#endif   
  } else if (text.startsWith("Light Configuration:")){
    setConfigFromString(text);
    msg = "New configuration setted!\n\n";
#ifdef DEBUG
  print_on_serial_config();
#endif    
  }else if (text == "/help" || text == "/commands") {
    msg = "Available Commands:\n";
    msg += "/water_now - Trigger immediate watering\n";
    msg += "/stop_watering - Stops water now\n";
    msg += "/lights_on - Turn on lights\n";
    msg += "/lights_off - Turn off lights\n";
    msg += "/status - Get the current status of the system\n";
    msg += "/set_schedule - Set or adjust the watering schedule\n";
    msg += "/cancel_schedule - Cancel the existing watering schedule\n";
    msg += "/get_config - Retrieve the current configuration settings of the system\n";
    msg += "/start_config - Starts the actual configuration\n";
    msg += "/help - List available commands and instructions on how to use them\n";

    // } else if (bot.messages[bot.last_message_received].type == "callback_query"){
    
    } else{
      // Response for unknown command
      msg = "Unknown command. Use /help to see available commands.";
    }
#ifdef DEBUG
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
  if(isValidConfigInEEPROM()){
  loadConfigFromEEPROM();
#ifdef DEBUG
  Serial.println("Configuration loaded from EEPROM!");
#endif  
  }else{
    initDefaultConfig();
#ifdef DEBUG
  Serial.println("Default configration loaded!");
#endif   
  }


  pinMode(VALVE_O_PIN, OUTPUT);
  pinMode(VALVE_C_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  controlWatering(false);
}

void handleNewMessages(int n_messages){
  for (int i = 0; i < n_messages; i++)
  {
    // if (bot.messages[i].type == "callback_query")
    // {
    //   handleCallbackQueries(bot.messages[i]);
    // }
    // else
    // {
      handleNewMessage(bot.messages[i]);
    // }
  }
}

void loop() {
  if (sleep){
    powerOffWiFi();
#ifdef DEBUG
    Serial.println("WIFI OFF!");
    Serial.end();
#endif
    ESP.deepSleep(5 * 1000000UL); 
    delay(500);
    ESP.restart();

//     powerOnWiFi();
// #ifdef DEBUG
//     Serial.println("WIFI ON!");
// #endif
//     // loadConfigFromEEPROM();
// #ifdef DEBUG
//   Serial.println("Configuration loaded from EEPROM!");
// #endif  
//     sleep = false;
// #ifdef DEBUG
//     Serial.print("Time to wake up!!");
// #endif
  }
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages)
  {
#ifdef DEBUG
    Serial.println("got response");
#endif
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}
