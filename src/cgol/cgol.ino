/*

Copyright (c) 2023, Colin Michael Roberts
All rights reserved.

This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree. 

https://colinmichaelroberts.com

/////////   DESCRIPTION   /////////

An Arduino program that simulates Conway's Game of Life on a 16x16 LED grid using an Arduino board. 
The simulation showcases the emergence of patterns and life-like behaviors based on simple cellular automaton rules.
The rules are as follows:

1. Any live cell with two or three live neighbours survives.
2. Any dead cell with three live neighbours becomes a live cell.
3. All other live cells die in the next generation. Similarly, all other dead cells stay dead.

https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life

I am using a Seeed Xiao M0 dev board for this project. It should work on any Arduino compatible board with enough pins and at least 2 interrupt pins.

USAGE:
-Turn the encoder to access different pages. Defaults to page 1 on boot which is a simple glider. The pages are as follows:
  -page 0 -- generates a random grid and runs the simulation. If the simulation becomes periodic, it generates a new grid after a few seconds
  -pages 1-3 -- pre-loaded grids. Can be changed in the code by changing binary data in p1, p2, and p3 arrays
  -pages 4-6 -- grids saved from page 0
-encoder button:
  -when on page 0, pressing the button saves the initial random grid of the current simulation to page 4, 5, or 6, cycling through
  -when on any other page, pressing the button will go to page 0, generating a new grid
  -if button is held, potentiometer will control brightness
-potentiometer:
  -controls the speed of the simulation
  -controls brightness while the encoder button is held

TO DO:
-optimize encoder
-utilize EEPROM (if this board has one) to save random grids even when powered off
-create circuit diagram and get a pcb printed
-redesign case?
-build freewire version?
-draw mode?
-universe wrap toggle?
-slow scan mode, horizontal, vertical, diagonal, middle out?

/////////   CIRCUIT   /////////

-hardware:
  -Seeed Xiao M0
  -MAX7219 8x8 LED dot matrix display
  -KY-040 rotary encoder
  -10k potentiometer
  -5v power supply module
  -12v dc power supply

-12v dc power power supply in to 5v dc converter
-converter 5v output to breadboard power rail
-converter gnd to breadboard ground rail

-Xiao 5v to 5v power rail
-Xiao gnd to ground rail

-4 8x8 LED matrices on a breakout board with MAX7219 are wired in series, numbered as follows:
 ________  ________ 
 |      |==|      |=
 |   3  |==|   2  |=
 |______|==|______|=
 ________  ________ 
=|      |==|      |
=|   1  |==|   0  |
=|______|==|______|

-matrix 0 data pin to Xiao pin 8
-matrix 0 clk pin to Xiao pin 9
-matrix 0 cs pin to Xiao pin 10
-matrix 0 vcc to 5v power rail
-matrix 0 gnd to ground rail

-potentiometer wiper to Xiao pin 3
-potentiometer power to Xiao 3.3v
-potentiometer ground to Xiao gnd

-encoder clk pin to Xiao pin 2
-encoder dt pin to Xiao pin 4
-encoder sw pin to Xiao pin 1
-encoder + to 5v power rail
-encoder gnd to ground rail

*/

/////////   LIBRARIES   /////////

#include <LedControl.h>  // libraries
#include <binary.h>      //
#include <Rotary.h>      //
#include <JC_Button.h>   //

/////////   HARDWARE   /////////

LedControl mat = LedControl(8, 9, 10, 4);  // led matrix definition

const int pot = 3;                // potentiometer for refreshRate
int potVal = 0;                   // variable for potentiometer
int minSpeed = 42;                // min and max refresh rate in ms, controlled by potentiometer
int maxSpeed = 2500;
int brightness;
int buttonLongPress = 500;        // amount of time the encoder button needs to be pressed to update brightness

const int enc1 = 2;               // encoder interrupt pins
const int enc2 = 4;
Rotary enc = Rotary(enc1, enc2);  // encoder definition
Button butt1(1, 40);              // encoder button

/////////   vARIABLES   /////////

int refreshRate = 100;                 // refresh rate variable in ms, controlled by potentiometer
unsigned long periodicTimeout = 5000;  // timeout for a periodic grid to reset
bool periodic = false;                 // true when a random grid becomes periodic

volatile unsigned long current;
volatile unsigned long previous = 0;
volatile unsigned long timeoutCurrent;
volatile unsigned long timeoutPrevious = 0;

int patternSel = 1;  // pattern select controlled with encoder
int lim = 9;         // variables for pattern limit
int counter = 0;     // counter for saving random patterns
int g = 0;           // generation counter

