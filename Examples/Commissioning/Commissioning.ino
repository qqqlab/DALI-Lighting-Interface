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

#include "qqqDali_ATMega328.h"

Dali_ATMega328 dali; 

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
      case '1': menu_flash(); menu(); break;    
      case '2': menu_scan_short_addr(); menu(); break;
      case '3': menu_commission(); menu(); break;
      case '4': menu_delete_short_addr(); menu(); break;
    }
  }
}

void menu() {
  Serial.println("----------------------------");
  Serial.println("1 Flash all lights");
  Serial.println("2 Scan short addresses");
  Serial.println("3 Commission short addresses");
  Serial.println("4 Delete short addresses");
  Serial.println("----------------------------");  
}


void menu_flash() {
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

void menu_scan_short_addr() {
  Serial.println("Running: Scan all short addresses");
  uint8_t sa;
  uint8_t cnt = 0;
  for(sa = 0; sa<64; sa++) {
    int16_t rv = dali.cmd(DALI_QUERY_STATUS,sa);
    if(rv!=DALI_RESULT_NO_REPLY) {
      cnt++;
      Serial.print("short address=");
      Serial.print(sa);
      Serial.print(" status=0x");
      Serial.print(rv,HEX);
      Serial.print(" minLevel=");
      Serial.print(dali.cmd(DALI_QUERY_MIN_LEVEL,sa));
      Serial.print("   flashing");
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
  Serial.print("DONE, found ");Serial.print(cnt);Serial.println(" short addresses");
}

//might need a couple of calls to find everything...
void menu_commission(){
  Serial.println("Running: Commission - be patient, this takes a while ....");
//  uint8_t cnt = dali.commission(0xff); //init_arg=0b11111111 : all without short address  
  uint8_t cnt = debug_commission(0xff); //init_arg=0b11111111 : all without short address  
  Serial.print("DONE, assigned ");Serial.print(cnt);Serial.println(" new short addresses");
}

void menu_delete_short_addr() {
  Serial.println("Running: Delete all short addresses");
  //remove all short addresses
  dali.cmd(DALI_DATA_TRANSFER_REGISTER0,0xFF);
  dali.cmd(DALI_SET_SHORT_ADDRESS, 0xFF);
  Serial.println("DONE delete");
}

//init_arg=11111111 : all without short address
//init_arg=00000000 : all 
//init_arg=0AAAAAA1 : only for this shortadr
//returns number of new short addresses assigned
uint8_t debug_commission(uint8_t init_arg) {
  uint8_t cnt = 0;
  uint8_t arr[64];
  uint8_t sa;
  for(sa=0; sa<64; sa++) arr[sa]=0;
  
  //find existing short addresses when not assigning all
  if(init_arg!=0b00000000) {
    Serial.println("Find existing short adr");
    for(sa = 0; sa<64; sa++) {
      int16_t rv = dali.cmd(DALI_QUERY_STATUS,sa);
      if(rv!=DALI_RESULT_NO_REPLY) {
        arr[sa]=1;
        Serial.print("sortadr=");
        Serial.print(sa);
        Serial.print(" status=0x");
        Serial.print(rv,HEX);
        Serial.print(" minLevel=");
        Serial.println(dali.cmd(DALI_QUERY_MIN_LEVEL,sa));
      }
    }
  }

  dali.cmd(DALI_INITIALISE,init_arg);
  dali.cmd(DALI_RANDOMISE,0x00);

  Serial.println("Find random adr");
  while(1) {
    uint32_t adr = dali.find_addr();
    if(adr>0xffffff) break;
    Serial.print("found=");
    Serial.println(adr,HEX);
  
    //find available address
    for(sa=0; sa<64; sa++) {
      if(arr[sa]==0) break;
    }
    if(sa>=64) break;
    arr[sa] = 1;
    cnt++;
 
    Serial.print("program short adr=");
    Serial.println(sa);
    dali.program_short_address(sa);  
    Serial.print("read short adr=");
    Serial.println(dali.query_short_address());

    dali.cmd(DALI_WITHDRAW,0x00);
  }
  
  dali.cmd(DALI_TERMINATE,0x00);
  return cnt;
}
