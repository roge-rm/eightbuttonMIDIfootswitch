/*
 * This is for toggle (latching) switches. If you have momentary switches check out 8buttonfootswitch-momentary
 */

#include <SPI.h>
#include <Wire.h>
#include <Bounce.h>
#include <MIDI.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 //OLED display width, in pixels
#define SCREEN_HEIGHT 64 //OLED display height, in pixels

//Definitions to make writing code quicker
#define BUTTON1 (button1.risingEdge() || button1.fallingEdge())
#define BUTTON2 (button2.risingEdge() || button2.fallingEdge())
#define BUTTON3 (button3.risingEdge() || button3.fallingEdge())
#define BUTTON4 (button4.risingEdge() || button4.fallingEdge())
#define BUTTON5 (button5.risingEdge() || button5.fallingEdge())
#define BUTTON6 (button6.risingEdge() || button6.fallingEdge())
#define BUTTON7 (button7.risingEdge() || button7.fallingEdge())
#define BUTTON8 (button8.risingEdge() || button8.fallingEdge())

//Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 //Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Create MIDI instance for 5 pin MIDI output
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

//Declare pins for footswitches
const byte switchPin[8] = {10, 11, 12, 13, 14, 15, 16, 17};

//Declare pins for LEDs;
const byte ledPin[8] = {32, 33, 34, 35, 28, 29, 30, 31};
const int ledDelay = 500; //How long to light LEDs up for visual confirmation

//Define time before before MIDI/CC OFF note is sent. This is needed because the footswitches are latching but we are pretending they are momentary
long MIDI_NOTEOFF_TIME;
long MIDI_CCOFF_TIME;

//Default channel for MIDI notes, CC, program change, control change
int MIDI_NOTE_CHAN;
int MIDI_CC_CHAN;
int MIDI_PC_CHAN;

//Arrays are declared here but loaded from EEPROM/defaults in eepromREAD()
int switchNote[8];
int switchVel[8];
int switchCC[8];
int switchCCValOn[8];
int switchCCValOff[8];
int switchPC[8];

//Switch states for runmodes that require toggling
bool switchState[8] = {0, 0, 0, 0, 0, 0, 0, 0};

//Create Bounce objects for each button and switch.
//Default debounce time is 5ms
const int DEBOUNCE_TIME = 5;

Bounce button1 = Bounce(switchPin[0], DEBOUNCE_TIME);
Bounce button2 = Bounce(switchPin[1], DEBOUNCE_TIME);
Bounce button3 = Bounce(switchPin[2], DEBOUNCE_TIME);
Bounce button4 = Bounce(switchPin[3], DEBOUNCE_TIME);
Bounce button5 = Bounce(switchPin[4], DEBOUNCE_TIME);
Bounce button6 = Bounce(switchPin[5], DEBOUNCE_TIME);
Bounce button7 = Bounce(switchPin[6], DEBOUNCE_TIME);
Bounce button8 = Bounce(switchPin[7], DEBOUNCE_TIME);

//Track runmode (function of footswitch) as well as pick default option if no change is made before timeout. Default timeout is 6000ms.
//0 = Startup screen (runmode select)
//1 = MIDI Note ON+OFF (timed), 2 = MIDI Note Toggle (must hit switch again to turn off)
//3 = MIDI CC ON+OFF (timed), 4 = MIDI CC Toggle (same as MIDI Note, but with CC)
//5 = Program Change, 8 = Settings
int RUNMODE = 0;
int RUNMODE_DEFAULT; //set by EEPROM
unsigned int RUNMODE_TIMEOUT = 6000;
const int RUNMODE_LONGTIMEOUT = 31000;

//Counter setup for menus requiring timeouts (due to lack of a back button)
unsigned long previousMillis = 0;

//For settings menu navigation
int menuCat[3] = {0, 0, 0};
unsigned long MENU_TIMEOUT;
unsigned long currentMillis = 0;

void setup() {
  eepromREAD(); //Set variable values from EEPROM (or defaults if EEPROM has not been written)

  //Serial.begin(9600); //Debugging

  MIDI.begin();

  //Configure switch pins as for input mode with pullup resistors
  pinMode (switchPin[0], INPUT_PULLUP);
  pinMode (switchPin[1], INPUT_PULLUP);
  pinMode (switchPin[2], INPUT_PULLUP);
  pinMode (switchPin[3], INPUT_PULLUP);
  pinMode (switchPin[4], INPUT_PULLUP);
  pinMode (switchPin[5], INPUT_PULLUP);
  pinMode (switchPin[6], INPUT_PULLUP);
  pinMode (switchPin[7], INPUT_PULLUP);

  //Configure LED pins for output
  pinMode (ledPin[0], OUTPUT);
  pinMode (ledPin[1], OUTPUT);
  pinMode (ledPin[2], OUTPUT);
  pinMode (ledPin[3], OUTPUT);
  pinMode (ledPin[4], OUTPUT);
  pinMode (ledPin[5], OUTPUT);
  pinMode (ledPin[6], OUTPUT);
  pinMode (ledPin[7], OUTPUT);

  //SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { //Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); //Don't proceed, loop forever
  }

  updatebuttons(); //Pull initial button status to prevent phantom MIDI messages from being sent once the loop starts

  display.display();
  delay(500);

  display.clearDisplay();
}

void loop() {
  currentMillis = millis();
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  updatebuttons(); //Poll for button presses

  if (RUNMODE == 0) {
    runmodeSELECTMODE();
  } else if (RUNMODE == 1) {
    runmodeMIDINOTETIMED();
  } else if (RUNMODE == 2) {
    runmodeMIDINOTETOGGLE();
  } else if (RUNMODE == 3) {
    runmodeCCTIMED();
  } else if (RUNMODE == 4) {
    runmodeCCTOGGLE();
  } else if (RUNMODE == 5) {
    runmodePROGRAMCHANGE();
  } else if (RUNMODE == 8) {
    runmodeSETTINGS();
  }

  //Break back to mode select. Set timeout to 31 seconds to give user more time for a selection
  if (BUTTON4 && BUTTON8) {
    RUNMODE = 0;
    RUNMODE_TIMEOUT = RUNMODE_LONGTIMEOUT;
    resetSWITCHES();
    resetMENU();
    previousMillis = currentMillis;
  }

  while (usbMIDI.read()) //Ignore incoming MIDI
  {
  }
}

void updatebuttons() {
  button1.update();
  button2.update();
  button3.update();
  button4.update();
  button5.update();
  button6.update();
  button7.update();
  button8.update();
}

void runmodeSELECTMODE() { //Give choice between running modes, choose default mode after timeout if no option selected
  //unsigned long currentMillis = millis();
  long timeOut = (RUNMODE_TIMEOUT / 1000 - ((currentMillis - previousMillis) / 1000));

  display.setTextColor(WHITE, BLACK);
  display.setTextSize(1);
  display.setCursor(10, 34);
  display.println(F("CHOOSE RUNMODE"));
  display.setCursor(100, 34);
  display.println(timeOut);

  display.setTextSize(2);
  display.setCursor(10, 50);
  if (RUNMODE_DEFAULT == 1) {
    display.setTextColor(BLACK, WHITE);
  } else display.setTextColor(WHITE, BLACK);
  display.println(F("1"));
  display.setCursor(40, 50);
  if (RUNMODE_DEFAULT == 2) {
    display.setTextColor(BLACK, WHITE);
  } else display.setTextColor(WHITE, BLACK);
  display.println(F("2"));
  display.setCursor(70, 50);
  if (RUNMODE_DEFAULT == 3) {
    display.setTextColor(BLACK, WHITE);
  } else display.setTextColor(WHITE, BLACK);
  display.println(F("3"));
  display.setCursor(100, 50);
  if (RUNMODE_DEFAULT == 4) {
    display.setTextColor(BLACK, WHITE);
  } else display.setTextColor(WHITE, BLACK);
  display.println(F("4"));
  display.setCursor(10, 10);
  if (RUNMODE_DEFAULT == 5) {
    display.setTextColor(BLACK, WHITE);
  } else display.setTextColor(WHITE, BLACK);
  display.println(F("5"));
  display.setCursor(100, 10);
  display.setTextColor(WHITE, BLACK);
  display.println(F("8"));

  if BUTTON1 {
  RUNMODE = 1;
  blinkLED(1);
    previousMillis = currentMillis;
  }
  if BUTTON2 {
  RUNMODE = 2;
  blinkLED(2);
    previousMillis = currentMillis;
  }
  if BUTTON3 {
  RUNMODE = 3;
  blinkLED(3);
    previousMillis = currentMillis;
  }
  if BUTTON4 {
  RUNMODE = 4;
  blinkLED(4);
    previousMillis = currentMillis;
  }
  if BUTTON5 {
  RUNMODE = 5;
  blinkLED(5);
    previousMillis = currentMillis;
  }
  if BUTTON8 {
  RUNMODE = 8;
  blinkLED(8);
    previousMillis = currentMillis;
  }

  if (currentMillis - previousMillis >= RUNMODE_TIMEOUT) {
    RUNMODE = RUNMODE_DEFAULT;
    blinkLED(RUNMODE_DEFAULT);
    previousMillis = currentMillis;
  }

  display.setTextColor(WHITE, BLACK);
  display.display();
}

