#include <TimerOne.h>
#include <SoftwareSerial.h>

#define STEPS  32   // Number of steps for one revolution of Internal shaft
// 2048 steps for one revolution of External shaft

volatile boolean TurnDetected;  // need volatile for Interrupts
volatile boolean rotationdirection;  // CW or CCW rotation

const int PinCLK = 2; // Generating interrupts using CLK signal
const int PinDT = 3;  // Reading DT signal
const int PinSW = 4;  // Reading Push Button switch

volatile bool buttonDown = false;
volatile bool pulseReceived = false;
volatile bool isRunning = false;

// Interrupt routine runs if CLK goes from HIGH to LOW
void isr ()  {
  delay(4);  // delay for Debouncing
  if (digitalRead(PinCLK))
    rotationdirection = digitalRead(PinDT);
  else
    rotationdirection = !digitalRead(PinDT);
  TurnDetected = true;
}

// Interrupt routine runs if CLK goes from HIGH to LOW
void pulse ()  {
  pulseReceived = true;
}

const int PinBtn = 7; //Button - replaced by rotary button

const unsigned long pulsesPerBeat = 24;
const unsigned long microSecondsPerMinute = 60000000;
const byte midiStart = 250;
const byte midiStop = 252;
const byte midiClock = 248;



int bpm = 80;
int beatsPerBar = 4;

unsigned long waitInterval = 0;
int pulseCount = -1;
int beatCount = -1;

// If using buzzer
int buzzer = 12;//the pin of the active buzzer
int newBarBuzzerWait = 15;
int inBarBuzzerWait = 8;

SoftwareSerial mySerial(10, 9);

void setup() {
  mySerial.begin(31250);
  Serial.begin(9200);

  pinMode(buzzer, OUTPUT); //initialize the buzzer pin as an output

  pinMode(PinBtn, INPUT_PULLUP);
  //attachInterrupt(0, Reset, FALLING);

  pinMode(PinCLK, INPUT);
  pinMode(PinDT, INPUT);
  pinMode(PinSW, INPUT);
  digitalWrite(PinSW, HIGH); // Pull-Up resistor for switch
  attachInterrupt (0, isr, FALLING); // interrupt 0 always connected to pin 2 on Arduino UNO

  Timer1.initialize();
  UpdateBpm();
  Timer1.attachInterrupt(pulse);
}

void loop() {
  checkForPulse();

  checkForButtonPress();

  checkForRotaryTurn();
}

void checkForPulse(){

    if (isRunning && pulseReceived) {
    pulseCount = (pulseCount + 1) % pulsesPerBeat;
    if (pulseCount == 0) {
      beatCount = (beatCount + 1) % beatsPerBar;
      //Serial.print(beatCount);
      if (beatCount == 0) {
        beepNewBar();
      }
      else {
        beepInBar();
      }
    }
    else {
      SendMidiCommand(midiClock);
    }
    pulseReceived = false;
  }
}

void checkForButtonPress(){
  if (!(digitalRead(PinSW))) {   // check if button is pressed
    if (!buttonDown) {
      Reset();
    }
    buttonDown = true;
  }
  else {
    buttonDown = false;
  }
}

void checkForRotaryTurn(){
  if (TurnDetected)  {
    if (rotationdirection && bpm < 220) {
      bpm++;
      UpdateBpm();
    }
    else if (!rotationdirection && bpm > 20) {
      bpm--;
      UpdateBpm();
    }

    TurnDetected = false;  // do NOT repeat IF loop until new rotation detected
  }
}

void beepNewBar() {
  digitalWrite(buzzer, HIGH);
  SendMidiCommand(midiClock);
  delay(newBarBuzzerWait - 3);
  digitalWrite(buzzer, LOW);
}

void beepInBar() {
  digitalWrite(buzzer, HIGH);
  SendMidiCommand(midiClock);
  delay(inBarBuzzerWait - 3);
  digitalWrite(buzzer, LOW);
}

void Reset()
{
  isRunning = !isRunning;
  if (!isRunning) {
    SendMidiCommand(midiStop);
    pulseCount = -1;
    beatCount = -1;
  }
  else {
    SendMidiCommand(midiStart);
  }
}

void UpdateBpm() {

  waitInterval = (microSecondsPerMinute / bpm) / pulsesPerBeat;
  Timer1.setPeriod(waitInterval);
  Serial.println(bpm);
}

void SendMidiCommand(byte command) {
  mySerial.write(0xFF);
  mySerial.write(command);
}

