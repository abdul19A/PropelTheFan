#include <Wire.h>
#include <SoftwareSerial.h>

const byte RESET_BUTTON = 2;
const byte BINARY_LED = 3;
const byte MEMORY_LED = 4;
const byte PASSWORD_LED = 5;
const byte FAN = 6;

const byte BINARY_RESET = 9;
const byte MEMORY_RESET = 10;
const byte OVERSEER_ADDRESS = 0x08;

const byte BINARY_DONE = 0x01;
const byte MEMORY_DONE = 0x02;
const byte PASSWORD_DONE = 0x04;
const byte ALL_DONE = BINARY_DONE | MEMORY_DONE | PASSWORD_DONE;

SoftwareSerial passwordLink(7,8);

volatile byte i2cCompletions = 0;
byte completed = 0;

void receivedCompletion(int byteCount){
  if(byteCount == 1){
    int game = Wire.read();
    if(game == 1){
      i2cCompletions |= BINARY_DONE;
    }else if(game == 2){
      i2cCompletions |= MEMORY_DONE;
    }
  }
  while(Wire.available()){
    Wire.read();
  }
}

void updateOutputs(){
  digitalWrite(BINARY_LED, (completed & BINARY_DONE) != 0);
  digitalWrite(MEMORY_LED, (completed & MEMORY_DONE) != 0);
  digitalWrite(PASSWORD_LED, (completed & PASSWORD_DONE) != 0);
  digitalWrite(FAN, completed == ALL_DONE);
}

bool resetPressed(){
  static int lastReading = HIGH;
  static int stableReading = HIGH;
  static unsigned long changedAt = 0;

  int reading = digitalRead(RESET_BUTTON);
  if(reading != lastReading){
    lastReading = reading;
    changedAt = millis();
  }

  if(reading != stableReading && millis() - changedAt >= 30){
    stableReading = reading;
    return stableReading == LOW;
  }

  return false;
}

void resetRound(){
  completed = 0;
  updateOutputs();

  pinMode(BINARY_RESET, OUTPUT);
  pinMode(MEMORY_RESET, OUTPUT);
  delay(10);
  pinMode(BINARY_RESET, INPUT);
  pinMode(MEMORY_RESET, INPUT);


  noInterrupts();
  i2cCompletions = 0;
  interrupts();

  while(passwordLink.available()){
    passwordLink.read();
  }
  passwordLink.write('R');
}

void setup(){
  pinMode(RESET_BUTTON, INPUT_PULLUP);
  pinMode(BINARY_LED, OUTPUT);
  pinMode(MEMORY_LED, OUTPUT);
  pinMode(PASSWORD_LED, OUTPUT);
  pinMode(FAN, OUTPUT);
  updateOutputs();

  pinMode(BINARY_RESET, INPUT);
  pinMode(MEMORY_RESET, INPUT);
  digitalWrite(BINARY_RESET, LOW);
  digitalWrite(MEMORY_RESET, LOW);

  passwordLink.begin(9600);
  Wire.begin(OVERSEER_ADDRESS);
  Wire.onReceive(receivedCompletion);
}

void loop(){
  if(resetPressed()){
    resetRound();
    return;
  }
  noInterrupts();
  byte received = i2cCompletions;
  i2cCompletions = 0;
  interrupts();
  completed |= received;

  while(passwordLink.available()){
    if(passwordLink.read() == 'C'){
      completed |= PASSWORD_DONE;
    }
  }

  updateOutputs();
}





































