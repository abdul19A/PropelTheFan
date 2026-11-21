// Propel The Fan - Challenge 3: Number Deduction 
//
// Description:
//   A Wordle-style number guessing game. The Arduino picks a random 4-digit
//   password (four different digits, 0-9). The player types a guess on the
//   IR remote; after the 4th digit the guess is checked and every digit gets
//   a hint.
//     G (green)  - right digit, right spot
//     Y (yellow) - digit is in the password, but in a different spot
//     R (red)    - digit is not in the password

//   The LCD shows all four hints at once, and the red/yellow/green hint LEDs
//   step through them one digit at a time. There is no guess limit
//   When the password is solved, the buzzer beeps and "complete" is sent to
//   the main Arduino.
//
//   The RGB LED shows the game status:
//     blue   - ready, waiting for a guess
//     purple - showing hints (input ignored)
//     green  - solved
//     red    - locked by the main Arduino (another challenge is in progress)
//
//   Serial messages with the main Arduino (one character each):
//     sent:      'S' = started (first digit of the game)   'C' = complete
//     received:  'L' = lock   'U' = unlock   'R' = reset (new game)
//   Any other received byte is ignored.

#include <LiquidCrystal.h>
#include <IRremote.hpp>

// ---------------------------------------------------------------------------
// Pins
// ---------------------------------------------------------------------------

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
const int LCD_COLUMNS = 16;

const int IR_RECEIVER_PIN = 7;
const int HINT_RED_PIN = 10;
const int HINT_YELLOW_PIN = 9;
const int HINT_GREEN_PIN = 8;
const int BUZZER_PIN = 6;
const int RGB_RED_PIN = A0;
const int RGB_GREEN_PIN = A1;
const int RGB_BLUE_PIN = A2;
const int RANDOM_SEED_PIN = A3;

// Set to true if the RGB LED's long leg goes to 5V instead of GND.
const bool RGB_COMMON_ANODE = false;

// ---------------------------------------------------------------------------
// Remote codes
// ---------------------------------------------------------------------------

// REMOTE_DIGIT_CODES[d] is the code of the button for digit d.
const uint8_t REMOTE_DIGIT_CODES[10] = {
  0x16, 0x0C, 0x18, 0x5E, 0x08, 0x1C, 0x5A, 0x42, 0x52, 0x4A
};
const uint8_t REMOTE_CLEAR_CODE = 0x44;   // rewind button: clear the guess

// ---------------------------------------------------------------------------
// Messages with the main Arduino "WIP"
// ---------------------------------------------------------------------------

const long BAUD_RATE = 9600;
const char MSG_STARTED = 'S';
const char MSG_COMPLETE = 'C';
const char MSG_LOCK = 'L';
const char MSG_UNLOCK = 'U';
const char MSG_RESET = 'R';

// ---------------------------------------------------------------------------
// Game settings
// ---------------------------------------------------------------------------

const int CODE_LENGTH = 4;
const unsigned long HINT_STEP_MS = 1000;   // time per digit on the hint LEDs
const unsigned long BEEP_ON_MS = 150;
const unsigned long BEEP_OFF_MS = 100;
const int BEEP_COUNT = 3;

// true: the ready screen shows the password (for testing only)
const bool SHOW_CODE_FOR_TESTING = false;

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------

const int PHASE_ENTERING = 0;        // player is typing a guess
const int PHASE_SHOWING_HINTS = 1;   // hint LEDs are stepping through digits
const int PHASE_SOLVED = 2;          // password found, waiting for reset
int phase = PHASE_ENTERING;

int secretCode[CODE_LENGTH];
int guess[CODE_LENGTH];
int digitsEntered = 0;
char hints[CODE_LENGTH];             // 'G', 'Y' or 'R' for each digit
int tries = 0;

int lastGuess[CODE_LENGTH];          // shown on the top row while typing
char lastHints[CODE_LENGTH];
bool hasLastGuess = false;

bool started = false;                // player has begun this challenge
bool locked = false;                 // main Arduino locked this challenge

int hintStep = 0;                    // which digit the hint LEDs are showing
unsigned long hintStepStartTime = 0;

int buzzerTogglesLeft = 0;           // remaining on/off changes for beeping
bool buzzerOn = false;
unsigned long buzzerLastChangeTime = 0;

// ---------------------------------------------------------------------------
// Output helpers
// ---------------------------------------------------------------------------

// Prints text on an LCD row, padded to 16 characters so nothing old remains.
void printLine(int row, const char *text) {
  char line[LCD_COLUMNS + 1];
  snprintf(line, sizeof(line), "%-16s", text);
  lcd.setCursor(0, row);
  lcd.print(line);
}

