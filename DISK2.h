/*
 * DISK2.h
 *
 * Created: 2013/10/08 22:45:24
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

#ifndef DISK2_H_
#define DISK2_H_

#include "SD.h"

// ON/OFF 4u sec timer
#ifdef SDISK2P
#define ON_TIMER TIMSK0|=(1<<OCIE0A)
#define OFF_TIMER TIMSK0&=~(1<<OCIE0A)
#else
#define ON_TIMER TCC4.INTCTRLA=TC4_OVFINTLVL0_bm
#define OFF_TIMER TCC4.INTCTRLA=0
#endif

// initialize DISK2 emulator
void DISK2_init(void);

// mount an image on the DISK2 emulator
void DISK2_mount(struct FILE *img, uint8_t drive);

// unmount an image from the DISK2 emulator
void DISK2_unmount(uint8_t drive);

// run the DISK2 emulator
int DISK2_run(uint8_t (*pf)(uint8_t));

// move head
void DISK2_moveHead(void);

// should be called before SDISK2P eject reset
void DISK2_eject(void);

// convert a DSK file to a NIC file
void DISK2_dsk2Nic(struct FILE *nicFile, struct FILE *dskFile, uint8_t volume, uint8_t *err);

// convert a NIC file to a DSK file
void DISK2_nic2Dsk(struct FILE *dskFile, struct FILE *nicFile, uint8_t *err);

#endif /* DISK2_H_ */