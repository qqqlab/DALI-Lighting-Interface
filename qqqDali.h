/*###########################################################################
        qqqDali.h - copyright qqqlab.com / github.com/qqqlab
 
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
#include <inttypes.h>

class Dali {
public:
  //Hardware Abstraction Layer overrides
  virtual void HAL_set_bus_low() const = 0; //set DALI bus to low state
  virtual void HAL_set_bus_high() const = 0; //set DALI bus to high state
  virtual uint8_t HAL_is_bus_low() const = 0; //is DALI bus in low state?
  virtual uint32_t HAL_micros() const = 0; //get microsecond time stamp
  void ISR_timer(); //call this function every 417us 
  void ISR_pinchange(); //call this function on change of DALI bus

  //callback on received data from DALI bus
  typedef void (*EventHandlerReceivedDataFuncPtr)(Dali *sender, uint8_t *data, uint8_t len);
  EventHandlerReceivedDataFuncPtr EventHandlerReceivedData;

  //high level functions
  void     set_level(uint8_t level, uint8_t adr=0xFF); //set arc level
  int16_t  cmd(uint16_t cmd, uint8_t arg); //execute DALI command, use a DALI_xxx command define as cmd argument, returns negative DALI_RESULT_xxx or reply byte
  uint8_t  set_operating_mode(uint8_t v, uint8_t adr=0xFF); //returns 0 on success
  uint8_t  set_max_level(uint8_t v, uint8_t adr=0xFF); //returns 0 on success
  uint8_t  set_min_level(uint8_t v, uint8_t adr=0xFF); //returns 0 on success
  uint8_t  set_system_failure_level(uint8_t v, uint8_t adr=0xFF); //returns 0 on success
  uint8_t  set_power_on_level(uint8_t v, uint8_t adr=0xFF); //returns 0 on success
  
  //commissioning
  uint8_t  commission(uint8_t init_arg=0xff);
  void     set_searchaddr(uint32_t adr);
  void     set_searchaddr_diff(uint32_t adr_new,uint32_t adr_current);
  uint8_t  compare();
  void     program_short_address(uint8_t shortadr);
  uint8_t  query_short_address();
  uint32_t find_addr();

  //low level functions
  uint8_t   send(uint8_t* tx_msg, uint8_t tx_len_bytes);
  uint8_t   sendwait(uint8_t* tx_msg, uint8_t tx_len_bytes, uint32_t timeout_us=500000);
  int16_t   tx(uint8_t cmd0, uint8_t cmd1, uint32_t timeout_us=500000);

  //initialize variables
  Dali() : tx_state(TX_IDLE), rx_state(RX_IDLE), tx_bus_low(0), tx_len(0), EventHandlerReceivedData(0) {};

protected:
  //low level functions
  enum tx_stateEnum { TX_IDLE=0,TX_START,TX_START_X,TX_BIT,TX_BIT_X,TX_STOP1,TX_STOP1_X,TX_STOP2,TX_STOP2_X,TX_STOP3};
  uint8_t   tx_msg[3]; //message to transmit
  uint8_t   tx_len; //number of bits to transmit
  volatile uint8_t tx_pos; //current bit transmit position
  volatile tx_stateEnum tx_state; //current state
  volatile uint8_t tx_collision; //collistion occured
  volatile uint8_t tx_bus_low; //bus is low according to transmitter?

  enum rx_stateEnum { RX_IDLE,RX_START,RX_BIT};
  volatile uint8_t rx_last_bus_low; //receiver as low at last pinchange
  volatile uint32_t rx_last_change_ts; //timestamp last pinchange
  volatile rx_stateEnum rx_state; //current state
  volatile uint8_t rx_msg[3]; //message received
  volatile uint8_t rx_len; //number of bytes received
  volatile int8_t rx_halfbitlen; //number of half bits received
  volatile uint8_t rx_last_halfbit; //last halfbit received
  


  volatile uint8_t bus_idle_te_cnt; //number of Te since start of idle bus

  void push_halfbit(uint8_t bit);
  
  //high level functions
  uint8_t check_yaaaaaa(uint8_t yaaaaaa); //check for yaaaaaa pattern
  uint8_t set_value(uint16_t setcmd, uint16_t getcmd, uint8_t v, uint8_t adr); //set a parameter value, returns 0 on success
}; 




#define DALI_BAUD 1200

#define DALI_RESULT_TIMEOUT          -1 //Timeout waiting for DALI bus
#define DALI_RESULT_DATA_TOO_LONG    -2 //Trying to send too many bytes (max 3)
#define DALI_RESULT_TX_TIMEOUT       -3 //Timeout during transmission
#define DALI_RESULT_NO_REPLY         -4 //cmd() did not receive a reply (i.e. received a 'NO' Backward Frame)
#define DALI_RESULT_INVALID_CMD      -5 //The cmd argument in the call to cmd() was invalid
#define DALI_RESULT_INVALID_REPLY    -6 //cmd() received an invalid reply (too long)

//bit8=extended commands, bit9=repeat
#define DALI_OFF 0 //0  - Turns off lighting.
#define DALI_UP 1 //1  - Increases the lighting control level for 200 ms according to the Fade rate.
#define DALI_DOWN 2 //2  - Decreases the lighting control level for 200 ms according to the Fade rate.
#define DALI_STEP_UP 3 //3  - Increments the lighting control level (without fade).
#define DALI_STEP_DOWN 4 //4  - Decrements the lighting control level (without fade).
#define DALI_RECALL_MAX_LEVEL 5 //5  - Maximizes the lighting control level (without fade).
#define DALI_RECALL_MIN_LEVEL 6 //6  - Minimizes the lighting control level (without fade)
#define DALI_STEP_DOWN_AND_OFF 7 //7  - Decrements the lighting control level and turns off lighting if the level is at the minimum (without fade).
#define DALI_ON_AND_STEP_UP 8 //8  - Increments the lighting control level and turns on lighting if lighting is off (with fade). 
#define DALI_ENABLE_DAPC_SEQUENCE 9 //9  - It shows the repeat start of the DAPC command.
#define DALI_GO_TO_LAST_ACTIVE_LEVEL 10 //10 DALI-2 - Adjusts the lighting control level to the last light control level according to the Fade time. (Command that exist only in IEC62386-102ed2.0)
#define DALI_RESERVED11 11 //11  - [Reserved]
#define DALI_RESERVED12 12 //12  - [Reserved]
#define DALI_RESERVED13 13 //13  - [Reserved]
#define DALI_RESERVED14 14 //14  - [Reserved]
#define DALI_RESERVED16 16 //16  - [Reserved]
#define DALI_GO_TO_SCENE0 16 //16  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE1 17 //17  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE2 18 //18  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE3 19 //19  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE4 20 //20  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE5 21 //21  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE6 22 //22  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE7 23 //23  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE8 24 //24  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE9 25 //25  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE10 26 //26  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE11 27 //27  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE12 28 //28  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE13 29 //29  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE14 30 //30  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_GO_TO_SCENE15 31 //31  - Adjusts the lighting control level for Scene XXXX according to the fade time.
#define DALI_RESET 544 //32 REPEAT - Makes a slave an RESET state.
#define DALI_STORE_ACTUAL_LEVEL_IN_THE_DTR0 545 //33 REPEAT - Saves the current lighting control level to the DTR (DTR0). (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SAVE_PERSISTENT_VARIABLES 546 //34 REPEAT DALI-2 - Saves a variable in nonvolatile memory (NVM). (Command that exist only in IEC62386-102ed2.0)
#define DALI_SET_OPERATING_MODE 547 //35 REPEAT DALI-2 - Sets data of DTR0 as an operating mode. (Command that exist only in IEC62386-102ed2.0)
#define DALI_RESET_MEMORY_BANK 548 //36 REPEAT DALI-2 - Changes to the reset value the specified memory bank in DTR0. (Command that exist only in IEC62386-102ed2.0)
#define DALI_IDENTIFY_DEVICE 549 //37 REPEAT DALI-2 - Starts the identification state of the device. (Command that exist only in IEC62386-102ed2.0)
#define DALI_RESERVED38 550 //38 REPEAT - [Reserved]
#define DALI_RESERVED39 551 //39 REPEAT - [Reserved]
#define DALI_RESERVED40 552 //40 REPEAT - [Reserved]
#define DALI_RESERVED41 553 //41 REPEAT - [Reserved]
#define DALI_SET_MAX_LEVEL 554 //42 REPEAT - Specifies the DTR data as the maximum lighting control level. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_MIN_LEVEL 555 //43 REPEAT - Specifies the DTR data as the minimum lighting control level. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SYSTEM_FAILURE_LEVEL 556 //44 REPEAT - Specifies the DTR data as the "FAILURELEVEL". (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_POWER_ON_LEVEL 557 //45 REPEAT - Specifies the DTR data as the "POWER ONLEVEL". (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_FADE_TIME 558 //46 REPEAT - Specifies the DTR data as the Fade time. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_FADE_RATE 559 //47 REPEAT - Specifies the DTR data as the Fade rate. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_EXTENDED_FADE_TIME 560 //48 REPEAT DALI-2 - Specifies the DTR data as the Extended Fade Time. (Command that exist only in IEC62386-102ed2.0)
#define DALI_RESERVED49 561 //49 REPEAT - [Reserved]
#define DALI_RESERVED50 562 //50 REPEAT - [Reserved]
#define DALI_RESERVED51 563 //51 REPEAT - [Reserved]
#define DALI_RESERVED52 564 //52 REPEAT - [Reserved]
#define DALI_RESERVED53 565 //53 REPEAT - [Reserved]
#define DALI_RESERVED54 566 //54 REPEAT - [Reserved]
#define DALI_RESERVED55 567 //55 REPEAT - [Reserved]
#define DALI_RESERVED56 568 //56 REPEAT - [Reserved]
#define DALI_RESERVED57 569 //57 REPEAT - [Reserved]
#define DALI_RESERVED58 570 //58 REPEAT - [Reserved]
#define DALI_RESERVED59 571 //59 REPEAT - [Reserved]
#define DALI_RESERVED60 572 //60 REPEAT - [Reserved]
#define DALI_RESERVED61 573 //61 REPEAT - [Reserved]
#define DALI_RESERVED62 574 //62 REPEAT - [Reserved]
#define DALI_RESERVED63 575 //63 REPEAT - [Reserved]
#define DALI_SET_SCENE0 576 //64 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE1 577 //65 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE2 578 //66 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE3 579 //67 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE4 580 //68 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE5 581 //69 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE6 582 //70 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE7 583 //71 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE8 584 //72 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE9 585 //73 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE10 586 //74 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE11 587 //75 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE12 588 //76 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE13 589 //77 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE14 590 //78 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_SET_SCENE15 591 //79 REPEAT - Specifies the DTR data as Scene XXXX. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_REMOVE_FROM_SCENE0 592 //80 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE1 593 //81 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE2 594 //82 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE3 595 //83 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE4 596 //84 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE5 597 //85 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE6 598 //86 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE7 599 //87 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE8 600 //88 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE9 601 //89 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE10 602 //90 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE11 603 //91 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE12 604 //92 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE13 605 //93 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE14 606 //94 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_REMOVE_FROM_SCENE15 607 //95 REPEAT - Deletes the Scene XXXX setting. (Specifies 1111 1111 for the scene register.)
#define DALI_ADD_TO_GROUP0 608 //96 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP1 609 //97 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP2 610 //98 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP3 611 //99 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP4 612 //100 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP5 613 //101 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP6 614 //102 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP7 615 //103 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP8 616 //104 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP9 617 //105 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP10 618 //106 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP11 619 //107 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP12 620 //108 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP13 621 //109 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP14 622 //110 REPEAT - Adds the slave to Group XXXX.
#define DALI_ADD_TO_GROUP15 623 //111 REPEAT - Adds the slave to Group XXXX.
#define DALI_REMOVE_FROM_GROUP0 624 //112 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP1 625 //113 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP2 626 //114 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP3 627 //115 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP4 628 //116 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP5 629 //117 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP6 630 //118 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP7 631 //119 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP8 632 //120 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP9 633 //121 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP10 634 //122 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP11 635 //123 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP12 636 //124 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP13 637 //125 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP14 638 //126 REPEAT - Deletes the slave from Group XXXX.
#define DALI_REMOVE_FROM_GROUP15 639 //127 REPEAT - Deletes the slave from Group XXXX.
#define DALI_SET_SHORT_ADDRESS 640 //128 REPEAT - Specifies the DTR data as a Short Address. (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_ENABLE_WRITE_MEMORY 641 //129 REPEAT - Allows writing of the memory bank.
#define DALI_RESERVED130 642 //130 REPEAT - [Reserved]
#define DALI_RESERVED131 643 //131 REPEAT - [Reserved]
#define DALI_RESERVED132 644 //132 REPEAT - [Reserved]
#define DALI_RESERVED133 645 //133 REPEAT - [Reserved]
#define DALI_RESERVED134 646 //134 REPEAT - [Reserved]
#define DALI_RESERVED135 647 //135 REPEAT - [Reserved]
#define DALI_RESERVED136 648 //136 REPEAT - [Reserved]
#define DALI_RESERVED137 649 //137 REPEAT - [Reserved]
#define DALI_RESERVED138 650 //138 REPEAT - [Reserved]
#define DALI_RESERVED139 651 //139 REPEAT - [Reserved]
#define DALI_RESERVED140 652 //140 REPEAT - [Reserved]
#define DALI_RESERVED141 653 //141 REPEAT - [Reserved]
#define DALI_RESERVED142 654 //142 REPEAT - [Reserved]
#define DALI_RESERVED143 655 //143 REPEAT - [Reserved]
#define DALI_QUERY_STATUS 144 //144  - Returns "STATUS INFORMATION"
#define DALI_QUERY_CONTROL_GEAR_PRESENT 145 //145  - Is there a slave that can communicate? (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_QUERY_LAMP_FAILURE 146 //146  - Is there a lamp problem?
#define DALI_QUERY_LAMP_POWER_ON 147 //147  - Is a lamp on?
#define DALI_QUERY_LIMIT_ERROR 148 //148  - Is the specified lighting control level out of the range from the minimum to the maximum values?
#define DALI_QUERY_RESET_STATE 149 //149  - Is the slave in 'RESET STATE'?
#define DALI_QUERY_MISSING_SHORT_ADDRESS 150 //150  - Does the slave not have a short address?
#define DALI_QUERY_VERSION_NUMBER 151 //151  - What is the corresponding IEC standard number?
#define DALI_QUERY_CONTENT_DTR0 152 //152  - What is the DTR content? (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_QUERY_DEVICE_TYPE 153 //153  - What is the device type?  (fluorescent lamp:0000 0000) (IEC62386-207 is 6 fixed)
#define DALI_QUERY_PHYSICAL_MINIMUM_LEVEL 154 //154  - What is the minimum lighting control level specified by the hardware?
#define DALI_QUERY_POWER_FAILURE 155 //155  - Has the slave operated without the execution of reset-command or the adjustment of the lighting control level?
#define DALI_QUERY_CONTENT_DTR1 156 //156  - What is the DTR1 content?
#define DALI_QUERY_CONTENT_DTR2 157 //157  - What is the DTR2 content?
#define DALI_QUERY_OPERATING_MODE 158 //158 DALI-2 - What is the Operating Mode? (Only IEC62386-102ed2.0 )
#define DALI_QUERY_LIGHT_SOURCE_TYPE 159 //159 DALI-2 - What is the Light source type? (Only IEC62386-102ed2.0 )
#define DALI_QUERY_ACTUAL_LEVEL 160 //160  - What is the "ACTUAL LEVEL" (the current lighting control level)?
#define DALI_QUERY_MAX_LEVEL 161 //161  - What is the maximum lighting control level?
#define DALI_QUERY_MIN_LEVEL 162 //162  - What is the minimum lighting control level?
#define DALI_QUERY_POWER_ON_LEVEL 163 //163  - What is the "POWER ON LEVEL" (the lighting control level when the power is turned on)?
#define DALI_QUERY_SYSTEM_FAILURE_LEVEL 164 //164  - What is the "SYSTEM FAILURE LEVEL" (the lighting control level when a failure occurs)?
#define DALI_QUERY_FADE_TIME_FADE_RATE 165 //165  - What are the Fade time and Fade rate?
#define DALI_QUERY_MANUFACTURER_SPECIFIC_MODE 166 //166 DALI-2 - What is the Specific Mode? (Command that exist only in IEC62386-102ed2.0)
#define DALI_QUERY_NEXT_DEVICE_TYPE 167 //167 DALI-2 - What is the next Device Type? (Command that exist only in IEC62386-102ed2.0)
#define DALI_QUERY_EXTENDED_FADE_TIME 168 //168 DALI-2 - What is the Extended Fade Time? (Command that exist only in IEC62386-102ed2.0)
#define DALI_QUERY_CONTROL_GEAR_FAILURE 169 //169 DALI-2 - Does a slave have the abnormality? (Command that exist only in IEC62386-102ed2.0)
#define DALI_RESERVED170 170 //170  - [Reserved]
#define DALI_RESERVED171 171 //171  - [Reserved]
#define DALI_RESERVED172 172 //172  - [Reserved]
#define DALI_RESERVED173 173 //173  - [Reserved]
#define DALI_RESERVED174 174 //174  - [Reserved]
#define DALI_RESERVED175 175 //175  - [Reserved]
#define DALI_QUERY_SCENE0_LEVEL 176 //176  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE1_LEVEL 177 //177  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE2_LEVEL 178 //178  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE3_LEVEL 179 //179  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE4_LEVEL 180 //180  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE5_LEVEL 181 //181  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE6_LEVEL 182 //182  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE7_LEVEL 183 //183  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE8_LEVEL 184 //184  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE9_LEVEL 185 //185  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE10_LEVEL 186 //186  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE11_LEVEL 187 //187  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE12_LEVEL 188 //188  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE13_LEVEL 189 //189  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE14_LEVEL 190 //190  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_SCENE15_LEVEL 191 //191  - What is the lighting control level for SCENE XXXX?
#define DALI_QUERY_GROUPS_0_7 192 //192  - Does the slave belong to a group among groups 0 to 7? (Each bit corresponds to agroup.)
#define DALI_QUERY_GROUPS_8_15 193 //193  - Does the slave belong to a group among groups 8 to 15? (Each bit corresponds to agroup.)
#define DALI_QUERY_RANDOM_ADDRESS_H 194 //194  - What are the higher 8 bits of the random address?
#define DALI_QUERY_RANDOM_ADDRESS_M 195 //195  - What are the middle 8 bits of the random address?
#define DALI_QUERY_RANDOM_ADDRESS_L 196 //196  - What are the lower 8 bits of the random address?
#define DALI_READ_MEMORY_LOCATION 197 //197  - What is the memory location content? 
#define DALI_RESERVED198 198 //198  - [Reserved]
#define DALI_RESERVED199 199 //199  - [Reserved]
#define DALI_RESERVED200 200 //200  - [Reserved]
#define DALI_RESERVED201 201 //201  - [Reserved]
#define DALI_RESERVED202 202 //202  - [Reserved]
#define DALI_RESERVED203 203 //203  - [Reserved]
#define DALI_RESERVED204 204 //204  - [Reserved]
#define DALI_RESERVED205 205 //205  - [Reserved]
#define DALI_RESERVED206 206 //206  - [Reserved]
#define DALI_RESERVED207 207 //207  - [Reserved]
#define DALI_RESERVED208 208 //208  - [Reserved]
#define DALI_RESERVED209 209 //209  - [Reserved]
#define DALI_RESERVED210 210 //210  - [Reserved]
#define DALI_RESERVED211 211 //211  - [Reserved]
#define DALI_RESERVED212 212 //212  - [Reserved]
#define DALI_RESERVED213 213 //213  - [Reserved]
#define DALI_RESERVED214 214 //214  - [Reserved]
#define DALI_RESERVED215 215 //215  - [Reserved]
#define DALI_RESERVED216 216 //216  - [Reserved]
#define DALI_RESERVED217 217 //217  - [Reserved]
#define DALI_RESERVED218 218 //218  - [Reserved]
#define DALI_RESERVED219 219 //219  - [Reserved]
#define DALI_RESERVED220 220 //220  - [Reserved]
#define DALI_RESERVED221 221 //221  - [Reserved]
#define DALI_RESERVED222 222 //222  - [Reserved]
#define DALI_RESERVED223 223 //223  - [Reserved]
#define DALI_REFERENCE_SYSTEM_POWER 224 //224 IEC62386-207 - Starts power measurement. (Command that exist only in IEC62386-207)
#define DALI_ENABLE_CURRENT_PROTECTOR 225 //225 IEC62386-207 - Enables the current protection. (Command that exist only in IEC62386-207)
#define DALI_DISABLE_CURRENT_PROTECTOR 226 //226 IEC62386-207 - Disables the current protection. (Command that exist only in IEC62386-207)
#define DALI_SELECT_DIMMING_CURVE 227 //227 IEC62386-207 - Selects Dimming curve. (Command that exist only in IEC62386-207)
#define DALI_STORE_DTR_AS_FAST_FADE_TIME 228 //228 IEC62386-207 - Sets the DTR of the data as Fast Fade Time.(Command that exist only in IEC62386-207)
#define DALI_RESERVED229 229 //229  - [Reserved]
#define DALI_RESERVED230 230 //230  - [Reserved]
#define DALI_RESERVED231 231 //231  - [Reserved]
#define DALI_RESERVED232 232 //232  - [Reserved]
#define DALI_RESERVED233 233 //233  - [Reserved]
#define DALI_RESERVED234 234 //234  - [Reserved]
#define DALI_RESERVED235 235 //235  - [Reserved]
#define DALI_RESERVED236 236 //236  - [Reserved]
#define DALI_QUERY_GEAR_TYPE 237 //237 IEC62386-207 - Returns ‘GEAR TYPE’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_DIMMING_CURVE 238 //238 IEC62386-207 - Returns ’Dimming curve’in use (Command that exist only in IEC62386-207)
#define DALI_QUERY_POSSIBLE_OPERATING_MODE 239 //239 IEC62386-207 - Returns ‘POSSIBLEG OPERATING MODE’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_FEATURES 240 //240 IEC62386-207 - Returns ‘FEATURES’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_FAILURE_STATUS 241 //241 IEC62386-207 - Returns ‘FAILURE STATUS’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_SHORT_CIRCUIT 242 //242 IEC62386-207 - Returns bit0 short circuit of ‘FAILURE STATUS’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_OPEN_CIRCUIT 243 //243 IEC62386-207 - Returns bit1 open circuit of ‘FAILURE STATUS’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_LOAD_DECREASE 244 //244 IEC62386-207 - Returns bit2 load decrease of ‘FAILURE STATUS’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_LOAD_INDREASE 245 //245 IEC62386-207 - Returns bit3 load increase of‘FAILURE STATUS’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_CURRENT_PROTECTOR_ACTIVE 246 //246 IEC62386-207 - Returns bit4 current protector active of ‘FAILURE STATUS’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_THERMAL_SHUTDOWN 247 //247 IEC62386-207 - Returns bit5 thermal shut down of ‘FAILURE STATUS’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_THERMAL_OVERLOAD 248 //248 IEC62386-207 - Returns bit6 thermal overload with light level reduction of ‘FAILURE STATUS’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_REFARENCE_RUNNING 249 //249 IEC62386-207 - Returns whetherReference System Power is in operation. (Command that exist only in IEC62386-207)
#define DALI_QUERY_REFERENCE_MEASURMENT_FAILED 250 //250 IEC62386-207 - Returns bit7 reference measurement failed  of ‘FAILURE STATUS’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_CURRENT_PROTECTOR_ENABLE 251 //251 IEC62386-207 - Returns state of Curent protector (Command that exist only in IEC62386-207)
#define DALI_QUERY_OPERATING_MODE 252 //252 IEC62386-207 - Returns ‘OPERATING MODE’ (Command that exist only in IEC62386-207)
#define DALI_QUERY_FAST_FADE_TIME 253 //253 IEC62386-207 - Returns set Fast fade time. (Command that exist only in IEC62386-207)
#define DALI_QUERY_MIN_FAST_FADE_TIME 254 //254 IEC62386-207 - Returns set Minimum fast fade time (Command that exist only in IEC62386-207)
#define DALI_QUERY_EXTENDED_VERSION_NUMBER 255 //255 IEC62386-207 - The version number of the extended support? IEC62386-207: 1, Other: NO(no response)
#define DALI_TERMINATE 0x01A1 //256  - Releases the INITIALISE state.
#define DALI_DATA_TRANSFER_REGISTER0 0x01A3 //257  - Stores the data XXXX XXXX to the DTR(DTR0). (In the parenthesis is a name in IEC62386-102ed2.0)
#define DALI_INITIALISE 0x03A5 //258 REPEAT - Sets the slave to the INITIALISE status for15 minutes. Commands 259 to 270 are enabled only for a slave in this status.
#define DALI_RANDOMISE 0x03A7 //259 REPEAT - Generates a random address.
#define DALI_COMPARE 0x01A9 //260  - Is the random address smaller or equal to the search address?
#define DALI_WITHDRAW 0x01AB //261  - Excludes slaves for which the random address and search address match from the Compare process.
#define DALI_RESERVED262 0x01AD //262  - [Reserved]
#define DALI_PING 0x01AF //263 DALI-2 - Ignores in the slave. (Command that exist only in IEC62386-102ed2.0)
#define DALI_SEARCHADDRH 0x01B1 //264  - Specifies the higher 8 bits of the search address.
#define DALI_SEARCHADDRM 0x01B3 //265  - Specifies the middle 8 bits of the search address.
#define DALI_SEARCHADDRL 0x01B5 //266  - Specifies the lower 8 bits of the search address.
#define DALI_PROGRAM_SHORT_ADDRESS 0x01B7 //267  - The slave shall store the received 6-bit address (AAA AAA) as a short address if it is selected.
#define DALI_VERIFY_SHORT_ADDRESS 0x01B9 //268  - Is the short address AAA AAA?
#define DALI_QUERY_SHORT_ADDRESS 0x01BB //269  - What is the short address of the slaveNote 2being selected?
#define DALI_PHYSICAL_SELECTION 0x01BD //270 not DALI-2 - Sets the slave to Physical Selection Mode and excludes the slave from the Compare process. (Excluding IEC62386-102ed2.0) (Command that exist only in IEC62386-102ed1.0, -207ed1.0)
#define DALI_RESERVED271 0x01BF //271  - [Reserved]
#define DALI_ENABLE_DEVICE_TYPE_X 0x01C1 //272  - Adds the device XXXX (a special device).
#define DALI_DATA_TRANSFER_REGISTER1 0x01C3 //273  - Stores data XXXX into DTR1.
#define DALI_DATA_TRANSFER_REGISTER2 0x01C5 //274  - Stores data XXXX into DTR2.
#define DALI_WRITE_MEMORY_LOCATION 0x01C7 //275  - Write data into the specified address of the specified memory bank. (There is BW) (DTR(DTR0)：address, DTR1：memory bank number)
#define DALI_WRITE_MEMORY_LOCATION_NO_REPLY 0x01C9 //276 DALI-2 - Write data into the specified address of the specified memory bank. (There is no BW) (DTR(DTR0)：address, TR1：memory bank number) (Command that exist only in IEC62386-102ed2.0)
#define DALI_RESERVED277 0x01CB //277  - [Reserved]
#define DALI_RESERVED278 0x01CD //278  - [Reserved]
#define DALI_RESERVED279 0x01CF //279  - [Reserved]
#define DALI_RESERVED280 0x01D1 //280  - [Reserved]
#define DALI_RESERVED281 0x01D3 //281  - [Reserved]
#define DALI_RESERVED282 0x01D5 //282  - [Reserved]
#define DALI_RESERVED283 0x01D7 //283  - [Reserved]
#define DALI_RESERVED284 0x01D9 //284  - [Reserved]
#define DALI_RESERVED285 0x01DB //285  - [Reserved]
#define DALI_RESERVED286 0x01DD //286  - [Reserved]
#define DALI_RESERVED287 0x01DF //287  - [Reserved]
#define DALI_RESERVED288 0x01E1 //288  - [Reserved]
#define DALI_RESERVED289 0x01E3 //289  - [Reserved]
#define DALI_RESERVED290 0x01E5 //290  - [Reserved]
#define DALI_RESERVED291 0x01E7 //291  - [Reserved]
#define DALI_RESERVED292 0x01E9 //292  - [Reserved]
#define DALI_RESERVED293 0x01EB //293  - [Reserved]
#define DALI_RESERVED294 0x01ED //294  - [Reserved]
#define DALI_RESERVED295 0x01DF //295  - [Reserved]
#define DALI_RESERVED296 0x01F1 //296  - [Reserved]
#define DALI_RESERVED297 0x01F3 //297  - [Reserved]
#define DALI_RESERVED298 0x01F5 //298  - [Reserved]
#define DALI_RESERVED299 0x01F7 //299  - [Reserved]
#define DALI_RESERVED300 0x01F9 //300  - [Reserved]
#define DALI_RESERVED301 0x01FB //301  - [Reserved]
#define DALI_RESERVED302 0x01FD //302  - [Reserved]




/*  
SIGNAL CHARACTERISTICS
High Level: 9.5 to 22.5 V (Typical 16 V)
Low Level: -6.5 to + 6.5 V (Typical 0 V)
Te = half cycle = 416.67 us +/- 10 %
10 us <= tfall <= 100 us 
10 us <= trise <= 100 us

BIT TIMING
msb send first
 logical 1 = 1Te Low 1Te High
 logical 0 = 1Te High 1Te Low
 Start bit = logical 1
 Stop bit = 2Te High

FRAME TIMING
FF: TX Forward Frame 2 bytes (38Te) = 2*(1start+16bits+2stop)
BF: RX Backward Frame 1 byte (22Te) = 2*(1start+8bits+2stop)
no reply: FF >22Te pause FF
with reply: FF >7Te <22Te pause BF >22Te pause FF


DALI commands
=============
In accordance with the DIN EN 60929 standard, addresses and commands are transmitted as numbers with a length of two bytes.

These commands take the form YAAA AAAS xxXXxx. Each letter here stands for one bit.

Y: type of address
     0bin:    short address
     1bin:    group address or collective call

A: significant address bit

S: selection bit (specifies the significance of the following eight bits):
     0bin:    the 8 xxXXxx bits contain a value for direct control of the lamp power
     1bin:    the 8 xxXXxx bits contain a command number.

x: a bit in the lamp power or in the command number


Type of Addresses
=================
Type of Addresses Byte Description
Short address 0AAAAAAS (AAAAAA = 0 to 63, S = 0/1)
Group address 100AAAAS (AAAA = 0 to 15, S = 0/1)
Broadcast address 1111111S (S = 0/1)
Special command 101CCCC1 (CCCC = command number)


Direct DALI commands for lamp power
===================================
These commands take the form YAAA AAA0 xxXXxx.

xxXXxx: the value representing the lamp power is transmitted in these 8 bits. It is calculated according to this formula: 

Pvalue = 10 ^ ((value-1) / (253/3)) * Pmax / 1000

253 values from 1dec to 254dec are available for transmission in accordance with this formula.

There are also 2 direct DALI commands with special meanings:

Command; Command No; Description; Answer
00hex; 0dec; The DALI device dims using the current fade time down to the parameterised MIN value, and then switches off.; - 
FFhex; 254dec; Mask (no change): this value is ignored in what follows, and is therefore not loaded into memory.; - 


Indirect DALI commands for lamp power
=====================================
These commands take the form YAAA AAA1 xxXXxx.

xxXXxx: These 8 bits transfer the command number. The available command numbers are listed and explained in the following tables in hexadecimal and decimal formats.

Command; Command No; Description; Answer
00hex 0dez Extinguish the lamp (without fading) - 
01hex 1dez Dim up 200 ms using the selected fade rate - 
02hex 2dez Dim down 200 ms using the selected fade rate - 
03hex 3dez Set the actual arc power level one step higher without fading. If the lamp is off, it will be not ignited. - 
04hex 4dez Set the actual arc power level one step lower without fading. If the lamp has already it's minimum value, it is not switched off. - 
05hex 5dez Set the actual arc power level to the maximum value. If the lamp is off, it will be ignited. - 
06hex 6dez Set the actual arc power level to the minimum value. If the lamp is off, it will be ignited. - 
07hex 7dez Set the actual arc power level one step lower without fading. If the lamp has already it's minimum value, it is switched off. - 
08hex 8dez Set the actual arc power level one step higher without fading. If the lamp is off, it will be ignited. - 
09hex ... 0Fhex 9dez ... 15dez reserved - 
1nhex
(n: 0hex ... Fhex) 16dez ... 31dez Set the light level to the value stored for the selected scene (n) - 


Configuration commands
======================
Command; Command No; Description; Answer
20hex 32dez Reset the parameters to default settings - 
21hex 33dez Store the current light level in the DTR (Data Transfer Register) - 
22hex ... 29hex 34dez ... 41dez reserved - 
2Ahex 42dez Store the value in the DTR as the maximum level - 
2Bhex 43dez Store the value in the DTR as the minimum level - 
2Chex 44dez Store the value in the DTR as the system failure level - 
2Dhex 45dez Store the value in the DTR as the power on level - 
2Ehex 46dez Store the value in the DTR as the fade time - 
2Fhex 47dez Store the value in the DTR as the fade rate - 
30hex ... 3Fhex 48dez ... 63dez reserved - 
4nhex
(n: 0hex ... Fhex) 64dez ... 79dez Store the value in the DTR as the selected scene (n) - 
5nhex
(n: 0hex ... Fhex) 80dez ... 95dez Remove the selected scene (n) from the DALI slave - 
6nhex
(n: 0hex ... Fhex) 96dez ... 111dez Add the DALI slave unit to the selected group (n) - 
7nhex
(n: 0hex ... Fhex) 112dez ... 127dez Remove the DALI slave unit from the selected group (n) - 
80hex 128dez Store the value in the DTR as a short address - 
81hex ... 8Fhex 129dez ... 143dez reserved - 
90hex 144dez Returns the status (XX) of the DALI slave XX 
91hex 145dez Check if the DALI slave is working yes/no 
92hex 146dez Check if there is a lamp failure yes/no 
93hex 147dez Check if the lamp is operating yes/no 
94hex 148dez Check if the slave has received a level out of limit yes/no 
95hex 149dez Check if the DALI slave is in reset state yes/no 
96hex 150dez Check if the DALI slave is missing a short address XX 
97hex 151dez Returns the version number as XX 
98hex 152dez Returns the content of the DTR as XX 
99hex 153dez Returns the device type as XX 
9Ahex 154dez Returns the physical minimum level as XX 
9Bhex 155dez Check if the DALI slave is in power failure mode yes/no 
9Chex ... 9Fhex 156dez ... 159dez reserved - 
A0hex 160dez Returns the current light level as XX 
A1hex 161dez Returns the maximum allowed light level as XX 
A2hex 162dez Returns the minimum allowed light level as XX 
A3hex 163dez Return the power up level as XX 
A4hex 164dez Returns the system failure level as XX 
A5hex 165dez Returns the fade time as X and the fade rate as Y XY 
A6hex ... AFhex 166dez ... 175dez reserved - 
Bnhex
(n: 0hex ... Fhex) 176dez ... 191dez Returns the light level XX for the selected scene (n) XX 
C0hex 192dez Returns a bit pattern XX indicating which group (0-7) the DALI slave belongs to XX 
C1hex 193dez Returns a bit pattern XX indicating which group (8-15) the DALI slave belongs to XX 
C2hex 194dez Returns the high bits of the random address as HH 
C3hex 195dez Return the middle bit of the random address as MM 
C4hex 196dez Returns the lower bits of the random address as LL 
C5hex ... DFhex 197dez ... 223dez reserved - 
E0hex ... FFhex 224dez ... 255dez Returns application specific extension commands   


Note Repeat of DALI commands 
============================
According to IEC 60929, a DALI Master has to repeat several commands within 100 ms, so that DALI-Slaves will execute them. 

The DALI Master Terminal KL6811 repeats the commands 32dez to 128dez, 258dez and 259dez (bold marked) automatically to make the the double call from the user program unnecessary.

The DALI Master Terminal KL6811 repeats also the commands 224dez to 255dez, if you have activated this with Bit 1 of the Control-Byte (CB.1) before.


DALI Control Device Type List
=============================
Type DEC Type HEX Name Comments
128 0x80 Unknown Device. If one of the devices below don't apply
129 0x81 Switch Device A Wall-Switch based Controller including, but not limited to ON/OFF devices, Scene switches, dimming device.
130 0x82 Slide Dimmer An analog/positional dimming controller 
131 0x83 Motion/Occupancy Sensor. A device that indicates the presence of people within a control area.
132 0x84 Open-loop daylight Controller. A device that outputs current light level and/or sends control messages to actuators based on light passing a threshold.
133 0x85 Closed-loop daylight controller. A device that outputs current light level and/or sends control messages to actuators based on a change in light level.
134 0x86 Scheduler. A device that establishes the building mode based on time of day, or which provides control outputs.
135 0x87 Gateway. An interface to other control systems or communication busses
136 0x88 Sequencer. A device which sequences lights based on a triggering event
137 0x89 Power Supply *). A DALI Power Supply device which supplies power for the communication loop
138 0x8a Emergency Lighting Controller. A device, which is certified for use in control of emergency lighting, or, if not certified, for noncritical backup lighting.
139 0x8b Analog input unit. A general device with analog input.
140 0x8c Data Logger. A unit logging data (can be digital or analog data)


Flash Variables and Offset in Information
=========================================
Memory Name Offset
Power On Level [0]
System Failure Level [1]
Minimum Level [2]
Maximum Level [3]
Fade Rate [4]
Fade Time [5]
Short Address [6]
Group 0 through 7 [7]
Group 8 through 15 [8]
Scene 0 through 15 [9-24]
Random Address [25-27]
Fast Fade Time [28]
Failure Status [29]
Operating Mode [30]
Dimming Curve [31]
*/
