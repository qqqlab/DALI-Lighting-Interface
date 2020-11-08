/*###########################################################################
        qqqDali.cpp - copyright qqqlab.com / github.com/qqqlab
 
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
2020-11-08 Created & tested on ATMega328 @ 8Mhz
###########################################################################*/
#include "qqqDali.h"
#include "arduino.h"

//###########################################################################
// Helpers
//###########################################################################
#define DALI_BUS_LOW() digitalWrite(this->tx_pin,LOW); this->tx_bus_low=1
#define DALI_BUS_HIGH() digitalWrite(this->tx_pin,HIGH); this->tx_bus_low=0
#define DALI_IS_BUS_LOW() (digitalRead(this->rx_pin)==LOW)
#define DALI_BAUD 1200
#define DALI_TE ((1000000+(DALI_BAUD))/(2*(DALI_BAUD)))  //417us
#define DALI_TE_MIN (80*DALI_TE)/100  
#define DALI_TE_MAX (120*DALI_TE)/100  
#define DALI_IS_TE(x) ((DALI_TE_MIN)<=(x) && (x)<=(DALI_TE_MAX))
#define DALI_IS_2TE(x) ((2*(DALI_TE_MIN))<=(x) && (x)<=(2*(DALI_TE_MAX)))

//###########################################################################
// Transmitter ISR
//###########################################################################
static Dali *IsrTimerHooks[DALI_HOOK_COUNT+1];

// timer compare interrupt service routine    
ISR(TIMER1_COMPA_vect) {        
 
  for(uint8_t i=0;i<DALI_HOOK_COUNT;i++) {
    if(IsrTimerHooks[i]==NULL) {return;}
    IsrTimerHooks[i]->ISR_timer();
  }
}

//called every Te period (417us)
void Dali::ISR_timer() {  
 if(this->bus_idle_te_cnt<0xff) this->bus_idle_te_cnt++;
  
  //send starbit, message bytes, 2 stop bits. 
  switch(this->tx_state) {
  case TX_IDLE: 
    break;
  case TX_START: 
    //wait for timeslot, then send start bit
    if(this->bus_idle_te_cnt >= 22) {
      DALI_BUS_LOW();
      this->tx_state = TX_START_X;
    }
    break;
  case TX_START_X: 
    DALI_BUS_HIGH();
    this->tx_pos=0;
    this->tx_state = TX_BIT;
    break;
  case TX_BIT: 
    if(this->tx_msg[this->tx_pos>>3] & 1<<(7-(this->tx_pos&0x7))) {DALI_BUS_LOW();} else {DALI_BUS_HIGH();}
    this->tx_state = TX_BIT_X;
    break;
  case TX_BIT_X: 
    if(this->tx_msg[this->tx_pos>>3] & 1<<(7-(this->tx_pos&0x7))) {DALI_BUS_HIGH();} else {DALI_BUS_LOW();}
    this->tx_pos++;
    if(this->tx_pos < this->tx_len) {this->tx_state = TX_BIT;} else {this->tx_state = TX_STOP1;}
    break;  	
  case TX_STOP1: 
    DALI_BUS_HIGH();
    this->tx_state = TX_STOP1_X;
    break;  
  case TX_STOP1_X: 
    this->tx_state = TX_STOP2;
    break;  
  case TX_STOP2: 
    this->tx_state = TX_STOP2_X;
    break;  
  case TX_STOP2_X: 
    this->tx_state = TX_STOP3;
    break;  
  case TX_STOP3: 
    this->bus_idle_te_cnt=0; 
    this->tx_state = TX_IDLE;
    this->rx_state = RX_IDLE;
	this->rx_len = 0;
    break;
  }
  
  //handle receiver stop bits
  if(this->rx_state == RX_BIT && this->bus_idle_te_cnt>4) {
    this->rx_state = RX_IDLE;
    //received two stop bits, got message in rx_msg + rx_halfbitlen
    uint8_t bitlen = (this->rx_halfbitlen+1)>>1;
    if((bitlen & 0x7) == 0) {
      this->rx_len = bitlen>>3;
      if(this->EventHandlerReceivedData!=NULL) this->EventHandlerReceivedData(this, (uint8_t*)this->rx_msg, this->rx_len);
    }else{
      //invalid bitlen
      //TODO handle this
    }  
  }
}

//###########################################################################
// Receiver ISR
//###########################################################################
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


