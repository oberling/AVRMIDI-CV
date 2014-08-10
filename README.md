AVRMIDI-CV
==========

This is my attempt to implement a MIDI-CV-Converter using a Atmel ATMEGA 8 microprocessor in combination with a DAC8568 as 16 Bit DAC. For the DAC8568 can only produce Voltages between 0-5V I also plan to use some Op-Amps to just double that voltage to get a full range of 0-10V out of my MIDI-CV-Converter.

I started this project as I was in need of a MIDI to CV Converter for my crOwBX 4 Voice Synthesizer (see www.cs80.com/crowbx/). I did not want to buy one and therefor naturally am now building my own.
It is not yet finished - no warranty at all what so ever :-) .

implemented Features
====================

* 4 Voice CV + 4 Gate + 4 Trigger Outputs
* Polyphonic mode
* Unison mode
* Retrigger (time based; MIDI Clock synchronized)

Teststatus
==========

The code is written blindly by me - it is not yet tested on the device (Atmel ATMEGA 8). I only compile it with the avr-gcc toolchain by now to see if it still fits the device and if there are any syntax errors and stuff. Because I didn't test it on the device I instead have a little testing program to test whether or not the data structures and algorithms/functions work as expected.
Therefor i can now safly say that the code compiles and works according to the tests.
What i have not yet tested and cannot test in software only is if it also works on the device - especially if my I/O-Functions work as expected.

TODO
====
* design a circuit
* test on device

