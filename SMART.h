/*
 * SMART.h
 *
 * Apple II SmartPort Emulator
 *
 * Based on a program by Robert Justice
 * http://www.users.on.net/~rjustice/SmartportCFA/SmartportCFA.htm
 * arranged for SDISK II/UNISDISK by Koichi Nishida
 *
 * Created: 2013/10/11 19:27:24
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

#ifndef SMART_H_
#define SMART_H_

#include "SD.h"
#include "UI.h"

extern uint8_t SMART_is2mg[];
extern uint8_t SMART_partition_num;
extern uint8_t SMART_desktop;

// initialize the SmartPort device emulator
void SMART_init(void);

// mount a SmartPort disk image
void SMART_mount(struct FILE *img, uint8_t partition);

// unmount a SmartPort disk image
void SMART_unmount(uint8_t partition);

// run the SmartPort device emulator
void SMART_run(uint8_t (*pf)(uint8_t));

#ifdef SDISK2P
// should be called before SDISK2P eject reset
void SMART_eject(void);
#endif

// execute SmartPort protocol
//void SMART_protocol(void);

#endif /* SMART_H_ */