void runmodeMIDINOTETIMED() {
  if BUTTON1 {
  display.setCursor(10, 50);
    display.println(switchNote[0]);
    digitalWrite(ledPin[0], HIGH);
    usbMIDI.sendNoteOn(switchNote[0], switchVel[0], MIDI_NOTE_CHAN);
    MIDI.sendNoteOn(switchNote[0], switchVel[0], MIDI_NOTE_CHAN);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote[0], 0, MIDI_NOTE_CHAN);
    MIDI.sendNoteOff(switchNote[0], 0, MIDI_NOTE_CHAN);
    digitalWrite(ledPin[0], LOW);
  }

  if BUTTON2 {
  display.setCursor(40, 50);
    display.println(switchNote[1]);
    digitalWrite(ledPin[1], HIGH);
    usbMIDI.sendNoteOn(switchNote[1], switchVel[1], MIDI_NOTE_CHAN);
    MIDI.sendNoteOn(switchNote[1], switchVel[1], MIDI_NOTE_CHAN);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote[1], 0, MIDI_NOTE_CHAN);
    MIDI.sendNoteOff(switchNote[1], 0, MIDI_NOTE_CHAN);
    digitalWrite(ledPin[1], LOW);
  }

  if BUTTON3 {
  display.setCursor(70, 50);
    display.println(switchNote[2]);
    digitalWrite(ledPin[2], HIGH);
    usbMIDI.sendNoteOn(switchNote[2], switchVel[2], MIDI_NOTE_CHAN);
    MIDI.sendNoteOn(switchNote[2], switchVel[2], MIDI_NOTE_CHAN);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote[2], 0, MIDI_NOTE_CHAN);
    MIDI.sendNoteOff(switchNote[2], 0, MIDI_NOTE_CHAN);
    digitalWrite(ledPin[2], LOW);
  }

  if BUTTON4 {
  display.setCursor(100, 50);
    display.println(switchNote[3]);
    digitalWrite(ledPin[3], HIGH);
    usbMIDI.sendNoteOn(switchNote[3], switchVel[3], MIDI_NOTE_CHAN);
    MIDI.sendNoteOn(switchNote[3], switchVel[3], MIDI_NOTE_CHAN);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote[3], 0, MIDI_NOTE_CHAN);
    MIDI.sendNoteOff(switchNote[3], 0, MIDI_NOTE_CHAN);
    digitalWrite(ledPin[3], LOW);
  }

  if BUTTON5 {
  display.setCursor(10, 10);
    display.println(switchNote[4]);
    digitalWrite(ledPin[4], HIGH);
    usbMIDI.sendNoteOn(switchNote[4], switchVel[4], MIDI_NOTE_CHAN);
    MIDI.sendNoteOn(switchNote[4], switchVel[4], MIDI_NOTE_CHAN);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote[4], 0, MIDI_NOTE_CHAN);
    MIDI.sendNoteOff(switchNote[4], 0, MIDI_NOTE_CHAN);
    digitalWrite(ledPin[4], LOW);
  }

  if BUTTON6 {
  display.setCursor(40, 10);
    display.println(switchNote[5]);
    digitalWrite(ledPin[5], HIGH);
    usbMIDI.sendNoteOn(switchNote[5], switchVel[5], MIDI_NOTE_CHAN);
    MIDI.sendNoteOn(switchNote[5], switchVel[5], MIDI_NOTE_CHAN);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote[5], 0, MIDI_NOTE_CHAN);
    MIDI.sendNoteOff(switchNote[5], 0, MIDI_NOTE_CHAN);
    digitalWrite(ledPin[5], LOW);
  }

  if BUTTON7 {
  display.setCursor(70, 10);
    display.println(switchNote[6]);
    digitalWrite(ledPin[6], HIGH);
    usbMIDI.sendNoteOn(switchNote[6], switchVel[6], MIDI_NOTE_CHAN);
    MIDI.sendNoteOn(switchNote[6], switchVel[6], MIDI_NOTE_CHAN);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote[6], 0, MIDI_NOTE_CHAN);
    MIDI.sendNoteOff(switchNote[6], 0, MIDI_NOTE_CHAN);
    digitalWrite(ledPin[6], LOW);
  }

  if BUTTON8 {
  display.setCursor(100, 10);
    display.println(switchNote[7]);
    digitalWrite(ledPin[7], HIGH);
    usbMIDI.sendNoteOn(switchNote[7], switchVel[7], MIDI_NOTE_CHAN);
    MIDI.sendNoteOn(switchNote[7], switchVel[7], MIDI_NOTE_CHAN);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote[7], 0, MIDI_NOTE_CHAN);
    MIDI.sendNoteOff(switchNote[7], 0, MIDI_NOTE_CHAN);
    digitalWrite(ledPin[7], LOW);
  }

  display.display();
}

void runmodeMIDINOTETOGGLE() {
  if BUTTON1 {
  display.setCursor(10, 50);
    display.println(switchNote[0]);
    if (!switchState[0]) {
      digitalWrite(ledPin[0], HIGH);
      usbMIDI.sendNoteOn(switchNote[0], switchVel[0], MIDI_NOTE_CHAN);
      MIDI.sendNoteOn(switchNote[0], switchVel[0], MIDI_NOTE_CHAN);
      switchState[0] = 1;
    } else {
      usbMIDI.sendNoteOff(switchNote[0], 0, MIDI_NOTE_CHAN);
      MIDI.sendNoteOff(switchNote[0], 0, MIDI_NOTE_CHAN);
      digitalWrite(ledPin[0], LOW);
      switchState[0] = 0;
    }
  }

  if BUTTON2 {
  display.setCursor(40, 50);
    display.println(switchNote[1]);
    if (!switchState[1]) {
      digitalWrite(ledPin[1], HIGH);
      usbMIDI.sendNoteOn(switchNote[1], switchVel[1], MIDI_NOTE_CHAN);
      MIDI.sendNoteOn(switchNote[1], switchVel[1], MIDI_NOTE_CHAN);
      switchState[1] = 1;
    } else {
      usbMIDI.sendNoteOff(switchNote[1], 0, MIDI_NOTE_CHAN);
      MIDI.sendNoteOff(switchNote[1], 0, MIDI_NOTE_CHAN);
      digitalWrite(ledPin[1], LOW);
      switchState[1] = 0;
    }
  }

  if BUTTON3 {
  display.setCursor(70, 50);
    display.println(switchNote[2]);
    if (!switchState[2]) {
      digitalWrite(ledPin[2], HIGH);
      usbMIDI.sendNoteOn(switchNote[2], switchVel[2], MIDI_NOTE_CHAN);
      MIDI.sendNoteOn(switchNote[2], switchVel[2], MIDI_NOTE_CHAN);
      switchState[2] = 1;
    } else {
      usbMIDI.sendNoteOff(switchNote[2], 0, MIDI_NOTE_CHAN);
      MIDI.sendNoteOff(switchNote[2], 0, MIDI_NOTE_CHAN);
      digitalWrite(ledPin[2], LOW);
      switchState[2] = 0;
    }
  }

  if BUTTON4 {
  display.setCursor(100, 50);
    display.println(switchNote[3]);
    if (!switchState[3]) {
      digitalWrite(ledPin[3], HIGH);
      usbMIDI.sendNoteOn(switchNote[3], switchVel[3], MIDI_NOTE_CHAN);
      MIDI.sendNoteOn(switchNote[3], switchVel[3], MIDI_NOTE_CHAN);
      switchState[3] = 1;
    } else {
      usbMIDI.sendNoteOff(switchNote[3], 0, MIDI_NOTE_CHAN);
      MIDI.sendNoteOff(switchNote[3], 0, MIDI_NOTE_CHAN);
      digitalWrite(ledPin[3], LOW);
      switchState[3] = 0;
    }
  }

  if BUTTON5 {
  display.setCursor(10, 10);
    display.println(switchNote[4]);
    if (!switchState[4]) {
      digitalWrite(ledPin[4], HIGH);
      usbMIDI.sendNoteOn(switchNote[4], switchVel[4], MIDI_NOTE_CHAN);
      MIDI.sendNoteOn(switchNote[4], switchVel[4], MIDI_NOTE_CHAN);
      switchState[4] = 1;
    } else {
      usbMIDI.sendNoteOff(switchNote[4], 0, MIDI_NOTE_CHAN);
      MIDI.sendNoteOff(switchNote[4], 0, MIDI_NOTE_CHAN);
      digitalWrite(ledPin[4], LOW);
      switchState[4] = 0;
    }
  }

  if BUTTON6 {
  display.setCursor(40, 10);
    display.println(switchNote[5]);
    if (!switchState[5]) {
      digitalWrite(ledPin[5], HIGH);
      usbMIDI.sendNoteOn(switchNote[5], switchVel[5], MIDI_NOTE_CHAN);
      MIDI.sendNoteOn(switchNote[5], switchVel[5], MIDI_NOTE_CHAN);
      switchState[5] = 1;
    } else {
      usbMIDI.sendNoteOff(switchNote[5], 0, MIDI_NOTE_CHAN);
      MIDI.sendNoteOff(switchNote[5], 0, MIDI_NOTE_CHAN);
      digitalWrite(ledPin[5], LOW);
      switchState[5] = 0;
    }
  }

  if BUTTON7 {
  display.setCursor(70, 10);
    display.println(switchNote[6]);
    if (!switchState[6]) {
      digitalWrite(ledPin[6], HIGH);
      usbMIDI.sendNoteOn(switchNote[6], switchVel[6], MIDI_NOTE_CHAN);
      MIDI.sendNoteOn(switchNote[6], switchVel[6], MIDI_NOTE_CHAN);
      switchState[6] = 1;
    } else {
      usbMIDI.sendNoteOff(switchNote[6], 0, MIDI_NOTE_CHAN);
      MIDI.sendNoteOff(switchNote[6], 0, MIDI_NOTE_CHAN);
      digitalWrite(ledPin[6], LOW);
      switchState[6] = 0;
    }
  }

  if BUTTON8 {
  display.setCursor(100, 10);
    display.println(switchNote[7]);
    if (!switchState[7]) {
      digitalWrite(ledPin[7], HIGH);
      usbMIDI.sendNoteOn(switchNote[7], switchVel[7], MIDI_NOTE_CHAN);
      MIDI.sendNoteOn(switchNote[7], switchVel[7], MIDI_NOTE_CHAN);
      switchState[7] = 1;
    } else {
      usbMIDI.sendNoteOff(switchNote[7], 0, MIDI_NOTE_CHAN);
      MIDI.sendNoteOff(switchNote[7], 0, MIDI_NOTE_CHAN);
      digitalWrite(ledPin[7], LOW);
      switchState[7] = 0;
    }
  }

  display.display();
}

