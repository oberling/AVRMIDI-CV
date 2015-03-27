AVRMIDI-CV
==========

This is my attempt to implement a MIDI-CV-Converter using a Atmel ATMEGA 8 microprocessor in combination with a DAC8568 as 16 Bit DAC. For the DAC8568 can only produce voltages between 0-5V I also plan to use some OpAmps to just double that voltage to get a full range of 0-10V out of my MIDI-CV-Converter.

I started this project as I was in need of a MIDI to CV Converter for my crOwBX 4 Voice Synthesizer (see www.cs80.com/crowbx/). I did not want to buy one and therefor naturally am now building and programming my own.
It is not yet finished - no warranty at all what so ever :-) .

implemented Features
====================

* 4 Voice CV + 4 Gate
 * + 4 TRIGGER in circuitry
* switchable polyphonic/unison mode
* either velocity per voice...
* ... or 2 soft LFO (switchable) + 2 clock divided trigger outputs
 * syncable to MIDI-Clock
 * adjustable LFO/clock trigger rate
 * 3 different waveshapes for the LFOs (triangle, pulse, sawtooth)

Teststatus
==========

The code is written more or less blindly by me - it is not yet extensively tested on the device (Atmel ATMEGA 8).
I have however a little testing program to test whether or not the data structures and algorithms/functions work as expected (on my Linux-PC).
Thats why I can safly say that the code compiles and works according to those tests.
I am now moving slowly from software- to hardware-tests building the circuit part-by-part and testing each module.

Tested in Hardware so far:
* MIDI-IN
* DAC output voltages

Test not completed for:
* shift register input for program modes
* GATE+Trigger via transistor
* doubling OpAmp-circuit

TODO
====
* design a circuit
 * check if doubling voltage circuit works better with +-15V
 * some simple transistors for GATE and trigger outputs
 * convert GATE to TRIGGER (4050) - make it switchable different voltage levels
 * last not least design a PCB layout
* test on device

