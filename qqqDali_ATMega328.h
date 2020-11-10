/*###########################################################################
        qqqDali_ATMega328.h - copyright qqqlab.com / github.com/qqqlab
 
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
 
----------------------------------------------------------------------------
Changelog:
2020-11-10 Created & tested on ATMega328 @ 8Mhz
###########################################################################*/
#include "arduino.h"
#include "qqqDali.h"

#define DALI_HOOK_COUNT 3

class Dali_ATMega328 : public Dali {
public:
  void begin(int8_t tx_pin, int8_t rx_pin);
  virtual void HAL_set_bus_low() const override;
  virtual void HAL_set_bus_high() const override;
  virtual uint8_t HAL_is_bus_low() const override;
  virtual uint32_t HAL_micros() const override;

private:
  uint8_t tx_pin; //transmitter pin
  uint8_t rx_pin; //receiver pin
};

//-----------------------------------------
//Hardware Abstraction Layer
void Dali_ATMega328::HAL_set_bus_low() const {
  digitalWrite(this->tx_pin,LOW); 
}

void Dali_ATMega328::HAL_set_bus_high() const {
  digitalWrite(this->tx_pin,HIGH);
}

uint8_t Dali_ATMega328::HAL_is_bus_low() const {
  return (digitalRead(this->rx_pin) == LOW);
}

uint32_t Dali_ATMega328::HAL_micros() const {
  return micros();
}

//-----------------------------------------
// Transmitter ISR
static Dali *IsrTimerHooks[DALI_HOOK_COUNT+1];

// timer compare interrupt service routine
ISR(TIMER1_COMPA_vect) {

  for(uint8_t i=0;i<DALI_HOOK_COUNT;i++) {
    if(IsrTimerHooks[i]==NULL) {return;}
    IsrTimerHooks[i]->ISR_timer();
  }
}

//-----------------------------------------
// Receiver ISR
//pin PCINT
//0-7 PCINT2_vect PCINT16-23
//8-13 PCINT0_vect PCINT0-5
//14-19 PCINT1_vect PCINT8-13
static Dali *IsrPCINT0Hook;
static Dali *IsrPCINT1Hook;
static Dali *IsrPCINT2Hook;

ISR(PCINT0_vect) {
  if(IsrPCINT0Hook!=NULL) IsrPCINT0Hook->ISR_pinchange();
} 
ISR(PCINT1_vect) {
  if(IsrPCINT1Hook!=NULL) IsrPCINT1Hook->ISR_pinchange();
} 
ISR(PCINT2_vect) {
  if(IsrPCINT2Hook!=NULL) IsrPCINT2Hook->ISR_pinchange();
} 


//-----------------------------------------
// begin
void Dali_ATMega328::begin(int8_t tx_pin, int8_t rx_pin) {
  this->tx_pin = tx_pin;
  this->rx_pin = rx_pin;

  //setup tx
  if(this->tx_pin>=0) {
    //setup tx pin
    pinMode(this->tx_pin, OUTPUT);  
    this->HAL_set_bus_high();
    this->tx_bus_low=0;

    //setup tx timer interrupt
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;
    OCR1A = (F_CPU+(DALI_BAUD))/(2*(DALI_BAUD)); // compare match register at baud rate * 2
    TCCR1B |= (1 << WGM12);   // CTC mode
    TCCR1B |= (1 << CS10);    // 1:1 prescaler 
    TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
    
    //setup timer interrupt hooks
    for(uint8_t i=0;i<DALI_HOOK_COUNT;i++) {
      if(IsrTimerHooks[i] == NULL) {
        IsrTimerHooks[i] = this;
        break;
      }
    }
  }

  //setup rx  
  if(this->rx_pin>=0) {
    //setup rx pin
    pinMode(this->rx_pin, INPUT);    

    //setup rx pinchange interrupt
    // 0- 7 PCINT2_vect PCINT16-23
    // 8-13 PCINT0_vect PCINT0-5
    //14-19 PCINT1_vect PCINT8-13
    if(this->rx_pin <= 7){
      PCICR |= (1 << PCIE2);
      PCMSK2 |= (1 << (this->rx_pin));
      IsrPCINT2Hook = this; //setup pinchange interrupt hook
    }else if(this->rx_pin <= 13) {
      PCICR |= (1 << PCIE0);
      PCMSK0 |= (1 << (this->rx_pin - 8));
      IsrPCINT0Hook = this; //setup pinchange interrupt hook
    }else if(this->rx_pin <= 19) {
      PCICR |= (1 << PCIE1);
      PCMSK1 |= (1 << (this->rx_pin - 14));
      IsrPCINT1Hook = this; //setup pinchange interrupt hook
    }
  }
}
