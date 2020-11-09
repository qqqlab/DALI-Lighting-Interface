/*###########################################################################
DALI Interface Demo

Commissioning demo - assign short addresses to all LED Drivers on the DALI bus

Works with Arduino ATMEGA328 + Mikroe DALI Click

----------------------------------------------------------------------------
Changelog:
2020-11-08 Created & tested on ATMega328 @ 8Mhz
----------------------------------------------------------------------------
    Commisioning.ino - copyright qqqlab.com / github.com/qqqlab
    
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
  Serial.println("DALI Commisioning Demo");
  
  //start the DALI interfaces
  //arguments: tx_pin, rx_pin (negative values disable transmitter / receiver)
  //Note: the receiver pins should be on different PCINT groups, e.g one 
  //receiver on pin0-7 (PCINT2) one on pin8-13 (PCINT0) and one on pin14-19 (PCINT1)
  dali.begin(DALI_OUTPUT_PIN, DALI_INPUT_PIN); 

  menu();
}


void loop() {
  while (Serial.available() > 0) {
    int incomingByte = Serial.read();      
    switch(incomingByte) {
      case '1': scan_short_addr(); menu(); break;
      case '2': delete_short_addr(); menu(); break;
      case '3': commission_wo_short(); menu(); break;
      case '4': commission_all(); menu(); break;
      case '5': flash(); menu(); break;    }
  }
}

void menu() {
  Serial.println("----------------------------");
  Serial.println("1 Scan all short addresses");
  Serial.println("2 Dele1te all short addresses");
  Serial.println("3 Commission w/o short adr");
  Serial.println("4 Commission all short addresses");
  Serial.println("5 Flash all lights");
  Serial.println("----------------------------");  
}

void delete_short_addr() {
  Serial.println("Running: Delete all short addresses");
  //remove all short addresses
  dali.cmd(DALI_DATA_TRANSFER_REGISTER0,0xFF);
  dali.cmd(DALI_SET_SHORT_ADDRESS, 0xFF);
}

void scan_short_addr() {
  Serial.println("Running: Scan all short addresses");
  uint8_t sa;
  for(sa = 0; sa<64; sa++) {
    int16_t rv = dali.cmd(DALI_QUERY_STATUS,sa);
    if(rv!=DALI_RESULT_NO_REPLY) {
      Serial.print(sa);
      Serial.print(" status=0x");
      Serial.print(rv,HEX);
      Serial.print(" minLevel=");
      Serial.print(dali.cmd(DALI_QUERY_MIN_LEVEL,sa));
      for(uint8_t i=0;i<5;i++) {
        dali.set_level(254,sa);
        Serial.print(".");
        delay(500);
        dali.set_level(0,sa);
        Serial.print(".");
        delay(500);
      }
      Serial.println();
    }
//      }else{
//        //remove all short addres for this sa
//        dali.cmd(DALI_DATA_TRANSFER_REGISTER0,0xFF);
//        dali.cmd(DALI_SET_SHORT_ADDRESS, sa);
//      }
  }  
}

//might need a couple of calls to find everything...
void commission_wo_short(){
  Serial.println("Running: Commission w/o short adr");
  dali.commission(0xff); //init_arg=0b11111111 : all without short address  
}

void commission_all(){
  Serial.println("Running: Commission all");
  dali.commission(0x00); //init_arg=0b00000000 : all 
}

void flash() {
  Serial.println("Running: Flash all");
  for(uint8_t i=0;i<5;i++) {
    dali.set_level(254);
    Serial.print(".");
    delay(500);
    dali.set_level(0);
    Serial.print(".");
    delay(500);
  }
  Serial.println();
}
