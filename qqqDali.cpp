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
2020-11-10 Split off hardware specific code into separate class
2020-11-08 Created & tested on ATMega328 @ 8Mhz
###########################################################################*/
#include "qqqDali.h"

//###########################################################################
// Helpers
//###########################################################################
#define DALI_TOL 25 //percentage tolerance on timing. DALI specs call for 10%, but use higher value to allow for implementation micros() jitter. NOTE: Max value is 50% to differentiate between TE and 2TE.
#define DALI_TE ((1000000+(DALI_BAUD))/(2*(DALI_BAUD)))  //417us
#define DALI_TE_MIN ((100-DALI_TOL)*DALI_TE)/100 
#define DALI_TE_MAX ((100+DALI_TOL)*DALI_TE)/100 
#define DALI_IS_TE(x) ((DALI_TE_MIN)<=(x) && (x)<=(DALI_TE_MAX))
#define DALI_IS_2TE(x) ((2*(DALI_TE_MIN))<=(x) && (x)<=(2*(DALI_TE_MAX)))

//###########################################################################
// Transmitter ISR
//###########################################################################
//called by derived class every Te period (417us)
void Dali::ISR_timer() {  
 if(this->bus_idle_te_cnt<0xff) this->bus_idle_te_cnt++;
  
  //send starbit, message bytes, 2 stop bits. 
  switch(this->tx_state) {
  case TX_IDLE: 
    break;
  case TX_START: 
    //wait for timeslot, then send start bit
    if(this->bus_idle_te_cnt >= 22) {
      this->HAL_set_bus_low();
      this->tx_bus_low=1;
      this->tx_state = TX_START_X;
    }
    break;
  case TX_START_X: 
    this->HAL_set_bus_high();
    this->tx_bus_low=0;
    this->tx_pos=0;
    this->tx_state = TX_BIT;
    break;
  case TX_BIT: 
    if(this->tx_msg[this->tx_pos>>3] & 1<<(7-(this->tx_pos&0x7))) {this->HAL_set_bus_low();this->tx_bus_low=1;} else {this->HAL_set_bus_high();this->tx_bus_low=0;}
    this->tx_state = TX_BIT_X;
    break;
  case TX_BIT_X: 
    if(this->tx_msg[this->tx_pos>>3] & 1<<(7-(this->tx_pos&0x7))) {this->HAL_set_bus_high();this->tx_bus_low=0;} else {this->HAL_set_bus_low();this->tx_bus_low=1;}
    this->tx_pos++;
    if(this->tx_pos < this->tx_len) {this->tx_state = TX_BIT;} else {this->tx_state = TX_STOP1;}
    break;  	
  case TX_STOP1: 
    this->HAL_set_bus_high();
    this->tx_bus_low=0;
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
      if(this->EventHandlerReceivedData) this->EventHandlerReceivedData(this, (uint8_t*)this->rx_msg, this->rx_len);
    }else{
      //invalid bitlen
      //TODO handle this
    }  
  }
}

