/*
  To set default credentials:
  - Duplicate credentials.example.h but rename as credentials.h
  - Fill in appropriate credentials
  - Keep in mind that once a variable has been set by serial command changes to that variable in your credentials.h file will not change the value used on your microcontroller

  Example command string (sent over serial or mqtt command topic):
  {"resetAfterCommandExecution": true, "command": "storeBulk", "storeBulk": [ {"variable": "WIFI_SSID", "value": ""}, {"variable": "WIFI_PASSWORD", "value": ""}, {"variable": "BASE_STATION_CLIENT_ID", "value": ""}, {"variable": "PUZZLE_CLIENT_ID", "value": ""}, {"variable": "COMMAND_TOPIC", "value": ""}, {"variable": "SHOW_DEBUG", "value": ""} ]}

  Variable Options:
  WIFI_SSID
  WIFI_PASSWORD
  MQTT_SERVER
  MQTT_USER
  MQTT_PASSWORD
  BASE_STATION_CLIENT_ID
  PUZZLE_CLIENT_ID
  BASE_STATION_TOPIC
  COMMAND_TOPIC
  SHOW_DEBUG
*/

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include "m_credentials.h"

#ifdef ESP32
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESP_WiFiManager.h>  //https://github.com/khoih-prog/ESP_WiFiManager
#include "SPIFFS.h"
#include "time.h"
const int ONBOARD_LED_ON = HIGH;
const int LED_ON = HIGH;
#else
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <FS.h>
#include <time.h>
const int ONBOARD_LED_ON = LOW;
const int LED_ON = HIGH;
#endif

const int LED_OFF = !LED_ON;
const int ONBOARD_LED_OFF = !ONBOARD_LED_ON;


unsigned long currentMillis = 0;
unsigned long previousPrintMillis;
unsigned long previousKeepaliveMillis;
const long printInterval = 1500;
const long keepaliveInterval = 1500;

bool printThisLoop = true;

String wifiSsid = WIFI_SSID;
String wifiPassword = WIFI_PASSWORD;
String _mqttServer = MQTT_SERVER;
String _mqttUser = MQTT_USER;
String _mqttPassword = MQTT_PASSWORD;
String baseStationClientId = BASE_STATION_CLIENT_ID;
String puzzleClientId = PUZZLE_CLIENT_ID;
String _baseStationTopic = BASE_STATION_TOPIC;
String _commandTopic = COMMAND_TOPIC;
String _statusTopic = puzzleClientId + "-status";
String showDebug = SHOW_DEBUG;

byte onboardLedPin = 2;
const char* ntpServer = "pool.ntp.org";
String scannerType = "event_module";

const char* mqttServer() {
  return _mqttServer.c_str();
}
const char* mqttUser() {
  return _mqttUser.c_str();
}
const char* mqttPassword() {
  return _mqttPassword.c_str();
}
const char* baseStationTopic() {
  return _baseStationTopic.c_str();
}
const char* commandTopic() {
  return _commandTopic.c_str();
}
const char* statusTopic() {
  return _statusTopic.c_str();
}


void (*extraMQTTCallback)(String inTopic, DynamicJsonDocument messageDoc);
void (*extraCommandCallback)(String inTopic, DynamicJsonDocument commandDoc);

void tempMQTTCallback(char* inTopic, byte* inPayload, unsigned int length) {
}

WiFiClient espClient;
WiFiUDP udpClient;
PubSubClient mqttClient(mqttServer(), 1883, tempMQTTCallback, espClient);
NTPClient timeClient(udpClient, ntpServer);


void printPeriodically(String info) {
  if (printThisLoop) Serial.print(info);
}

void printPeriodicallyLn(String info) {
  printPeriodically(info);
  printPeriodically("\n");
}

void debug(String info) {
  if (showDebug == "true") Serial.print(info);
}

void debugln(String info) {
  debug(info);
  debug("\n");
}

