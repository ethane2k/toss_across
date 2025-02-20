#define SECRET_COMPARTMENT

#include "escapeos.h"
#include "blinker.h"
/*
EXAMPLE COMMANDS:
Open with Standard behavior:
{"topic": "eOS-scGroup1","message":"event","status":"open"}

Open for 1 second:
{"topic": "eOS-scGroup1","message":"openTime","openSeconds":1}

Update WiFi Credentials:
{"topic": "eOS-scGroup1", "resetAfterCommandExecution": true, "command": "storeBulk", "storeBulk": [ {"variable": "WIFI_SSID", "value": "ReplaceWithYourSSID"}, {"variable": "WIFI_PASSWORD", "value": "ReplaceWithYourWiFiPassword"} ]}
*/

const byte solenoidPin = 14;  // D5
bool solenoidEngaged = false;
unsigned long solenoidEngagedAt = 0;
const int solenoidEngageTime = 10 * 1000;

const int SOLENOID_ENGAGED = HIGH;
const int SOLENOID_DISENGAGED = !SOLENOID_ENGAGED;


const byte doorPin = 5;  // D1
bool doorIsClosed = false;
bool doorWasClosed = false;
unsigned long doorLastChangeedAt = 0;
const int doorDebounceTime = 1000;

const byte ledPin = 4; // D2

const int DOOR_CLOSED = LOW;
const int DOOR_OPENED = !DOOR_CLOSED;

Blinker onboardLedBlinker(onboardLedPin, 5, 200, 1000, ONBOARD_LED_ON);
Blinker mainLedBlinker(ledPin, 5, 200, 1000, LED_ON);


void puzzleMQTTCallback(String inTopic, DynamicJsonDocument messageDoc) {
}

void puzzleCommandCallback(String inTopic, DynamicJsonDocument commandDoc) {
  String message = commandDoc["message"];
  String status = commandDoc["status"];
  if (message == "event" && status == "open") {
    int blinks = 10;

    char lastChar = inTopic.charAt(inTopic.length() - 1);
    if (isDigit(lastChar)) {
      blinks = (String(lastChar)).toInt();
      serialln(String(blinks));
    }
    serialln(String(blinks));

    engageSolenoid();

    onboardLedBlinker.setPattern(blinks, 200, 1000);
    onboardLedBlinker.start();

    mainLedBlinker.setPattern(blinks, 200, 1000);
    mainLedBlinker.start();
  } else if (message == "openTime") {
    int openSeconds = commandDoc["openSeconds"];
    engageSolenoidForTime(openSeconds);
  }
}

void setup() {
  baseEscapeOSSetup(puzzleMQTTCallback, puzzleCommandCallback);
  // put your puzzle setup code here, to run once:
  pinMode(solenoidPin, OUTPUT);
  pinMode(doorPin, INPUT_PULLUP);

  handleSolenoidState();
  handleDoorState();
  publishPuzzleStatus();

  onboardLedBlinker.solidOn();
  mainLedBlinker.solidOn();

  serialln("Setup complete");
}

void loop() {
  baseEscapeOSLoop();
  // Put your main puzzle code here, to run repeatedly:

  handleSolenoidState();
  handleDoorState();

  onboardLedBlinker.update();
  mainLedBlinker.update();

  delay(10);
}

void engageSolenoid() {
  solenoidEngaged = true;
  solenoidEngagedAt = currentMillis;
  publishPuzzleStatus();
}

void engageSolenoidForTime(int delaySeconds) {
  solenoidEngaged = true;
  digitalWrite(solenoidPin, SOLENOID_ENGAGED);

  publishPuzzleStatus();
  delay(delaySeconds * 1000);

  digitalWrite(solenoidPin, SOLENOID_DISENGAGED);
}

void handleSolenoidState() {
  if (solenoidEngaged && (currentMillis - solenoidEngagedAt >= solenoidEngageTime)) {
    solenoidEngaged = false;
    publishPuzzleStatus();
    onboardLedBlinker.pause();
    mainLedBlinker.pause();
    onboardLedBlinker.solidOn();
    mainLedBlinker.solidOn();
  }
  digitalWrite(solenoidPin, solenoidEngaged ? SOLENOID_ENGAGED : SOLENOID_DISENGAGED);
}

void handleDoorState() {
  doorIsClosed = digitalRead(doorPin) == DOOR_CLOSED;
  if (currentMillis - doorLastChangeedAt > doorDebounceTime) {
    if (doorIsClosed != doorWasClosed) {
      publishPuzzleStatus();
      doorLastChangeedAt = currentMillis;
      doorWasClosed = doorIsClosed;
    }
  }
}

void publishPuzzleStatus() {
  DynamicJsonDocument statusDoc(512);
  // TODO: (Assignment) Replace this static status document with one that captures the full current status of your puzzle
  statusDoc["solenoidEngaged"] = solenoidEngaged;
  statusDoc["doorIsClosed"] = doorIsClosed;
  statusDoc["timestamp"] = unixTimestamp();  // Always include the unix timestamp in the status message

  publishMQTT(statusTopic(), statusDoc, true);  // In most cases, retain (param 3) should be false. Puzzle status is the only case where it should be true
}
