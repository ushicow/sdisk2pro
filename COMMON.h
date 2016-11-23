/*
 * COMMON.h
 *
 * Created: 2013/09/23 14:11:33
 *  Author: Koichi Nishida
 */ 
/*
Copyright (C) 2013 Koichi NISHIDA
email to Koichi NISHIDA: tulip-house@msf.biglobe.ne.jp

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COMMON_H_
#define COMMON_H_

#include "SD.h"

#ifdef SDISK2P
#define DISK2_WRITE_BUF_NUM 3
#else
#define DISK2_WRITE_BUF_NUM 5
#endif

// mode : DISK2MODE | SMARTMODE
enum MODE {DISK2MODE, SMARTMODE};
extern enum MODE mode;

// decremented by a 0.01 sec counter. used by push buttons.
extern volatile uint16_t timerOneShot, timerOneShot2;
extern volatile uint8_t timerBlink;

// ON/OFF 0.01 sec timer
#ifdef SDISK2P
#define ON_TIMER2 TIMSK1|=(1<<OCIE1A)
#define OFF_TIMER2 TIMSK1&=~(1<<OCIE1A)
#else
#define ON_TIMER2 TCC5.INTCTRLA = TC5_OVFINTLVL0_bm;
#define OFF_TIMER2 TCC5.INTCTRLA = 0;
#endif

#ifndef SDISK2P
#define ON_PHASEINT PORTA.INTCTRL = PORT_INTLVL_LO_gc;
#define OFF_PHASEINT { PORTA.INTCTRL = 0; }
#endif

#define PHASE_bm 0b00001111

#ifdef SDISK2P
#define LED_ON PORTD|=(1<<5)
#define LED_OFF PORTD&=~(1<<5)
#endif

#ifdef SDISK2P
#define DISABLE_CS PORTB|=(1<<SPI_SS_bm)
#define ENABLE_CS PORTB&=~(1<<SPI_SS_bm)
#else
#define DISABLE_CS PORTC_OUTSET=SPI_SS_bm
#define ENABLE_CS PORTC_OUTCLR=SPI_SS_bm
#endif

#ifdef SDISK2P
#define PHASE (PINC&0b00001111)
#define WDAT (PINB&(1<<1))
#define WREQ (PIND&(1<<2))
#define EN1 (PIND&(1<<7))
#else
#define PHASE (/*PORTA_IN*/VPORT0_IN&0b00001111)
#define WDAT (/*PORTA_IN*/VPORT0_IN&PIN6_bm)
#define WREQ (/*PORTD_IN*/VPORT2_IN&PIN2_bm)
#define EN2 (/*PORTD_IN*/VPORT2_IN&PIN5_bm)
#define EN1 (/*PORTD_IN*/VPORT2_IN&PIN6_bm)
#define HDSEL (/*PORTA_IN*/VPORT0_IN&PIN7_bm)
#define DIK35 (/*PORTD_IN*/VPORT2_IN&PIN4_bm)
#endif

#define nop() __asm__ __volatile__ ("nop");

// image files
extern struct FILE img[];

// common initialization
void COMMON_init(void);

// read a long word data from memory
uint32_t readmem_long(uint8_t *mem);

// read a word data from memory
uint16_t readmem_word(uint8_t *mem);

#endif /* COMMON_H_ */