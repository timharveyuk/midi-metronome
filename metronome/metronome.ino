#include "metronome.h"
#include <pitches.h>
#include <Encoder.h>
#include <SendOnlySoftwareSerial.h>
#include <TimerOne.h>
#include <LiquidCrystal.h>


const int PinCLK = 2; // Generating interrupts using CLK signal
const int PinDT = 3;  // Reading DT signal
const int PinBtnRotary = 4;  // Reading Push Button switch

const int PinBtnStart = A3;
const int PinBtnS1 = A2;
const int PinBtnS2 = A1;
const int PinBtnTap = A0;

const int buttonHoldMs = 1000;

const int numSettingButtons = 2;
Settings S1;
Settings S2;
Settings settingsButtons[2];

bool PinBtnStartHeld = false;
bool PinBtnTapHeld = false;
bool PinBtnRotaryHeld = false;

bool SettingsFieldShowing = false;

volatile bool lcdSelectedTopField = true;

Encoder rotaryKnob(PinCLK, PinDT);

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

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
  lcd.setCursor(0, 0);
  lcd.print(">bpm: ");
  lcd.setCursor(0, 1);
  lcd.print(" time: ");
  UpdateTimeSignature();

  mySerial.begin(31250);
  Serial.begin(9200);

  settingsButtons[0].bpm = 80;
  settingsButtons[0].beatsPerBar = 4;
  settingsButtons[0].name = "S1";
  settingsButtons[0].pin = PinBtnS1;
  settingsButtons[1].bpm = 110;
  settingsButtons[1].beatsPerBar = 3;
  settingsButtons[1].name = "S2";
  settingsButtons[1].pin = PinBtnS2;


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
      Reset();
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
        ShowSetting(settingsButtons[i].name);
      }
      settingsButtons[i].buttonDown = true;
      if (settingsButtons[i].buttonDown) {
        Serial.println("yes");
      } else {
        Serial.println("no");
      }
    }
    else {
      if (settingsButtons[i].buttonDown && !settingsButtons[i].stored) {
        Serial.println("2");
        lcd.setCursor(13, 0);
        lcd.print(settingsButtons[i].name);
        if (bpm != settingsButtons[i].bpm) {
          bpm = settingsButtons[i].bpm;
          UpdateBpm();
          if (lcdSelectedTopField) {
            rotaryKnob.write(bpm * 4);
          }
        }
        if (beatsPerBar != settingsButtons[i].beatsPerBar) {
          Serial.println("update ts");
          beatsPerBar = settingsButtons[i].beatsPerBar;
          UpdateTimeSignature();
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
      bpm = newBpm;
      UpdateBpm();
    }
  } else {
    int newTimeSignature = (rotaryKnob.read() / 4) % 4;
    int newBeatsPerBar = 4;
    Serial.println(newTimeSignature);
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
  lcd.setCursor(5, 0);
  // print the number of seconds since reset:
  lcd.print(bpm);
  Serial.println("updated bpm");
  Serial.println(bpm);
  if (bpm < 100) {
    lcd.print(" ");
  }

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
    lcd.setCursor(13, 0);
    lcd.print(matchingSetting);
    SettingsFieldShowing = true;
  }
  else if (SettingsFieldShowing) {
    lcd.setCursor(13, 0);
    lcd.print("  ");
    SettingsFieldShowing = false;
  }
}

void UpdateTimeSignature() {
  lcd.setCursor(6, 1);
  lcd.print(beatsPerBar);
  lcd.print("/4");
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
  lcd.setCursor(13, 0);
  lcd.print(name);
  SettingsFieldShowing = true;
}

void SendMidiCommand(byte command) {
  mySerial.write(0xFF);
  mySerial.write(command);
}