void serial(String info) {
  Serial.print(info);
}

void serialln(String info) {
  Serial.print(info);
  Serial.print("\n");
}

void resetESP(String resetReason) {
  serialln(resetReason);
  serialln("Software Force Reset due to failure.");
#ifdef ESP32
  esp_restart();
#else
  ESP.restart();
#endif
}

String getDefaultValue(String baseFilename) {
  if (baseFilename == "WIFI_SSID") {
    return WIFI_SSID;
  }
  if (baseFilename == "WIFI_PASSWORD") {
    return WIFI_PASSWORD;
  }
  if (baseFilename == "MQTT_SERVER") {
    return MQTT_SERVER;
  }
  if (baseFilename == "MQTT_USER") {
    return MQTT_USER;
  }
  if (baseFilename == "MQTT_PASSWORD") {
    return MQTT_PASSWORD;
  }
  if (baseFilename == "BASE_STATION_CLIENT_ID") {
    return BASE_STATION_CLIENT_ID;
  }
  if (baseFilename == "PUZZLE_CLIENT_ID") {
    return PUZZLE_CLIENT_ID;
  }
  if (baseFilename == "BASE_STATION_TOPIC") {
    return BASE_STATION_TOPIC;
  }
  if (baseFilename == "COMMAND_TOPIC") {
    return COMMAND_TOPIC;
  }
  if (baseFilename == "SHOW_DEBUG") {
    return SHOW_DEBUG;
  }
  resetESP("Unknown baseFilename of: " + baseFilename);
  return "unknown";
}

bool initiateSpiffs () {
#ifdef ESP32
  if (!SPIFFS.begin(true)) {
#else
  if (!SPIFFS.begin()) {
#endif
    serialln("An Error has occurred while mounting SPIFFS");
    return false;
  }
  return true;
}

void writeToMemory(String baseFilename, String value) {
  if (!initiateSpiffs()) {
    serialln("An Error has occurred while mounting SPIFFS");
    return;
  }

  File file = SPIFFS.open("/" + baseFilename + ".txt", "w");

  if (!file) {
    serialln("Failed to open file " + baseFilename + " for writing");
    return;
  }

  if (file.print(value)) {
    serialln("File " + baseFilename + " written successfully");
  } else {
    serialln("Write failed");
  }

  file.close();
}

String readFromMemory(String baseFilename) {
  if (!initiateSpiffs()) {
    resetESP("An Error has occurred while mounting SPIFFS");
    return getDefaultValue(baseFilename);
  }

  String filename = "/" + baseFilename + ".txt";

  if (!SPIFFS.exists(filename)) {
    serialln("Failed to open file " + baseFilename + " for reading. Use default from creds file: " + getDefaultValue(baseFilename));
    return getDefaultValue(baseFilename);
  }

  File file = SPIFFS.open(filename, "r");

  String value = file.readString();

  file.close();

  return value;
}

void handleStoreCommand(DynamicJsonDocument storeDoc) {
  String variable = storeDoc["variable"];
  String value = storeDoc["value"];
  serialln("Updating " + variable + " to be: " + value);
  writeToMemory(variable, value);
}

void baseCommandCallback(String inTopic, DynamicJsonDocument commandDoc) {
  String command = commandDoc["command"];
  if (command == "store") {
    handleStoreCommand(commandDoc);
  }
  if (command == "storeBulk") {
    JsonArray storeArray = commandDoc["storeBulk"].as<JsonArray>();
    for (JsonObject storeObj : storeArray) {
      handleStoreCommand(storeObj);
    }
  }

  extraCommandCallback(inTopic, commandDoc);

  bool resetAfterCommandExecution = commandDoc["resetAfterCommandExecution"];
  if (resetAfterCommandExecution) {
    resetESP("Commanded to reset after command execution");
  }
}

bool isValidCommandTopic(String inTopic) {
  if (inTopic == _commandTopic) {
    return true;
  }

  DynamicJsonDocument commandTopicsDoc(1024);
  DeserializationError commandTopicError = deserializeJson(commandTopicsDoc, _commandTopic);

  if (!commandTopicError) {
    if (commandTopicsDoc.is<JsonArray>()) {
      for (String commandTopic : commandTopicsDoc.as<JsonArray>()) {
        if (inTopic == commandTopic) {
          return true;
        }
      }
    }
  }
}

void baseMQTTCallback(char* inTopic, byte* inPayload, unsigned int length) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, inPayload);
  if (error) {
    debug("\nError deserializing Json with code: ");
    debugln(error.c_str());
    return;
  }

  if (isValidCommandTopic(inTopic)) {
    baseCommandCallback(inTopic, doc);
  }

  extraMQTTCallback(inTopic, doc);
}

