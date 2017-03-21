AVRMIDI-CV
==========

This is my attempt to implement a MIDI-CV-Converter using a Atmel ATMEGA 8 microprocessor in combination with a DAC8568 as 16 Bit DAC. For the DAC8568 can only produce voltages between 0-5V I  am using some OpAmps to double that voltage to for a full range of 0-10V out of my MIDI-CV-Converter.

I started this project as I was in need of a MIDI to CV Converter for my crOwBX 4 Voice Synthesizer (see www.cs80.com/crowbx/). I did not want to buy one and therefor naturally am now building and programming my own.
It is not yet finished - no warranty at all what so ever :-) .

implemented Features
====================

* 4 Voice CV + 4 Gate
* switchable polyphonic/unison mode
* 4 additional CV outputs assignable
 * either velocity per voice...
 * ... 4 CC outputs (MIDI learn)...
 * ... or 2 soft LFO (switchable) + 2 clock divided trigger outputs
  * syncable to MIDI Clock or free running
  * adjustable LFO/clock trigger rate
  * 4 different waveshapes for the LFOs (triangle, pulse, sawtooth, reverse sawtooth)
* accurate octave tuning (~500 steps per semitone)
 * each C note can be tuned seperately to even out non-linear behavior
* MIDI learn for assigning CC controls

Two modes explained
===================
As the firmware grows more and more there are now two different modes: NORMAL\_MODE and CONTROL\_MODE
## NORMAL\_MODE
In NORMAL\_MODE the unit works as a MIDI-CV converter to be played polyphonically or in unison. 4 Voices just as explained above - nothing too special here.
## CONTROL\_MODE
In CONTROL\_MODE different parameters of the firmware can be set up to liking and saved to the EEPROM (persistent throughout reboot).
This includes
* Octave tuning
* CC assignments
* Velocity/CC mode flag

To reach CONTROL\_MODE the button connected to PC0 must be held down for at least 2 seconds (LED flashes fast while pressing and then changes to slower flashing when ready for CONTROL\_MODE). If the button is not pressed until the flashing light flashes slow the unit switches back to NORMAL\_MODE.
To exit CONTROL\_MODE saving the adjustments the button connected to PC0 must be held down again for at least 2 seconds (LED flashes fast while pressing and then changes to constant light when back to NORMAL\_MODE). If the Button is not presset until the flashing light changes to constant light the adjustments are not saved to EEPROM and thus the editing in CONTROL\_MODE is aborted.
In CONTROL\_MODE the MIDI-CV converter switches to unison. It will however switch back to whatever has been chosen on the UI on exit of CONTROL\_MODE.

### Octave tuning
In CONTROL\_MODE the octave tuning can be achieved as followes:
1. pressing any note C of an octave (except lowest C (MIDI Note 0) which cannot be adjusted) selects this octave to be tuned
2. while still pressing note C of the octave to be tuned press
 * note D to coarse tune down the C
 * note E to finer tune down the C
 * note F to make final adjustments downwards
 * note G to make final adjustments upwards
 * note A to finer tune up the C
 * note B to coarse tune up the C
3. repeat the steps for all octaves as necessary

### CC assignemnt (MIDI learn)
In CONTROL\_MODE CC assignment (MIDI learn) is achieved by holding down the 
* lowest C (MIDI Note 0) for CC-output 1
* lowest C# (MIDI Note 1) for CC-output 2
* lowest D (MIDI Note 2) for CC-output 3
* lowest D# (MIDI Note 3) for CC-output 4
The standard CC assignments are
* CC 16 - output 1
* CC 17 - output 2
* CC 18 - output 3
* CC 19 - output 4

### switching Velocity or CC output
In CONTROL\_MODE MIDI Note 4 (lowest E) toggles between using note velocity (polyphonic or unison depending on the selected mode) or CC values as source for the CV-conversion.


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