void setRgb(bool red, bool green, bool blue) {
  digitalWrite(RGB_RED_PIN, (red != RGB_COMMON_ANODE) ? HIGH : LOW);
  digitalWrite(RGB_GREEN_PIN, (green != RGB_COMMON_ANODE) ? HIGH : LOW);
  digitalWrite(RGB_BLUE_PIN, (blue != RGB_COMMON_ANODE) ? HIGH : LOW);
}

void rgbReady()   { setRgb(false, false, true); }   // blue
void rgbHints()   { setRgb(true, false, true); }    // purple
void rgbSolved()  { setRgb(false, true, false); }   // green
void rgbLocked()  { setRgb(true, false, false); }   // red

// Lights the hint LED for one hint letter ('G', 'Y', 'R'); anything else
// turns all three off
void showHintLed(char hint) {
  digitalWrite(HINT_RED_PIN, hint == 'R' ? HIGH : LOW);
  digitalWrite(HINT_YELLOW_PIN, hint == 'Y' ? HIGH : LOW);
  digitalWrite(HINT_GREEN_PIN, hint == 'G' ? HIGH : LOW);
}

void startBeeping() {
  buzzerTogglesLeft = BEEP_COUNT * 2;   // each beep = on + off
  buzzerOn = false;
  buzzerLastChangeTime = millis() - BEEP_OFF_MS;   // first beep right away
}

// Non-blocking beep pattern for the active buzzer
void updateBuzzer() {
  if (buzzerTogglesLeft == 0) {
    return;
  }
  unsigned long waitMs = buzzerOn ? BEEP_ON_MS : BEEP_OFF_MS;
  if (millis() - buzzerLastChangeTime >= waitMs) {
    buzzerOn = !buzzerOn;
    digitalWrite(BUZZER_PIN, buzzerOn ? HIGH : LOW);
    buzzerLastChangeTime = millis();
    buzzerTogglesLeft--;
  }
}

// ---------------------------------------------------------------------------
// LCD screens
// ---------------------------------------------------------------------------

// Typing screen. Top: title last guess with its hints
// Bottom: the digits typed so far
void drawEnteringScreen() {
  char line[LCD_COLUMNS + 1];

  if (hasLastGuess) {
    snprintf(line, sizeof(line), "Last: %d%d%d%d %c%c%c%c",
             lastGuess[0], lastGuess[1], lastGuess[2], lastGuess[3],
             lastHints[0], lastHints[1], lastHints[2], lastHints[3]);
  } else if (SHOW_CODE_FOR_TESTING) {
    snprintf(line, sizeof(line), "Test code: %d%d%d%d",
             secretCode[0], secretCode[1], secretCode[2], secretCode[3]);
  } else {
    snprintf(line, sizeof(line), "Guess the code!");
  }
  printLine(0, line);

  char entry[LCD_COLUMNS + 1] = "Enter:";
  int length = 6;
  for (int i = 0; i < CODE_LENGTH; i++) {
    entry[length++] = ' ';
    entry[length++] = (i < digitsEntered) ? ('0' + guess[i]) : '_';
  }
  entry[length] = '\0';
  printLine(1, entry);
}

// Hint screen: "Guess: 3 5 1 8" over "Hint:  Y G Y R" that line up
void drawHintScreen() {
  char top[LCD_COLUMNS + 1] = "Guess:";
  char bottom[LCD_COLUMNS + 1] = "Hint: ";
  int length = 6;
  for (int i = 0; i < CODE_LENGTH; i++) {
    top[length] = ' ';
    bottom[length] = ' ';
    top[length + 1] = '0' + guess[i];
    bottom[length + 1] = hints[i];
    length += 2;
  }
  top[length] = '\0';
  bottom[length] = '\0';
  printLine(0, top);
  printLine(1, bottom);
}

void drawSolvedScreen() {
  char line[LCD_COLUMNS + 1];
  printLine(0, "Code cracked!");
  snprintf(line, sizeof(line), "%d%d%d%d in %d %s",
           secretCode[0], secretCode[1], secretCode[2], secretCode[3],
           tries, tries == 1 ? "try" : "tries");
  printLine(1, line);
}

void drawLockedScreen() {
  printLine(0, "Challenge locked");
  printLine(1, "Finish other one");
}

// ---------------------------------------------------------------------------
// Game logic
// ---------------------------------------------------------------------------

// Picks four different random digits by shuffling 0-9 and taking the first 4.
void pickSecretCode() {
  int digits[10];
  for (int i = 0; i < 10; i++) {
    digits[i] = i;
  }
  for (int i = 9; i > 0; i--) {
    int j = random(i + 1);
    int temp = digits[i];
    digits[i] = digits[j];
    digits[j] = temp;
  }
  for (int i = 0; i < CODE_LENGTH; i++) {
    secretCode[i] = digits[i];
  }
}

void startNewGame() {
  pickSecretCode();
  phase = PHASE_ENTERING;
  digitsEntered = 0;
  tries = 0;
  hasLastGuess = false;
  started = false;
  locked = false;

  showHintLed(' ');
  buzzerTogglesLeft = 0;
  digitalWrite(BUZZER_PIN, LOW);

  rgbReady();
  drawEnteringScreen();
}