void runmodeCCTIMED() {
  if BUTTON1 {
  display.setCursor(10, 50);
    display.println(switchCC[0]);
    digitalWrite(ledPin[0], HIGH);
    usbMIDI.sendControlChange(switchCC[0], switchCCValOn[0], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[0], switchCCValOn[0], MIDI_CC_CHAN);
    delay(MIDI_CCOFF_TIME);
    usbMIDI.sendControlChange(switchCC[0], switchCCValOff[0], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[0], switchCCValOff[0], MIDI_CC_CHAN);
    digitalWrite(ledPin[0], LOW);
  }

  if BUTTON2 {
  display.setCursor(40, 50);
    display.println(switchCC[1]);
    digitalWrite(ledPin[1], HIGH);
    usbMIDI.sendControlChange(switchCC[1], switchCCValOn[1], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[1], switchCCValOn[1], MIDI_CC_CHAN);
    delay(MIDI_CCOFF_TIME);
    usbMIDI.sendControlChange(switchCC[1], switchCCValOff[1], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[1], switchCCValOff[1], MIDI_CC_CHAN);
    digitalWrite(ledPin[1], LOW);
  }

  if BUTTON3 {
  display.setCursor(70, 50);
    display.println(switchCC[2]);
    digitalWrite(ledPin[2], HIGH);
    usbMIDI.sendControlChange(switchCC[2], switchCCValOn[2], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[2], switchCCValOn[2], MIDI_CC_CHAN);
    delay(MIDI_CCOFF_TIME);
    usbMIDI.sendControlChange(switchCC[2], switchCCValOff[2], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[2], switchCCValOff[2], MIDI_CC_CHAN);
    digitalWrite(ledPin[2], LOW);
  }

  if BUTTON4 {
  display.setCursor(100, 50);
    display.println(switchCC[3]);
    digitalWrite(ledPin[3], HIGH);
    usbMIDI.sendControlChange(switchCC[3], switchCCValOn[3], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[3], switchCCValOn[3], MIDI_CC_CHAN);
    delay(MIDI_CCOFF_TIME);
    usbMIDI.sendControlChange(switchCC[3], switchCCValOff[3], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[3], switchCCValOff[3], MIDI_CC_CHAN);
    digitalWrite(ledPin[3], LOW);
  }

  if BUTTON5 {
  display.setCursor(10, 10);
    display.println(switchCC[4]);
    digitalWrite(ledPin[4], HIGH);
    usbMIDI.sendControlChange(switchCC[4], switchCCValOn[4], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[4], switchCCValOn[4], MIDI_CC_CHAN);
    delay(MIDI_CCOFF_TIME);
    usbMIDI.sendControlChange(switchCC[4], switchCCValOff[4], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[4], switchCCValOff[4], MIDI_CC_CHAN);
    digitalWrite(ledPin[4], LOW);
  }

  if BUTTON6 {
  display.setCursor(40, 10);
    display.println(switchCC[5]);
    digitalWrite(ledPin[5], HIGH);
    usbMIDI.sendControlChange(switchCC[5], switchCCValOn[5], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[5], switchCCValOn[5], MIDI_CC_CHAN);
    delay(MIDI_CCOFF_TIME);
    usbMIDI.sendControlChange(switchCC[5], switchCCValOff[5], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[5], switchCCValOff[5], MIDI_CC_CHAN);
    digitalWrite(ledPin[5], LOW);
  }

  if BUTTON7 {
  display.setCursor(70, 10);
    display.println(switchCC[6]);
    digitalWrite(ledPin[6], HIGH);
    usbMIDI.sendControlChange(switchCC[6], switchCCValOn[6], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[6], switchCCValOn[6], MIDI_CC_CHAN);
    delay(MIDI_CCOFF_TIME);
    usbMIDI.sendControlChange(switchCC[6], switchCCValOff[6], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[6], switchCCValOff[6], MIDI_CC_CHAN);
    digitalWrite(ledPin[6], LOW);
  }

  if BUTTON8 {
  display.setCursor(100, 10);
    display.println(switchCC[7]);
    digitalWrite(ledPin[7], HIGH);
    usbMIDI.sendControlChange(switchCC[7], switchCCValOn[7], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[7], switchCCValOn[7], MIDI_CC_CHAN);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendControlChange(switchCC[7], switchCCValOff[7], MIDI_CC_CHAN);
    MIDI.sendControlChange(switchCC[7], switchCCValOff[7], MIDI_CC_CHAN);
    digitalWrite(ledPin[7], LOW);
  }

  display.display();
}


void runmodeCCTOGGLE() {
  if BUTTON1 {
  display.setCursor(10, 50);
    display.println(switchCC[0]);
    if (!switchState[0]) {
      digitalWrite(ledPin[0], HIGH);
      usbMIDI.sendControlChange(switchCC[0], switchCCValOn[0], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[0], switchCCValOn[0], MIDI_CC_CHAN);
      switchState[0] = 1;
    } else {
      usbMIDI.sendControlChange(switchCC[0], switchCCValOff[0], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[0], switchCCValOff[0], MIDI_CC_CHAN);
      digitalWrite(ledPin[0], LOW);
      switchState[0] = 0;
    }
  }

  if BUTTON2 {
  display.setCursor(40, 50);
    display.println(switchCC[1]);
    if (!switchState[1]) {
      digitalWrite(ledPin[1], HIGH);
      usbMIDI.sendControlChange(switchCC[1], switchCCValOn[1], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[1], switchCCValOn[1], MIDI_CC_CHAN);
      switchState[1] = 1;
    } else {
      usbMIDI.sendControlChange(switchCC[1], switchCCValOff[1], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[1], switchCCValOff[1], MIDI_CC_CHAN);
      digitalWrite(ledPin[1], LOW);
      switchState[1] = 0;
    }
  }

  if BUTTON3 {
  display.setCursor(70, 50);
    display.println(switchCC[2]);
    if (!switchState[2]) {
      digitalWrite(ledPin[2], HIGH);
      usbMIDI.sendControlChange(switchCC[2], switchCCValOn[2], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[2], switchCCValOn[2], MIDI_CC_CHAN);
      switchState[2] = 1;
    } else {
      usbMIDI.sendControlChange(switchCC[2], switchCCValOff[2], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[2], switchCCValOff[2], MIDI_CC_CHAN);
      digitalWrite(ledPin[2], LOW);
      switchState[2] = 0;
    }
  }

  if BUTTON4 {
  display.setCursor(100, 50);
    display.println(switchCC[3]);
    if (!switchState[3]) {
      digitalWrite(ledPin[3], HIGH);
      usbMIDI.sendControlChange(switchCC[3], switchCCValOn[3], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[3], switchCCValOn[3], MIDI_CC_CHAN);
      switchState[3] = 1;
    } else {
      usbMIDI.sendControlChange(switchCC[3], switchCCValOff[3], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[3], switchCCValOff[3], MIDI_CC_CHAN);
      digitalWrite(ledPin[3], LOW);
      switchState[3] = 0;
    }
  }

  if BUTTON5 {
  display.setCursor(10, 10);
    display.println(switchCC[4]);
    if (!switchState[4]) {
      digitalWrite(ledPin[4], HIGH);
      usbMIDI.sendControlChange(switchCC[4], switchCCValOn[4], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[4], switchCCValOn[4], MIDI_CC_CHAN);
      switchState[4] = 1;
    } else {
      usbMIDI.sendControlChange(switchCC[4], switchCCValOff[4], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[4], switchCCValOff[4], MIDI_CC_CHAN);
      digitalWrite(ledPin[4], LOW);
      switchState[4] = 0;
    }
  }

  if BUTTON6 {
  display.setCursor(40, 10);
    display.println(switchCC[5]);
    if (!switchState[5]) {
      digitalWrite(ledPin[5], HIGH);
      usbMIDI.sendControlChange(switchCC[5], switchCCValOn[5], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[5], switchCCValOn[5], MIDI_CC_CHAN);
      switchState[5] = 1;
    } else {
      usbMIDI.sendControlChange(switchCC[5], switchCCValOff[5], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[5], switchCCValOff[5], MIDI_CC_CHAN);
      digitalWrite(ledPin[5], LOW);
      switchState[5] = 0;
    }
  }

  if BUTTON7 {
  display.setCursor(70, 10);
    display.println(switchCC[6]);
    if (!switchState[6]) {
      digitalWrite(ledPin[6], HIGH);
      usbMIDI.sendControlChange(switchCC[6], switchCCValOn[6], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[6], switchCCValOn[6], MIDI_CC_CHAN);
      switchState[6] = 1;
    } else {
      usbMIDI.sendControlChange(switchCC[6], switchCCValOff[6], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[6], switchCCValOff[6], MIDI_CC_CHAN);
      digitalWrite(ledPin[6], LOW);
      switchState[6] = 0;
    }
  }

  if BUTTON8 {
  display.setCursor(100, 10);
    display.println(switchCC[7]);
    if (!switchState[7]) {
      digitalWrite(ledPin[7], HIGH);
      usbMIDI.sendControlChange(switchCC[7], switchCCValOn[7], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[7], switchCCValOn[7], MIDI_CC_CHAN);
      switchState[7] = 1;
    } else {
      usbMIDI.sendControlChange(switchCC[7], switchCCValOff[7], MIDI_CC_CHAN);
      MIDI.sendControlChange(switchCC[7], switchCCValOff[7], MIDI_CC_CHAN);
      digitalWrite(ledPin[7], LOW);
      switchState[7] = 0;
    }
  }

  display.display();
}


void runmodePROGRAMCHANGE() {
  if BUTTON1 {
  display.setCursor(10, 50);
    display.println(switchPC[0]);
    if (switchState[0] != 1) {
      resetSWITCHES();
      switchState[0] = 1;
      digitalWrite(ledPin[0], HIGH);
    }
    usbMIDI.sendProgramChange(switchPC[0], MIDI_PC_CHAN);
    MIDI.sendProgramChange(switchPC[0], MIDI_PC_CHAN);
  }

  if BUTTON2 {
  display.setCursor(40, 50);
    display.println(switchPC[1]);
    if (switchState[1] != 1) {
      resetSWITCHES();
      switchState[1] = 1;
      digitalWrite(ledPin[1], HIGH);
    }
    usbMIDI.sendProgramChange(switchPC[1], MIDI_PC_CHAN);
    MIDI.sendProgramChange(switchPC[1], MIDI_PC_CHAN);
  }

  if BUTTON3 {
  display.setCursor(70, 50);
    display.println(switchPC[2]);
    if (switchState[2] != 1) {
      resetSWITCHES();
      switchState[1] = 1;
      digitalWrite(ledPin[2], HIGH);
    }
    usbMIDI.sendProgramChange(switchPC[2], MIDI_PC_CHAN);
    MIDI.sendProgramChange(switchPC[2], MIDI_PC_CHAN);
  }

  if BUTTON4 {
  display.setCursor(100, 50);
    display.println(switchPC[3]);
    if (switchState[3] != 1) {
      resetSWITCHES();
      switchState[3] = 1;
      digitalWrite(ledPin[3], HIGH);
    }
    usbMIDI.sendProgramChange(switchPC[3], MIDI_PC_CHAN);
    MIDI.sendProgramChange(switchPC[3], MIDI_PC_CHAN);
  }

  if BUTTON5 {
  display.setCursor(10, 10);
    display.println(switchPC[4]);
    if (switchState[4] != 1) {
      resetSWITCHES();
      switchState[4] = 1;
      digitalWrite(ledPin[4], HIGH);
    }
    usbMIDI.sendProgramChange(switchPC[4], MIDI_PC_CHAN);
    MIDI.sendProgramChange(switchPC[4], MIDI_PC_CHAN);
  }

  if BUTTON6 {
  display.setCursor(40, 10);
    display.println(switchPC[5]);
    if (switchState[5] != 1) {
      resetSWITCHES();
      switchState[5] = 1;
      digitalWrite(ledPin[5], HIGH);
    }
    usbMIDI.sendProgramChange(switchPC[5], MIDI_PC_CHAN);
    MIDI.sendProgramChange(switchPC[5], MIDI_PC_CHAN);
  }

  if BUTTON7 {
  display.setCursor(70, 10);
    display.println(switchPC[6]);
    if (switchState[6] != 1) {
      resetSWITCHES();
      switchState[6] = 1;
      digitalWrite(ledPin[6], HIGH);
    }
    usbMIDI.sendProgramChange(switchPC[6], MIDI_PC_CHAN);
    MIDI.sendProgramChange(switchPC[6], MIDI_PC_CHAN);
  }

  if BUTTON8 {
  display.setCursor(100, 10);
    display.println(switchPC[7]);
    if (switchState[7] != 1) {
      resetSWITCHES();
      switchState[7] = 1;
      digitalWrite(ledPin[7], HIGH);
    }
    usbMIDI.sendProgramChange(switchPC[7], MIDI_PC_CHAN);
    MIDI.sendProgramChange(switchPC[7], MIDI_PC_CHAN);
  }

  display.display();
}

