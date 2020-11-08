/*###########################################################################
DALI Interface Demo

Dimms all lights connected to the DALI bus up and down

Works with Arduino ATMEGA328 + Mikroe DALI Click

----------------------------------------------------------------------------
Changelog:
2020-11-08 Created & tested on ATMega328 @ 8Mhz
----------------------------------------------------------------------------
		Dimmer.ino - copyright qqqlab.com / github.com/qqqlab
		
        This program is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program.  If not, see <http://www.gnu.org/licenses/>.
###########################################################################*/

#define DALI_OUTPUT_PIN 3
#define DALI_INPUT_PIN 4

#include "qqqDali.h"

Dali dali; 

void setup() {
  Serial.begin(115200);
  Serial.println("DALI Dimmer Demo");
  
  //start the DALI interfaces
  //arguments: tx_pin, rx_pin (negative values disable transmitter / receiver)
  //Note: the receiver pins should be on different PCINT groups, e.g one 
  //receiver on pin0-7 (PCINT2) one on pin8-13 (PCINT0) and one on pin14-19 (PCINT1)
  dali.begin(DALI_OUTPUT_PIN, DALI_INPUT_PIN); 
}

#define MIN_LEVEL 100   //most LED Drivers do not get much lower than this
int16_t level = 254;    //254 is max level, 1 is min level (if driver supports it), 0 is off
int8_t level_step = 4;

void loop() {
  Serial.print("set level: ");
  Serial.println(level);
  dali.set_level(level);
  
  level+=level_step;
  if(level>=254) {
    level = 254;
    level_step = -level_step;
  }
  if(level<MIN_LEVEL) {
    level = MIN_LEVEL;
    level_step = -level_step;
  }  
}
