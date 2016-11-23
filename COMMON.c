/*
 * COMMON.c
 *
 * Created: 2013/10/10 21:37:49
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

#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include "BUFFER.h"
#include "COMMON.h"
#include "SD.h"
#include "LCD.h"
#include "DISK2.h"
#include "SMART.h"

// mode : DISK2MODE | SMARTMODE
enum MODE mode;

// decremented toward 0 by a 0.01 sec counter.
volatile uint16_t timerOneShot, timerOneShot2;
// blink at 0.33 sec
volatile uint8_t timerBlink = 0;

// initialize
void COMMON_init(void)
{
#ifdef SDISK2P
	// initialize input ports
	// PHASE
	PORTC |= 0b00001111;
	DDRC &= ~0b00001111;
	//WDAT
	PORTB |= (1<<1);
	DDRB &= ~(1<<1);
	//WREQ
	PORTD &= ~(1<<2);
	DDRD &= ~(1<<2);
	//EN
	PORTD &= (1<<7);
	DDRD &= ~(1<<7);

	// initialize output ports
	//RDR
	DDRC |= (1<<4);
	//SENSE
	DDRD |= (1<<4);
	//LED
	DDRD |= (1<<5);

	// ON PHASE interrupt
	PCMSK1 |= 0b00001111;
	PCICR |= (1<<PCIE1);

	// timer1 (about 0.01 sec)
	OCR1A = 264;
	TCCR1A = 0;
	TCCR1B = (1<<WGM12)|(1<<CS12)|(1<<CS10);

	if (mode==DISK2MODE) {
		buffer2.disk2.img[0].valid = 0;
	} else {
		buffer2.smart.img[0].valid = 0;
	}
	
#else

	//if (PORTD.PIN1CTRL != PORT_OPC_PULLUP_gc) while (1);

	// initialize input ports
	// PHASE
	PORTA.DIRCLR = PHASE_bm;
	PORTCFG.MPCMASK = PHASE_bm;
	PORTA.PIN0CTRL = PORT_OPC_PULLUP_gc;
	// WDAT	
	PORTA.DIRCLR = PIN6_bm;
	PORTA.PIN6CTRL = PORT_OPC_PULLUP_gc;
	// WREQ
	PORTD.DIRCLR = PIN2_bm;
	PORTD.PIN2CTRL = PORT_OPC_PULLUP_gc;
	// EN2
	PORTD.DIRCLR = PIN5_bm;
	PORTD.PIN5CTRL = PORT_OPC_PULLUP_gc;
	// EN1
	PORTD.DIRCLR = PIN6_bm;
	PORTD.PIN6CTRL = PORT_OPC_PULLUP_gc;
	// HDSEL
	PORTA.DIRCLR = PIN7_bm;
	PORTA.PIN7CTRL = PORT_OPC_PULLUP_gc;
	// DIK35
	PORTD.DIRCLR = PIN4_bm;
	PORTD.PIN4CTRL = PORT_OPC_PULLUP_gc;

	// initialize output ports
	// RDR
	PORTD.DIRSET = PIN0_bm;
	// SENSE
	PORTD.DIRSET = PIN7_bm;

	// PHASE interrupt
	PORTA.PIN0CTRL |= PORT_ISC_BOTHEDGES_gc;
	PORTA.INTMASK |= PHASE_bm;
	PORTA.INTCTRL = PORT_INTLVL_LO_gc;
	PMIC.CTRL = PMIC_LOLVLEN_bm; 

	// TC5 about 0.01 sec used by UI
	TCC5.PER = 313;
	TCC5.CTRLA = TC5_CLKSEL2_bm|TC5_CLKSEL1_bm|TC5_CLKSEL0_bm;			// prescaler : clk/1024
	TCC5.INTCTRLA = 0;	// meddle level interrupt, but off for the time being
	PMIC.CTRL = PMIC_LOLVLEN_bm;
	if (mode == DISK2MODE)
		for (uint8_t i = 0; i < 2; i++) buffer2.disk2.img[i].valid = 0;
	else
		for (uint8_t i = 0; i < 4; i++) buffer2.smart.img[i].valid = 0;
#endif
}

uint32_t readmem_long(uint8_t *mem)
{
	return *(uint32_t *)mem;
}

uint16_t readmem_word(uint8_t *mem)
{
	return *(uint16_t *)mem;
}

// timer handler
#ifdef SDISK2P
ISR(TIMER1_COMPA_vect) {
#else
ISR(TCC5_OVF_vect) {
	TCC5_INTFLAGS = 1;
#endif
	static uint8_t counter = 0;
	if (timerOneShot>0) timerOneShot--;
	if (timerOneShot2>0) timerOneShot2--;
		
	if (++counter==33) {counter = 0; timerBlink ^= 1;}
}
