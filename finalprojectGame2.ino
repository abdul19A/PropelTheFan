#include <LiquidCrystal.h>
#include <Servo.h>
// LCD const
const byte rs = 12, en = 11, d4 = 10, d5 = 9, d6 = 8, d7 = 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const byte servo_pin = 2;
const byte joystickX = A0, joystickY = A1;
const byte led_pins[4] = {A2, 4, 5, 6}; // using analog pin because I ran out
const byte buzzer = 3;  

const int buzzer_freq[] = {300, 400, 500, 600};

Servo servo;

const char seq[]= "0101";
const int seq_len = 4;

const char RUNNING_SIGNAL = 'R';
const char RESET_SIGNAL   = 'X';
const char WINNING_SIGNAL = 'W';
const char LOSING_SIGNAL  = 'L';

long seq_timer = millis();
const long on_dur = 1000;
const long off_dur = 100;
const long time_to_match = 2000;
const long setup_time = 3000;

const int servo_locs [4]= {0, 45, 135, 180};
const int deadzoneSqrd = 40*40; // out of fifty
const int n_blinks_max = 2;

// hmm could add a losing mechanic
  
int n_success_blinks = n_blinks_max;
int cur_showing = 0;
int n_match = 0;
int n_max = 0;
int servo_on = false;
bool accept_input = false;

enum state: byte{
  start,
  show_seq,
  gather_input,
  eval_inputs,
  failed,
  success,
  ended
};

