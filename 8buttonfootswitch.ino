#include <SPI.h>
#include <Wire.h>
#include <Bounce.h>
#include <MIDI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Declare pins for footswitches
const byte switchPin[8] = {10, 11, 12, 13, 14, 15, 16, 17};

// Declare pins for LEDs;
const byte ledPin[8] = {32, 33, 34, 35, 28, 29, 30, 31};

// Define time before before MIDI OFF note is sent. This is needed because the footswitches are latching but we are pretending they are momentary
// Default note off time is 10ms
const int MIDI_NOTEOFF_TIME = 25;
const int MIDI_CCOFF_TIME = 25;

// Default channel for MIDI notes, CC, program change, control change
const byte MIDI_NOTE_CHAN = 11;
const byte MIDI_CC_CHAN = 11;
const byte MIDI_PC_CHAN = 11;

// Create MIDI instance for 5 pin MIDI output
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

// Define MIDI notes
const byte switchNote[8] = {60, 61, 62, 63, 64, 65, 66, 67};

// Define MIDI note velocities
const byte switchVel[8] = {64, 64, 64, 64, 64, 64, 64, 64};

// Define MIDI CCs
const byte switchCC[8] = {20, 21, 22, 23, 24, 25, 26, 27};

// Define MIDI CC on/off values
const byte switchCCValOn[8] = {127, 127, 127, 127, 127, 127, 127, 127};
const byte switchCCValOff[8] = {0, 0, 0, 0, 0, 0, 0, 0};

// Define Program Change messages
const byte switchPC[8] = {1, 2, 3, 4, 5, 6, 7, 8};

// Switch states for runmodes that require toggling
bool switchState[8] = {0, 0, 0, 0, 0, 0, 0, 0};

// Create Bounce objects for each button and switch.
// Default debounce time is 5ms
const byte DEBOUNCE_TIME = 5;

Bounce button1 = Bounce(switchPin[0], DEBOUNCE_TIME);
Bounce button2 = Bounce(switchPin[1], DEBOUNCE_TIME);
Bounce button3 = Bounce(switchPin[2], DEBOUNCE_TIME);
Bounce button4 = Bounce(switchPin[3], DEBOUNCE_TIME);
Bounce button5 = Bounce(switchPin[4], DEBOUNCE_TIME);
Bounce button6 = Bounce(switchPin[5], DEBOUNCE_TIME);
Bounce button7 = Bounce(switchPin[6], DEBOUNCE_TIME);
Bounce button8 = Bounce(switchPin[7], DEBOUNCE_TIME);

// Track runmode (function of footswitch) as well as pick default option if no change is made before timeout. Default timeout is 6000ms.
// 0 = Startup screen (runmode select)
// 1 = MIDI Note ON+OFF (timed), 2 = MIDI Note Toggle (must hit switch again to turn off)
// 3 = MIDI CC ON+OFF (timed), 4 = MIDI CC Toggle (same as MIDI Note, but with CC)
// 5 = Program Change
// 8 = Settings (planned, not implemented)
byte RUNMODE = 0;
const byte RUNMODE_DEFAULT = 1;
const int RUNMODE_TIMEOUT = 6000;

// Counter setup for menus requiring timeouts
long previousMillis = 0;

void setup() {
  Serial.begin(9600);

  MIDI.begin();

  // Configure switch pins as for input mode with pullup resistors
  pinMode (switchPin[0], INPUT_PULLUP);
  pinMode (switchPin[1], INPUT_PULLUP);
  pinMode (switchPin[2], INPUT_PULLUP);
  pinMode (switchPin[3], INPUT_PULLUP);
  pinMode (switchPin[4], INPUT_PULLUP);
  pinMode (switchPin[5], INPUT_PULLUP);
  pinMode (switchPin[6], INPUT_PULLUP);
  pinMode (switchPin[7], INPUT_PULLUP);

  // Configure LED pins for output
  pinMode (ledPin[0], OUTPUT);
  pinMode (ledPin[1], OUTPUT);
  pinMode (ledPin[2], OUTPUT);
  pinMode (ledPin[3], OUTPUT);
  pinMode (ledPin[4], OUTPUT);
  pinMode (ledPin[5], OUTPUT);
  pinMode (ledPin[6], OUTPUT);
  pinMode (ledPin[7], OUTPUT);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  updatebuttons(); // Pull initial button status to prevent phantom MIDI messages from being sent once the loop starts

  display.display();
  delay(500);

  display.clearDisplay();
}