void resetSWITCHES() {
  for (byte i = 0; i < 8; i++) { //Set all switch states to off
    switchState[i] = 0;
  }
  for (byte i = 0; i < 8; i++) { //Turn off LEDs
    digitalWrite(ledPin[i], LOW);
  }
}

void runmodeSETTINGS() {
  display.setTextColor(WHITE, BLACK);
  display.setTextSize(1);

  if (menuCat[0] == 0) menuLEVEL0(); //Display top level menu
  else if (menuCat[0] == 1) { //Default channels submenu
    menuLEVEL1();
  } else if (menuCat[0] == 2) { //Notes submenu
    menuLEVEL2();
  } else if (menuCat[0] == 3) { //CC submenu
    menuLEVEL3();
  } else if (menuCat[0] == 4) { //PC submenu
    menuLEVEL4();
  } else if (menuCat[0] == 5) { //Default runmode,timeouts
    menuLEVEL5();
  }

  //serialUPDATE(); //Debugging
  display.display();
}

void menuLEVEL0() { //Top level
  displayTEXT(0, "CHANGE SETTINGS");
  displayTEXT(1, "CHAN");
  displayTEXT(2, "NOTE");
  displayTEXT(3, " CC");
  displayTEXT(4, " PC");
  displayTEXT(5, "DEF");
  displayTEXT(6, "LOAD");
  displayTEXT(7, "SAVE");
  displayTEXT(8, "EXIT");

  if BUTTON1 {
  blinkLED(1);
    menuCat[0] = 1;
  }
  if BUTTON2 {
  blinkLED(2);
    menuCat[0] = 2;
  }
  if BUTTON3 {
  blinkLED(3);
    menuCat[0] = 3;
  }
  if BUTTON4 {
  blinkLED(4);
    menuCat[0] = 4;
  }
  if BUTTON5 {
  blinkLED(5);
    menuCat[0] = 5;
  }
  if BUTTON6 {
  blinkLED(6);
    eepromREAD();
    display.clearDisplay();
    displayTEXT(9, "LOADED FROM EEPROM");
    display.display();
    delay(2000);
    //serialUPDATE();
  }
  if BUTTON7 {
  blinkLED(7);
    eepromUPDATE();
    display.clearDisplay();
    displayTEXT(0, "SAVED TO EEPROM");
    display.display();
    delay(2000);
    //serialUPDATE();
  }
  if BUTTON8 {
  blinkLED(8);
    if (menuCat[0] == 0) {
      RUNMODE = 0;
      RUNMODE_TIMEOUT = RUNMODE_LONGTIMEOUT;
      resetSWITCHES();
      resetMENU();
      previousMillis = currentMillis;
    }
  }
}

void menuLEVEL1() { //Note edit submenu
  if (menuCat[1] > 0) {
    displayTEXT(1, "+1");
    displayTEXT(2, "+10");
    displayTEXT(5, "-1");
    displayTEXT(6, "-10");

    if BUTTON1 {
    if (menuCat[1] == 1) MIDI_NOTE_CHAN++;
      if (menuCat[1] == 2) MIDI_CC_CHAN++;
      if (menuCat[1] == 3) MIDI_PC_CHAN++;
    }
    else if BUTTON2 {
    if (menuCat[1] == 1) MIDI_NOTE_CHAN += 10;
      if (menuCat[1] == 2) MIDI_CC_CHAN += 10;
      if (menuCat[1] == 3) MIDI_PC_CHAN += 10;
    }
    else if BUTTON5 {
    if (menuCat[1] == 1) MIDI_NOTE_CHAN--;
      if (menuCat[1] == 2) MIDI_CC_CHAN--;
      if (menuCat[1] == 3) MIDI_PC_CHAN--;
    }
    else if BUTTON6 {
    if (menuCat[1] == 1) MIDI_NOTE_CHAN -= 10;
      if (menuCat[1] == 2) MIDI_CC_CHAN -= 10;
      if (menuCat[1] == 3) MIDI_PC_CHAN -= 10;
    }

    valueCHECK();
  }

  if (menuCat[1] == 1) {
    displayTEXT(0, "NOTE CHAN:");
    displayVALUE(0, MIDI_NOTE_CHAN);
  }
  else if (menuCat[1] == 2) {
    displayTEXT(0, "CC CHAN:");
    displayVALUE(0, MIDI_CC_CHAN);
  }
  else if (menuCat[1] == 3) {
    displayTEXT(0, "PC CHAN:");
    displayVALUE(0, MIDI_PC_CHAN);
  }
  else if (menuCat[1] < 1) {
    displayTEXT(0, "CHANGE DEF CHAN");
    displayTEXT(1, "NOTE");
    displayTEXT(2, " CC");
    displayTEXT(3, " PC");

    if BUTTON1 {
    blinkLED(1);
      menuCat[1] = 1;
    }
    if BUTTON2 {
    blinkLED(2);
      menuCat[1] = 2;
    }
    if BUTTON3 {
    blinkLED(3);
      menuCat[1] = 3;
    }

  }

  displayTEXT(8, "BACK");
  if BUTTON8 {
  blinkLED(8);
    if (menuCat[1] < 1) resetMENU();
    else menuCat[1] = 0;
    previousMillis = currentMillis;
  }
}

