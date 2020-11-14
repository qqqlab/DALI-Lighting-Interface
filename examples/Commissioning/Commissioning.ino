/*###########################################################################
        copyright qqqlab.com / github.com/qqqlab
 
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
#include "qqqDALI.h"

Dali dali;

//ATMEGA328 specific
#define TX_PIN 3
#define RX_PIN 4

//is bus asserted
uint8_t bus_is_high() {
  return digitalRead(RX_PIN); //slow version
  //return PIND & (1 << 4); //fast version
}

//assert bus
void bus_set_low() {
  digitalWrite(TX_PIN,HIGH); //opto slow version
  //PORTD |= (1 << 3); //opto fast version
  
  //digitalWrite(TX_PIN,LOW); //diy slow version
  //PORTD &= ~(1 << 3); //diy fast version
}

//release bus
void bus_set_high() {
  digitalWrite(TX_PIN,LOW); //opto slow version
  //PORTD &= ~(1 << 3); //opto fast version
  
  //digitalWrite(TX_PIN,HIGH); //diy slow version
  //PORTD |= (1 << 3); //diy fast version
}

void bus_init() {
  //setup rx pin
  pinMode(4, INPUT);

  //setup tx pin
  pinMode(3, OUTPUT);
  
  //setup tx timer interrupt
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A  = (F_CPU + 8 * DALI_BAUD / 2) / (8 * DALI_BAUD); // compare match register at baud rate * 8
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS10);    // 1:1 prescaler 
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
}

ISR(TIMER1_COMPA_vect) {
  dali.timer();
}

void setup() {
  Serial.begin(115200);
  Serial.println("DALI Commissioning Demo");
  
  dali.begin(bus_is_high, bus_set_high, bus_set_low);
  bus_init();  

  menu();
}

void loop() {
  while (Serial.available() > 0) {
    int incomingByte = Serial.read();      
    switch(incomingByte) {
      case '1': menu_blink(); menu(); break;    
      case '2': menu_scan_short_addr(); menu(); break;
      case '3': menu_commission(); menu(); break;
      case '4': menu_commission_debug(); menu(); break;
      case '5': menu_delete_short_addr(); menu(); break;
      case '6': menu_read_memory(); menu(); break;
    }
  }
}

void menu() {
  Serial.println("----------------------------");
  Serial.println("1 Blink all lamps");
  Serial.println("2 Scan short addresses");
  Serial.println("3 Commission short addresses");
  Serial.println("4 Commission short addresses (VERBOSE)");
  Serial.println("5 Delete short addresses");
  Serial.println("6 Read memory bank");
  Serial.println("----------------------------");  
}

void menu_blink() {
  Serial.println("Running: Blinking all lamps");
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
    if(rv>=0) {
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
    }else if (-rv != DALI_RESULT_NO_REPLY) {
      Serial.print("short address=");
      Serial.print(sa);
      Serial.print(" ERROR=");
      Serial.println(-rv);     
    }
  }  
  Serial.print("DONE, found ");Serial.print(cnt);Serial.println(" short addresses");
}

//might need a couple of calls to find everything...
void menu_commission(){
  Serial.println("Running: Commission");
  Serial.println("Might need a couple of runs to find all lamps ...");
  Serial.println("Be patient, this takes a while ...");
  uint8_t cnt = dali.commission(0xff); //init_arg=0b11111111 : all without short address  
  Serial.print("DONE, assigned ");Serial.print(cnt);Serial.println(" new short addresses");
}

//might need a couple of calls to find everything...
void menu_commission_debug(){
  Serial.println("Running: Commission (VERBOSE)");
  Serial.println("Might need a couple of runs to find all lamps ...");
  Serial.println("Be patient, this takes a while ...");
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

  dali.cmd(DALI_INITIALISE,init_arg);
  dali.cmd(DALI_RANDOMISE,0x00);
  //need 100ms pause after RANDOMISE, scan takes care of this...
  
  //find used short addresses (run always, seems to work better than without...)
  Serial.println("Find existing short adr");
  for(sa = 0; sa<64; sa++) {
    int16_t rv = dali.cmd(DALI_QUERY_STATUS,sa);
    if(rv>=0) {
      if(init_arg!=0b00000000) arr[sa]=1; //remove address from list if not in "all" mode
      Serial.print("sortadr=");
      Serial.print(sa);
      Serial.print(" status=0x");
      Serial.print(rv,HEX);
      Serial.print(" minLevel=");
      Serial.println(dali.cmd(DALI_QUERY_MIN_LEVEL,sa));
    }
  }

//  dali.set_searchaddr(0x000000);
//  dali.set_searchaddr(0xFFFFFF);
//while(1) {
//  dali.compare();
//  delay(200);
//}



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

void menu_read_memory() {
/*
  uint8_t v = 123;
  uint8_t adr = 0xff;

  while(1) {
    int16_t rv = dali.cmd(DALI_DATA_TRANSFER_REGISTER0, v); //store value in DTR
    Serial.print("rv=");
    Serial.print(rv);
    
    int16_t dtr = dali.cmd(DALI_QUERY_CONTENT_DTR0, adr); //get DTR value
    Serial.print(" dtr=");
    Serial.println(dtr);
    delay(13);
  }
*/
  
  Serial.println("Running: Scan all short addresses");
  uint8_t sa;
  uint8_t cnt = 0;
  for(sa = 0; sa<64; sa++) {
    int16_t rv = dali.cmd(DALI_QUERY_STATUS,sa);
    if(rv>=0) {
      cnt++;
      Serial.print("\nshort address ");
      Serial.println(sa);
      Serial.print("status=0x");
      Serial.println(rv,HEX);
      Serial.print("minLevel=");
      Serial.println(dali.cmd(DALI_QUERY_MIN_LEVEL,sa));

      dali_read_memory_bank_verbose(0,sa);
      
    }else if (-rv != DALI_RESULT_NO_REPLY) {
      Serial.print("short address=");
      Serial.print(sa);
      Serial.print(" ERROR=");
      Serial.println(-rv);     
    }
  }  
  Serial.print("DONE, found ");Serial.print(cnt);Serial.println(" short addresses"); 
}

uint8_t dali_read_memory_bank_verbose(uint8_t bank, uint8_t adr) {
  uint16_t rv;

  if(dali.set_dtr0(0, adr)) return 1;
  if(dali.set_dtr1(bank, adr)) return 2;
    
  //uint8_t data[255];
  uint16_t len = dali.cmd(DALI_READ_MEMORY_LOCATION, adr);
  Serial.print("memlen=");
  Serial.println(len);
  for(uint8_t i=0;i<len;i++) {
    int16_t mem = dali.cmd(DALI_READ_MEMORY_LOCATION, adr);
    if(mem>=0) {
      //data[i] = mem;
      Serial.print(i,HEX);
      Serial.print(":");
      Serial.print(mem);
      Serial.print(" 0x");
      Serial.print(mem,HEX);
      Serial.print(" ");
      if(mem>=32 && mem <127) Serial.print((char)mem);   
      Serial.println(); 
    }else if(mem!=-DALI_RESULT_NO_REPLY) {
      Serial.print(i,HEX);
      Serial.print(":err=");
      Serial.println(mem);   
    }
  }

  uint16_t dtr0 = dali.cmd(DALI_QUERY_CONTENT_DTR0,adr); //get DTR value
  if(dtr0 != 255) return 4;
}