const int x = 2;      // number of modules, x axis
const int y = 2;      // number of modules, y axis
const int s = 8;      // module size in leds, assuming sqaure
const int n = y * s;  // led grid y
const int m = x * s;  // led grid x

/////////   GRIDS   /////////

unsigned int a[n][x];    // LED matrix, must be unsigned for bitshift to work
unsigned int aTemp[n];   // temporary copies of A grid for updating data
unsigned int aTemp2[n];  // 16x1 instead of 8x2 for simpler bit math
unsigned int mem[n][x];  // memory for saving new patterns
unsigned int r1[n];      // previous grids for checking priodicity
unsigned int r2[n];
unsigned int r3[n];
unsigned int r4[n];
unsigned int r64[n];

/* patterns */
unsigned int p1[n][x] = { { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00111000, 0b00000000 },
                          { 0b00001000, 0b00000000 },
                          { 0b00010000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },

                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 } };
unsigned int p2[n][x] = { { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000101, 0b10000000 },
                          { 0b00001000, 0b00010000 },
                          { 0b00011000, 0b10010000 },
                          { 0b11010000, 0b01100000 },

                          { 0b11010000, 0b01100000 },
                          { 0b00011000, 0b10010000 },
                          { 0b00001000, 0b00010000 },
                          { 0b00000101, 0b10000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 } };
unsigned int p3[n][x] = { { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00011100, 0b01110000 },
                          { 0b00100010, 0b10001000 },
                          { 0b00100010, 0b10001000 },
                          { 0b01011110, 0b11110100 },

                          { 0b01100000, 0b00001100 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000110, 0b11000000 },
                          { 0b00001010, 0b10100000 },
                          { 0b00000100, 0b01000000 },
                          { 0b00000000, 0b00000000 },
                          { 0b00000000, 0b00000000 } };

/* empty grids for saving random patterns */
unsigned int p4[n][x];
unsigned int p5[n][x];
unsigned int p6[n][x];
/* draw mode grids */
// unsigned int p7[n][x];
// unsigned int p8[n][x];
// unsigned int p9[n][x];
/* ripple grids */
unsigned int rip[15][n][x] = { { { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000001, 0b10000000 },

                                 { 0b00000001, 0b10000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 } },

                               { { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000011, 0b11000000 },

                                 { 0b00000011, 0b11000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 } },

                               { { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000111, 0b11100000 },

                                 { 0b00000111, 0b11100000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 } },

                               { { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00001111, 0b11110000 },

                                 { 0b00001111, 0b11110000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 } },

                               { { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00011111, 0b11111000 },

                                 { 0b00011111, 0b11111000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 } },

                               { { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00111111, 0b11111100 },

                                 { 0b00111111, 0b11111100 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000000, 0b00000000 },
                                 { 0b00000000, 0b00000000 } },

                               { { 0b00000000, 0b00000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b01111111, 0b11111110 },

                                 { 0b01111111, 0b11111110 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000001, 0b10000000 },
                                 { 0b00000000, 0b00000000 } },

                               { { 0b00000001, 0b10000000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b11111111, 0b11111111 },

                                 { 0b11111111, 0b11111111 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00000011, 0b11000000 },
                                 { 0b00000001, 0b10000000 } },

                               { { 0b00000011, 0b11000000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },

                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00000111, 0b11100000 },
                                 { 0b00000011, 0b11000000 } },

                               { { 0b00000111, 0b11100000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },

                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00001111, 0b11110000 },
                                 { 0b00000111, 0b11100000 } },

                               { { 0b00001111, 0b11110000 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },

                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b00011111, 0b11111000 },
                                 { 0b00001111, 0b11110000 } },

                               { { 0b00011111, 0b11111000 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },

                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b00111111, 0b11111100 },
                                 { 0b00011111, 0b11111000 } },

                               { { 0b00111111, 0b11111100 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },

                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b01111111, 0b11111110 },
                                 { 0b00111111, 0b11111100 } },

                               { { 0b01111111, 0b11111110 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },

                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b01111111, 0b11111110 } },

                               { { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },

                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 },
                                 { 0b11111111, 0b11111111 } } };

unsigned int ripMem[15][n][x];

/////////   LOOP   /////////

void setup() {

  Serial.begin(9600);

  Serial.println("start");

  for (int i = 0; i < (x * y); i++) {
    mat.shutdown(i, false);
    mat.setIntensity(i, 0);
    mat.clearDisplay(i);
  }

  // pinMode(enc1, INPUT);
  // pinMode(enc2, INPUT);

  attachInterrupt(digitalPinToInterrupt(enc1), pattern, CHANGE);
  attachInterrupt(digitalPinToInterrupt(enc2), pattern, CHANGE);
  butt1.begin();

  for (int i = 0; i < (x * y); i++) {
    mat.shutdown(i, false);
    mat.setIntensity(i, 0);
    mat.clearDisplay(i);
  }

  randomSeed(analogRead(0));

  // test();
  rippleTest();

  memcpy(a, p1, sizeof(a));
  updateDisplay(a);

  previous = millis();
  timeoutPrevious = millis();
}

