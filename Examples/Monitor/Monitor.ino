/*###########################################################################
DALI Interface Demo

Monitors all messages on the DALI bus

----------------------------------------------------------------------------
Changelog:
2020-11-10 Created & tested on ATMega328 @ 8Mhz
----------------------------------------------------------------------------
		Monitor.ino - copyright qqqlab.com / github.com/qqqlab
		
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

#include "qqqDali_ATMega328.h"

Dali_ATMega328 dali; 

uint8_t rx_out=0;
volatile uint8_t rx_in=0;
uint8_t rx_data[256];

//callback to handle received data on dali2 interface - executes in interrupt context, store data in buffer and display in loop()
void dali_receiver(Dali *d, uint8_t *data, uint8_t len) {
  //push data in buffer
  int16_t freebuf = (int16_t)256 + rx_in - rx_out - len - 2;
  if(freebuf<=1) return; //buffer full
  uint8_t j = rx_in;
  rx_data[j++] = len; //push length
  for(uint8_t i=0;i<len;i++) rx_data[j++]=data[i]; //push data
  rx_in = j; //increment in pointer upon completion
}


void setup() {
  Serial.begin(115200);
  Serial.println("DALI Monitor Demo");
  
  //start the DALI interfaces
  //arguments: tx_pin, rx_pin (negative values disable transmitter / receiver)
  //Note: the receiver pins should be on different PCINT groups, e.g one 
  //receiver on pin0-7 (PCINT2) one on pin8-13 (PCINT0) and one on pin14-19 (PCINT1)
  dali.begin(DALI_OUTPUT_PIN, DALI_INPUT_PIN); 

  //attach a received data handler
  dali.EventHandlerReceivedData = dali_receiver;  
}

#define MIN_LEVEL 100   //most LED Drivers do not get much lower than this
int16_t level = 254;    //254 is max level, 1 is min level (if driver supports it), 0 is off
int8_t level_step = 4;

void loop() {  
  if(rx_out!=rx_in) {
    //Serial.print("RX:");
    uint8_t len = rx_data[rx_out++];   
    for(uint8_t i=0;i<len;i++) {
      Serial.print(rx_data[rx_out++],HEX);
      Serial.print(' ');
    }
    Serial.println();
  }
}
