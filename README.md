# DALI Lighting Interface
DALI LED Lighting Interface. Control LED Drivers with a microcontroller.

The code in qqqDali.cpp and qqqDali.h does not depend on Arduino, can be used in any C++ project by writing hardware specific hooks for a periodic interrupt.

Examples included:
- Dimmer: Dims all lamps up and down
- Commissioning: Assign short addresses to lamps
- Monitor: Monitor DALI bus data

Needs a DALI hardware interface such as Mikroe DALI click. Or use this very basic DALI interface design for your experiments. 

```
3-5V Micro-          5.6V               ___      
controller           Zener        +----|___|---- 12V Power Supply 
             ___     Diode        |     220
 RX ---+----|___|-----|>|---------+------------- DALI+
       |     10K                  |  
      +-+                         |                 
      | |100K           ___     |/   PNP             DALI BUS
      +-+        TX ---|___|----|    Transistor
       |                 1K     |\   
       |                          V
GND ---+--------------------------+------------- DALI-
 ```
NOTE: For this interface, reverse the polarity of TX in the code.

### Explanation of the DIY circuit

TX pin: The DALI bus needs to be powered with a 16V (9.5V to 22.5V) power supply, current limited to 250mA. In the circuit the 220 ohm resister is the current limiter (gives approx 50 mA with 12V power supply). Using a lower current allows us to use a common general purpose 100mA PNP transistor such as BC547 or similar. 

RX pin: On the DALI bus a logic low is -6.5V to 6.5V, a logic high is 9.5V to 22.5V. The zener diode takes removes the 6.5V offset of the DALI signal. The 10K resistors together with the shunt diodes in the GPIO port protect the microcontroller from overcurrent & overvoltage. The 100K pull down resistor ensures that a low DALI bus state results in a low RX pin.

DALI specifies that each device on the bus should not draw more than 2mA, and the minimum bus voltage is approx 10V. So every device on the bus will lower the supply voltage by 220 Ohm * 2mA = 0.5V, so this simple circuit with a 12V supply should work with up to 4 devices on the bus. If you want more devices, use a higher supply voltage and/or lower the 220 Ohm resistor value.
 