void loop() {

  readPot();
  readButt();

  current = millis();

  if (current - previous >= refreshRate) {
    // Serial.print(potVal);
    // Serial.print(" - "); 
    // Serial.println(refreshRate);

    rules();  // cgol rules

    previous = current;

    timeoutCurrent = millis();

    if (periodic == false) {
      checkPeriodic();
    } else {
      if (timeoutCurrent - timeoutPrevious >= periodicTimeout) {
        patterns();
      }
    }
  }

  delay(1);
}

/////////   INPUT   /////////

void readPot() {

  /*
    potentiometer controls speed, when button is long pressed and held
  */

  potVal = analogRead(pot);
  refreshRate = map(potVal, 970, 10, minSpeed, maxSpeed);
  refreshRate = constrain(refreshRate, 42, 2500);

  if (butt1.pressedFor(buttonLongPress)) {
    brightness = map(potVal, 0, 970, 0, 15);
    for (int i = 0; i < (x * y); i++) {
      mat.setIntensity(i, brightness);
    }
  }
}

void readButt() {

  /*
    encoder button: saves random grids when patternSel == 0, goes to new random grid otherwise, hold to chang brightness with knob
  */

  butt1.read();


  if (butt1.wasPressed()) {
    if (patternSel == 0) {
      int c = counter % 3;
      Serial.println(c);
      if (c == 0) {
        memcpy(p4, mem, sizeof(mem));
        counter++;
        patternSel = 4;
        patterns();
      } else if (c == 1) {
        memcpy(p5, mem, sizeof(mem));
        counter++;
        patternSel = 5;
        patterns();
      } else if (c == 2) {
        memcpy(p6, mem, sizeof(mem));
        counter++;
        patternSel = 6;
        patterns();
      }
    } else {
      patternSel = 0;
      patterns();
    }
  }
}

/////////   DISPLAY   /////////

void generateData() {
  long rando;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < x; j++) {
      rando = random(256);
      //Serial.println(rando);
      a[i][j] = rando;
      mem[i][j] = rando;
    }
  }
}

void updateDisplay(unsigned int u[n][x]) {  // display driver
  for (int i = 0; i < s; i++) {
    mat.setRow(0, i, u[i + 8][1]);
    mat.setRow(1, i, u[i + 8][0]);
    mat.setRow(2, i, u[i][1]);
    mat.setRow(3, i, u[i][0]);
  }
}

void updateDisplayScan(unsigned int u[n][x]) {  // display driver with progressive scan lines

  int z = map(refreshRate, 42, 1000, 0, 50); // scan rate

  for (int i = 0; i < s; i++) {
    mat.setRow(2, i, u[i][1]);
    mat.setRow(3, i, u[i][0]);
    delay(z / 2);
  }

  for (int i = 0; i < s; i++) {
    mat.setRow(0, i, u[i + 8][1]);
    mat.setRow(1, i, u[i + 8][0]);
    delay(z / 2);
  }
}

void updateDisplayRipple(unsigned int u[n][x]) {

  int z = map(refreshRate, 42, 1000, 0, 20);

  for (int q = 0; q < 15; q++) {
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < x; j++) {
        ripMem[q][i][j] = u[i][j] & rip[q][i][j];
      }
    }
  }

  for (int q = 0; q < 15; q++) { 
    for (int i = 0; i < s; i++) {
      mat.setRow(0, i, ripMem[q][i + 8][1]);
      mat.setRow(1, i, ripMem[q][i + 8][0]);
      mat.setRow(2, i, ripMem[q][i][1]);
      mat.setRow(3, i, ripMem[q][i][0]);
    }
    delay(z);
  }

}

void clearDisplays() {
  for (int i = 0; i < (x * y); i++) {
    mat.clearDisplay(i);
  }
}

/////////   RULES   /////////

