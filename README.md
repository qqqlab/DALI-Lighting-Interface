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
controller    Zener  +----|___|---- 10-22V
        ___   Diode  |     100
 RX ---|___|---|>|---+------------- DALI+
        10K          |  
        ___        |/
 TX ---|___|-------|    PNP Transistor
        220        |\   30V 250mA
                     V
GND -----------------+------------- DALI-
 ```
NOTE: For this interface, reverse the polarity of TX in the code.

### Explanation of the circuit

The DALI bus needs to be powered with a 16V (9.5V to 22.5V) power supply, current limited to 250mA. In the circuit the 100 ohm resister acts as the current limiter. A logic low is -6.5V to 6.5V, a logic high is 9.5V to 22.5V. The zener diode takes removes the 6.5V offset, and the 10K resistor together with the shunt diodes in the GPIO port protect the microcontroller from overcurrent & overvoltage.
 
