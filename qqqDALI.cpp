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

----------------------------------------------------------------------------
Changelog:
2020-11-14 Rewrite with sampling instead of pinchange
2020-11-10 Split off hardware specific code into separate class
2020-11-08 Created & tested on ATMega328 @ 8Mhz
###########################################################################*/

//#define DALI_DEBUG

//=================================================================
// LOW LEVEL DRIVER
//=================================================================
#include "qqqDALI.h"

#ifdef DALI_DEBUG
#include "arduino.h"
#endif

//timing
#define BEFORE_CMD_IDLE_MS 13 //require 13ms idle time before sending a cmd()

//busstate
#define IDLE 0
#define RX 1
#define COLLISION_RX 2
#define TX 3
#define COLLISION_TX 4

void Dali::begin(uint8_t (*bus_is_high)(), void (*bus_set_low)(), void (*bus_set_high)())
{
  this->bus_is_high = bus_is_high;
  this->bus_set_low = bus_set_low;
  this->bus_set_high = bus_set_high;
  _init();
}

void Dali::_set_busstate_idle() {
  bus_set_high();
  idlecnt = 0;
  busstate = IDLE;
}

void Dali::_init() {
  _set_busstate_idle();
  rxstate = EMPTY;
  txcollision = 0;  
}

uint16_t Dali::milli() {
  while(ticks==0xFF); //wait for _millis update to finish
  return _milli;
}

// timer interrupt service routine, called 9600 times per second
void Dali::timer() {
  //get bus sample
  uint8_t busishigh = (bus_is_high() ? 1 : 0); //bus_high is 1 on high (non-asserted), 0 on low (asserted)

  //millis update
  ticks++;
  if(ticks==10) {
    ticks = 0xff; //signal _millis is updating
    _milli++;
    ticks = 0; 
  }
  
  switch(busstate) {
  case IDLE:
    if(busishigh) {
      if(idlecnt != 0xff) idlecnt++;
      break;
    }
    //set busstate = RX
    rxpos = 0;
    rxbitcnt = 0;
    rxidle = 0;
    rxstate = RECEIVING;
    busstate = RX;
    //fall-thru to RX
  case RX:
    //store sample
    rxbyte = (rxbyte << 1) | busishigh;
    rxbitcnt++;
    if(rxbitcnt == 8) {
      rxdata[rxpos] = rxbyte;
      rxpos++;
      if(rxpos > DALI_RX_BUF_SIZE - 1) rxpos = DALI_RX_BUF_SIZE - 1;
      rxbitcnt = 0;
    }
    //check for reception of 2 stop bits
    if(busishigh) {
      rxidle++;
      if(rxidle >= 16) { 
        rxdata[rxpos] = 0xFF;
        rxpos++;
        rxstate = COMPLETED;
        _set_busstate_idle();
        break;
      }
    }else{
      rxidle = 0;
    }
    break;
  case TX:
    if(txhbcnt >= txhblen) {
      //all bits transmitted, go back to IDLE
      _set_busstate_idle();
    }else{
      //check for collisions (transmitting high but bus is low)      
      if( (
            txcollisionhandling == DALI_TX_COLLISSION_ON //handle all
            || (txcollisionhandling == DALI_TX_COLLISSION_AUTO && txhblen != 2+8+4) //handle only if not transmitting 8 bits (2+8+4 half bits)
          ) && (txhigh && !busishigh)  //transmitting high, but bus is low 
          && (txspcnt==1 || txspcnt==2) ) // in middle of transmitting low period
      {
        if(txcollision != 0xFF) txcollision++;
        txspcnt = 0;
        busstate = COLLISION_TX;  
        return;      
      }
    
      //send data bits (MSB first) to bus every 4th sample time
      if(txspcnt == 0) {
        //send bit
        uint8_t pos = txhbcnt >> 3;
        uint8_t bitmask = 1 << (7 - (txhbcnt & 0x7));
        if( txhbdata[pos] & bitmask ) {
          bus_set_low();
          txhigh = 0;  
        }else{        
          bus_set_high();
          txhigh = 1;
        }
        //update half bit counter
        txhbcnt++; 
        //next transmit in 4 sample times
        txspcnt = 4;
      }
      txspcnt--;
    }
    break;    
  case COLLISION_TX:
    //keep bus low for 16 samples = 4 TE
    bus_set_low();
    txspcnt++;
    if(txspcnt >= 16) _set_busstate_idle();
    break;  
  }
}