void rules() {  // cgol rules, called in loop

  int total;

  for (int i = 0; i < n; i++) {
    aTemp[i] = (a[i][0] << 8) + a[i][1];
    aTemp2[i] = aTemp[i];
  }

  for (int i = 0; i < n; i++) {
    for (int k = 0; k < m; k++) {

      total = bitRead(aTemp[(i + (n - 1)) % n], (k + (m - 1)) % m) + bitRead(aTemp[i], (k + (m - 1)) % m) + bitRead(aTemp[(i + 1) % n], (k + (m - 1)) % m) +

              bitRead(aTemp[(i + (n - 1)) % n], k) + bitRead(aTemp[(i + 1) % n], k) +

              bitRead(aTemp[(i + (n - 1)) % n], (k + 1) % m) + bitRead(aTemp[i], (k + 1) % m) + bitRead(aTemp[(i + 1) % n], (k + 1) % m);

      if (bitRead(aTemp[i], k) == 1) {
        if (total < 2 || total > 3) {
          bitWrite(aTemp2[i], k, 0);
        }
      } else {
        if (total == 3) {
          bitWrite(aTemp2[i], k, 1);
        }
      }
    }
  }

  for (int i = 0; i < n; i++) {
    a[i][0] = aTemp2[i] >> 8;
    a[i][1] = aTemp2[i] & 255;
  }
  updateDisplayScan(a);
  g++;
  // Serial.print("Gen: ");
  // Serial.println(g);
}

bool checkPeriodic() {

  memmove(r4, r3, sizeof(r3));  // move previous generations up to next memory slot
  memmove(r3, r2, sizeof(r2));
  memmove(r2, r1, sizeof(r1));

  for (int i = 0; i < n; i++) {
    r1[i] = (a[i][0] << 8) + a[i][1];  // convert a[][] array into a 1-dimensional array for easier comparison
  }

  int check1 = memcmp(r1, r2, sizeof(r1));  // check for repetition
  int check2 = memcmp(r1, r3, sizeof(r1));
  int check3 = memcmp(r1, r4, sizeof(r1));

  if (g % 64 == 0) {
    int check64 = memcmp(r1, r64, sizeof(r1));  // glider exception
    if (check64 == 0) {
      periodic = true;
      timeoutPrevious = timeoutCurrent;
    }
    memcpy(r64, r1, sizeof(r1));
  }

  if (check1 == 0 || check2 == 0 || check3 == 0) {
    periodic = true;
    timeoutPrevious = timeoutCurrent;
  }
}

void pattern() {

  unsigned char result = enc.process();

  if (result == DIR_CW) {
    patternSel++;
    patternSel = constrain(patternSel, 0, lim);
    // Serial.println(patternSel);
    patterns();
  } else if (result == DIR_CCW) {
    patternSel--;
    patternSel = constrain(patternSel, 0, lim);
    // Serial.println(patternSel);
    patterns();
  }
}

void patterns() {
  if (patternSel == 0) {
    periodic = false;
    generateData();
    updateDisplay(a);
    previous = current;
  } else if (patternSel == 1) {
    memcpy(a, p1, sizeof(a));
    updateDisplay(a);
    previous = current;
  } else if (patternSel == 2) {
    memcpy(a, p2, sizeof(a));
    updateDisplay(a);
    previous = current;
  } else if (patternSel == 3) {
    memcpy(a, p3, sizeof(a));
    updateDisplay(a);
    periodic = false;
    previous = current;
  } else if (patternSel == 4) {
    memcpy(a, p4, sizeof(a));
    updateDisplay(a);
    periodic = false;
    previous = current;
  } else if (patternSel == 5) {
    memcpy(a, p5, sizeof(a));
    updateDisplay(a);
    periodic = false;
    previous = current;
  } else if (patternSel == 6) {
    memcpy(a, p6, sizeof(a));
    updateDisplay(a);
    periodic = false;
    previous = current;
    // } else if (patternSel == 7) {
    //   memcpy(a, p7, sizeof(a));
    //   updateDisplay(a);
    //   previous = current;
    // } else if (patternSel == 8) {
    //   memcpy(a, p8, sizeof(a));
    //   updateDisplay(a);
    //   previous = current;
    // } else if (patternSel == 9) {
    //   memcpy(a, p9, sizeof(a));
    //   updateDisplay(a);
    //   previous = current;
  }
}

void test() {
  clearDisplays();
  mat.setLed(3, 0, 0, 1);
  delay(400);
  mat.setLed(2, 1, 1, 1);
  delay(400);
  mat.setLed(1, 2, 2, 1);
  delay(400);
  mat.setLed(0, 3, 3, 1);
  delay(1000);
  clearDisplays();
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      mat.setLed(0, i, j, 1);
      mat.setLed(1, i, j, 1);
      mat.setLed(2, i, j, 1);
      mat.setLed(3, i, j, 1);
      delay(100);
    }
    clearDisplays();
  }
}

void rippleTest() {

  for (int q = 0; q < 15; q++) {
    for (int i = 0; i < s; i++) {
      mat.setRow(0, i, rip[q][i + 8][1]);
      mat.setRow(1, i, rip[q][i + 8][0]);
      mat.setRow(2, i, rip[q][i][1]);
      mat.setRow(3, i, rip[q][i][0]);
    }
    delay(50);
  }
}