void Dali::ISR_pinchange() {
  uint32_t ts = micros(); //get timestamp of change
  this->bus_idle_te_cnt=0; //reset idle counter
  uint8_t bus_low = DALI_IS_BUS_LOW();

  //exit if transmitting
  if(this->tx_state != TX_IDLE) {
    //check tx collision
    if(bus_low && !this->tx_bus_low) {
      this->tx_state = TX_IDLE; //stop transmitter
      this->tx_collision = 1; //mark collision
    }
    return;
  }

  //no bus change, ignore
  if(bus_low == this->rx_last_bus_low) return;

  //store values for next loop
  uint32_t dt = ts - this->rx_last_change_ts;
  this->rx_last_change_ts = ts;
  this->rx_last_bus_low = bus_low;

  switch(this->rx_state) {
  case RX_IDLE: 
    if(bus_low) {
      this->rx_state = RX_START;
    }
    break;	  
  case RX_START: 
    if(bus_low || !DALI_IS_TE(dt)) {
      this->rx_state = RX_IDLE;
    }else{
      this->rx_halfbitlen=-1;
      for(uint8_t i=0;i<7;i++) this->rx_msg[0]=0;
      this->rx_state = RX_BIT;
    }
    break;
  case RX_BIT:
    if(DALI_IS_TE(dt)) {
      //got a single Te pulse
      this->push_halfbit(bus_low);
    } else if(DALI_IS_2TE(dt)) {
      //got a double Te pulse
      this->push_halfbit(bus_low);
      this->push_halfbit(bus_low);
    } else {
      //got something else -> no good
      this->rx_state = RX_IDLE;
      //TODO rx error
      return;
    }		
    break;
  }
}

void Dali::push_halfbit(uint8_t bit) {
  bit = (~bit)&1;
  if((this->rx_halfbitlen & 1)==0) {
    uint8_t i = this->rx_halfbitlen>>4;
    if(i<3) {
      this->rx_msg[i] = (this->rx_msg[i]<<1) | bit;
    }
  }
  this->rx_halfbitlen++;
}


//###########################################################################
// Dali Class
//###########################################################################
void Dali::begin(int8_t tx_pin, int8_t rx_pin) {
  this->tx_pin = tx_pin;
  this->rx_pin = rx_pin;
  this->tx_state = TX_IDLE;
  this->rx_state = RX_IDLE;
    
  //setup tx
  if(this->tx_pin>=0) {
    //setup tx pin
    pinMode(this->tx_pin, OUTPUT);  
    DALI_BUS_HIGH();	
    
    //setup tx timer interrupt	
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    OCR1A = (F_CPU+(DALI_BAUD))/(2*(DALI_BAUD)); // compare match register 16MHz/256/2Hz
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
    if(this->rx_pin<=7){
      PCICR |= (1<<PCIE2);
      PCMSK2 |= (1<< (this->rx_pin));
      IsrPCINT2Hook = this; //setup pinchange interrupt hook
    }else if(this->rx_pin<=13) {
      PCICR |= (1<<PCIE0);
      PCMSK0 |= (1<< (this->rx_pin-8));
      IsrPCINT0Hook = this; //setup pinchange interrupt hook
    }else if(this->rx_pin<=19) {
      PCICR |= (1<<PCIE1);
      PCMSK1 |= (1<< (this->rx_pin-14));
      IsrPCINT1Hook = this; //setup pinchange interrupt hook
    }
  }
}


uint8_t Dali::send(uint8_t* tx_msg, uint8_t tx_len_bytes) {
  if(tx_len_bytes>3) return -(DALI_RESULT_INVALID_TOO_LONG);
  if(this->tx_state != TX_IDLE) return -(DALI_RESULT_TIMEOUT); 
  for(uint8_t i=0;i<tx_len_bytes;i++) this->tx_msg[i]=tx_msg[i];
  this->tx_len = tx_len_bytes<<3;
  this->tx_collision=0;
  this->tx_state = TX_START;
  return 0;
}

uint8_t Dali::sendwait(uint8_t* tx_msg, uint8_t tx_len_bytes, uint32_t timeout_ms) {
  if(tx_len_bytes>3) return -(DALI_RESULT_INVALID_TOO_LONG);
  uint32_t ts = millis();
  //wait for idle
  while(this->tx_state != TX_IDLE) {
    if(millis() - ts > timeout_ms) return -(DALI_RESULT_TIMEOUT); 
  }
  //start transmit
  uint8_t rv = this->send(tx_msg,tx_len_bytes);
  if(rv) return rv;
  //wait for completion
  while(this->tx_state != TX_IDLE) {
    if(millis() - ts > timeout_ms) return -(DALI_RESULT_TX_TIMEOUT); 
  }
  return 0;
}

