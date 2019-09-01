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

// Number of footswitches
const int NUM_SWITCHES = 8;

// Declare pins for footswitches
const int switchPin1 = 10;
const int switchPin2 = 11;
const int switchPin3 = 12;
const int switchPin4 = 13;
const int switchPin5 = 14;
const int switchPin6 = 15;
const int switchPin7 = 16;
const int switchPin8 = 17;

// Declare pins for LEDs;
const int ledPin1 = 32;
const int ledPin2 = 33;
const int ledPin3 = 34;
const int ledPin4 = 35;
const int ledPin5 = 28;
const int ledPin6 = 29;
const int ledPin7 = 30;
const int ledPin8 = 31;

// Define time before before MIDI OFF note is sent. This is needed because the footswitches are latching but we are pretending they are momentary
// Default note off time is 10ms

const int MIDI_NOTEOFF_TIME = 25;

// Default channel for MIDI notes

const int MIDI_CHAN = 11;

// Create MIDI instance for 5 pin MIDI output

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

// Default MIDI notes/velocities
const int switchNote1 = 60;
const int switchNote2 = 61;
const int switchNote3 = 62;
const int switchNote4 = 63;
const int switchNote5 = 64;
const int switchNote6 = 65;
const int switchNote7 = 66;
const int switchNote8 = 67;

const int switchVel1 = 64;
const int switchVel2 = 64;
const int switchVel3 = 64;
const int switchVel4 = 64;
const int switchVel5 = 64;
const int switchVel6 = 64;
const int switchVel7 = 64;
const int switchVel8 = 64;

const int switchChan1 = 11;
const int switchChan2 = 11;
const int switchChan3 = 11;
const int switchChan4 = 11;
const int switchChan5 = 11;
const int switchChan6 = 11;
const int switchChan7 = 11;
const int switchChan8 = 11;

// Create Bounce objects for each button and switch.
// Default debounce time is 5ms

const int DEBOUNCE_TIME = 5;

Bounce button1 = Bounce(switchPin1, DEBOUNCE_TIME);
Bounce button2 = Bounce(switchPin2, DEBOUNCE_TIME);
Bounce button3 = Bounce(switchPin3, DEBOUNCE_TIME);
Bounce button4 = Bounce(switchPin4, DEBOUNCE_TIME);
Bounce button5 = Bounce(switchPin5, DEBOUNCE_TIME);
Bounce button6 = Bounce(switchPin6, DEBOUNCE_TIME);
Bounce button7 = Bounce(switchPin7, DEBOUNCE_TIME);
Bounce button8 = Bounce(switchPin8, DEBOUNCE_TIME);

void setup() {
  Serial.begin(9600);

  MIDI.begin();

  // Configure switch pins as for input mode with pullup resistors

  pinMode (switchPin1, INPUT_PULLUP);
  pinMode (switchPin2, INPUT_PULLUP);
  pinMode (switchPin3, INPUT_PULLUP);
  pinMode (switchPin4, INPUT_PULLUP);
  pinMode (switchPin5, INPUT_PULLUP);
  pinMode (switchPin6, INPUT_PULLUP);
  pinMode (switchPin7, INPUT_PULLUP);
  pinMode (switchPin8, INPUT_PULLUP);

  // Configure LED pins for output

  pinMode (ledPin1, OUTPUT);
  pinMode (ledPin2, OUTPUT);
  pinMode (ledPin3, OUTPUT);
  pinMode (ledPin4, OUTPUT);
  pinMode (ledPin5, OUTPUT);
  pinMode (ledPin6, OUTPUT);
  pinMode (ledPin7, OUTPUT);
  pinMode (ledPin8, OUTPUT);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  display.display();
  delay(2000);

  display.clearDisplay();
}

