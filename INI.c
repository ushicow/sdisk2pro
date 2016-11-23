/*
 * INI.c
 *
 * Created: 2013/11/06 1:16:33
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
#include "INI.h"
#include "SD.h"
#include "COMMON.h"
#include "LCD.h"

// UNISDISK.INI file
struct FILE iniFile;

// open or create UNISDISK.INI file
void INI_openCreate(uint8_t *err)
{
	FILE_open(&iniFile, 0, "UNISDISK", "INI", err);
	if (*err) {
		*err = 0;
		if (WP) {
			LCD_printAll("Write   Protect.");
		} else {
			FILE_create(0, "UNISDISKINI", 512, err);
			if (*err) return;
			FILE_open(&iniFile, 0, "UNISDISK", "INI", err);
			if (*err) return;
			FILE_writeBegin(&iniFile, 0, err);	
			for (uint8_t j = 0; j < 6; j++) {
				SPI_writeByte(0, err);
				if (*err) return;
				for (uint16_t i = 1; i < 64; i++) {
					SPI_writeByte(' ', err);
					if (*err) return;
				}
			}
			for (uint8_t j = 0; j < 128; j++) {
				SPI_writeByte(' ', err);
				if (*err) return;
			}
			FILE_writeEnd(err);
		}
	}
}

// read INI file to a buffer
void INI_read(uint8_t *buff, uint8_t *err)
{
	FILE_readBegin(&iniFile, 0, err);
	if (*err) return;
	for (uint16_t i=0; i<512; i++) {
		buff[i] = SPI_readByte(err);
		if (*err) return;
	}
	FILE_readEnd(err);
}

// write INI file
void INI_write(uint8_t *buff, uint8_t *err)
{
	FILE_writeBegin(&iniFile, 0, err);
	if (*err) return;
	for (uint16_t i=0; i<512; i++) {
		SPI_writeByte(buff[i], err);
		if (*err) return;
	}
	FILE_writeEnd(err);
}

// read a file name in UNISDISK.INI
void INI_readFName(uint8_t *buff, uint8_t drv, char *name, uint8_t *err)
{
	INI_read(buff, err);
	if (*err) return;
	strcpy(name, (char *)buff+(uint16_t)drv*64);
}

// substitute a file name in UNISDISK.INI
void INI_substitute(uint8_t *buff, uint8_t drv, char *name, uint8_t *err)
{
	INI_read(buff, err);
	if (*err) return;
	for (uint8_t i=0; i<64; i++) {
		buff[(uint16_t)drv*64+i]=' ';
	}
	strcpy((char *)buff+(uint16_t)drv*64, name);
	INI_write(buff, err);
}
