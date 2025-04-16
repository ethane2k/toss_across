#include "escapeos.h"
#include "blinker.h"
#include "BluetoothSerial.h"

#define LED_STRIP 25
#define LITE_BRITE 26

bool solved = 0;

// Any additional library inclusions, variables, etc. can be put here. Ensure that variable names do not collide with variable names from escapeos.h and do not include libraries already included in escapeos.h
const byte mainLedPin = 23;
Blinker onboardLedBlinker(onboardLedPin, 10, 200, 1000, ONBOARD_LED_ON);
Blinker mainLedBlinker(mainLedPin, 10, 200, 1000, LED_ON);

void puzzleMQTTCallback (String inTopic, DynamicJsonDocument messageDoc) {

}

void puzzleCommandCallback(String inTopic, DynamicJsonDocument commandDoc) {
  // Insert any logic that your puzzle should handle coming on its command topic or serially (e.g., solve puzzle immediately if message === "solved")
  // EXAMPLE CODE
  String message = commandDoc["message"];
  serialln(message);
}

void setup() {
  baseEscapeOSSetup(puzzleMQTTCallback, puzzleCommandCallback); // DO NOT REMOVE

  // DEFINE INPUTS AND OUTPUTS
  pinMode(LED_STRIP, OUTPUT);
  pinMode(LITE_BRITE, OUTPUT);
  
  Serial.begin(115200);

  SerialBT.begin("ESP32_Receriver"); 
  Serial.println("ESP32_Receriver ready, waiting for connection");
  
  // EXAMPLE for initial verification after upload. This will cause the board to send the achieveCheckpoint message once after connecting to WiFi
  serialln("Sending achieve checkpoint message once at the end of setup");
  achieveCheckpoint();
  onboardLedBlinker.start();
  mainLedBlinker.start();
  serialln("Setup complete. Will blink indefinitely");
}

void loop() {
  baseEscapeOSLoop(); // DO NOT REMOVE

  // Put your main puzzle code here. This will run indefinitely after setup has completed
  // Please follow best practices. Your loop should be able to be read very clearly, almost as if it were a recipe or a story. Abstract any complexity/implementation details into descriptively named functions
  // Debounce any input that should be debounced (such as buttons/reed switch input). This prevents the signal from being sent multiple times for one button press
  // For any RFID puzzles, do not use the UUID pre-loaded onto RFID tags. Write a string to the RFID tag's first memory block and have your puzzle read these blocks instead of UUID to determine behavior

  if (SerialBT.available()){
    String received = SerialBT.readStringUntil('\n);
    received.trim();
    
      if(received == "TRIGGER"){
        Serial.println("Received: TRIGGER");
        digitalWrite(LITE_BRITE, 1);
        digitalWrite(LED_STRIP, 1);
        solved = true;
      }
   }

   publishPuzzleStatus();

  // "Whiteboard" Code for Button assignment
  // if (buttonIsPressed()) {
  //   acheiveCheckpoint();
  //   publishPuzzleStatus();
  // }

  delay(10); // Do not remove this delay
}

// Any helper functions for your puzzle can be put here. Arduino will hoist functions in .ino files up to where they are needed unlike standard .cpp files

// Call publishPuzzleStatus once per loop in which a change is detected/made
void publishPuzzleStatus() {
  DynamicJsonDocument statusDoc(512);
  // TODO: (Assignment) Replace this static status document with one that captures the full current status of your puzzle
  statusDoc["solved"] = solved;
  statusDoc["exampleData"] = "example";
  statusDoc["timestamp"] = unixTimestamp(); // Always include the unix timestamp in the status message

  publishMQTT(statusTopic(), statusDoc, true); // In most cases, retain (param 3) should be false. Puzzle status is the only case where it should be true
}