void menuLEVEL2() { //Note edit submenu
  if (menuCat[1] > 0) {
    if (menuCat[2] > 0) {
      displayTEXT(1, "+1");
      displayTEXT(2, "+10");
      displayTEXT(5, "-1");
      displayTEXT(6, "-10");

      if BUTTON1 {
      if (menuCat[2] == 1) {
          if (menuCat[1] == 1) switchNote[0]++;
          if (menuCat[1] == 2) switchNote[1]++;
          if (menuCat[1] == 3) switchNote[2]++;
          if (menuCat[1] == 4) switchNote[3]++;
          if (menuCat[1] == 5) switchNote[4]++;
          if (menuCat[1] == 6) switchNote[5]++;
          if (menuCat[1] == 7) switchNote[6]++;
          if (menuCat[1] == 8) switchNote[7]++;
        }
        else if (menuCat[2] == 2) {
          if (menuCat[1] == 1) switchVel[0]++;
          if (menuCat[1] == 2) switchVel[1]++;
          if (menuCat[1] == 3) switchVel[2]++;
          if (menuCat[1] == 4) switchVel[3]++;
          if (menuCat[1] == 5) switchVel[4]++;
          if (menuCat[1] == 6) switchVel[5]++;
          if (menuCat[1] == 7) switchVel[6]++;
          if (menuCat[1] == 8) switchVel[7]++;
        }
      }
      else if BUTTON2 {
      if (menuCat[2] == 1) {
          if (menuCat[1] == 1) switchNote[0] += 10;
          if (menuCat[1] == 2) switchNote[1] += 10;
          if (menuCat[1] == 3) switchNote[2] += 10;
          if (menuCat[1] == 4) switchNote[3] += 10;
          if (menuCat[1] == 5) switchNote[4] += 10;
          if (menuCat[1] == 6) switchNote[5] += 10;
          if (menuCat[1] == 7) switchNote[6] += 10;
          if (menuCat[1] == 8) switchNote[7] += 10;
        }
        else if (menuCat[2] == 2) {
          if (menuCat[1] == 1) switchVel[0] += 10;
          if (menuCat[1] == 2) switchVel[1] += 10;
          if (menuCat[1] == 3) switchVel[2] += 10;
          if (menuCat[1] == 4) switchVel[3] += 10;
          if (menuCat[1] == 5) switchVel[4] += 10;
          if (menuCat[1] == 6) switchVel[5] += 10;
          if (menuCat[1] == 7) switchVel[6] += 10;
          if (menuCat[1] == 8) switchVel[7] += 10;
        }
      }
      else if BUTTON5 {
      if (menuCat[2] == 1) {
          if (menuCat[1] == 1) switchNote[0]--;
          if (menuCat[1] == 2) switchNote[1]--;
          if (menuCat[1] == 3) switchNote[2]--;
          if (menuCat[1] == 4) switchNote[3]--;
          if (menuCat[1] == 5) switchNote[4]--;
          if (menuCat[1] == 6) switchNote[5]--;
          if (menuCat[1] == 7) switchNote[6]--;
          if (menuCat[1] == 8) switchNote[7]--;
        }
        else if (menuCat[2] == 2) {
          if (menuCat[1] == 1) switchVel[0]--;
          if (menuCat[1] == 2) switchVel[1]--;
          if (menuCat[1] == 3) switchVel[2]--;
          if (menuCat[1] == 4) switchVel[3]--;
          if (menuCat[1] == 5) switchVel[4]--;
          if (menuCat[1] == 6) switchVel[5]--;
          if (menuCat[1] == 7) switchVel[6]--;
          if (menuCat[1] == 8) switchVel[7]--;
        }
      }
      else if BUTTON6 {
      if (menuCat[2] == 1) {
          if (menuCat[1] == 1) switchNote[0] -= 10;
          if (menuCat[1] == 2) switchNote[1] -= 10;
          if (menuCat[1] == 3) switchNote[2] -= 10;
          if (menuCat[1] == 4) switchNote[3] -= 10;
          if (menuCat[1] == 5) switchNote[4] -= 10;
          if (menuCat[1] == 6) switchNote[5] -= 10;
          if (menuCat[1] == 7) switchNote[6] -= 10;
          if (menuCat[1] == 8) switchNote[7] -= 10;
        }
        else if (menuCat[2] == 2) {
          if (menuCat[1] == 1) switchVel[0] -= 10;
          if (menuCat[1] == 2) switchVel[1] -= 10;
          if (menuCat[1] == 3) switchVel[2] -= 10;
          if (menuCat[1] == 4) switchVel[3] -= 10;
          if (menuCat[1] == 5) switchVel[4] -= 10;
          if (menuCat[1] == 6) switchVel[5] -= 10;
          if (menuCat[1] == 7) switchVel[6] -= 10;
          if (menuCat[1] == 8) switchVel[7] -= 10;
        }
      }
      valueCHECK();
    }
  }

  if (menuCat[1] < 1) {
    if (menuCat[2] < 1) {
      currentMillis = millis();
      long timeOut = (MENU_TIMEOUT / 1000 - ((currentMillis - previousMillis) / 1000));

      displayTEXT(16, "CHANGE BTN NOTE");
      displayTEXT(13, "TIMEOUT IN");
      displayVALUE(2, timeOut);
      displayTEXT(1, "BTN1");
      displayTEXT(2, "BTN2");
      displayTEXT(3, "BTN3");
      displayTEXT(4, "BTN4");
      displayTEXT(5, "BTN5");
      displayTEXT(6, "BTN6");
      displayTEXT(7, "BTN7");
      displayTEXT(8, "BTN8");

      if BUTTON1 {
      blinkLED(1);
        menuCat[1] = 1;
      }
      else if BUTTON2 {
      blinkLED(2);
        menuCat[1] = 2;
      }
      else if BUTTON3 {
      blinkLED(3);
        menuCat[1] = 3;
      }
      else if BUTTON4 {
      blinkLED(4);
        menuCat[1] = 4;
      }
      else if BUTTON5 {
      blinkLED(5);
        menuCat[1] = 5;
      }
      else if BUTTON6 {
      blinkLED(6);
        menuCat[1] = 6;
      }
      else if BUTTON7 {
      blinkLED(7);
        menuCat[1] = 7;
      }
      else if BUTTON8 {
      blinkLED(8);
        menuCat[1] = 8;
      }

      if (currentMillis - previousMillis >= MENU_TIMEOUT) {
        if ((menuCat[0] > 1) && (menuCat[1] < 1)) menuCat[0] = 0;
        previousMillis = currentMillis;
      }
    }
  }
  else if (menuCat[1] > 0) {
    if (menuCat[2] == 1) {
      if (menuCat[1] == 1) {
        displayTEXT(0, "BTN 1 NOTE:");
        displayVALUE(1, switchNote[0]);
      }
      else if (menuCat[1] == 2) {
        displayTEXT(0, "BTN 2 NOTE:");
        displayVALUE(1, switchNote[1]);
      }
      else if (menuCat[1] == 3) {
        displayTEXT(0, "BTN 3 NOTE:");
        displayVALUE(1, switchNote[2]);
      }
      else if (menuCat[1] == 4) {
        displayTEXT(0, "BTN 4 NOTE:");
        displayVALUE(1, switchNote[3]);
      }
      else if (menuCat[1] == 5) {
        displayTEXT(0, "BTN 5 NOTE:");
        displayVALUE(1, switchNote[4]);
      }
      else if (menuCat[1] == 6) {
        displayTEXT(0, "BTN 6 NOTE:");
        displayVALUE(1, switchNote[5]);
      }
      else if (menuCat[1] == 7) {
        displayTEXT(0, "BTN 7 NOTE:");
        displayVALUE(1, switchNote[6]);
      }
      else if (menuCat[1] == 8) {
        displayTEXT(0, "BTN 8 NOTE:");
        displayVALUE(1, switchNote[7]);
      }
    }
    else if (menuCat[2] == 2) {
      if (menuCat[1] == 1) {
        displayTEXT(0, "BTN 1 VEL:");
        displayVALUE(1, switchVel[0]);
      }
      else if (menuCat[1] == 2) {
        displayTEXT(0, "BTN 2 VEL:");
        displayVALUE(1, switchVel[1]);
      }
      else if (menuCat[1] == 3) {
        displayTEXT(0, "BTN 3 VEL:");
        displayVALUE(1, switchVel[2]);
      }
      else if (menuCat[1] == 4) {
        displayTEXT(0, "BTN 4 VEL:");
        displayVALUE(1, switchVel[3]);
      }
      else if (menuCat[1] == 5) {
        displayTEXT(0, "BTN 5 VEL:");
        displayVALUE(1, switchVel[4]);
      }
      else if (menuCat[1] == 6) {
        displayTEXT(0, "BTN 6 VEL:");
        displayVALUE(1, switchVel[5]);
      }
      else if (menuCat[1] == 7) {
        displayTEXT(0, "BTN 7 VEL:");
        displayVALUE(1, switchVel[6]);
      }
      else if (menuCat[1] == 8) {
        displayTEXT(0, "BTN 8 VEL:");
        displayVALUE(1, switchVel[7]);
      }
    }
    else if (menuCat[2] < 1) {
      if (menuCat[1] > 0) {
        displayTEXT(15, "BUTTON");
        displayVALUE(3, menuCat[1]);
        displayTEXT(14, "CHANGE WHICH ATTR");
        displayTEXT(1, "NOTE");
        displayTEXT(2, "VEL");

        if BUTTON1 {
        blinkLED(1);
          menuCat[2] = 1;
        }
        else if BUTTON2 {
        blinkLED(2);
          menuCat[2] = 2;
        }
      }
    }

    displayTEXT(8, "BACK");
    if BUTTON8 {
    blinkLED(8);
      if (menuCat[2] < 1) {
        menuCat[1] = 0;
        previousMillis = currentMillis;
      } else menuCat[2] = 0;
    }
  }
}

