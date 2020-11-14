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
  Serial.println("DALI Dimmer Demo");
  
  dali.begin(bus_is_high, bus_set_high, bus_set_low);
  bus_init();  
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