#include <SoftwareSerial.h>

// Serials
SoftwareSerial units[3] = {
  SoftwareSerial(2, 3),           
  SoftwareSerial(4, 5),
  SoftwareSerial(6, 7)
};


const char OFF_SIGNAL     = 'O';
const char RUNNING_SIGNAL = 'R';
const char RESET_SIGNAL   = 'X';
const char WINNING_SIGNAL = 'W';
const char LOSING_SIGNAL  = 'L';

char currentSignal = OFF_SIGNAL;
int selected = 0;

void TransmitReset(int i){
  // transmits one character 'X' to the serial
  units[i].write(RESET_SIGNAL);
}

void SelectArduino(int i){
  units[i].listen();
  selected = i;
  currentSignal = OFF_SIGNAL;
}

void ReadTransmission(){
  // reads the last byte
  if (units[selected].available() > 0){
    char c = units[selected].read();
    if (c == RUNNING_SIGNAL || c == WINNING_SIGNAL || c == LOSING_SIGNAL) {
      currentSignal = c;
    }
  }
}

// the following is what is checked on each arduino

bool doReset(){
  if (Serial.available() > 0){
    return RESET_SIGNAL == Serial.read();
  }
  return false;
}

void TransmitWin(){
  Serial.write(WINNING_SIGNAL);
}
void TransmitLose(){
  Serial.write(LOSING_SIGNAL);
}
void TransmitRunning(){
  Serial.write(RUNNING_SIGNAL);
}

void setup() {
  // for main
  for (int i = 0; i < 3; i++){
    units[i].begin(9600);
  }
  SelectArduino(0);
  // others
  Serial.begin(9600); // agreed on baud

}

void actuallyReset();


void loop() {
  // main arduino
  ReadTransmission(); 
  // other arduinos
  TransmitRunning();
  //TransmitLose();
  //TransmitWin();

  if (doReset()){
    actuallyReset();
  }
}
