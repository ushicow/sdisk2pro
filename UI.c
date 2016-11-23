/*
 * UI.c
 *
 * Created: 2013/10/10 21:26:42
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
#include <avr/interrupt.h>
#include "COMMON.h"
#include "UI.h"
#include "SD.h"
#include "LCD.h"
#include "BUFFER.h"
#include "INI.h"
#include "DISK2.h"
#include "SMART.h"
#include <string.h>

// properties
uint32_t UI_dir;
char *UI_name;
int8_t UI_depth;
uint16_t UI_cur;
int8_t UI_drv;
volatile int8_t UI_running = 0;

//return 0: no push0
//       1: short push (less than 0.4 sec)
//	     2: long push
uint8_t button(uint8_t n)
{
	static uint8_t b[3];
	
	if (PSW(n)) { // released...
		if (b[n]) {
			timerOneShot = 2;
			while (timerOneShot) if (!PSW(n)) { return 0; }
			b[n] = 0;
		}
		return 0;
	} else if (!b[n]) { // pushed...
		timerOneShot = 2;
		while (timerOneShot) if (PSW(n)) { return 0; }
		b[n] = 1;
		timerOneShot = 40;
		while (timerOneShot) if (PSW(n)) { return 1; }
		return 2;
	} else return 0;
}

// initialize user interface
void UI_init(void)
{
	// PSW1
	PORTA.DIRCLR = PIN4_bm;
	PORTA.PIN4CTRL = PORT_OPC_PULLUP_gc;
	// PORTA.INTMASK |= PIN4_bm;
	
	// PSW2
	PORTC.DIRCLR = PIN2_bm;
	PORTC.PIN2CTRL = PORT_OPC_PULLUP_gc;
	
	// PSW3
	PORTD.DIRCLR = PIN3_bm;
	PORTD.PIN3CTRL = PORT_OPC_PULLUP_gc;
	
	// PSW4
	PORTD.DIRCLR = PIN1_bm;
	PORTD.PIN1CTRL = PORT_OPC_PULLUP_gc;
	
	UI_clearProperties();
}

// clear properties
void UI_clearProperties(void)
{
	UI_drv = 0;
	UI_running = 0;
	UI_dir = 0;
	UI_name = (char *)buffer2.ui.fullpath;
	UI_depth = 0;
	UI_cur = 0;
}

// choose file from an SD card
uint8_t UI_chooseFile(void *tempBuff, uint8_t *err)
{
	uint32_t stclst;
	struct FILELST *list = (struct FILELST *)tempBuff;
	OFF_PHASEINT;
	LCD_printAll("Wait a  moment. ");
	uint16_t num = FILE_makeList(UI_dir, list, (mode==DISK2MODE)?"DSKDO ":"PO 2MG", err);

	ON_PHASEINT;
	char name_[8], ext_[3];
	int16_t prevCur = -1;
	uint16_t i;
	uint8_t attr = 0;

	if (num==0) {
		LCD_printAll("No imagefiles.  ");
		timerOneShot = 100;
		while (timerOneShot) ;
		LCD_cls();
		return 0;
	} else {
		// if there is at least one image file,
		timerOneShot2 = 300;
		while (1) {
			if (EJECT) {*err=1; return 0;}
			LCD_locate(0,0);
			LCD_print("DR   ", 5);
			LCD_locate(2,0);
			LCD_printdec(UI_drv+1,1);
			// determine first file
			if (UI_name[UI_drv]) {
				for (i=0; i<num; i++) {
					OFF_PHASEINT;
					if ((list[UI_cur].blkAdr==0)&&(list[UI_cur].offset==0)) {
						memcpy(name_, "UNMOUNT ", 8);
						memcpy(ext_, "   ",3);
					} else FILE_getEntry(&list[i], name_, ext_, 0, 0, err);
					ON_PHASEINT;
					if (*err) return 0;
					if ((memcmp(name_, UI_name, 8)==0)&&(memcmp(ext_, UI_name+8, 3)==0)) {
						UI_cur = i;
						break;
					}
				}
			}
			while (1) {
				uint8_t b;

				if (EJECT) {*err=1; return 0;}
				if ((b=button(1))!=0) {
					if (b==1) {
						if (UI_cur<(num-1)) UI_cur++;
					} else {
						if (UI_drv>0) {
							UI_drv--;
						}
					}
				}
				if ((b=button(2))!=0) {
					if (b==1) {
						if (UI_cur > 0) UI_cur--;
					} else {
						if (UI_drv<((mode==DISK2MODE)?1:3)) {
							UI_drv++;
						}
					}
				}
				LCD_locate(2,0);
				LCD_printdec(UI_drv+1,1);
				// display file name
				if (prevCur != UI_cur) {
					prevCur = UI_cur;
					OFF_PHASEINT;
					
					if ((list[UI_cur].blkAdr==0)&&(list[UI_cur].offset==0)) {
						memcpy(name_, "UNMOUNT ", 8);
						memcpy(ext_, "   ",3);
						attr = 0;
					} else FILE_getEntry(&list[UI_cur], name_, ext_, &attr, 0, err);
					ON_PHASEINT;
					if (*err) return 0;
					LCD_locate(5,0);
					LCD_print(attr&0b00010000?"DIR":ext_, 3);
				}
				if ((b=button(3))!=0) {
					LCD_cls();
					LCD_locate(0,1);
					LCD_print(name_, 8);
					break;
				}
				
				if (button(4)) {
					LCD_cls();
					return 0;
				}
				LCD_locate(0,1);
				LCD_print((timerBlink?name_:"        "), 8);
			}
			OFF_PHASEINT;
			if ((list[UI_cur].blkAdr==0)&&(list[UI_cur].offset==0)) {
				memcpy(name_, "UNMOUNT ", 8);
				memcpy(ext_, "   ",3);
				attr = 0;
				stclst = 0;
			} else FILE_getEntry(&list[UI_cur], name_, ext_, &attr, &stclst, err);
			ON_PHASEINT;
			if (*err) return 0;
			if (attr&0b00010000) {
				if ((memcmp(name_, "..", 2)==0)||(UI_depth<4)) {
					UI_dir = stclst;
					OFF_PHASEINT;
					LCD_printAll("Wait a   moment.");
					num = FILE_makeList(UI_dir, list, (mode==DISK2MODE)?"DSKDO ":"PO 2MG" , err);
					ON_PHASEINT;
					if (*err) {
						return 0;
					}
					UI_cur = 0;
					prevCur = -1;
				}
				if (memcmp(name_, "..", 2)==0) {
					UI_name-=11;
					UI_depth--;
				} else {
					if (UI_depth<4) {
						memcpy(UI_name, name_, 8);
						memcpy(UI_name+8, ext_, 3);
						UI_name+=11;
						UI_depth++;
					}
				}
			} else {
				break;
			}
		}
		if (memcmp(name_, "UNMOUNT ",8)==0) {
			for (uint8_t i=0; i<64; i++) buffer2.ui.fullpath[i]=0x20;
			buffer2.ui.fullpath[11]=0;
		} else {	
			memcpy(UI_name, name_, 8);
			memcpy(UI_name+8, ext_, 3);
			UI_name[11]=0;
		}
		return 1;
	}
}

// called from DISK2 / SmartPort emulator
uint8_t UI_checkExecute()
{
	static uint8_t rel4 = 1; 
	
	if (rel4 && !PSW4) {
		uint8_t isDsk2 = (mode == DISK2MODE);
		rel4 = 0;
		while (!PSW4) ;
		OFF_TIMER;
		ON_TIMER2;
		
		UI_running = 1;
		if (isDsk2 && !PSW3) {
			if (!WP) {
				uint8_t err = 0;
				
				INI_read(buffer2.ini.ini, &err);
				if (err) { UI_running = 0; return 0; }
				for (uint8_t drv = 0; drv < 2; drv++) {
					if (buffer2.disk2.img[drv].valid && buffer2.disk2.img[drv].written) {
						uint8_t err = 0;

						FILE_openAbs(&buffer2.ini.img[drv], (char *)buffer2.ini.ini+drv*64, &err);
						if (!err && !buffer2.ini.img[drv].protect) {
							DISK2_nic2Dsk(&buffer2.ini.img[drv], &buffer2.disk2.img[drv], &err);
							if (!err) buffer2.disk2.img[drv].written = 0;
						}
					}
				}
			}
		} else {	
			uint8_t err = 0;
			
			//OFF_PHASEINT;
			INI_readFName(buffer2.ui.filelist, isDsk2?UI_drv:(UI_drv+2), (char *)buffer2.ui.fullpath, &err);
			//ON_PHASEINT;
			if (err) { UI_running = 0; return 0; }
			if (UI_chooseFile((void *)buffer2.ui.filelist, &err)) {
				//OFF_PHASEINT;
				INI_substitute(buffer2.ui.filelist, isDsk2?UI_drv:(UI_drv+2), (char *)buffer2.ui.fullpath, &err);
				//ON_PHASEINT;
				//OFF_PHASEINT;
				if (memcmp(buffer2.ui.fullpath, "           ",11) == 0) buffer2.smart.img[UI_drv].valid = 0; 
				else FILE_openAbs(isDsk2?&buffer2.ui.img[UI_drv]:&buffer2.smart.img[UI_drv], (char *)buffer2.ui.fullpath, &err);
				//ON_PHASEINT;
				if (isDsk2) {
					cli();
					if (!err) {
						FILE_substituteFullpathExtwith((char *)buffer2.ui.fullpath, "NIC");
						FILE_openAbs(&buffer2.disk2.img[UI_drv], (char *)buffer2.ui.fullpath, &err);
						if (err) {
							if (!WP) {
								err = 0;
								LCD_printAll("CreatingNIC file");
								// Notice : buffer2.sd.buf[512] is also used!
								FILE_createAbs(&buffer2.disk2.img[UI_drv], (char *)buffer2.ui.fullpath, 286720, &err);
								if (!err) {
									FILE_openAbs(&buffer2.disk2.img[UI_drv], (char *)buffer2.ui.fullpath, &err);
									buffer2.disk2.img[UI_drv].protect = buffer2.ui.img[UI_drv].protect;	
									DISK2_dsk2Nic(&buffer2.disk2.img[UI_drv], &buffer2.ui.img[UI_drv], 0xfe, &err);
									if (err) buffer2.disk2.img[UI_drv].valid = 0;
								}
							} else {
								LCD_printAll("Write   protect.");							
							}
						}
					} else buffer2.disk2.img[UI_drv].valid = 0;
					sei();
				} else {
					SMART_is2mg[UI_drv] = (!err && buffer2.smart.img[UI_drv].valid && (memcmp(buffer2.smart.img[UI_drv].name+8, "2MG", 3)==0));
				}
			}
			//ON_PHASEINT;
			err = 0;
			UI_running = 0;
		}
		OFF_TIMER2;
		return 1;
	} else if (PSW4) {
		rel4 = 1;
	}
	return 0;
}