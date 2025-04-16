class Blinker {
private:
  int ledPin;                    // Pin to control the LED
  int blinkCount;                // Number of blinks in a pattern
  int currentBlink;              // Current blink in the sequence
  unsigned long blinkInterval;   // Interval between LED ON/OFF
  unsigned long pauseInterval;   // Pause duration after completing the pattern
  unsigned long previousLedStateMillis;  // Stores the last time the LED state changed
  bool ledState;                 // Current state of the LED
  bool inPause;                  // Whether in pause phase or not
  bool isRunning;                // Whether the blinker is active or not
  int ledOn;
  int ledOff;

public:
  // Constructor to initialize the blinker
  Blinker(int pin, int count, unsigned long blinkInt, unsigned long pauseInt, int ledOnVal) {
    ledPin = pin;
    blinkCount = count;
    blinkInterval = blinkInt;
    pauseInterval = pauseInt;
    ledOn = ledOnVal;
    ledOff = !ledOnVal;
    currentBlink = 0;
    ledState = false;
    inPause = false;
    isRunning = false;
    previousLedStateMillis = 0;
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, ledOff);
  }

  // Method to set a new blink pattern dynamically
  void setPattern(int count, unsigned long blinkInt, unsigned long pauseInt) {
    blinkCount = count;
    blinkInterval = blinkInt;
    pauseInterval = pauseInt;
    reset();
  }

  // Reset the blinker state
  void reset() {
    currentBlink = 0;
    ledState = false;
    inPause = false;
    previousLedStateMillis = currentMillis;
    digitalWrite(ledPin, ledOff);
  }

  // Pause the blinking
  void pause() {
    isRunning = false;
    digitalWrite(ledPin, ledOff); // Turn off LED when paused
  }

  // Resume blinking
  void start() {
    if (!isRunning) {
      isRunning = true;
      reset(); // Restart the pattern from the beginning
    }
  }

  void solidOn() {
    digitalWrite(ledPin, ledOn);
  }


  void solidOff() {
    digitalWrite(ledPin, ledOff);
  }

  // Method to check if blinking is running
  bool isPaused() {
    return !isRunning;
  }

  // Method to handle the blinking logic
  void update() {
    if (!isRunning) {
      return;
    }

    // Check if we are in the pause phase
    if (inPause) {
      if (currentMillis - previousLedStateMillis >= pauseInterval) {
        // End pause, reset the blink sequence
        inPause = false;
        currentBlink = 0;
        previousLedStateMillis = currentMillis;
      }
      return;
    }

    // Blink the LED
    if (currentMillis - previousLedStateMillis >= blinkInterval) {
      previousLedStateMillis = currentMillis;

      if (currentBlink < blinkCount) { // Each blink has ON and OFF
        ledState = !ledState;
        digitalWrite(ledPin, ledState);

        if (!ledState) {
          currentBlink++; // Increment only on OFF state
        }
      } else {
        // Blink pattern complete, enter pause phase
        inPause = true;
        digitalWrite(ledPin, ledOff); // Ensure LED is off
      }
    }
  }
};