//push 2 half bits into the half bit transmit buffer, MSB first, 0x0=stop, 0x1= bit value 0, 0x2= bit value 1/start
void Dali::_tx_push_2hb(uint8_t hb) {
  uint8_t pos = txhblen>>3;
  uint8_t shift = 6 - (txhblen & 0x7);
  txhbdata[pos] |= hb << shift;
  txhblen += 2;
}
  
//non-blocking transmit
//transmit if bus is IDLE, without checking hold off times, sends start+stop bits
uint8_t Dali::tx(uint8_t *data, uint8_t bitlen) {
  if(bitlen > 32) return DALI_RESULT_FRAME_TOO_LONG;
  if(busstate != IDLE) return DALI_RESULT_BUS_NOT_IDLE;

  //clear data
  for(uint8_t i=0; i<9; i++) txhbdata[i]=0;
  
  //push data in transmit buffer
  txhblen = 0;
  _tx_push_2hb(0x2); //start bit
  //data bits MSB first
  for(uint8_t i=0; i<bitlen; i++) {
    uint8_t pos = i>>3;
    uint8_t mask = 1 << (7 - (i & 0x7));
    _tx_push_2hb( data[pos] & mask ? 0x2 : 0x1 );
  }
  _tx_push_2hb(0x0); //stop bit
  _tx_push_2hb(0x0); //stop bit

  //setup tx vars
  txhbcnt = 0;
  txspcnt = 0;
  txcollision = 0;
  rxstate = EMPTY;
  busstate = TX;
  return DALI_OK;
}

uint8_t Dali::tx_state() {
  if(txcollision) {
    txcollision = 0;
    return DALI_RESULT_COLLISION;
  }  
  if(busstate == TX) return DALI_RESULT_TRANSMITTING;
  return DALI_OK;
}  
  



//-------------------------------------------------------------------
//manchester decode
/*

Prefectly matched transmitter and sampling: 8 samples per bit        
---------+   +---+   +-------+       +-------+   +------------------------
         |   |   |   |       |       |       |   |
         +---+   +---+       +-------+       +---+
sample-> 012345670123456701234567012345670123456701234567012345670
sync->   ^       ^       ^       ^       ^       ^       ^       ^
decode-> start   1       1       0       1       0       stop    stop


slow transmitter: 9 samples per bit
---------+   +----+    +--------+        +--------+   +------------------------
         |   |    |    |        |        |        |   |
         +---+    +----+        +--------+        +---+
sample-> 0123456780123456780123456780123456780123456780123456780123456780
sync->   ^        ^        ^        ^        ^        ^        ^        ^
decode-> start    1        1        0        1        0        stop     stop

*/

//compute weight for a 8 bit sample i
uint8_t Dali::_man_weight(uint8_t i) {
  int8_t w = 0;
  w += ((i>>7) & 1) ? 1 : -1;
  w += ((i>>6) & 1) ? 2 : -2; //put more weight in middle
  w += ((i>>5) & 1) ? 2 : -2; //put more weight in middle
  w += ((i>>4) & 1) ? 1 : -1;
  w -= ((i>>3) & 1) ? 1 : -1;
  w -= ((i>>2) & 1) ? 2 : -2; //put more weight in middle
  w -= ((i>>1) & 1) ? 2 : -2; //put more weight in middle
  w -= ((i>>0) & 1) ? 1 : -1;
  //w at this point: 
  //w = -12 perfect manchester encoded value 1 
  //... 
  //w =  -2 very weak value 1
  //w =   0 unknown (all samples high or low)
  //... 
  //w =  12 perfect manchester encoded value 0

  w *= 2;
  if(w<0) w = -w + 1;  
  return w;
}       

