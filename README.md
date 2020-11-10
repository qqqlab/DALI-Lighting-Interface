# DALI-arduino
DALI LED Lighting Interface

Control LED Drivers with a microcontroller.

Examples for Arduino ATMEGA328 included. 

Core code does not depend on Arduino, can be used in any C++ project by adding hardware specific code. 

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
                                     +----------------------<< DALI -
 ```
 NOTE: For this interface, reverse the polarity of TX in the code.
 
 
