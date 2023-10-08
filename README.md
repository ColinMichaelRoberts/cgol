# cgol

An Arduino program that simulates Conway's Game of Life on a 16x16 LED grid using an Arduino-compatible board. 
The simulation showcases the emergence of patterns and life-like behaviors based on simple cellular automaton rules.
The rules are as follows:

1. Any live cell with two or three live neighbours survives.
2. Any dead cell with three live neighbours becomes a live cell.
3. All other live cells die in the next generation. Similarly, all other dead cells stay dead.

Read more about Conway's Game of Life [here](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)

![Game of Life simulation LED display](/media/cgol-1.jpg)

I am using a Seeed Xiao M0 dev board for this project. It should work on any Arduino compatible board with enough pins and at least 2 interrupt pins.


## Usage

Turn the encoder to access different pages. Defaults to page 1 on boot which is a simple glider. The pages are as follows:
- page 0 -- generates a random grid and runs the simulation. If the simulation becomes periodic, it generates a new grid after a few seconds
- pages 1-3 -- pre-loaded grids. Can be changed in the code by changing binary data in p1, p2, and p3 arrays
- pages 4-6 -- grids saved from page 0

Encoder button:
- when on page 0, pressing the button saves the initial random grid of the current simulation to page 4, 5, or 6, cycling through
- when on any other page, pressing the button will go to page 0, generating a new grid
- if button is held, potentiometer will control brightness

Potentiometer:
- controls the speed of the simulation
- controls brightness while the encoder button is held

Check out my video on the project [here](https://www.youtube.com/watch?v=iLSJuxAfPZc)


## Circuit

![Game of Life internal circuit](/media/cgol-open.jpg)

Hardware:
- Seeed Xiao M0
- MAX7219 8x8 LED dot matrix display
- KY-040 rotary encoder
- 10k potentiometer
- 5v power supply module
- 12v dc power supply

![Circuit schematic](/media/cgol-schematic.jpg)

* note: everything is 5v except potentiometer on 3.3v from Xiao

Connections:

- 4 8x8 LED matrices on a breakout board with MAX7219 are wired in series
- matrices are ordered 0, 1, 2, 3, wired bottom right, bottom left, top right, top left, respectively

- matrix 0 data pin to Xiao pin 8
- matrix 0 clk pin to Xiao pin 9
- matrix 0 cs pin to Xiao pin 10

- potentiometer wiper to Xiao pin 3

- encoder clk pin to Xiao pin 2
- encoder dt pin to Xiao pin 4
- encoder sw pin to Xiao pin 1


## TO DO

- optimize encoder
- utilize EEPROM to save grids through power cycle
- build circuit diagram and get a pcb printed
- redesign 3d-printed case
- build freewire version?
- add more functionality, draw mode, universe wrap toggle, different scan modes

![Game of Life LEDs close-up](/media/cgol-2.jpg)


## License

This project is licensed under the MIT License(https://opensource.org/license/mit/).


## Libraries

This project uses the LedControl.h library, which is distributed under the MIT License.

Library Name: LedControl.h
Author: Eberhard Fahle
License: MIT License, https://opensource.org/license/mit/
Library Homepage: https://wayoda.github.io/LedControl/


This project uses the JC_Button.h library, which is distributed under the MIT License.

Library Name: JC_Button.h
Author: Jack Christensen
License: GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
Library Homepage: https://github.com/JChristensen/JC_Button


## Contact

https://colinmichaelroberts.com/contact

Visit my [homepage](https://colinmichaelroberts.com) for other projects!