//call with bitpos <= DALI_RX_BUF_SIZE*8-8;
uint8_t Dali::_man_sample(uint8_t *edata, uint16_t bitpos, uint8_t *stop_coll) {
  uint8_t pos = bitpos>>3;
  uint8_t shift = bitpos & 0x7;
  uint8_t sample = (edata[pos] << shift) | (edata[pos+1] >> (8-shift));

  //stop bit: received high (non-asserted) bus for last 8 samples
  if(sample == 0xFF) *stop_coll = 1;
    
  //collision: received low (asserted) bus for last 8 samples
  if(sample == 0x00) *stop_coll = 2;
    
  return sample;
}


//decode 8 times oversampled encoded data
//returns bitlen of decoded data, or 0 on collision
uint8_t Dali::_man_decode(uint8_t *edata, uint8_t ebitlen, uint8_t *ddata) {
  uint8_t dbitlen = 0;
  uint16_t ebitpos = 1;
  while(ebitpos+1<ebitlen) { 
    uint8_t stop_coll = 0;
    //weight at nominal oversample rate
    uint8_t sample = _man_sample(edata, ebitpos, &stop_coll);    
    uint8_t weightmax = _man_weight(sample); //weight of maximum
    uint8_t pmax = 8; //position of maximum

    //weight at nominal oversample rate - 1
    sample = _man_sample(edata, ebitpos - 1, &stop_coll);
    uint8_t w = _man_weight(sample);   
    if( weightmax < w) { //when equal keep pmax=8, the nominal oversample baud rate
      weightmax = w;
      pmax = 7;
    }

    //weight at nominal oversample rate + 1
    sample = _man_sample(edata, ebitpos + 1, &stop_coll);
    w = _man_weight(sample);    
    if( weightmax < w ) { //when equal keep previous value
      weightmax = w;
      pmax = 9;
    }

    //handle stop/collision
    if(stop_coll==1) break; //stop
    if(stop_coll==2) return 0; //collison

    //store mancheter bit
    if(dbitlen > 0) { //ignore start bit
      uint8_t bytepos = (dbitlen - 1) >> 3;
      uint8_t bitpos = (dbitlen - 1) & 0x7;
      if(bitpos == 0) ddata[bytepos] = 0; //empty data before storing first bit
      ddata[bytepos] = (ddata[bytepos] << 1) | (weightmax & 1); //get databit from bit0 of weight
    }
    dbitlen++;
    ebitpos += pmax; //jump to next mancheter bit, skipping over number of samples with max weight   
  }
  if(dbitlen>1) dbitlen--;
  return dbitlen;  
}

//non-blocking receive, 
//returns 0 empty, 1 if busy receiving, 2 decode error, >2 number of bits received
uint8_t Dali::rx(uint8_t *ddata) {
  switch(rxstate) {
  case EMPTY: return 0;
  case RECEIVING: return 1;
  case COMPLETED: 
    rxstate = EMPTY;   
    uint8_t dlen = _man_decode(rxdata,rxpos*8,ddata);
    

#ifdef DALI_DEBUG
  if(dlen!=8){
    //print received samples
    Serial.print("RX: len=");
    Serial.print(rxpos*8);
    Serial.print(" ");
    for(uint8_t i=0;i<rxpos;i++) {
      for(uint8_t m=0x80;m!=0x00;m>>=1) {
        if(rxdata[i]&m) Serial.print("1"); else Serial.print("0");
      }
      Serial.print(" ");
    }

    //print decoded data
    Serial.print("decoded: len=");
    Serial.print(dlen);
    Serial.print(" ");
    for(uint8_t i=0;i<dlen;i++) {
      if( ddata[i>>3] & (1 << (7 - (i & 0x7))) ) Serial.print("1"); else Serial.print("0");
    }
    Serial.println();   
  }
#endif
    
    if(dlen<3) return 2;
    return dlen;
  }
  return 0; //should not get here
}


//=================================================================
// HIGH LEVEL FUNCTIONS
//=================================================================

