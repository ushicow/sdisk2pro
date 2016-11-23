/*
 * UI.h
 *
 * Created: 2013/10/10 20:52:19
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

#ifndef SDISKP

#ifndef UI_H_
#define UI_H_

#define PSW1 (/*PORTA_IN*/VPORT0_IN&PIN4_bm)
#define PSW2 (/*PORTC_IN*/VPORT1_IN&PIN2_bm)
#define PSW3 (/*PORTD_IN*/VPORT2_IN&PIN3_bm)
#define PSW4 (/*PORTD_IN*/VPORT2_IN&PIN1_bm)

#define PSW(n) ((n==1)?PSW1:((n==2)?PSW2:((n==3)?PSW3:PSW4)))

// whether UI is running or not
extern volatile int8_t UI_running;

// initialize user interface
void UI_init(void);

// clear properties
void UI_clearProperties(void);

// choose a file from an SD card
uint8_t UI_chooseFile(void *tempBuff, uint8_t *err);

// called from DISK2 / SmartPort emulator
uint8_t UI_checkExecute(void);

#endif /* UI_H_ */

#endif