//transmit 2 byte command, receive 1 byte reply
int16_t Dali::tx(uint8_t cmd0, uint8_t cmd1, uint32_t timeout_ms) {
  uint8_t tx[2];
  tx[0] = cmd0; 
  tx[1] = cmd1;
  int16_t rv = this->sendwait(tx,2);
  this->rx_halfbitlen = 0;
  if(rv) return -rv;;

  //wait up to 10 ms for start of reply
  uint32_t ts = millis();
  while(this->rx_state == RX_IDLE) {
    if(millis() - ts > 10) return DALI_RESULT_NO_REPLY;
  }
  //wait up to 15 ms for completion of reply
  ts = millis();
  while(this->rx_len == 0) {
    if(millis() - ts > 15) return DALI_RESULT_NO_REPLY;
  }
  if(this->rx_len > 1) return DALI_RESULT_INVALID_REPLY;
  return this->rx_msg[0];
}



//=================================================================
// High level
//=================================================================
//check YAAAAAA: 0000 0000 to 0011 1111 adr, 0100 0000 to 0100 1111 group, x111 1111 broadcast
uint8_t Dali::check_yaaaaaa(uint8_t yaaaaaa) {
  return (yaaaaaa<=0b01001111 || yaaaaaa==0b01111111 || yaaaaaa==0b11111111);
}

void Dali::set_level(uint8_t level, uint8_t adr) {
  if(this->check_yaaaaaa(adr)) this->tx(adr<<1,level);
}

int16_t Dali::cmd(uint16_t cmd, uint8_t arg) {
  //Serial.print("dali_cmd[");Serial.print(cmd,HEX);Serial.print(",");Serial.print(arg,HEX);Serial.print(")");
  uint8_t cmd0,cmd1;
  if(cmd&0x0100) {
    //special commands: MUST NOT have YAAAAAAX pattern for cmd
    //Serial.print(" SPC");
    if(!this->check_yaaaaaa(cmd>>1)) {
      cmd0 = cmd;
      cmd1 = arg;
    }else{
      return DALI_RESULT_INVALID_CMD;
    }
  }else{
    //regular commands: MUST have YAAAAAA pattern for arg

    //Serial.print(" REG");
    if(this->check_yaaaaaa(arg)) { 
      cmd0 = arg<<1|1;
      cmd1 = cmd;
    }else{
      return DALI_RESULT_INVALID_CMD;
    }
  }
  if(cmd&0x0200) {
    //Serial.print(" REPEAT");
    this->tx(cmd0, cmd1);
  }
  int16_t rv = this->tx(cmd0, cmd1);
  //Serial.print(" rv=");Serial.println(rv);
  return rv;
}


uint8_t Dali::set_operating_mode(uint8_t v, uint8_t adr) {
  return set_value(DALI_SET_OPERATING_MODE, DALI_QUERY_OPERATING_MODE, v, adr);
}

uint8_t Dali::set_max_level(uint8_t v, uint8_t adr) {
  return set_value(DALI_SET_MAX_LEVEL, DALI_QUERY_MAX_LEVEL, v, adr);
}

uint8_t Dali::set_min_level(uint8_t v, uint8_t adr) {
  return set_value(DALI_SET_MIN_LEVEL, DALI_QUERY_MIN_LEVEL, v, adr);
}


uint8_t Dali::set_system_failure_level(uint8_t v, uint8_t adr) {
  return set_value(DALI_SET_SYSTEM_FAILURE_LEVEL, DALI_QUERY_SYSTEM_FAILURE_LEVEL, v, adr);
}

uint8_t Dali::set_power_on_level(uint8_t v, uint8_t adr) {
  return set_value(DALI_SET_POWER_ON_LEVEL, DALI_QUERY_POWER_ON_LEVEL, v, adr);
}

//set a parameter value, returns 0 on success
uint8_t Dali::set_value(uint16_t setcmd, uint16_t getcmd, uint8_t v, uint8_t adr) {
  int16_t current_v = this->cmd(getcmd,adr); //get current parameter value
  if(current_v == v) return 0;
  this->cmd(DALI_DATA_TRANSFER_REGISTER0,v); //store value in DTR
  int16_t dtr = this->cmd(DALI_QUERY_CONTENT_DTR0,adr); //get DTR value
  if(dtr != v) return 1;
  this->cmd(setcmd,adr); //set parameter value = DTR
  current_v = this->cmd(getcmd,adr); //get current parameter value
  if(current_v != v) return 2;
  return 0;
}

  
