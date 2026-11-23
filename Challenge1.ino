#include <LiquidCrystal.h>
#include <Wire.h>


LiquidCrystal LCD(12, 11, 5, 4, 3, 2);
   
const int LEDPins[] = {6, 7, 8, 9};
const int incButton = 13;
const int decButton = 10;
const int enterButton = A1;
const int overseerAddress = 0x08;

int targetNum;
int playerCount;

bool challengePassed = false;
bool showIntro = true;
bool showIncorrect = false;

unsigned long previousTime = 0;
const unsigned long introTime = 3000;
const unsigned long incorrectTime = 3000;
const unsigned long debounceTime = 30;

const int buttonPins[] = {incButton, decButton, enterButton};
int lastReading[3] = {HIGH, HIGH, HIGH};
int buttonState[3] = {HIGH, HIGH, HIGH};

unsigned long lastDebounceTime[3] = {0, 0, 0};


bool pressedButton(int buttonPin, int index) {
  int reading = digitalRead(buttonPin);
  unsigned long currentTime = millis();

  if (reading != lastReading[index]) {
    lastDebounceTime[index] = currentTime;
    lastReading[index] = reading;
  }

  if (currentTime - lastDebounceTime[index] >= debounceTime) {
    if (reading != buttonState[index]) {
      buttonState[index] = reading;

      if (buttonState[index] == LOW) {
        return true;
      }
    }
  }

  return false;
}


void updateLEDs() {
  for (int i = 0; i < 4; i++) {
    int bitValue = (playerCount >> (3 - i)) & 1;
    digitalWrite(LEDPins[i], bitValue);
  }
}


void challengeStart() {
  targetNum = random(0, 16);
  playerCount = 0;

  updateLEDs();

  LCD.clear();
  LCD.setCursor(0, 0);
  LCD.print("Target: ");
  LCD.print(targetNum);

  LCD.setCursor(0, 1);
  LCD.print("UP/DOWN/ENTER");
}


void challengeComplete() {
  Wire.beginTransmission(overseerAddress);
  Wire.write(1);
  Wire.endTransmission();
}


void checkAnswer() {
  LCD.clear();

  if (playerCount == targetNum) {
    challengePassed = true;

    LCD.setCursor(0, 0);
    LCD.print("Correct!");

    LCD.setCursor(0, 1);
    LCD.print("Challenge Passed");

    challengeComplete();
  }
  else {
    LCD.setCursor(0, 0);
    LCD.print("Incorrect!");

    LCD.setCursor(0, 1);
    LCD.print("Restarting...");

    previousTime = millis();

    showIncorrect = true;
  }
}


void setup() {
  LCD.begin(16, 2);

  for (int i = 0; i < 4; i++) {
    pinMode(LEDPins[i], OUTPUT);
  }

  for (int i = 0; i < 3; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  for (int i = 0; i < 3; i++) {
    lastReading[i] = digitalRead(buttonPins[i]);
    buttonState[i] = digitalRead(buttonPins[i]);
  }

  Wire.begin();
  randomSeed(analogRead(A0));

  updateLEDs();

  LCD.setCursor(0, 0);
  LCD.print("Challenge #1");

  LCD.setCursor(0, 1);
  LCD.print("Num Conversion");

  previousTime = millis();
}


void loop() {
  unsigned long currentTime = millis();

  if (challengePassed) {
    return;
  }

  if (showIntro) {
    if (currentTime - previousTime >= introTime) {
      showIntro = false;
      challengeStart();
    }

    return;
  }

  if (showIncorrect) {
    if (currentTime - previousTime >= incorrectTime) {
      showIncorrect = false;
      challengeStart();
    }

    return;
  }

  bool incButtonPressed = pressedButton(incButton, 0);
  bool decButtonPressed = pressedButton(decButton, 1);
  bool enterButtonPressed = pressedButton(enterButton, 2);

  if (incButtonPressed) {
    if (playerCount < 15) {
      playerCount++;
    }

    updateLEDs();
  }

  if (decButtonPressed) {
    if (playerCount > 0) {
      playerCount--;
    }

    updateLEDs();
  }

  if (enterButtonPressed) {
    checkAnswer();
  }
}