void menuLEVEL3() { //CC edit submenu
  if (menuCat[1] > 0) {
    if (menuCat[2] > 0) {
      displayTEXT(1, "+1");
      displayTEXT(2, "+10");
      displayTEXT(5, "-1");
      displayTEXT(6, "-10");

      if BUTTON1 {
      if (menuCat[2] == 1) {
          if (menuCat[1] == 1) switchCC[0]++;
          if (menuCat[1] == 2) switchCC[1]++;
          if (menuCat[1] == 3) switchCC[2]++;
          if (menuCat[1] == 4) switchCC[3]++;
          if (menuCat[1] == 5) switchCC[4]++;
          if (menuCat[1] == 6) switchCC[5]++;
          if (menuCat[1] == 7) switchCC[6]++;
          if (menuCat[1] == 8) switchCC[7]++;
        }
        else if (menuCat[2] == 2) {
          if (menuCat[1] == 1) switchCCValOn[0]++;
          if (menuCat[1] == 2) switchCCValOn[1]++;
          if (menuCat[1] == 3) switchCCValOn[2]++;
          if (menuCat[1] == 4) switchCCValOn[3]++;
          if (menuCat[1] == 5) switchCCValOn[4]++;
          if (menuCat[1] == 6) switchCCValOn[5]++;
          if (menuCat[1] == 7) switchCCValOn[6]++;
          if (menuCat[1] == 8) switchCCValOn[7]++;
        }
        else if (menuCat[2] == 3) {
          if (menuCat[1] == 1) switchCCValOff[0]++;
          if (menuCat[1] == 2) switchCCValOff[1]++;
          if (menuCat[1] == 3) switchCCValOff[2]++;
          if (menuCat[1] == 4) switchCCValOff[3]++;
          if (menuCat[1] == 5) switchCCValOff[4]++;
          if (menuCat[1] == 6) switchCCValOff[5]++;
          if (menuCat[1] == 7) switchCCValOff[6]++;
          if (menuCat[1] == 8) switchCCValOff[7]++;
        }
      }
      else if BUTTON2 {
      if (menuCat[2] == 1) {
          if (menuCat[1] == 1) switchCC[0] += 10;
          if (menuCat[1] == 2) switchCC[1] += 10;
          if (menuCat[1] == 3) switchCC[2] += 10;
          if (menuCat[1] == 4) switchCC[3] += 10;
          if (menuCat[1] == 5) switchCC[4] += 10;
          if (menuCat[1] == 6) switchCC[5] += 10;
          if (menuCat[1] == 7) switchCC[6] += 10;
          if (menuCat[1] == 8) switchCC[7] += 10;
        }
        else if (menuCat[2] == 2) {
          if (menuCat[1] == 1) switchCCValOn[0] += 10;
          if (menuCat[1] == 2) switchCCValOn[1] += 10;
          if (menuCat[1] == 3) switchCCValOn[2] += 10;
          if (menuCat[1] == 4) switchCCValOn[3] += 10;
          if (menuCat[1] == 5) switchCCValOn[4] += 10;
          if (menuCat[1] == 6) switchCCValOn[5] += 10;
          if (menuCat[1] == 7) switchCCValOn[6] += 10;
          if (menuCat[1] == 8) switchCCValOn[7] += 10;
        }
        else if (menuCat[2] == 3) {
          if (menuCat[1] == 1) switchCCValOff[0] += 10;
          if (menuCat[1] == 2) switchCCValOff[1] += 10;
          if (menuCat[1] == 3) switchCCValOff[2] += 10;
          if (menuCat[1] == 4) switchCCValOff[3] += 10;
          if (menuCat[1] == 5) switchCCValOff[4] += 10;
          if (menuCat[1] == 6) switchCCValOff[5] += 10;
          if (menuCat[1] == 7) switchCCValOff[6] += 10;
          if (menuCat[1] == 8) switchCCValOff[7] += 10;
        }
      }

      else if BUTTON5 {
      if (menuCat[2] == 1) {
          if (menuCat[1] == 1) switchCC[0]--;
          if (menuCat[1] == 2) switchCC[1]--;
          if (menuCat[1] == 3) switchCC[2]--;
          if (menuCat[1] == 4) switchCC[3]--;
          if (menuCat[1] == 5) switchCC[4]--;
          if (menuCat[1] == 6) switchCC[5]--;
          if (menuCat[1] == 7) switchCC[6]--;
          if (menuCat[1] == 8) switchCC[7]--;
        }
        else if (menuCat[2] == 2) {
          if (menuCat[1] == 1) switchCCValOn[0]--;
          if (menuCat[1] == 2) switchCCValOn[1]--;
          if (menuCat[1] == 3) switchCCValOn[2]--;
          if (menuCat[1] == 4) switchCCValOn[3]--;
          if (menuCat[1] == 5) switchCCValOn[4]--;
          if (menuCat[1] == 6) switchCCValOn[5]--;
          if (menuCat[1] == 7) switchCCValOn[6]--;
          if (menuCat[1] == 8) switchCCValOn[7]--;
        }
        else if (menuCat[2] == 3) {
          if (menuCat[1] == 1) switchCCValOff[0]--;
          if (menuCat[1] == 2) switchCCValOff[1]--;
          if (menuCat[1] == 3) switchCCValOff[2]--;
          if (menuCat[1] == 4) switchCCValOff[3]--;
          if (menuCat[1] == 5) switchCCValOff[4]--;
          if (menuCat[1] == 6) switchCCValOff[5]--;
          if (menuCat[1] == 7) switchCCValOff[6]--;
          if (menuCat[1] == 8) switchCCValOff[7]--;
        }
      }
      else if BUTTON6 {
      if (menuCat[2] == 1) {
          if (menuCat[1] == 1) switchCC[0] -= 10;
          if (menuCat[1] == 2) switchCC[1] -= 10;
          if (menuCat[1] == 3) switchCC[2] -= 10;
          if (menuCat[1] == 4) switchCC[3] -= 10;
          if (menuCat[1] == 5) switchCC[4] -= 10;
          if (menuCat[1] == 6) switchCC[5] -= 10;
          if (menuCat[1] == 7) switchCC[6] -= 10;
          if (menuCat[1] == 8) switchCC[7] -= 10;
        }
        else if (menuCat[2] == 2) {
          if (menuCat[1] == 1) switchCCValOn[0] -= 10;
          if (menuCat[1] == 2) switchCCValOn[1] -= 10;
          if (menuCat[1] == 3) switchCCValOn[2] -= 10;
          if (menuCat[1] == 4) switchCCValOn[3] -= 10;
          if (menuCat[1] == 5) switchCCValOn[4] -= 10;
          if (menuCat[1] == 6) switchCCValOn[5] -= 10;
          if (menuCat[1] == 7) switchCCValOn[6] -= 10;
          if (menuCat[1] == 8) switchCCValOn[7] -= 10;
        }
        else if (menuCat[2] == 3) {
          if (menuCat[1] == 1) switchCCValOff[0] -= 10;
          if (menuCat[1] == 2) switchCCValOff[1] -= 10;
          if (menuCat[1] == 3) switchCCValOff[2] -= 10;
          if (menuCat[1] == 4) switchCCValOff[3] -= 10;
          if (menuCat[1] == 5) switchCCValOff[4] -= 10;
          if (menuCat[1] == 6) switchCCValOff[5] -= 10;
          if (menuCat[1] == 7) switchCCValOff[6] -= 10;
          if (menuCat[1] == 8) switchCCValOff[7] -= 10;
        }
      }
      valueCHECK();
    }
  }

  if (menuCat[1] < 1) {
    if (menuCat[2] < 1) {
      currentMillis = millis();
      long timeOut = (MENU_TIMEOUT / 1000 - ((currentMillis - previousMillis) / 1000));

      displayTEXT(12, "CHANGE BTN CC");
      displayTEXT(13, "TIMEOUT IN");
      displayVALUE(2, timeOut);
      displayTEXT(1, "BTN1");
      displayTEXT(2, "BTN2");
      displayTEXT(3, "BTN3");
      displayTEXT(4, "BTN4");
      displayTEXT(5, "BTN5");
      displayTEXT(6, "BTN6");
      displayTEXT(7, "BTN7");
      displayTEXT(8, "BTN8");

      if BUTTON1 {
      blinkLED(1);
        menuCat[1] = 1;
      }
      else if BUTTON2 {
      blinkLED(2);
        menuCat[1] = 2;
      }
      else if BUTTON3 {
      blinkLED(3);
        menuCat[1] = 3;
      }
      else if BUTTON4 {
      blinkLED(4);
        menuCat[1] = 4;
      }
      else if BUTTON5 {
      blinkLED(5);
        menuCat[1] = 5;
      }
      else if BUTTON6 {
      blinkLED(6);
        menuCat[1] = 6;
      }
      else if BUTTON7 {
      blinkLED(7);
        menuCat[1] = 7;
      }
      else if BUTTON8 {
      blinkLED(8);
        menuCat[1] = 8;
      }

      if (currentMillis - previousMillis >= MENU_TIMEOUT) {
        if ((menuCat[0] > 1) && (menuCat[1] < 1)) menuCat[0] = 0;
        previousMillis = currentMillis;
      }
    }
  }
  else if (menuCat[1] > 0) {
    if (menuCat[2] == 1) {
      if (menuCat[1] == 1) {
        displayTEXT(0, "BTN 1 CC:");
        displayVALUE(1, switchCC[0]);
      }
      else if (menuCat[1] == 2) {
        displayTEXT(0, "BTN 2 CC:");
        displayVALUE(1, switchCC[1]);
      }
      else if (menuCat[1] == 3) {
        displayTEXT(0, "BTN 3 CC:");
        displayVALUE(1, switchCC[2]);
      }
      else if (menuCat[1] == 4) {
        displayTEXT(0, "BTN 4 CC:");
        displayVALUE(1, switchCC[3]);
      }
      else if (menuCat[1] == 5) {
        displayTEXT(0, "BTN 5 CC:");
        displayVALUE(1, switchCC[4]);
      }
      else if (menuCat[1] == 6) {
        displayTEXT(0, "BTN 6 CC:");
        displayVALUE(1, switchCC[5]);
      }
      else if (menuCat[1] == 7) {
        displayTEXT(0, "BTN 7 CC:");
        displayVALUE(1, switchCC[6]);
      }
      else if (menuCat[1] == 8) {
        displayTEXT(0, "BTN 8 CC:");
        displayVALUE(1, switchCC[7]);
      }
    }
    else if (menuCat[2] == 2) {
      if (menuCat[1] == 1) {
        displayTEXT(0, "BTN 1 ValON:");
        displayVALUE(1, switchCCValOn[0]);
      }
      else if (menuCat[1] == 2) {
        displayTEXT(0, "BTN 2 ValON:");
        displayVALUE(1, switchCCValOn[1]);
      }
      else if (menuCat[1] == 3) {
        displayTEXT(0, "BTN 3 ValON:");
        displayVALUE(1, switchCCValOn[2]);
      }
      else if (menuCat[1] == 4) {
        displayTEXT(0, "BTN 4 ValON:");
        displayVALUE(1, switchCCValOn[3]);
      }
      else if (menuCat[1] == 5) {
        displayTEXT(0, "BTN 5 ValON:");
        displayVALUE(1, switchCCValOn[4]);
      }
      else if (menuCat[1] == 6) {
        displayTEXT(0, "BTN 6 ValON:");
        displayVALUE(1, switchCCValOn[5]);
      }
      else if (menuCat[1] == 7) {
        displayTEXT(0, "BTN 7 ValON:");
        displayVALUE(1, switchCCValOn[6]);
      }
      else if (menuCat[1] == 8) {
        displayTEXT(0, "BTN 8 ValON:");
        displayVALUE(1, switchCCValOn[7]);
      }
    }
    else if (menuCat[2] == 3) {
      if (menuCat[1] == 1) {
        displayTEXT(0, "BTN 1 ValOFF:");
        displayVALUE(1, switchCCValOff[0]);
      }
      else if (menuCat[1] == 2) {
        displayTEXT(0, "BTN 2 ValOFF:");
        displayVALUE(1, switchCCValOff[1]);
      }
      else if (menuCat[1] == 3) {
        displayTEXT(0, "BTN 3 ValOFF:");
        displayVALUE(1, switchCCValOff[2]);
      }
      else if (menuCat[1] == 4) {
        displayTEXT(0, "BTN 4 ValOFF:");
        displayVALUE(1, switchCCValOff[3]);
      }
      else if (menuCat[1] == 5) {
        displayTEXT(0, "BTN 5 ValOFF:");
        displayVALUE(1, switchCCValOff[4]);
      }
      else if (menuCat[1] == 6) {
        displayTEXT(0, "BTN 6 ValOFF:");
        displayVALUE(1, switchCCValOff[5]);
      }
      else if (menuCat[1] == 7) {
        displayTEXT(0, "BTN 7 ValOFF:");
        displayVALUE(1, switchCCValOff[6]);
      }
      else if (menuCat[1] == 8) {
        displayTEXT(0, "BTN 8 ValOFF:");
        displayVALUE(1, switchCCValOff[7]);
      }
    }
    else if (menuCat[2] < 1) {
      if (menuCat[1] > 0) {
        displayTEXT(15, "BUTTON");
        displayVALUE(3, menuCat[1]);
        displayTEXT(14, "CHANGE WHICH ATTR");
        displayTEXT(1, " CC");
        displayTEXT(2, "VON");
        displayTEXT(3, "VOFF");

        if BUTTON1 {
        blinkLED(1);
          menuCat[2] = 1;
        }
        else if BUTTON2 {
        blinkLED(2);
          menuCat[2] = 2;
        }
        else if BUTTON3 {
        blinkLED(3);
          menuCat[2] = 3;
        }
      }
    }

    displayTEXT(8, "BACK");
    if BUTTON8 {
    blinkLED(8);
      if (menuCat[2] < 1) {
        menuCat[1] = 0;
        previousMillis = currentMillis;
      } else menuCat[2] = 0;
    }
  }
}

