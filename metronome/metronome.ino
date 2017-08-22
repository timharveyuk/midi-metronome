#include "metronome.h"
#include <EEPROM.h>
#include <pitches.h>
#include <Encoder.h>
#include <SendOnlySoftwareSerial.h>
#include <TimerOne.h>
#include <LiquidCrystal.h>


const int PinCLK = 2; // Generating interrupts using CLK signal
const int PinDT = 3;  // Reading DT signal
const int PinBtnRotary = 4;  // Reading Push Button switch
const int PinMidi = 5;
const int PinBuzzer = 6;//the pin of the active buzzer
const int PinBtnStart = A3;
const int PinBtnS1 = A2;
const int PinBtnS2 = A1;
const int PinBtnTap = A0;

const int buttonHoldMs = 1000;

const int numSettingButtons = 2;
Settings settingsButtons[numSettingButtons];

bool PinBtnStartHeld = false;
bool PinBtnTapHeld = false;
bool PinBtnRotaryHeld = false;

bool SettingsFieldShowing = false;

volatile bool lcdSelectedTopField = true;

Encoder rotaryKnob(PinCLK, PinDT);

LiquidCrystal lcd(12,11,10,9,8,7);

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

int bpm = DEFAULT_TEMPO;
int beatsPerBar = DEFAULT_TS;

unsigned long waitInterval = 0;
int pulseCount = -1;
int beatCount = -1;

SendOnlySoftwareSerial midiSerial(PinMidi);

void setup() {
  midiSerial.begin(31250);
  Serial.begin(9200);
  
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print(">bpm: ");
  lcd.setCursor(0, 1);
  lcd.print(" time: ");
  UpdateTimeSignature();

  settingsButtons[0].name = "S1";
  settingsButtons[0].pin = PinBtnS1;
  settingsButtons[1].name = "S2";
  settingsButtons[1].pin = PinBtnS2;
  for(int i=0;i<numSettingButtons;i++){
    ReadSetting(i);
  }

  pinMode(PinBuzzer, OUTPUT); //initialize the buzzer pin as an output
  pinMode(PinBtnRotary, INPUT);
  pinMode(PinBtnStart, INPUT_PULLUP);
  pinMode(PinBtnS1, INPUT_PULLUP);
  pinMode(PinBtnS2, INPUT_PULLUP);
  pinMode(PinBtnTap, INPUT_PULLUP);
  digitalWrite(PinBtnRotary, HIGH); // Pull-Up resistor for switch

  rotaryKnob.write(bpm * 4);

  Timer1.initialize();
  UpdateBpm();
  Timer1.attachInterrupt(pulse);
}

