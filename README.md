# DALI-arduino
DALI LED Lighting Interface. Control LED Drivers with a microcontroller.

The code in qqqDali.cpp and qqqDali.h does not depend on Arduino, can be used in any C++ project by copying qqqDali_ATMega328.h and adding hardware specific code for a periodic interrupt and pin change interrupt.

Examples for Arduino ATMEGA328 included:
- Dimmer: Dims all lamps up and down
- Commissioning: Assign short addresses to lamps
- Monitor: Monitor DALI bus data

Needs a DALI hardware interface such as Mikroe DALI click. Or use this very basic DALI interface design for your experiments. 

```
Micro-        5.6V         ___      DALI BUS
controller    Zener  +----|___|---- 16-22V
        ___   Diode  |    100
 RX ---|___|---|>|---+------------- DALI+
        10K          |  
        ___        |/
 TX ---|___|-------|    PNP Transistor
        220        |\   30V 250mA
                     V
GND -----------------+------------- DALI-
 ```
 NOTES: 
 - For this interface, reverse the polarity of TX in the code.
 - Might not work with all DALI components because of unmatched voltage levels.
 
 
