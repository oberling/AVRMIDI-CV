AVRMIDI-CV
==========

This is my attempt to implement a MIDI-CV-Converter using a Atmel ATMEGA 8 microprocessor in combination with a DAC8568 as 16 Bit DAC. For the DAC8568 can only produce voltages between 0-5V I  am using some OpAmps to double that voltage to for a full range of 0-10V out of my MIDI-CV-Converter.

I started this project as I was in need of a MIDI to CV Converter for my crOwBX 4 Voice Synthesizer (see www.cs80.com/crowbx/). I did not want to buy one and therefor naturally am now building and programming my own.
It is not yet finished - no warranty at all what so ever :-) .

implemented Features
====================

* 4 Voice CV + 4 Gate
* switchable polyphonic/unison mode
* either velocity per voice...
* ... or 2 soft LFO (switchable) + 2 clock divided trigger outputs
 * syncable to MIDI Clock
 * adjustable LFO/clock trigger rate
 * 4 different waveshapes for the LFOs (triangle, pulse, sawtooth, reverse sawtooth)

Teststatus
==========

The code has been tested on the hardware prototype PCB designed for it and runs as expected.
I have a growing testing program to test whether or not the data structures and algorithms/functions work as expected (on my Linux-PC).

Tested in hardware so far:
* MIDI-IN
* DAC output voltages (polyphonic mode, unison mode, lfo and velocity outputs)
* doubling OpAmp-circuit
* syncing LFO to MIDI Clock in different rates
* rate adjustment in synced and free running LFO mode via potentiometers
* shift registers input for multiple switches
* MIDI channel switching via dip switch
* cmos logic for switching the lfo wave forms to get 4 switch positions down to 2 input bits

Test not completed for:
* MIDI THRU
* Clock divided trigger outputs

TODO
====
* update the PCB layout to remove the known bugs from the prototype PCB
* test remaining untested parts :) little
* add a bootloader
* add MIDI learn for MIDI CC instead of velocity (since velocity seems to be not that useful after all)