void menuLEVEL4() { //PC edit submenu
  if (menuCat[1] > 0) {
    displayTEXT(1, "+1");
    displayTEXT(2, "+10");
    displayTEXT(5, "-1");
    displayTEXT(6, "-10");

    if BUTTON1 {
    if (menuCat[1] == 1) switchPC[0]++;
      if (menuCat[1] == 2) switchPC[1]++;
      if (menuCat[1] == 3) switchPC[2]++;
      if (menuCat[1] == 4) switchPC[3]++;
      if (menuCat[1] == 5) switchPC[4]++;
      if (menuCat[1] == 6) switchPC[5]++;
      if (menuCat[1] == 7) switchPC[6]++;
      if (menuCat[1] == 8) switchPC[7]++;
    }
    else if BUTTON2 {
    if (menuCat[1] == 1) switchPC[0] += 10;
      if (menuCat[1] == 2) switchPC[1] += 10;
      if (menuCat[1] == 3) switchPC[2] += 10;
      if (menuCat[1] == 4) switchPC[3] += 10;
      if (menuCat[1] == 5) switchPC[4] += 10;
      if (menuCat[1] == 6) switchPC[5] += 10;
      if (menuCat[1] == 7) switchPC[6] += 10;
      if (menuCat[1] == 8) switchPC[7] += 10;
    }
    else if BUTTON5 {
    if (menuCat[1] == 1) switchPC[0]--;
      if (menuCat[1] == 2) switchPC[1]--;
      if (menuCat[1] == 3) switchPC[2]--;
      if (menuCat[1] == 4) switchPC[3]--;
      if (menuCat[1] == 5) switchPC[4]--;
      if (menuCat[1] == 6) switchPC[5]--;
      if (menuCat[1] == 7) switchPC[6]--;
      if (menuCat[1] == 8) switchPC[7]--;
    }
    else if BUTTON6 {
    if (menuCat[1] == 1) switchPC[0] -= 10;
      if (menuCat[1] == 2) switchPC[1] -= 10;
      if (menuCat[1] == 3) switchPC[2] -= 10;
      if (menuCat[1] == 4) switchPC[3] -= 10;
      if (menuCat[1] == 5) switchPC[4] -= 10;
      if (menuCat[1] == 6) switchPC[5] -= 10;
      if (menuCat[1] == 7) switchPC[6] -= 10;
      if (menuCat[1] == 8) switchPC[7] -= 10;
    }

    valueCHECK();
  }

  if (menuCat[1] == 1) {
    displayTEXT(0, "BUTTON 1 PC:");
    displayVALUE(0, switchPC[0]);
  }
  else if (menuCat[1] == 2) {
    displayTEXT(0, "BUTTON 2 PC:");
    displayVALUE(0, switchPC[1]);
  }
  else if (menuCat[1] == 3) {
    displayTEXT(0, "BUTTON 3 PC:");
    displayVALUE(0, switchPC[2]);
  }
  else if (menuCat[1] == 4) {
    displayTEXT(0, "BUTTON 4 PC:");
    displayVALUE(0, switchPC[3]);
  }
  else if (menuCat[1] == 5) {
    displayTEXT(0, "BUTTON 5 PC:");
    displayVALUE(0, switchPC[4]);
  }
  else if (menuCat[1] == 6) {
    displayTEXT(0, "BUTTON 6 PC:");
    displayVALUE(0, switchPC[5]);
  }
  else if (menuCat[1] == 7) {
    displayTEXT(0, "BUTTON 7 PC:");
    displayVALUE(0, switchPC[6]);
  }
  else if (menuCat[1] == 8) {
    displayTEXT(0, "BUTTON 8 PC:");
    displayVALUE(0, switchPC[7]);
  }
  else if (menuCat[1] < 1) {
    currentMillis = millis();
    long timeOut = (MENU_TIMEOUT / 1000 - ((currentMillis - previousMillis) / 1000));

    displayTEXT(12, "CHANGE BTN PC");
    displayTEXT(13, "TIMEOUT IN");
    displayVALUE(2, timeOut);
    displayTEXT(1, "BTN1");
    displayTEXT(2, "BTN2");
    displayTEXT(3, "BTN3");
    displayTEXT(4, "BTN4");
    displayTEXT(5, "BTN5");
    displayTEXT(6, "BTN6");
    displayTEXT(7, "BTN7");
    displayTEXT(8, "BTN8");

    if BUTTON1 {
    blinkLED(1);
      menuCat[1] = 1;
    }
    else if BUTTON2 {
    blinkLED(2);
      menuCat[1] = 2;
    }
    else if BUTTON3 {
    blinkLED(3);
      menuCat[1] = 3;
    }
    else if BUTTON4 {
    blinkLED(4);
      menuCat[1] = 4;
    }
    else if BUTTON5 {
    blinkLED(5);
      menuCat[1] = 5;
    }
    else if BUTTON6 {
    blinkLED(6);
      menuCat[1] = 6;
    }
    else if BUTTON7 {
    blinkLED(7);
      menuCat[1] = 7;
    }
    else if BUTTON8 {
    blinkLED(8);
      menuCat[1] = 8;
    }

    if (currentMillis - previousMillis >= MENU_TIMEOUT) {
      if ((menuCat[0] > 1) && (menuCat[1] < 1)) menuCat[0] = 0;
      previousMillis = currentMillis;
    }

  }
  if (menuCat[1] > 0) {
    displayTEXT(8, "BACK");
    if BUTTON8 {
    blinkLED(8);
      menuCat[1] = 0;
      previousMillis = currentMillis;
    }
  }
}

void menuLEVEL5() { //Defaults submenu
  if ((menuCat[1] == 1) || (menuCat[1] == 2)) {
    displayTEXT(1, "+1");
    displayTEXT(2, "+5");
    displayTEXT(5, "-1");
    displayTEXT(6, "-5");
  }
  else if (menuCat[1] > 2) {
    displayTEXT(1, "+5");
    displayTEXT(2, "+10");
    displayTEXT(5, "-5");
    displayTEXT(6, "-10");
  }

  if (menuCat[1] > 0) {
    if BUTTON1 {
    if (menuCat[1] == 1) RUNMODE_DEFAULT++;
      if (menuCat[1] == 2) MENU_TIMEOUT += 1000;
      if (menuCat[1] == 3) MIDI_NOTEOFF_TIME += 5;
      if (menuCat[1] == 4) MIDI_CCOFF_TIME += 5;
    }
    else if BUTTON2 {
    if (menuCat[1] == 1) RUNMODE_DEFAULT += 5;
      if (menuCat[1] == 2) MENU_TIMEOUT += 5000;
      if (menuCat[1] == 3) MIDI_NOTEOFF_TIME += 10;
      if (menuCat[1] == 4) MIDI_CCOFF_TIME += 10;
    }
    else if BUTTON5 {
    if (menuCat[1] == 1) RUNMODE_DEFAULT--;
      if (menuCat[1] == 2) MENU_TIMEOUT -= 1000;
      if (menuCat[1] == 3) MIDI_NOTEOFF_TIME -= 5;
      if (menuCat[1] == 4) MIDI_CCOFF_TIME -= 5;
    }
    else if BUTTON6 {
    if (menuCat[1] == 1) RUNMODE_DEFAULT -= 5;
      if (menuCat[1] == 2) MENU_TIMEOUT -= 5000;
      if (menuCat[1] == 3) MIDI_NOTEOFF_TIME -= 10;
      if (menuCat[1] == 4) MIDI_CCOFF_TIME -= 10;
    }

    valueCHECK();
  }

  if (menuCat[1] == 1) {
    displayTEXT(9, "DEF RUNMODE:");
    displayVALUE(0, RUNMODE_DEFAULT);
  }
  else if (menuCat[1] == 2) {
    displayTEXT(9, "MENU TIMEOUT:");
    displayVALUE(0, (MENU_TIMEOUT / 1000));
  }
  else if (menuCat[1] == 3) {
    displayTEXT(9, "NOTE OFF TIME:");
    displayVALUE(1, MIDI_NOTEOFF_TIME);
  }
  else if (menuCat[1] == 4) {
    displayTEXT(9, "CC OFF TIME:");
    displayVALUE(1, MIDI_CCOFF_TIME);
  }
  else if (menuCat[1] < 1) {
    displayTEXT(0, "CHANGE DEFAULT");
    displayTEXT(1, "RUNM"); //Default runmode
    displayTEXT(2, "MENU"); //Menu timeout
    displayTEXT(5, "NOFF"); //Note off time
    displayTEXT(6, "COFF"); //CC off time

    if BUTTON1 {
    blinkLED(1);
      menuCat[1] = 1;
    }
    if BUTTON2 {
    blinkLED(2);
      menuCat[1] = 2;
    }
    if BUTTON5 {
    blinkLED(5);
      menuCat[1] = 3;
    }
    if BUTTON6 {
    blinkLED(6);
      menuCat[1] = 4;
    }
  }

  displayTEXT(8, "BACK");
  if BUTTON8 {
  blinkLED(8);
    if (menuCat[1] < 1) resetMENU();
    else menuCat[1] = 0;
    previousMillis = currentMillis;
  }
}

void displayTEXT(int displayPOS, String displayCONTENT) {
  if (displayPOS == 0) display.setCursor(20, 34); //Centre of display
  else if (displayPOS == 1) display.setCursor(10, 55); //Button 1
  else if (displayPOS == 2) display.setCursor(40, 55); //Button 2
  else if (displayPOS == 3) display.setCursor(70, 55); //Button 3
  else if (displayPOS == 4) display.setCursor(100, 55); //Button 4
  else if (displayPOS == 5) display.setCursor(10, 10); //Button 5
  else if (displayPOS == 6) display.setCursor(40, 10); //Button 6
  else if (displayPOS == 7) display.setCursor(70, 10); //Button 7
  else if (displayPOS == 8) display.setCursor(100, 10); //Button 8
  else if (displayPOS == 9) display.setCursor(10, 34); //Left of centre
  else if (displayPOS == 10) display.setCursor(12, 28); //Left of centre, up from centre
  else if (displayPOS == 11) display.setCursor(18, 28); //similar but less left
  else if (displayPOS == 12) display.setCursor(26, 28); //PC heading
  else if (displayPOS == 13) display.setCursor(30, 38); //Timeout
  else if (displayPOS == 14) display.setCursor(14, 38); //Choose attribute
  else if (displayPOS == 15) display.setCursor(40, 28);
  else if (displayPOS == 16) display.setCursor(22, 28); //Note heading
  display.println(displayCONTENT);
}

void displayVALUE(int displayPOS, int displayCONTENT) {
  if (displayPOS == 0) display.setCursor(90, 34); //Channel values
  else if (displayPOS == 1) display.setCursor(100, 34);
  else if (displayPOS == 2) display.setCursor(95, 38);
  else if (displayPOS == 3) display.setCursor(85, 28);
  display.println(displayCONTENT);
}

void blinkLED(int ledNUM) {
  digitalWrite(ledPin[ledNUM - 1], HIGH);
  delay(ledDelay);
  digitalWrite(ledPin[ledNUM - 1], LOW);
}