void handleSerial() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    DynamicJsonDocument serialDoc(1024);
    DeserializationError error = deserializeJson(serialDoc, command);
    if (error) {
      debugln("\nError deserializing Json with code: ");
      debugln(error.c_str());
      debugln("Invalid command");
      return;
    }
    String topic = serialDoc["topic"];
    if (topic == "") {
      topic = "serial";
    }
    serialln("Received serial command: " + command);
    baseCommandCallback(topic, serialDoc);
  }
}

void wifiCheckLoop() {
  int wifiCheck = 0;

  while (WiFi.status() != WL_CONNECTED) {
    handleSerial();  // Check for serial command to update credentials which may include new WiFi creds
    delay(500);
    serial(".");
    if (wifiCheck >= 240) {
      resetESP("\nFailed to connect to WiFi in a reasonable time. Will try again");
    } else if (wifiCheck % 10 == 0) {
      WiFi.begin((char*)wifiSsid.c_str(), (char*)wifiPassword.c_str());
    }

    wifiCheck++;
  }

  serialln("");
}

void connectWiFi() {
  serialln("SSID: " + wifiSsid);

  WiFi.begin((char*)wifiSsid.c_str(), (char*)wifiPassword.c_str());

  wifiCheckLoop();

  serialln("WiFi connected\n\n");
}

void subscribeToTopic(String subscribeTopic) {
  if (mqttClient.subscribe(subscribeTopic.c_str())) {
    debugln("Subscribed to Topic: " + subscribeTopic);
  } else {
    resetESP("Failed to subscribe to Topic: " + subscribeTopic);
  }
}

void connectMQTT() {
  int retry = 0;

  StaticJsonDocument<256> outDocLastWill;
  outDocLastWill["message"] = "disconnected";
  outDocLastWill["clientID"] = puzzleClientId;
  outDocLastWill["scanner_type"] = scannerType;

  String lastWill;
  serializeJson(outDocLastWill, lastWill);

  debugln("LastWill message is: " + lastWill);
  delay(10);

  mqttClient.setServer(mqttServer(), 1883);
  mqttClient.setCallback(baseMQTTCallback);
  mqttClient.setClient(espClient);

  if (mqttClient.connect(((char*)puzzleClientId.c_str()), mqttUser(), mqttPassword(), ((char*)puzzleClientId.c_str()), 0, 1, ((char*)lastWill.c_str()))) {
    debugln("Connected to MQTT broker");

    debug("Status topic is: ");
    debugln(_statusTopic);

    DynamicJsonDocument commandTopicsDoc(1024);
    DeserializationError commandTopicError = deserializeJson(commandTopicsDoc, _commandTopic);

    if (!commandTopicError) {
      if (commandTopicsDoc.is<JsonArray>()) {
        for (String subscribeTopic : commandTopicsDoc.as<JsonArray>()) {
          subscribeToTopic(subscribeTopic);
        }
      } else {
        resetESP("Malformed command topic(s): " + _commandTopic);
      }
    } else {
      subscribeToTopic(_commandTopic);
    }

  } else {
    debugln("MQTT connect failed");
    debugln("Will reset and try again...");
    resetESP("Failed to connect to MQTT");
  }


  StaticJsonDocument<256> outDoc;
  outDoc["message"] = "connected";
  outDoc["clientID"] = puzzleClientId;
  outDoc["scanner_type"] = scannerType;

  String payload;
  serializeJson(outDoc, payload);

  mqttClient.publish((char*)puzzleClientId.c_str(), (char*)payload.c_str(), 1);

  debug("Sent to MQTT: ");
  debug((char*)payload.c_str());

  debugln("\n\n");
}