//blocking send - wait until successful send or timeout
uint8_t Dali::tx_wait(uint8_t* data, uint8_t bitlen, uint16_t timeout_ms) {
  if(bitlen>32) return DALI_RESULT_DATA_TOO_LONG;
  uint16_t start_ms = milli();  
  while(1) {
    //wait for 10ms idle
     while(idlecnt < BEFORE_CMD_IDLE_MS){
      //Serial.print('w');
      if(milli() - start_ms > timeout_ms) return DALI_RESULT_TIMEOUT;
    }   
    //try transmit
    while(tx(data,bitlen) != DALI_OK){
      //Serial.print('w');
      if(milli() - start_ms > timeout_ms) return DALI_RESULT_TIMEOUT;
    }
    //wait for completion
    uint8_t rv;
    while(1) {
      rv = tx_state();
      if(rv != DALI_RESULT_TRANSMITTING) break;
      if(milli() - start_ms > timeout_ms) return DALI_RESULT_TIMEOUT;
    }
    //exit if transmit was ok
    if(rv == DALI_OK) return DALI_OK;
    //not ok (for example collision) - retry until timeout
  }
  return DALI_RESULT_TIMEOUT;
}

//blocking transmit 2 byte command, receive 1 byte reply (if a reply was sent)
//returns >=0 with reply byte
//returns <0 with negative result code
int16_t Dali::tx_wait_rx(uint8_t cmd0, uint8_t cmd1, uint16_t timeout_ms) {
#ifdef DALI_DEBUG  
  Serial.print("TX");
  Serial.print(cmd0>>4,HEX);
  Serial.print(cmd0&0xF,HEX);
  Serial.print(cmd1>>4,HEX);
  Serial.print(cmd1&0xF,HEX);
  Serial.print(" ");
#endif
  uint8_t data[4];
  data[0] = cmd0; 
  data[1] = cmd1;
  int16_t rv = tx_wait(data, 16, timeout_ms);
  if(rv) return -rv;;

  //wait up to 10 ms for start of reply, additional 15ms for receive to complete
  uint16_t rx_start_ms = milli();
  uint16_t rx_timeout_ms = 10;
  while(1) {
    rv = rx(data);
    switch( rv ) {
      case 0: break; //nothing received yet, wait
      case 1: rx_timeout_ms = 25; break; //extend timeout, wait for RX completion
      case 2: return -DALI_RESULT_COLLISION; //report collision
      default: 
        if(rv==8) 
          return data[0];
        else
          return -DALI_RESULT_INVALID_REPLY;
    }
    if(milli() - rx_start_ms > rx_timeout_ms) return -DALI_RESULT_NO_REPLY;
  }
  return -DALI_RESULT_NO_REPLY; //should not get here
}


//check YAAAAAA: 0000 0000 to 0011 1111 adr, 0100 0000 to 0100 1111 group, x111 1111 broadcast
uint8_t Dali::_check_yaaaaaa(uint8_t yaaaaaa) {
  return (yaaaaaa<=0b01001111 || yaaaaaa==0b01111111 || yaaaaaa==0b11111111);
}

void Dali::set_level(uint8_t level, uint8_t adr) {
  if(_check_yaaaaaa(adr)) tx_wait_rx(adr<<1,level);
}

int16_t Dali::cmd(uint16_t cmd, uint8_t arg) {
  //Serial.print("dali_cmd[");Serial.print(cmd,HEX);Serial.print(",");Serial.print(arg,HEX);Serial.print(")");
  uint8_t cmd0,cmd1;
  if(cmd & 0x0100) {
    //special commands: MUST NOT have YAAAAAAX pattern for cmd
    //Serial.print(" SPC");
    if(!_check_yaaaaaa(cmd>>1)) {
      cmd0 = cmd;
      cmd1 = arg;
    }else{
      return DALI_RESULT_INVALID_CMD;
    }
  }else{
    //regular commands: MUST have YAAAAAA pattern for arg

    //Serial.print(" REG");
    if(_check_yaaaaaa(arg)) { 
      cmd0 = arg<<1|1;
      cmd1 = cmd;
    }else{
      return DALI_RESULT_INVALID_CMD;
    }
  }
  if(cmd & 0x0200) {
    //Serial.print(" REPEAT");
    tx_wait_rx(cmd0, cmd1);
  }
  int16_t rv = tx_wait_rx(cmd0, cmd1);
  //Serial.print(" rv=");Serial.println(rv);
  return rv;
}