void eepromREAD() {
  if (EEPROM.read(1023) == 1) { //Check the EEPROM update flag (address 127) to see if custom values have been written. If so, load those values.
    //Channels
    MIDI_NOTE_CHAN = EEPROM.read(0);
    MIDI_CC_CHAN = EEPROM.read(1);
    MIDI_PC_CHAN = EEPROM.read(2);

    //Notes
    switchNote[0] = EEPROM.read(10);
    switchNote[1] = EEPROM.read(12);
    switchNote[2] = EEPROM.read(14);
    switchNote[3] = EEPROM.read(16);
    switchNote[4] = EEPROM.read(18);
    switchNote[5] = EEPROM.read(20);
    switchNote[6] = EEPROM.read(22);
    switchNote[7] = EEPROM.read(24);

    //Velocities
    switchVel[0] = EEPROM.read(11);
    switchVel[1] = EEPROM.read(13);
    switchVel[2] = EEPROM.read(15);
    switchVel[3] = EEPROM.read(17);
    switchVel[4] = EEPROM.read(19);
    switchVel[5] = EEPROM.read(21);
    switchVel[6] = EEPROM.read(23);
    switchVel[7] = EEPROM.read(25);

    //CC
    switchCC[0] = EEPROM.read(30);
    switchCC[1] = EEPROM.read(33);
    switchCC[2] = EEPROM.read(36);
    switchCC[3] = EEPROM.read(39);
    switchCC[4] = EEPROM.read(42);
    switchCC[5] = EEPROM.read(45);
    switchCC[6] = EEPROM.read(48);
    switchCC[7] = EEPROM.read(51);

    switchCCValOn[0] = EEPROM.read(31);
    switchCCValOn[1] = EEPROM.read(34);
    switchCCValOn[2] = EEPROM.read(37);
    switchCCValOn[3] = EEPROM.read(40);
    switchCCValOn[4] = EEPROM.read(43);
    switchCCValOn[5] = EEPROM.read(46);
    switchCCValOn[6] = EEPROM.read(49);
    switchCCValOn[7] = EEPROM.read(52);

    switchCCValOff[0] = EEPROM.read(32);
    switchCCValOff[1] = EEPROM.read(35);
    switchCCValOff[2] = EEPROM.read(38);
    switchCCValOff[3] = EEPROM.read(41);
    switchCCValOff[4] = EEPROM.read(44);
    switchCCValOff[5] = EEPROM.read(47);
    switchCCValOff[6] = EEPROM.read(50);
    switchCCValOff[7] = EEPROM.read(53);

    //PC
    switchPC[0] = EEPROM.read(60);
    switchPC[1] = EEPROM.read(61);
    switchPC[2] = EEPROM.read(62);
    switchPC[3] = EEPROM.read(63);
    switchPC[4] = EEPROM.read(64);
    switchPC[5] = EEPROM.read(65);
    switchPC[6] = EEPROM.read(66);
    switchPC[7] = EEPROM.read(67);

    //Defaults
    RUNMODE_DEFAULT = EEPROM.read(70);
    MIDI_NOTEOFF_TIME = EEPROM.read(71);
    MIDI_CCOFF_TIME = EEPROM.read(72);
    MENU_TIMEOUT = (EEPROM.read(73) * 1000);

  } else { //Otherwise load the default values
    MIDI_NOTE_CHAN = 11;
    MIDI_CC_CHAN = 11;
    MIDI_PC_CHAN = 11;

    switchNote[0] = 60;
    switchNote[1] = 61;
    switchNote[2] = 62;
    switchNote[3] = 63;
    switchNote[4] = 64;
    switchNote[5] = 65;
    switchNote[6] = 66;
    switchNote[7] = 67;

    switchVel[0] = 64;
    switchVel[1] = 64;
    switchVel[2] = 64;
    switchVel[3] = 64;
    switchVel[4] = 64;
    switchVel[5] = 64;
    switchVel[6] = 64;
    switchVel[7] = 64;

    switchCC[0] = 20;
    switchCC[1] = 21;
    switchCC[2] = 22;
    switchCC[3] = 23;
    switchCC[4] = 24;
    switchCC[5] = 25;
    switchCC[6] = 26;
    switchCC[7] = 27;

    switchCCValOn[0] = 127;
    switchCCValOn[1] = 127;
    switchCCValOn[2] = 127;
    switchCCValOn[3] = 127;
    switchCCValOn[4] = 127;
    switchCCValOn[5] = 127;
    switchCCValOn[6] = 127;
    switchCCValOn[7] = 127;

    switchCCValOff[0] = 0;
    switchCCValOff[1] = 0;
    switchCCValOff[2] = 0;
    switchCCValOff[3] = 0;
    switchCCValOff[4] = 0;
    switchCCValOff[5] = 0;
    switchCCValOff[6] = 0;
    switchCCValOff[7] = 0;

    switchPC[0] = 1;
    switchPC[1] = 2;
    switchPC[2] = 3;
    switchPC[3] = 4;
    switchPC[4] = 5;
    switchPC[5] = 6;
    switchPC[6] = 7;
    switchPC[7] = 8;

    RUNMODE_DEFAULT = 1;
    MIDI_NOTEOFF_TIME = 25;
    MIDI_CCOFF_TIME = 25;
    MENU_TIMEOUT = 11000;
  }
}

void eepromUPDATE() {
  EEPROM.update(1023, 1); //Set EEPROM write flag so that it is loaded next boot

  EEPROM.update(0, MIDI_NOTE_CHAN);
  EEPROM.update(1, MIDI_CC_CHAN);
  EEPROM.update(2, MIDI_PC_CHAN);

  EEPROM.update(10, switchNote[0]);
  EEPROM.update(12, switchNote[1]);
  EEPROM.update(14, switchNote[2]);
  EEPROM.update(16, switchNote[3]);
  EEPROM.update(18, switchNote[4]);
  EEPROM.update(20, switchNote[5]);
  EEPROM.update(22, switchNote[6]);
  EEPROM.update(24, switchNote[7]);

  EEPROM.update(11, switchVel[0]);
  EEPROM.update(13, switchVel[1]);
  EEPROM.update(15, switchVel[2]);
  EEPROM.update(17, switchVel[3]);
  EEPROM.update(19, switchVel[4]);
  EEPROM.update(21, switchVel[5]);
  EEPROM.update(23, switchVel[6]);
  EEPROM.update(25, switchVel[7]);

  EEPROM.update(30, switchCC[0]);
  EEPROM.update(33, switchCC[1]);
  EEPROM.update(36, switchCC[2]);
  EEPROM.update(39, switchCC[3]);
  EEPROM.update(42, switchCC[4]);
  EEPROM.update(45, switchCC[5]);
  EEPROM.update(48, switchCC[6]);
  EEPROM.update(51, switchCC[7]);

  EEPROM.update(31, switchCCValOn[0]);
  EEPROM.update(34, switchCCValOn[1]);
  EEPROM.update(37, switchCCValOn[2]);
  EEPROM.update(40, switchCCValOn[3]);
  EEPROM.update(43, switchCCValOn[4]);
  EEPROM.update(46, switchCCValOn[5]);
  EEPROM.update(49, switchCCValOn[6]);
  EEPROM.update(52, switchCCValOn[7]);

  EEPROM.update(32, switchCCValOff[0]);
  EEPROM.update(35, switchCCValOff[1]);
  EEPROM.update(38, switchCCValOff[2]);
  EEPROM.update(41, switchCCValOff[3]);
  EEPROM.update(44, switchCCValOff[4]);
  EEPROM.update(47, switchCCValOff[5]);
  EEPROM.update(50, switchCCValOff[6]);
  EEPROM.update(53, switchCCValOff[7]);

  EEPROM.update(60, switchPC[0]);
  EEPROM.update(61, switchPC[1]);
  EEPROM.update(62, switchPC[2]);
  EEPROM.update(63, switchPC[3]);
  EEPROM.update(64, switchPC[4]);
  EEPROM.update(65, switchPC[5]);
  EEPROM.update(66, switchPC[6]);
  EEPROM.update(67, switchPC[7]);

  EEPROM.update(70, RUNMODE_DEFAULT);
  EEPROM.update(71, MIDI_NOTEOFF_TIME);
  EEPROM.update(72, MIDI_CCOFF_TIME);
  EEPROM.update(73, (MENU_TIMEOUT / 1000));
}

void serialUPDATE() {

}

void valueCHECK() { //Check values when editing to keep things real
  if (MIDI_NOTE_CHAN < 1) MIDI_NOTE_CHAN = 1 ;
  if (MIDI_NOTE_CHAN > 16) MIDI_NOTE_CHAN = 16;
  if (MIDI_CC_CHAN < 1) MIDI_CC_CHAN = 1;
  if (MIDI_CC_CHAN > 16) MIDI_CC_CHAN = 16;
  if (MIDI_PC_CHAN < 1) MIDI_PC_CHAN = 1;
  if (MIDI_PC_CHAN > 16) MIDI_PC_CHAN = 16;

  if (RUNMODE_DEFAULT < 1) RUNMODE_DEFAULT = 1;
  if (RUNMODE_DEFAULT > 5) RUNMODE_DEFAULT = 5;
  if (MENU_TIMEOUT < 1000) MENU_TIMEOUT = 1000;
  if (MENU_TIMEOUT > 100000) MENU_TIMEOUT = 1000;
  if (MIDI_NOTEOFF_TIME < 5) MIDI_NOTEOFF_TIME = 5;
  if (MIDI_NOTEOFF_TIME > 100000) MIDI_NOTEOFF_TIME = 100000;
  if (MIDI_CCOFF_TIME < 5) MIDI_CCOFF_TIME = 5;
  if (MIDI_CCOFF_TIME > 100000) MIDI_CCOFF_TIME = 100000;

  for (int i = 0; i < 8; i++) {
    if (switchNote[i] < 0) switchNote[i] = 0;
    if (switchNote[i] > 127) switchNote[i] = 127;

    if (switchVel[i] < 0) switchVel[i] = 0;
    if (switchVel[i] > 127) switchVel[i] = 127;

    if (switchCC[i] < 0) switchCC[i] = 0;
    if (switchCC[i] > 127) switchCC[i] = 127;

    if (switchCCValOn[i] < 0) switchCCValOn[i] = 0;
    if (switchCCValOn[i] > 127) switchCCValOn[i] = 127;

    if (switchCCValOff[i] < 0) switchCCValOff[i] = 0;
    if (switchCCValOff[i] > 127) switchCCValOff[i] = 127;

    if (switchPC[i] < 0) switchPC[i] = 0;
    if (switchPC[i] > 127) switchPC[i] = 127;
  }
}

void resetMENU() {
  for (int i = 0; i < 3; i++) menuCat[i] = 0; //Reset menu level tracker
}