state cur_state = start;
void printScrolled(char* s, int length){
  // helper to print
  // speed is in character per second
  if (length <= 16){
    lcd.print(s);
    return;
  }
  int over = (length - 16);
  int i = min((int)((millis()) / 500) % (over+1), over); // this line calculates how much to scroll
  // calculates first how much to move 
  // then modulos that over how much the string is over 15 (to hang for that last character)
  // and it is minimized to not go over the limit

  char buf[17];
  snprintf(buf, 17, "%s", s + i);
  lcd.print(buf);
}
int readStickInput(){
  int x = (analogRead(joystickX)-500); // max 50
  int y = (analogRead(joystickY)-500); // max 50
  int xdiv = x/10;
  int ydiv = y/10;
  if (xdiv*xdiv+ydiv*ydiv < deadzoneSqrd) return -1;
  if (x < 0){
    if (y > 0) return 3;
    return 2;
  }  else {
    if (y > 0) return 0;
    return 1;
  }
}
bool waiting(){
  if (millis() - seq_timer > 3000){
    seq_timer = millis();
    return true;
  }
  return false;
}
bool blinking(){
  int cur = seq[cur_showing] - '0';
  int num = led_pins[cur];
  int num_servo = servo_locs[cur];
  long diff = millis() - seq_timer;

  if (diff <= on_dur){
    digitalWrite(num, HIGH);
    if (!servo_on){
      servo.write(num_servo);
      servo_on = true;
      tone(buzzer, buzzer_freq[cur], on_dur-diff);
    }
  }
  else if (diff <= on_dur+off_dur){
    digitalWrite(num, LOW);
    servo_on= false;
  }
  else {
    // we are not done
    digitalWrite(num, LOW);
    cur_showing++;
    servo_on = false;
    seq_timer = millis();

    if (cur_showing > n_max){
      servo.write(90);
      cur_showing = 0;
      return false;
    }
    return true;
  }
  return true;
}
bool succeeded(){
  long diff = millis() - seq_timer;
  if (diff < on_dur){
    for (int i = 0; i < 4; i++){
      digitalWrite(led_pins[i], HIGH);
    }
  }
  else if (diff < on_dur+off_dur){
    for (int i = 0; i < 4; i++){
      digitalWrite(led_pins[i], LOW);
    }
  }
  else{
    seq_timer = millis();
    n_success_blinks--;
    if (n_success_blinks <= 0){
      n_success_blinks = n_blinks_max;
      return false;
    } 
  }
  return true;
}
bool failing(){
  long diff = millis() - seq_timer;
  byte led = led_pins[seq[n_match]];

  if (diff < on_dur){
    digitalWrite(led, HIGH);
    return true;
  }
  else if (diff < (on_dur+off_dur)){
    digitalWrite(led, LOW);
    return true;
  }
  else{
    n_match = 0;
    return false;
  }

}
bool readingInput(){
  long diff = millis() - seq_timer;
  int toMatch = (seq[n_match] - '0');
  if (diff <= time_to_match){
    int input = readStickInput();
    if (input == -1){
      accept_input = true;
      return true;
    }
    else if (!accept_input) return true;

    if(input == toMatch){
      n_match++;
      seq_timer = millis();
      accept_input = false;
      return false;
    }
    else {
      n_max = -1;
      accept_input = false;
      return false;
    }
  }
  else {
    n_max = -1;
    accept_input = false;
    return false;
  }
}
void evaluateInput(){
  if (n_max == -1){ // failed
    cur_state = failed;
    n_max = 0;
  }
  else if (n_match <= n_max){ // keep going
    cur_state = gather_input;
  }
  else if (n_max < seq_len-1){ // all done with the sequence
    n_match = 0;
    cur_state = success;
    n_max++;
  }
  else { // wow got it all
    cur_state = ended;
  }
}
bool starting(){
  // display instructions
  // wait for an input before returning false 
  lcd.setCursor(0, 0);
  printScrolled("Copy the sequence!", 18);
  lcd.setCursor(0, 1);
  printScrolled("Move Joystick To Start!", 23);

  if (readStickInput() != -1){
    lcd.clear();
    return false;
  }
  else return true;
}
void selectState(){
  state prev = cur_state;
  switch (cur_state){
    case start:
    if (!starting()) cur_state = show_seq;
    break;

    case show_seq:
    lcd.setCursor(0, 0);
    lcd.print("sequence #");
    lcd.print(n_max+1);
    if (!blinking()) cur_state = gather_input;
    break;

    case gather_input:
    if (!readingInput()) cur_state = eval_inputs;
    break;

    case eval_inputs:
    evaluateInput();  // branches to other states automatically
    break;

    case failed:
    lcd.setCursor(0, 0);
    lcd.print("failed at #");
    lcd.print(n_match+1);
    if (!failing()) cur_state = show_seq;
    break;

    case success:
    lcd.setCursor(0, 0);
    lcd.print("Finished ");
    lcd.print(n_max);
    if (!succeeded()) cur_state = show_seq;
    break;

    case ended:
    lcd.setCursor(0, 0);
    lcd.print("YOU WON");
    succeeded();
    break;
  }

  if (prev != cur_state){ // reset timer based on new state
    seq_timer = millis();
    lcd.clear();
  }
}

void applyReset(){
  // reset everything
  lcd.clear();
  noTone(buzzer);
  servo.write(90);
  for (int i = 0; i < 4; i++){
    digitalWrite(led_pins[i], LOW);
  }

  n_success_blinks = n_blinks_max;
  cur_showing = 0;
  n_match = 0;
  n_max = 0;
  servo_on = false;
  accept_input = false;
  cur_state = start; // starts back up 
  seq_timer = millis();
}

void TransmitState(){
  // ended is when its over
  if (cur_state == ended){
    Serial.write(WINNING_SIGNAL);
  }
  else {
    Serial.write(RUNNING_SIGNAL);
  }
}

bool doReset(){
  // note: this can be easily tested by writing directly to the arduino through a computer
  if (Serial.available() > 0){
    return RESET_SIGNAL == Serial.read();
  }
  return false;
}

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);

  pinMode(joystickX, INPUT);
  pinMode(joystickY, INPUT);

  for (int i = 0; i < 4; i++){
    pinMode(led_pins[i], OUTPUT);
  }

  pinMode(buzzer, OUTPUT);
  servo.attach(servo_pin);
  servo.write(90);
}

void loop() { 
  selectState(); // do what its supposed to
  TransmitState(); // <- send out
  if (doReset()){ // <- read
    applyReset();
  }
}
