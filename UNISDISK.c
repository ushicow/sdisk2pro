/*
 * main
 *
 * Created: 2013/09/21 20:00:03
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
#include <string.h>
#include <avr/eeprom.h>
#include "COMMON.h"
#include "SYSTEM.h"
#include "LCD.h"
#include "SD.h"
#include "DISK2.h"
#include "SMART.h"
#include "UI.h"
#include "INI.h"
#include "BUFFER.h"

#define EEP_MODE (uint8_t *)0x0001

uint8_t exchangeSD(uint8_t start)
{
	uint8_t err = 0;
	uint8_t isDsk2 = (mode == DISK2MODE);
	uint8_t drvNum = (isDsk2?2:4);

	if (EJECT || start) {
#ifdef SDISK2P
		buffer2.smart.img[0].valid = 0;
#else
		if (isDsk2) for (uint8_t drv = 0; drv < 2; drv++) buffer2.disk2.img[drv].valid = 0;
		else for (uint8_t drv = 0; drv < 4; drv++) buffer2.smart.img[drv].valid = 0;
#endif
		SD_p.inited = 0;
		UI_clearProperties();
	}
	if (!SD_detect(start)) return 0;
	cli();
	INI_openCreate(&err);
	sei();
	if (err) return 0;
	
	SMART_partition_num = 0;
	for (uint8_t drv = 0; drv < drvNum; drv++) {
		uint8_t n = (isDsk2?drv:(drv+2));
		err = 0;
		cli();
		INI_read(buffer2.ini.ini, &err);
		if (err) { sei(); return 0; }
		FILE_openAbs(isDsk2?&buffer2.ini.img[drv]:&buffer2.smart.img[drv], (char *)buffer2.ini.ini+n*64, &err);
		sei();
		if (err) { if (isDsk2) buffer2.disk2.img[drv].valid=0; else buffer2.smart.img[drv].valid = 0; continue; }
		else {SMART_partition_num++; LCD_locate(0,1); LCD_print(buffer2.smart.img[drv].name, 8);}
		if (isDsk2) {
			FILE_substituteFullpathExtwith((char *)buffer2.ini.ini+n*64, "NIC");
			cli();	
			FILE_openAbs(&buffer2.disk2.img[drv], (char *)buffer2.ini.ini+n*64, &err);
			sei();
			if(!err) { continue; }
			err = 0;
			if (!WP) {
				cli();
				LCD_printAll("CreatingNIC file");
				// Notice : buffer2.sd.buf[512] is also used!
				FILE_createAbs(&buffer2.disk2.img[drv], (char *)buffer2.ini.ini+n*64, 286720, &err);
				if (err) { buffer2.disk2.img[drv].valid=0; sei(); continue; }	
				FILE_openAbs(&buffer2.disk2.img[drv], (char *)buffer2.ini.ini+n*64, &err);
				if (err) { buffer2.disk2.img[drv].valid=0; sei(); continue; }
				buffer2.disk2.img[drv].protect = buffer2.ini.img[drv].protect;
				DISK2_dsk2Nic(&buffer2.disk2.img[drv], &buffer2.ini.img[drv], 0xfe, &err);
				if (err) { buffer2.disk2.img[drv].valid=0; sei(); continue; }
				sei();
			} else {
				LCD_printAll("Write   protect.");
			}
			sei();
		} else 	SMART_is2mg[drv] = (!err && buffer2.smart.img[drv].valid && (memcmp(buffer2.smart.img[drv].name+8, "2MG", 3)==0));
	}
	return 1;
}

int main(void)
{
	uint8_t firmwritten = GPIO0;

	NVM_LOCKBITS = ((NVM_LOCKBITS&0b11000011)|0b00101000);

	system_clocks_init();
	COMMON_init();
	LCD_init();
	UI_init();

	uint8_t err = 0;
	SD_init(&err);

	SMART_desktop = (!PSW3 && !PSW2);

	if (!PSW3 && PSW2) {
		char str[16];
		while(1) {
			str[0] = DIK35?'0':'1';
			str[1] = EN2?'0':'1';
			str[2] = (PHASE&1)?'0':'1';
			str[3] = (PHASE&2)?'0':'1';
			str[4] = (PHASE&4)?'0':'1';
			str[5] = (PHASE&8)?'0':'1';
			str[6] = WREQ?'0':'1';
			str[7] = HDSEL?'0':'1';
			str[8] = EN1?'0':'1';
			str[9] = WDAT?'0':'1';
			str[10] = EJECT?'0':'1';
			str[11] = WP?'0':'1';
			str[12] = PSW1?'0':'1';
			str[13] = PSW2?'0':'1';
			str[14] = PSW3?'0':'1';
			str[15] = PSW4?'0':'1';

			if (!PSW1) PORTD.OUTSET = PIN0_bm;
			else PORTD.OUTCLR = PIN0_bm;
			if (!PSW2) PORTD.OUTSET = PIN7_bm;
			else PORTD.OUTCLR = PIN7_bm;
			
			LCD_locate(0,0);
			LCD_print(str,8);
			LCD_locate(0,1);
			LCD_print(str+8,8);
		}
	}

	if (firmwritten) {
		LCD_locate(0,0);
		LCD_printAll("FirmwareWritten!");
		while (1);
	}

	LCD_locate(0,0);
	LCD_print("UNISDISK",8);

	// determine mode
	eeprom_busy_wait();
	mode = eeprom_read_byte(EEP_MODE);
	eeprom_busy_wait();
	if (!PSW1) {
		mode = DISK2MODE;
		eeprom_write_byte(EEP_MODE, mode);
		
	} else if (!PSW2) {
		mode = SMARTMODE;
		eeprom_write_byte(EEP_MODE, mode);
	}
	LCD_locate(0,1);
	LCD_print((mode==DISK2MODE)?"DSK2MODE":"SMRTMODE",8);

	exchangeSD(1);
	sei();
	switch (mode) {
		case DISK2MODE:
			DISK2_init();
			DISK2_run(exchangeSD);
		case SMARTMODE:
			SMART_init();
			SMART_run(exchangeSD);
	}
}