void publishMQTT(const char* outTopic, DynamicJsonDocument outDoc, bool retain = false) {
  String payload;
  serializeJson(outDoc, payload);

  int attempts = 0;
  while (attempts < 100) {
    if (mqttClient.publish(outTopic, (char*) payload.c_str(), retain)) {
      serialln("\nSent to MQTT on " + String(outTopic) + ": " + payload);
      return;
    } else {
      serialln("Failed to publish to MQTT. Trying again...\n");
      delay(10);
      attempts++;
    }
  }
}

void achieveCheckpoint() {
  DynamicJsonDocument outDoc(256);
  outDoc["message"] = "checkpoint";
  outDoc["clientID"] = baseStationClientId;
  outDoc["rfid"] = puzzleClientId;
  publishMQTT(baseStationTopic(), outDoc);
}

void keepaliveMQTT() {
  StaticJsonDocument<256> outDoc;
  outDoc["message"] = "keepalive";
  outDoc["clientID"] = puzzleClientId;
  outDoc["scanner_type"] = scannerType;

  String payload;
  serializeJson(outDoc, payload);

  if (!mqttClient.publish("keepalive", (char*)payload.c_str())) {
    resetESP("Keepalive message failed to deliver");
  }
}

void configureFromMemory() {
  wifiSsid = readFromMemory("WIFI_SSID");
  wifiPassword = readFromMemory("WIFI_PASSWORD");
  _mqttServer = readFromMemory("MQTT_SERVER");
  _mqttUser = readFromMemory("MQTT_USER");
  _mqttPassword = readFromMemory("MQTT_PASSWORD");
  baseStationClientId = readFromMemory("BASE_STATION_CLIENT_ID");
  puzzleClientId = readFromMemory("PUZZLE_CLIENT_ID");
  _baseStationTopic = readFromMemory("BASE_STATION_TOPIC");
  _commandTopic = readFromMemory("COMMAND_TOPIC");
  showDebug = readFromMemory("SHOW_DEBUG");

#ifdef SECRET_COMPARTMENT
  if (_commandTopic == "") {
    _commandTopic = puzzleClientId;
  }
#endif
}

unsigned long unixTimestamp() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}

void baseEscapeOSSetup(void (*puzzleMQTTCallback)(String inTopic, DynamicJsonDocument messageDoc), void (*puzzleCommandCallback)(String inTopic, DynamicJsonDocument commandDoc)) {
  Serial.begin(115200);
  delay(10);
  serialln("\nBeginning base setup");

  pinMode(onboardLedPin, OUTPUT);

  configureFromMemory();

  extraMQTTCallback = puzzleMQTTCallback;
  extraCommandCallback = puzzleCommandCallback;

  serialln("\Wifi start");
  connectWiFi();
  serialln("\Wifi done");
  connectMQTT();

  serialln("Base setup complete");
}

void baseEscapeOSLoop() {
  currentMillis = millis();

  if ((currentMillis - previousKeepaliveMillis) > keepaliveInterval) {
    keepaliveMQTT();
    previousKeepaliveMillis = currentMillis;
  }

  if ((currentMillis - previousPrintMillis) > printInterval) {
    printThisLoop = true;
    previousPrintMillis = currentMillis;
  } else {
    printThisLoop = false;
  }

  mqttClient.loop();

  handleSerial();
}