uint8_t Dali::set_operating_mode(uint8_t v, uint8_t adr) {
  return _set_value(DALI_SET_OPERATING_MODE, DALI_QUERY_OPERATING_MODE, v, adr);
}

uint8_t Dali::set_max_level(uint8_t v, uint8_t adr) {
  return _set_value(DALI_SET_MAX_LEVEL, DALI_QUERY_MAX_LEVEL, v, adr);
}

uint8_t Dali::set_min_level(uint8_t v, uint8_t adr) {
  return _set_value(DALI_SET_MIN_LEVEL, DALI_QUERY_MIN_LEVEL, v, adr);
}


uint8_t Dali::set_system_failure_level(uint8_t v, uint8_t adr) {
  return _set_value(DALI_SET_SYSTEM_FAILURE_LEVEL, DALI_QUERY_SYSTEM_FAILURE_LEVEL, v, adr);
}

uint8_t Dali::set_power_on_level(uint8_t v, uint8_t adr) {
  return _set_value(DALI_SET_POWER_ON_LEVEL, DALI_QUERY_POWER_ON_LEVEL, v, adr);
}

//set a parameter value, returns 0 on success
uint8_t Dali::_set_value(uint16_t setcmd, uint16_t getcmd, uint8_t v, uint8_t adr) {
  int16_t current_v = cmd(getcmd,adr); //get current parameter value
  if(current_v == v) return 0;
  cmd(DALI_DATA_TRANSFER_REGISTER0,v); //store value in DTR
  int16_t dtr = cmd(DALI_QUERY_CONTENT_DTR0,adr); //get DTR value
  if(dtr != v) return 1;
  cmd(setcmd,adr); //set parameter value = DTR
  current_v = cmd(getcmd,adr); //get current parameter value
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
  cmd(DALI_SEARCHADDRH,adr>>16);
  cmd(DALI_SEARCHADDRM,adr>>8);
  cmd(DALI_SEARCHADDRL,adr);
}

//set search address, but set only changed bytes (takes less time)
void Dali::set_searchaddr_diff(uint32_t adr_new,uint32_t adr_current) {
  if( (uint8_t)(adr_new>>16) !=  (uint8_t)(adr_current>>16) ) cmd(DALI_SEARCHADDRH,adr_new>>16);
  if( (uint8_t)(adr_new>>8)  !=  (uint8_t)(adr_current>>8)  ) cmd(DALI_SEARCHADDRM,adr_new>>8);
  if( (uint8_t)(adr_new)     !=  (uint8_t)(adr_current)     ) cmd(DALI_SEARCHADDRL,adr_new);
}

//Is the random address smaller or equal to the search address?
//as more than one device can reply, the reply gets garbled
uint8_t Dali::compare() {
  uint8_t retry = 2;
  while(retry>0) {
    //compare is true if we received any activity on the bus as reply.
    //sometimes the reply is not registered... so only accept retry times 'no reply' as a real false compare
    int16_t rv = cmd(DALI_COMPARE,0x00);
    if(rv == -DALI_RESULT_COLLISION) return 1;
    if(rv == -DALI_RESULT_INVALID_REPLY) return 1;
    if(rv == 0xFF) return 1;

    retry--;
  }
  return 0;
}

//The slave shall store the received 6-bit address (AAAAAA) as a short address if it is selected.
void Dali::program_short_address(uint8_t shortadr) {
  cmd(DALI_PROGRAM_SHORT_ADDRESS, (shortadr << 1) | 0x01);
}

