# eightbuttonMIDIfootswitch
An eight button USB/5 pin MIDI footswitch

I built this to go along with my Synthstrom Deluge as a way to manage the new looping functionality in the (pending) 3.0 firmware. 

The idea is to build a footswitch that sends MIDI messages via USB and regular 5 pin MIDI. I am not a coder, a 3D designer, a good solderer(er), etc so if you are good at any of those things you will likely see things you will not be proud of. I'm just happy to be here and happy to share what I've learned, hopefully you can use this information to make yourself something.

Items used in this build:
- 3D Printer
- Soldering iron
- Tape
- Teensy ++2.0
- 8 x Guitar Footswitch (https://www.aliexpress.com/item/32826054526.html) - I did not realize these were latching switches when I ordered them, momentary switches would have been a better choice.
- 0.96 OLED I2C display (https://www.aliexpress.com/item/32896971385.html)
- 8 5mm 5V LEDs from eBay (I don't have a link to the specific item, they have resistors built into the wiring)
- 5 pin MIDI port (https://www.aliexpress.com/item/32972269819.html)
- 7mm momentary push button (https://www.aliexpress.com/item/32790920961.html) as an easy way to update the Teensy without opening the case. Eventually I will replace this with a DC input jack.

Pinout/wiring diagram:
![Teensy ++2.0 Pinout](https://raw.githubusercontent.com/hunked/eightbuttonMIDIfootswitch/master/pinout.png)

More instructions and better diagrams to follow. 
Code is WIP, I am currently having issues with phantom MIDI messages being sent at poweron that will hopefully be fixed soon.