void loop() {
  checkForPulse();

  checkForRotaryTurn();

  checkStartButton();

  checkRotaryButton();

  checkSettingsButtons();
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

void checkStartButton() {
  if (digitalRead(PinBtnStart) == LOW)
  {
    if (!PinBtnStartHeld) {
      StartButtonPressed();
    }
    PinBtnStartHeld = true;
  }
  else {
    PinBtnStartHeld = false;
  }
}


void checkSettingsButtons() {
  for (int i = 0; i < numSettingButtons; i++) {
    if (digitalRead(settingsButtons[i].pin) == LOW)
    {
      if (!settingsButtons[i].buttonDown) {
        Serial.println("!settingsButtons[i].buttonDown");
        Serial.println(settingsButtons[i].name);
        // store timestamp so we can know if long held
        settingsButtons[i].buttonDownStartMs = millis();
        settingsButtons[i].stored = false;
      }
      else if (settingsButtons[i].stored == false && (millis() - settingsButtons[i].buttonDownStartMs) > buttonHoldMs) {
        Serial.println("LOW");
        settingsButtons[i].stored = true;
        settingsButtons[i].bpm = bpm;
        settingsButtons[i].beatsPerBar = beatsPerBar;
        StoreSetting(i);
        ShowSetting(settingsButtons[i].name);
      }
      settingsButtons[i].buttonDown = true;
    }
    else {
      if (settingsButtons[i].buttonDown && !settingsButtons[i].stored) {
        if (bpm != settingsButtons[i].bpm) {
          bpm = settingsButtons[i].bpm;
          UpdateBpm();
          if (lcdSelectedTopField) {
            UpdateRotary();
          }
        }
        if (beatsPerBar != settingsButtons[i].beatsPerBar) {
          Serial.println("update ts");
          beatsPerBar = settingsButtons[i].beatsPerBar;
          UpdateTimeSignature();
          if (!lcdSelectedTopField) {
            UpdateRotary();
          }
        }
      }
      settingsButtons[i].buttonDown = false;
    }
  }
}

void checkRotaryButton() {
  if (digitalRead(PinBtnRotary) == LOW)
  {
    if (!PinBtnRotaryHeld) {
      lcdSelectedTopField = ! lcdSelectedTopField;
      UpdateRotary();      
      UpdateSelectedField();
    }
    PinBtnRotaryHeld = true;
  }
  else {
    PinBtnRotaryHeld = false;
  }
}

void checkForRotaryTurn() {
  if (lcdSelectedTopField) {
    long newBpm;
    newBpm = rotaryKnob.read() / 4;
    if (newBpm != bpm) {
      if(newBpm>=MIN_TEMPO&&newBpm<=MAX_TEMPO){
        bpm = newBpm;
        UpdateBpm();
      }
      else if (lcdSelectedTopField) {
        UpdateRotary();
      }
    }
  } else {
    int newTimeSignature = (rotaryKnob.read() / 4) % 4;
    int newBeatsPerBar = 4;
    switch (newTimeSignature) {
      case 0:
        newBeatsPerBar = 2;
        break;
      case 1:
        newBeatsPerBar = 3;
        break;
      case 2:
        newBeatsPerBar = 4;
        break;
      case 3:
        newBeatsPerBar = 6;
        break;
    }
    if (newBeatsPerBar != beatsPerBar) {
      beatsPerBar = newBeatsPerBar;
      UpdateTimeSignature();
    }
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

void StartButtonPressed()
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

void UpdateRotary(){
  if (lcdSelectedTopField) {
    rotaryKnob.write(bpm * 4);
  }
  else {
    int rotaryPosition = 2; // 4/4 by default
    switch (beatsPerBar) {
      case 2:
        rotaryPosition = 0;
        break;
      case 3:
        rotaryPosition = 1;
        break;
      case 4:
        rotaryPosition = 2;
        break;
      case 6:
        rotaryPosition = 3;
        break;
    }
    rotaryKnob.write(rotaryPosition * 4);
  }
}

void StoreSetting(int setting){
  int eeAddress = (setting*sizeof(int)*2);
  EEPROM.put(eeAddress, settingsButtons[setting].bpm);
  eeAddress+=sizeof(int);
  EEPROM.put(eeAddress, settingsButtons[setting].beatsPerBar);
}

void ReadSetting(int setting){
  int eeAddress = (setting*sizeof(int)*2);
  EEPROM.get(eeAddress, settingsButtons[setting].bpm);
  eeAddress+=sizeof(int);
  EEPROM.get(eeAddress, settingsButtons[setting].beatsPerBar);
  if(settingsButtons[setting].bpm<MIN_TEMPO || settingsButtons[setting].bpm>MAX_TEMPO){
    settingsButtons[setting].bpm = DEFAULT_TEMPO;
    settingsButtons[setting].beatsPerBar = DEFAULT_TS;
  }
}

void UpdateBpm() {

  waitInterval = (microSecondsPerMinute / bpm) / pulsesPerBeat;
  Timer1.setPeriod(waitInterval);
  ShowUpdatedBpm();

  ShowSettingsField();
}

void ShowUpdatedBpm() {
  lcd.setCursor(5, 0);
  // print the number of seconds since reset:
  lcd.print(bpm);
  if (bpm < 100) {
    lcd.print(" ");
  }
}

void ShowSettingsField(){
  String matchingSetting;
  bool matchingSettingFound = false;
  for (int i = 0; i < numSettingButtons; i++) {
    if (bpm == settingsButtons[i].bpm && beatsPerBar == settingsButtons[i].beatsPerBar) {
      matchingSettingFound = true;
      matchingSetting = settingsButtons[i].name;
      break;
    }
  }

  if (matchingSettingFound) {
    ShowSetting(matchingSetting);
  }
  else if (SettingsFieldShowing) {
    ShowSetting("  ");
  }
}

void UpdateTimeSignature() {
  ShowUpdatedTimeSignature();

  ShowSettingsField();
}

void ShowUpdatedTimeSignature() {
  lcd.setCursor(6, 1);
  lcd.print(beatsPerBar);
  lcd.print("/");
  if(beatsPerBar>=6){
    lcd.print("8");
  }else{
    lcd.print("4");
  }
}

void UpdateSelectedField() {
  if (lcdSelectedTopField) {
    lcd.setCursor(0, 0);
    lcd.print(">");
    lcd.setCursor(0, 1);
    lcd.print(" ");
  } else {
    lcd.setCursor(0, 0);
    lcd.print(" ");
    lcd.setCursor(0, 1);
    lcd.print(">");
  }
}

void ShowSetting(String name) {
  lcd.setCursor(14, 0);
  lcd.print(name);
  SettingsFieldShowing = true;
}

void SendMidiCommand(byte command) {
  midiSerial.write(0xFF);
  midiSerial.write(command);
}

