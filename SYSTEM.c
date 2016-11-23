/*
 * SYSTEM.c
 *
 * Created: 2013/09/23 14:21:18
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
#include "SYSTEM.h"

// System Clocks initialization
void system_clocks_init(void)
{
	unsigned char s,n;
	// Save interrupts enabled/disabled state
	s=SREG;
	// Disable interrupts
	asm("cli");

#if F_CPU==32000000
	// Internal 32 kHz RC oscillator initialization
	// Enable the internal 32 kHz RC oscillator
	OSC.CTRL|=OSC_RC32KEN_bm;
	// Wait for the internal 32 kHz RC oscillator to stabilize
	while ((OSC.STATUS & OSC_RC32KRDY_bm)==0);

	// Internal 32 MHz RC oscillator initialization
	// Enable the internal 32 MHz RC oscillator
	OSC.CTRL|=OSC_RC32MEN_bm;

	// System clock prescaler A division factor: 1
	// System clock prescalers B & C division factors: B:1, C:1
	// ClkPer4: 32000.000 kHz
	// ClkPer2: 32000.000 kHz
	// ClkPer:  32000.000 kHz
	// ClkCPU:  32000.000 kHz
	n=(CLK.PSCTRL & (~(CLK_PSADIV_gm | CLK_PSBCDIV1_bm | CLK_PSBCDIV0_bm))) |
	CLK_PSADIV_1_gc | CLK_PSBCDIV_1_1_gc;
	CCP=CCP_IOREG_gc;
	CLK.PSCTRL=n;

	// Internal 32 MHz RC osc. calibration reference clock source: 32.768 kHz Internal Osc.
	OSC.DFLLCTRL&=0b11110011;
	// Enable the autocalibration of the internal 32 MHz RC oscillator
	DFLLRC32M.CTRL|=DFLL_ENABLE_bm;

	// Wait for the internal 32 MHz RC oscillator to stabilize
	while ((OSC.STATUS & OSC_RC32MRDY_bm)==0);

	// Select the system clock source: 32 MHz Internal RC Osc.
	n=(CLK.CTRL & (~CLK_SCLKSEL_gm)) | CLK_SCLKSEL_RC32M_gc;
	CCP=CCP_IOREG_gc;
	CLK.CTRL=n;

	// Disable the unused oscillators: 2 MHz, external clock/crystal oscillator, PLL
	OSC.CTRL&= ~(OSC_RC2MEN_bm | OSC_XOSCEN_bm | OSC_PLLEN_bm);

	// Peripheral Clock output: Disabled
	// PORTCFG.CLKOUT=(PORTCFG.CLKEVOUT & (~PORTCFG_CLKOUT_gm)) | PORTCFG_CLKOUT_OFF_gc;
#endif

	// Restore interrupts enabled/disabled state
	SREG=s;
} // system_clocks_init()