//###########################################################################
// Receiver ISR
//###########################################################################
//called by derived class on bus state change
void Dali::ISR_pinchange() {
  uint32_t ts = this->HAL_micros(); //get timestamp of change
  this->bus_idle_te_cnt=0; //reset idle counter
  uint8_t bus_low = this->HAL_is_bus_low();

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

//non blocking send - check tx_state for completion, check tx_collision for collision errors
uint8_t Dali::send(uint8_t* tx_msg, uint8_t tx_len_bytes) {
  if(tx_len_bytes>3) return -(DALI_RESULT_DATA_TOO_LONG);
  if(this->tx_state != TX_IDLE) return -(DALI_RESULT_TIMEOUT); 
  for(uint8_t i=0;i<tx_len_bytes;i++) this->tx_msg[i]=tx_msg[i];
  this->tx_len = tx_len_bytes<<3;
  this->tx_collision=0;
  this->tx_state = TX_START; //start transmission
  return 0;
}

//blocking send - wait until successful send or timeout
uint8_t Dali::sendwait(uint8_t* tx_msg, uint8_t tx_len_bytes, uint32_t timeout_us) {
  if(tx_len_bytes>3) return -(DALI_RESULT_DATA_TOO_LONG);
  uint32_t ts = HAL_micros();
  
  while(1) {
    //wait for idle
    while(this->tx_state != TX_IDLE) {
      if(HAL_micros() - ts > timeout_us) return -(DALI_RESULT_TIMEOUT); 
    }
    //start transmit
    uint8_t rv = this->send(tx_msg,tx_len_bytes);
    if(rv) return rv;
    //wait for completion
    while(this->tx_state != TX_IDLE) {
      if(HAL_micros() - ts > timeout_us) return -(DALI_RESULT_TX_TIMEOUT); 
    }
    //check for collisions
    if(this->tx_collision==0) return 0;
  }
  return -(DALI_RESULT_TIMEOUT);
}

//blocking transmit 2 byte command, receive 1 byte reply (if reply was sent)
int16_t Dali::tx(uint8_t cmd0, uint8_t cmd1, uint32_t timeout_us) {
  uint8_t tx[2];
  tx[0] = cmd0; 
  tx[1] = cmd1;
  int16_t rv = this->sendwait(tx, 2, timeout_us);
  this->rx_halfbitlen = 0;
  if(rv) return -rv;;

  //wait up to 10 ms for start of reply
  uint32_t ts = HAL_micros();
  while(this->rx_state == RX_IDLE) {
    if(HAL_micros() - ts > 10000) return DALI_RESULT_NO_REPLY;
  }
  //wait up to 15 ms for completion of reply
  ts = HAL_micros();
  while(this->rx_len == 0) {
    if(HAL_micros() - ts > 15000) return DALI_RESULT_NO_REPLY;
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


//======================================================================
// Commissioning short addresses
//======================================================================

//Sets the slave Note 1 to the INITIALISE status for15 minutes.
//Commands 259 to 270 are enabled only for a slave in this
//status.

//set search address
void Dali::set_searchaddr(uint32_t adr) {
  this->cmd(DALI_SEARCHADDRH,adr>>16);
  this->cmd(DALI_SEARCHADDRM,adr>>8);
  this->cmd(DALI_SEARCHADDRL,adr);
}

//set search address, but set only changed bytes (takes less time)
void Dali::set_searchaddr_diff(uint32_t adr_new,uint32_t adr_current) {
  if( (uint8_t)(adr_new>>16) !=  (uint8_t)(adr_current>>16) ) this->cmd(DALI_SEARCHADDRH,adr_new>>16);
  if( (uint8_t)(adr_new>>8)  !=  (uint8_t)(adr_current>>8)  ) this->cmd(DALI_SEARCHADDRM,adr_new>>8);
  if( (uint8_t)(adr_new)     !=  (uint8_t)(adr_current)     ) this->cmd(DALI_SEARCHADDRL,adr_new);
}

//Is the random address smaller or equal to the search address?
uint8_t Dali::compare() {
  return (0xff == this->cmd(DALI_COMPARE,0x00));
}

//The slave shall store the received 6-bit address (AAAAAA) as a short address if it is selected.
void Dali::program_short_address(uint8_t shortadr) {
  this->cmd(DALI_PROGRAM_SHORT_ADDRESS, (shortadr << 1) | 0x01);
}

//What is the short address of the slave being selected?
uint8_t Dali::query_short_address() {
  return this->cmd(DALI_QUERY_SHORT_ADDRESS, 0x00) >> 1;
}

//find addr with binary search
uint32_t Dali::find_addr() {
  uint32_t adr = 0x800000;
  uint32_t addsub = 0x400000;
  uint32_t adr_last = adr;
  this->set_searchaddr(adr);
  
  while(addsub) {
    this->set_searchaddr_diff(adr,adr_last);
    adr_last = adr;
    uint8_t cmp = this->compare();
    //Serial.print("cmp ");
    //Serial.print(adr,HEX);
    //Serial.print(" = ");
    //Serial.println(cmp);
    if(cmp) adr-=addsub; else adr+=addsub;
    addsub >>= 1;
  }
  this->set_searchaddr_diff(adr,adr_last);
  adr_last = adr;
  if(!this->compare()) {
    adr++;
    this->set_searchaddr_diff(adr,adr_last);
  }
  return adr;
}

//init_arg=11111111 : all without short address
//init_arg=00000000 : all 
//init_arg=0AAAAAA1 : only for this shortadr
//returns number of new short addresses assigned
uint8_t Dali::commission(uint8_t init_arg) {
  uint8_t cnt = 0;
  uint8_t arr[64];
  uint8_t sa;
  for(sa=0; sa<64; sa++) arr[sa]=0;

  //start commissioning
  this->cmd(DALI_RESET,0x00);
  this->cmd(DALI_INITIALISE,init_arg);
  this->cmd(DALI_RANDOMISE,0x00);
  
  //find used short addresses (run always, seems to work better than without...)
  for(sa = 0; sa<64; sa++) {
    int16_t rv = this->cmd(DALI_QUERY_STATUS,sa);
    if(rv!=DALI_RESULT_NO_REPLY) {
      if(init_arg!=0b00000000) arr[sa]=1; //remove address from list if not in "all" mode
    }
  }

  //find random addresses and assign unused short addresses
  while(1) {
    uint32_t adr = this->find_addr();
    if(adr>0xffffff) break; //no more random addresses found -> exit

    //find first unused short address
    for(sa=0; sa<64; sa++) {
      if(arr[sa]==0) break;
    }
    if(sa>=64) break; //all 64 short addresses assigned -> exit

    //mark short address as used
    arr[sa] = 1;
    cnt++;
 
    //assign short address
    this->program_short_address(sa);

    //Serial.println(this->query_short_address()); //TODO check read adr, handle if not the same...

    //remove the device from the search
    this->cmd(DALI_WITHDRAW,0x00);
  }

  //terminate the DALI_INITIALISE command
  this->cmd(DALI_TERMINATE,0x00);
  return cnt;
}



//======================================================================
