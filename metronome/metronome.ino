#include <pitches.h>
#include <Encoder.h>
#include <SendOnlySoftwareSerial.h>
#include <TimerOne.h>
#include <LiquidCrystal.h>


const int PinCLK = 2; // Generating interrupts using CLK signal
const int PinDT = 3;  // Reading DT signal
const int PinSW = 4;  // Reading Push Button switch

Encoder rotaryKnob(PinCLK, PinDT);

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

volatile bool buttonDown = false;
volatile bool sendMidiClock = false;
volatile bool isRunning = false;

// Timer1 interrupt requesting midi clock signal to be sent
void pulse ()  {
  sendMidiClock = true;
}

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
int PinBuzzer = 6;//the pin of the active buzzer
int newBarBuzzerWait = 15;
int inBarBuzzerWait = 8;

SendOnlySoftwareSerial mySerial(5);

void setup() {
  lcd.begin(16, 2);
  UpdateTimeSignature();
  
  mySerial.begin(31250);
  Serial.begin(9200);

  pinMode(PinBuzzer, OUTPUT); //initialize the buzzer pin as an output
  pinMode(PinSW, INPUT);
  digitalWrite(PinSW, HIGH); // Pull-Up resistor for switch

  rotaryKnob.write(bpm*4);

  Timer1.initialize();
  UpdateBpm();
  Timer1.attachInterrupt(pulse);
}

void loop() {
  checkForPulse();

  checkForButtonPress();

  checkForRotaryTurn();
}

void checkForPulse() {

  if (isRunning && sendMidiClock) {
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
    sendMidiClock = false;
  }
}

void checkForButtonPress() {
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

void checkForRotaryTurn() {
  long newBpm;
  newBpm = rotaryKnob.read()/4;
  if (newBpm != bpm) {
    bpm = newBpm;
    UpdateBpm();
  }
  // if a character is sent from the serial monitor,
  // reset both back to zero.
  if (Serial.available()) {
    Serial.read();
    Serial.println("Reset knob to zero");
    rotaryKnob.write(0);
  }
}

void beepNewBar() {
  
  //digitalWrite(PinBuzzer, HIGH);
  tone(PinBuzzer, NOTE_G7, 30);
  SendMidiCommand(midiClock);
  
  //delay(newBarBuzzerWait - 3);
 // digitalWrite(PinBuzzer, LOW);
}

void beepInBar() {
 // digitalWrite(PinBuzzer, HIGH);
 tone(PinBuzzer, NOTE_C6, 20);
  SendMidiCommand(midiClock);
  
 // delay(inBarBuzzerWait - 3);
 // digitalWrite(PinBuzzer, LOW);
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
  lcd.setCursor(0, 0);
  // print the number of seconds since reset:
  lcd.print("bpm: ");
  lcd.print(bpm);
}

void UpdateTimeSignature() {
  lcd.setCursor(0, 1);
  // print the number of seconds since reset:
  lcd.print("time: ");
  lcd.print(beatsPerBar);
  lcd.print("/4");
}

void SendMidiCommand(byte command) {
  mySerial.write(0xFF);
  mySerial.write(command);
}

