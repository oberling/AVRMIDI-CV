AVRMIDI-CV
==========

This is my attempt to implement a MIDI-CV-Converter using a Atmel ATMEGA 8 microprocessor in combination with a DAC8568 as 16 Bit DAC. For the DAC8568 can only produce voltages between 0-5V I also plan to use some OpAmps to just double that voltage to get a full range of 0-10V out of my MIDI-CV-Converter.

I started this project as I was in need of a MIDI to CV Converter for my crOwBX 4 Voice Synthesizer (see www.cs80.com/crowbx/). I did not want to buy one and therefor naturally am now building and programming my own.
It is not yet finished - no warranty at all what so ever :-) .

implemented Features
====================

* 4 Voice CV + 4 Gate + 4 Trigger outputs
* Polyphonic mode
* Unison mode
* Retrigger (time based; MIDI Clock synchronized)

Teststatus
==========

The code is written more or less blindly by me - it is not yet extensively tested on the device (Atmel ATMEGA 8).
I have however a little testing program to test whether or not the data structures and algorithms/functions work as expected (on my Linux-PC).
Thats why I can safly say that the code compiles and works according to those tests.
I am now moving slowly from software- to hardware-tests building the circuit part-by-part and testing each module.

TODO
====
* design a circuit
 * especially design the OpAmp-circuit to double the 5V-voltages for key and velocity
 * last not least design a PCB layout
* test on device

