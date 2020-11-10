# DALI-arduino
DALI LED Lighting Interface. Control LED Drivers with a microcontroller.

Core code in qqqDali.cpp and qqqDali.h does not depend on Arduino, can be used in any C++ project copying qqqDali_ATMega328.h by adding hardware specific code.

Examples for Arduino ATMEGA328 included:
- Dimmer: Dims all lamps up and down
- Commissioning: Assign short addresses to lamps
- Monitor: Monitor DALI bus data

Needs a DALI hardware interface such as Mikroe DALI click. Or use this very basic DALI interface design for your experiments. 

```
  DALI BUS Power   +---------+
      12-22V >>----+ 100 Ohm +--------+--------------------<< DALI +
                   +---------+        |
                                      |
                   +---------+        |
       uC RX >>----+ 10K Ohm +--------+ 
                   +---------+        |                 
                                      |                       DALI BUS
                                     /
                                   |/
       uC TX >>--------------------K    PNP Transistor
                                   |\
                                     V
                                     |
         GND >>----------------------+----------------------<< DALI -
 ```
 NOTES: 
 - For this interface, reverse the polarity of TX in the code.
 - Might not work with all DALI components because of unmatched voltage levels.
 
 