void loop() {
  updatebuttons();

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(50, 30);

  if (button1.risingEdge() || button1.fallingEdge()) {
    display.setCursor(10, 50);
    display.println(switchNote1);
    digitalWrite(ledPin1, HIGH);
    usbMIDI.sendNoteOn(switchNote1, switchVel1, switchChan1);
    MIDI.sendNoteOn(switchNote1, switchVel1, switchChan1);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote1, 0, switchChan1);
    MIDI.sendNoteOff(switchNote1, 0, switchChan1);
    digitalWrite(ledPin1, LOW);
  }

  if (button2.risingEdge() || button2.fallingEdge()) {
    display.setCursor(40, 50);
    display.println(switchNote2);
    digitalWrite(ledPin2, HIGH);
    usbMIDI.sendNoteOn(switchNote2, switchVel2, switchChan2);
    MIDI.sendNoteOn(switchNote2, switchVel2, switchChan2);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote2, 0, switchChan2);
    MIDI.sendNoteOff(switchNote2, 0, switchChan2);
    digitalWrite(ledPin2, LOW);
  }

  if (button3.risingEdge() || button3.fallingEdge()) {
    display.setCursor(70, 50);
    display.println(switchNote3);
    digitalWrite(ledPin3, HIGH);
    usbMIDI.sendNoteOn(switchNote3, switchVel3, switchChan3);
    MIDI.sendNoteOn(switchNote3, switchVel3, switchChan3);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote3, 0, switchChan3);
    MIDI.sendNoteOff(switchNote3, 0, switchChan3);
    digitalWrite(ledPin3, LOW);
  }

  if (button4.risingEdge() || button4.fallingEdge()) {
    display.setCursor(100, 50);
    display.println(switchNote4);
    digitalWrite(ledPin4, HIGH);
    usbMIDI.sendNoteOn(switchNote4, switchVel4, switchChan4);
    MIDI.sendNoteOn(switchNote4, switchVel4, switchChan4);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote4, 0, switchChan4);
    MIDI.sendNoteOff(switchNote4, 0, switchChan4);
    digitalWrite(ledPin4, LOW);
  }

  if (button5.risingEdge() || button5.fallingEdge()) {
    display.setCursor(10, 10);
    display.println(switchNote5);
    digitalWrite(ledPin5, HIGH);
    usbMIDI.sendNoteOn(switchNote5, switchVel5, switchChan5);
    MIDI.sendNoteOn(switchNote5, switchVel5, switchChan5);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote5, 0, switchChan5);
    MIDI.sendNoteOff(switchNote5, 0, switchChan5);
    digitalWrite(ledPin5, LOW);
  }

  if (button6.risingEdge() || button6.fallingEdge()) {
    display.setCursor(40, 10);
    display.println(switchNote6);
    digitalWrite(ledPin6, HIGH);
    usbMIDI.sendNoteOn(switchNote6, switchVel6, switchChan6);
    MIDI.sendNoteOn(switchNote6, switchVel6, switchChan6);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote6, 0, switchChan6);
    MIDI.sendNoteOff(switchNote6, 0, switchChan6);
    digitalWrite(ledPin6, LOW);
  }

  if (button7.risingEdge() || button7.fallingEdge()) {
    display.setCursor(70, 10);
    display.println(switchNote7);
    digitalWrite(ledPin7, HIGH);
    usbMIDI.sendNoteOn(switchNote7, switchVel7, switchChan7);
    MIDI.sendNoteOn(switchNote7, switchVel7, switchChan7);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote7, 0, switchChan7);
    MIDI.sendNoteOff(switchNote7, 0, switchChan7);
    digitalWrite(ledPin7, LOW);
  }

  if (button8.risingEdge() || button8.fallingEdge()) {
    display.setCursor(70, 10);
    display.println(switchNote8);
    digitalWrite(ledPin8, HIGH);
    usbMIDI.sendNoteOn(switchNote8, switchVel8, switchChan8);
    MIDI.sendNoteOn(switchNote8, switchVel8, switchChan8);
    delay(MIDI_NOTEOFF_TIME);
    usbMIDI.sendNoteOff(switchNote8, 0, switchChan8);
    MIDI.sendNoteOff(switchNote8, 0, switchChan8);
    digitalWrite(ledPin8, LOW);
  }

  display.display();
  display.clearDisplay();
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