// Wordle rules: greens first, then yellows. Each password digit can give
// credit to only one guess digit, so repeated guesses never mislead.
void computeHints() {
  bool secretUsed[CODE_LENGTH] = {false, false, false, false};

  for (int i = 0; i < CODE_LENGTH; i++) {
    if (guess[i] == secretCode[i]) {
      hints[i] = 'G';
      secretUsed[i] = true;
    } else {
      hints[i] = 'R';
    }
  }

  for (int i = 0; i < CODE_LENGTH; i++) {
    if (hints[i] != 'R') {
      continue;
    }
    for (int j = 0; j < CODE_LENGTH; j++) {
      if (!secretUsed[j] && guess[i] == secretCode[j]) {
        hints[i] = 'Y';
        secretUsed[j] = true;
        break;
      }
    }
  }
}

void checkGuess() {
  tries++;
  computeHints();

  for (int i = 0; i < CODE_LENGTH; i++) {
    lastGuess[i] = guess[i];
    lastHints[i] = hints[i];
  }

  bool solved = true;
  for (int i = 0; i < CODE_LENGTH; i++) {
    if (hints[i] != 'G') {
      solved = false;
    }
  }

  if (solved) {
    phase = PHASE_SOLVED;
    Serial.write(MSG_COMPLETE);
    rgbSolved();
    showHintLed('G');
    startBeeping();
    drawSolvedScreen();
    return;
  }

  // Start stepping the hint LEDs through the four digits.
  phase = PHASE_SHOWING_HINTS;
  hintStep = 0;
  hintStepStartTime = millis();
  rgbHints();
  showHintLed(hints[0]);
  drawHintScreen();
}

// Moves the hint LEDs to the next digit every HINT_STEP_M, so after the last
// digit, goes back to typing the next guess
void updateHints() {
  if (phase != PHASE_SHOWING_HINTS) {
    return;
  }
  if (millis() - hintStepStartTime < HINT_STEP_MS) {
    return;
  }

  hintStep++;
  hintStepStartTime = millis();

  if (hintStep < CODE_LENGTH) {
    showHintLed(hints[hintStep]);
  } else {
    showHintLed(' ');
    phase = PHASE_ENTERING;
    digitsEntered = 0;
    hasLastGuess = true;
    rgbReady();
    drawEnteringScreen();
  }
}

void enterDigit(int digit) {
  if (!started) {
    started = true;
    Serial.write(MSG_STARTED);
  }

  guess[digitsEntered] = digit;
  digitsEntered++;
  drawEnteringScreen();

  if (digitsEntered == CODE_LENGTH) {
    checkGuess();
  }
}

void clearGuess() {
  digitsEntered = 0;
  drawEnteringScreen();
}

// ---------------------------------------------------------------------------
// Inputs
// ---------------------------------------------------------------------------

// Reads one remote button, if any, and acts on it.
void handleRemote() {
  if (!IrReceiver.decode()) {
    return;
  }

  bool isRepeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
  uint8_t command = IrReceiver.decodedIRData.command;
  IrReceiver.resume();

  // Ignore held-button repeats, and all input unless the player can type
  if (isRepeat || locked || phase != PHASE_ENTERING) {
    return;
  }

  if (command == REMOTE_CLEAR_CODE) {
    clearGuess();
    return;
  }

  for (int digit = 0; digit < 10; digit++) {
    if (command == REMOTE_DIGIT_CODES[digit]) {
      enterDigit(digit);
      return;
    }
  }
  // Any other button is ignored
}

// Handles messages from the main Arduino
void handleSerial() {
  while (Serial.available() > 0) {
    char message = Serial.read();

    if (message == MSG_LOCK) {
      // A lock means another challenge is in progress, so it only applies
      // if the player hasn't started this one
      if (!started && !locked) {
        locked = true;
        rgbLocked();
        drawLockedScreen();
      }
    } else if (message == MSG_UNLOCK) {
      if (locked) {
        locked = false;
        rgbReady();
        drawEnteringScreen();
      }
    } else if (message == MSG_RESET) {
      startNewGame();
    }
    // Anything else is ignored
  }
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(BAUD_RATE);
  lcd.begin(LCD_COLUMNS, 2);

  pinMode(HINT_RED_PIN, OUTPUT);
  pinMode(HINT_YELLOW_PIN, OUTPUT);
  pinMode(HINT_GREEN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);

  IrReceiver.begin(IR_RECEIVER_PIN, ENABLE_LED_FEEDBACK);
  randomSeed(analogRead(RANDOM_SEED_PIN));

  startNewGame();
}

void loop() {
  handleRemote();   // player input
  handleSerial();   // messages from the main Arduino
  updateHints();    // step the hint LEDs
  updateBuzzer();   // success beeps
}