void loop() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  updatebuttons();

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
  }

  while (usbMIDI.read())
  {
    // ignoring incoming messages, so don't do anything here.
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

void runmodeSELECTMODE() { // Give choice between running modes, choose default mode after timeout if no option selected
  unsigned long currentMillis = millis();
  long timeOut = (RUNMODE_TIMEOUT / 1000 - (currentMillis / 1000));

  display.setTextColor(WHITE, BLACK);
  display.setTextSize(1);
  display.setCursor(10, 34);
  display.println(F("CHOOSE RUNMODE"));
  display.setCursor(110, 34);
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

  if (button1.risingEdge() || button1.fallingEdge()) {
    RUNMODE = 1;
    digitalWrite(ledPin[0], HIGH);
    delay(500);
    digitalWrite(ledPin[0], LOW);
  }
  if (button2.risingEdge() || button2.fallingEdge()) {
    RUNMODE = 2;
    digitalWrite(ledPin[1], HIGH);
    delay(500);
    digitalWrite(ledPin[1], LOW);
  }
  if (button3.risingEdge() || button3.fallingEdge()) {
    RUNMODE = 3;
    digitalWrite(ledPin[2], HIGH);
    delay(500);
    digitalWrite(ledPin[2], LOW);
  }
  if (button4.risingEdge() || button4.fallingEdge()) {
    RUNMODE = 4;
    digitalWrite(ledPin[3], HIGH);
    delay(500);
    digitalWrite(ledPin[3], LOW);
  }
  if (button5.risingEdge() || button5.fallingEdge()) {
    RUNMODE = 5;
    digitalWrite(ledPin[4], HIGH);
    delay(500);
    digitalWrite(ledPin[4], LOW);
  }

  if (currentMillis - previousMillis > RUNMODE_TIMEOUT) {
    RUNMODE = RUNMODE_DEFAULT;
    digitalWrite(ledPin[RUNMODE_DEFAULT - 1], HIGH);
    delay(500);
    digitalWrite(ledPin[RUNMODE_DEFAULT - 1], LOW);
  }
  
  display.setTextColor(WHITE, BLACK);
  display.display();
}

void runmodeMIDINOTETIMED() {
  if (button1.risingEdge() || button1.fallingEdge()) {
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

  if (button2.risingEdge() || button2.fallingEdge()) {
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

  if (button3.risingEdge() || button3.fallingEdge()) {
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

  if (button4.risingEdge() || button4.fallingEdge()) {
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

  if (button5.risingEdge() || button5.fallingEdge()) {
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

  if (button6.risingEdge() || button6.fallingEdge()) {
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

  if (button7.risingEdge() || button7.fallingEdge()) {
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

  if (button8.risingEdge() || button8.fallingEdge()) {
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
  display.clearDisplay();
}

void runmodeMIDINOTETOGGLE() {
  if (button1.risingEdge() || button1.fallingEdge()) {
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

  if (button2.risingEdge() || button2.fallingEdge()) {
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

  if (button3.risingEdge() || button3.fallingEdge()) {
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

  if (button4.risingEdge() || button4.fallingEdge()) {
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

  if (button5.risingEdge() || button5.fallingEdge()) {
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

  if (button6.risingEdge() || button6.fallingEdge()) {
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

  if (button7.risingEdge() || button7.fallingEdge()) {
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

  if (button8.risingEdge() || button8.fallingEdge()) {
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
  display.clearDisplay();
}

void runmodeCCTIMED() {
  if (button1.risingEdge() || button1.fallingEdge()) {
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

  if (button2.risingEdge() || button2.fallingEdge()) {
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

  if (button3.risingEdge() || button3.fallingEdge()) {
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

  if (button4.risingEdge() || button4.fallingEdge()) {
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

  if (button5.risingEdge() || button5.fallingEdge()) {
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

  if (button6.risingEdge() || button6.fallingEdge()) {
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

  if (button7.risingEdge() || button7.fallingEdge()) {
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

  if (button8.risingEdge() || button8.fallingEdge()) {
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
  display.clearDisplay();
}


void runmodeCCTOGGLE() {
  if (button1.risingEdge() || button1.fallingEdge()) {
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

  if (button2.risingEdge() || button2.fallingEdge()) {
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

  if (button3.risingEdge() || button3.fallingEdge()) {
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

  if (button4.risingEdge() || button4.fallingEdge()) {
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

  if (button5.risingEdge() || button5.fallingEdge()) {
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

  if (button6.risingEdge() || button6.fallingEdge()) {
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

  if (button7.risingEdge() || button7.fallingEdge()) {
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

  if (button8.risingEdge() || button8.fallingEdge()) {
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
  display.clearDisplay();
}

void runmodePROGRAMCHANGE() {
  if (button1.risingEdge() || button1.fallingEdge()) {
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

  if (button2.risingEdge() || button2.fallingEdge()) {
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

  if (button3.risingEdge() || button3.fallingEdge()) {
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

  if (button4.risingEdge() || button4.fallingEdge()) {
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

  if (button5.risingEdge() || button5.fallingEdge()) {
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

  if (button6.risingEdge() || button6.fallingEdge()) {
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

  if (button7.risingEdge() || button7.fallingEdge()) {
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

  if (button8.risingEdge() || button8.fallingEdge()) {
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
  display.clearDisplay();
}

void resetSWITCHES() {
  for (byte i = 0; i < 9; i++) {
    switchState[i] = 0;
  }

  digitalWrite(ledPin[0], LOW);
  digitalWrite(ledPin[1], LOW);
  digitalWrite(ledPin[2], LOW);
  digitalWrite(ledPin[3], LOW);
  digitalWrite(ledPin[4], LOW);
  digitalWrite(ledPin[5], LOW);
  digitalWrite(ledPin[6], LOW);
  digitalWrite(ledPin[7], LOW);
}