//What is the short address of the slave being selected?
uint8_t Dali::query_short_address() {
  return cmd(DALI_QUERY_SHORT_ADDRESS, 0x00) >> 1;
}

//find addr with binary search
uint32_t Dali::find_addr() {
  uint32_t adr = 0x800000;
  uint32_t addsub = 0x400000;
  uint32_t adr_last = adr;
  set_searchaddr(adr);
  
  while(addsub) {
    set_searchaddr_diff(adr,adr_last);
    adr_last = adr;
    //Serial.print("cmpadr=");
    //Serial.print(adr,HEX);
    uint8_t cmp = compare(); //returns 1 if searchadr > adr
    //Serial.print("cmp ");
    //Serial.print(adr,HEX);
    //Serial.print(" = ");
    //Serial.println(cmp);
    if(cmp) adr-=addsub; else adr+=addsub;
    addsub >>= 1;
  }
  set_searchaddr_diff(adr,adr_last);
  adr_last = adr;
  if(!compare()) {
    adr++;
    set_searchaddr_diff(adr,adr_last);
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
  cmd(DALI_INITIALISE,init_arg);
  cmd(DALI_RANDOMISE,0x00);
  //need 100ms pause after RANDOMISE, scan takes care of this...

  //find used short addresses (run always, seems to work better than without...)
  for(sa = 0; sa<64; sa++) {
    int16_t rv = cmd(DALI_QUERY_STATUS,sa);
    if(rv>=0) {
      if(init_arg!=0b00000000) arr[sa]=1; //remove address from list if not in "all" mode
    }
  }

  //find random addresses and assign unused short addresses
  while(1) {
    uint32_t adr = find_addr();
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
    program_short_address(sa);

    //Serial.println(query_short_address()); //TODO check read adr, handle if not the same...

    //remove the device from the search
    cmd(DALI_WITHDRAW,0x00);
  }

  //terminate the DALI_INITIALISE command
  cmd(DALI_TERMINATE,0x00);
  return cnt;
}

//======================================================================
// Memory
//======================================================================
uint8_t Dali::set_dtr0(uint8_t value, uint8_t adr) {
  uint8_t retry=3;
  while(retry) {
    cmd(DALI_DATA_TRANSFER_REGISTER0,value); //store value in DTR
    int16_t dtr = cmd(DALI_QUERY_CONTENT_DTR0,adr); //get DTR value
    if(dtr == value) return 0;
    retry--;    
  }
  return 1;
}

uint8_t Dali::set_dtr1(uint8_t value, uint8_t adr) {
  uint8_t retry=3;
  while(retry) {
    cmd(DALI_DATA_TRANSFER_REGISTER1,value); //store value in DTR
    int16_t dtr = cmd(DALI_QUERY_CONTENT_DTR1,adr); //get DTR value
    if(dtr == value) return 0;
    retry--;    
  }
  return 1;
}

uint8_t Dali::set_dtr2(uint8_t value, uint8_t adr) {
  uint8_t retry=3;
  while(retry) {
    cmd(DALI_DATA_TRANSFER_REGISTER2,value); //store value in DTR
    int16_t dtr = cmd(DALI_QUERY_CONTENT_DTR2,adr); //get DTR value
    if(dtr == value) return 0;
    retry--;    
  }
  return 1;
}

uint8_t Dali::read_memory_bank(uint8_t bank, uint8_t adr) {
  uint16_t rv;

  if(set_dtr0(0, adr)) return 1;
  if(set_dtr1(bank, adr)) return 2;
    
  //uint8_t data[255];
  uint16_t len = cmd(DALI_READ_MEMORY_LOCATION, adr);
#ifdef DALI_DEBUG
  Serial.print("memlen=");
  Serial.println(len);
  for(uint8_t i=0;i<len;i++) {
    int16_t mem = cmd(DALI_READ_MEMORY_LOCATION, adr);
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
    //delay(10);
  }
#endif

  uint16_t dtr0 = cmd(DALI_QUERY_CONTENT_DTR0,adr); //get DTR value
  if(dtr0 != 255) return 4;
}


//======================================================================
