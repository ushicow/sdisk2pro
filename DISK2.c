/*-------------------------------

	DISK II Emulator (1 of 2)

	(C) 2013 Koichi Nishida
	
-------------------------------*/
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
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "DISK2.h"
#include "COMMON.h"
#include "BUFFER.h"
#include "SD.h"
#ifndef SDISK2P
#include "LCD.h"
#include "UI.h"
#endif

#ifdef SDISK2P
#define EEP_PH_TRACK (uint8_t *)0x0001
#endif

#ifdef SDISK2P
#define DISK2_DRIVENUM 1
#else
#define DISK2_DRIVENUM 2
#endif

// DISK II properties
volatile uint8_t DISK2_ph_track[DISK2_DRIVENUM];// physical track numbers : 0 - 139
uint8_t DISK2_volume[DISK2_DRIVENUM];			// disk volumes
volatile uint8_t DISK2_currentDrive;			// 0 : drive1, 1 : drive2
volatile uint8_t DISK2_sector;					// sector : 0 - 15
volatile uint8_t DISK2_readPulse;				// for reading data
volatile uint8_t DISK2_magState;				// for writing data
uint8_t DISK2_formatting;						// 1 if formatting
volatile uint8_t DISK2_prepare;					// requesting the next sector preparation
uint8_t DISK2_sectors[DISK2_WRITE_BUF_NUM];
uint8_t DISK2_tracks[DISK2_WRITE_BUF_NUM];		// for remembering written sectors & tracks
volatile uint8_t DISK2_WrtBuffNum;				// write buffer number
volatile uint8_t *DISK2_writePtr;				// write buffer pointer
volatile uint8_t DISK2_doBuffering;				// request write buffering
volatile uint8_t DISK2_byteData;				// read byte
volatile uint8_t DISK2_posBit;					// bit position of data read
volatile uint8_t *DISK2_ptrByte;				// pointer for data read

// a table for head stepper motor movement
PROGMEM const uint8_t DISK2_stepper_table[4] = {0x0f,0xed,0x03,0x21};

// encode / decode table for a nib image
PROGMEM const uint8_t DISK2_encTable[] = {
	0x96,0x97,0x9A,0x9B,0x9D,0x9E,0x9F,0xA6,
	0xA7,0xAB,0xAC,0xAD,0xAE,0xAF,0xB2,0xB3,
	0xB4,0xB5,0xB6,0xB7,0xB9,0xBA,0xBB,0xBC,
	0xBD,0xBE,0xBF,0xCB,0xCD,0xCE,0xCF,0xD3,
	0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,
	0xDF,0xE5,0xE6,0xE7,0xE9,0xEA,0xEB,0xEC,
	0xED,0xEE,0xEF,0xF2,0xF3,0xF4,0xF5,0xF6,
	0xF7,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

PROGMEM const int8_t DISK2_decTable[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x02,0x03,0x00,0x04,0x05,0x06,
	0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x08,0x00,0x00,0x00,0x09,0x0a,0x0b,0x0c,0x0d,
	0x00,0x00,0x0e,0x0f,0x10,0x11,0x12,0x13,0x00,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1b,0x00,0x1c,0x1d,0x1e,
	0x00,0x00,0x00,0x1f,0x00,0x00,0x20,0x21,0x00,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
	0x00,0x00,0x00,0x00,0x00,0x29,0x2a,0x2b,0x00,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,
	0x00,0x00,0x33,0x34,0x35,0x36,0x37,0x38,0x00,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f
};

// for dsk2Nic
PROGMEM const uint8_t DISK2_interwieve[] = {0,13,11,9,7,5,3,1,14,12,10,8,6,4,2,15};
PROGMEM const uint8_t DISK2_flipBit[] = { 0,  2,  1,  3  };
PROGMEM const uint8_t DISK2_flipBit1[] = { 0, 2,  1,  3  };
PROGMEM const uint8_t DISK2_flipBit2[] = { 0, 8,  4,  12 };
PROGMEM const uint8_t DISK2_flipBit3[] = { 0, 32, 16, 48 };

// buffer clear
void DISK2_clearBuffer(void)
{
	uint8_t i;
	uint16_t j;
	
	for (i=0; i<DISK2_WRITE_BUF_NUM; i++)
		for (j=0; j<350; j++)
			buffer2.disk2.writebuf[i*350+j] = 0;
	for (i=0; i<DISK2_WRITE_BUF_NUM; i++)
		DISK2_sectors[i] = DISK2_tracks[i] = 0xff;
}

// initialize the DISK2 emulator
void DISK2_init(void)
{	
	// initialize input ports
#ifdef SDISK2P
	// INT0 falling edge interrupt for WREQ	
	EIMSK |= 0b00000001;
	EICRA = (EICRA&0b11111100)|0b00000010;

	// timer0 (4u sec)
	OCR0A = 108;
	TCCR0A = (1<<WGM01);
	TCCR0B = (1<<CS00);
#else
	// PORT D
	PORTD.INTCTRL = PORT_INTLVL_LO_gc;
	
	// WREQ
	PORTD.PIN2CTRL |= PORT_ISC_FALLING_gc; 
	PORTD.INTMASK = PIN2_bm;

	// timer (4u sec)
	TCC4.PER = 125; // adjust 128;
	TCC4.CTRLA = TC4_CLKSEL0_bm;			// prescaler : clk
	TCC4.INTCTRLA = 0; //TC4_OVFINTLVL0_bm;	// meddle level interrupt, but off for the time being
#endif

	// initialize variables
	DISK2_ph_track[0]=70;
	DISK2_volume[0] = 0xfe;
	
#if DISK2_DRIVENUM == 2	
	DISK2_ph_track[1]=70;
	DISK2_volume[1] = 0xfe;
#endif

	DISK2_currentDrive = 0;
	DISK2_sector = 0;
	DISK2_prepare = 1;
	DISK2_doBuffering = 0;
	DISK2_formatting = 0;
	DISK2_readPulse = 0;
	DISK2_magState = 0;
	DISK2_WrtBuffNum = 0;
	DISK2_writePtr = &(buffer2.disk2.writebuf[DISK2_WrtBuffNum*350+0]);
	DISK2_ptrByte = buffer1;
	DISK2_posBit = 1;

	DISK2_clearBuffer();
}

// move head
// called from ISR(PCINT1_vect/PORTA_INT_vect) in SMART.c
void DISK2_moveHead()
{	
	uint8_t current_drive = 0;
	uint8_t stp;
	static uint8_t prevStp = 0;

	if (EJECT) return;
	
#ifdef SDISK2P
	if (EN1) return;
#else	
	if (!EN1) current_drive = 0;
	else if (!EN2) current_drive = 1;
	else return;
#endif

	stp = PHASE;
	if (stp != prevStp) {
		prevStp = stp;
		uint8_t ofs =
			((stp==0b00001000)?2:
			((stp==0b00000100)?4:
			((stp==0b00000010)?6:
			((stp==0b00000001)?0:0xff))));
		if (ofs != 0xff) {
			uint8_t phtrk = DISK2_ph_track[current_drive];
			ofs = ((ofs+phtrk)&7);
			uint8_t bt = pgm_read_byte_near(DISK2_stepper_table + (ofs>>1));
			prevStp = stp;
			if (ofs&1) bt &= 0x0f; else bt >>= 4;
			phtrk += ((bt & 0x08) ? (0xf8 | bt) : bt);
			if (phtrk > 196) phtrk = 0;	
			if (phtrk > 139) phtrk = 139;
			DISK2_ph_track[current_drive] = phtrk;	
		}
	}
}

void DISK2_writeBackSub(uint8_t bn, uint8_t sc, uint8_t track)
{
	uint8_t c, err = 0;
	uint16_t i;
	uint16_t long_sector = (unsigned short)track*16+sc;
	struct FILE *imgp = &buffer2.disk2.img[DISK2_currentDrive];

	if (imgp) {
		FILE_writeBegin(imgp, long_sector, &err);
	
		// 22 ffs
		for (i = 0; i < 22; i++) {
			SPI_writeByte(0xff, &err);
		}

		// sync header
		SPI_writeByte(0x03, &err);
		SPI_writeByte(0xfc, &err);
		SPI_writeByte(0xff, &err);
		SPI_writeByte(0x3f, &err);
		SPI_writeByte(0xcf, &err);
		SPI_writeByte(0xf3, &err);
		SPI_writeByte(0xfc, &err);
		SPI_writeByte(0xff, &err);
		SPI_writeByte(0x3f, &err);
		SPI_writeByte(0xcf, &err);
		SPI_writeByte(0xf3, &err);
		SPI_writeByte(0xfc, &err);

		// address header
		SPI_writeByte(0xd5, &err);
		SPI_writeByte(0xAA, &err);
		SPI_writeByte(0x96, &err);
		SPI_writeByte((DISK2_volume[DISK2_currentDrive]>>1)|0xaa, &err);
		SPI_writeByte(DISK2_volume[DISK2_currentDrive]|0xaa, &err);
		SPI_writeByte((track>>1)|0xaa, &err);
		SPI_writeByte(track|0xaa, &err);
		SPI_writeByte((sc>>1)|0xaa, &err);
		SPI_writeByte(sc|0xaa, &err);
		c = (DISK2_volume[DISK2_currentDrive]^track^sc);
		SPI_writeByte((c>>1)|0xaa, &err);
		SPI_writeByte(c|0xaa, &err);
		SPI_writeByte(0xde, &err);
		SPI_writeByte(0xAA, &err);
		SPI_writeByte(0xeb, &err);

		// sync header
		SPI_writeByte(0xff, &err);	
		SPI_writeByte(0xff, &err);
		SPI_writeByte(0xff, &err);
		SPI_writeByte(0xff, &err);
		SPI_writeByte(0xff, &err);

		// data
		for (i = 0; i < 349; i++) {
			c = buffer2.disk2.writebuf[bn*350+i];
			SPI_writeByte(c, &err);
		}
		for (i = 0; i < 14; i++) {
			SPI_writeByte(0xff, &err);
		}
		for (i = 0; i < 96; i++) {
			SPI_writeByte(0, &err);
		}
	
		FILE_writeEnd(&err);
	}
}

// write back into the SD card
void DISK2_writeBack(void)
{
	uint8_t i, j;

	for (j=0; j<DISK2_WRITE_BUF_NUM; j++) {
		if (DISK2_sectors[j]!=0xff) {
			for (i=0; i<DISK2_WRITE_BUF_NUM; i++) {
				if (DISK2_sectors[i] != 0xff)
					DISK2_writeBackSub(i, DISK2_sectors[i], DISK2_tracks[i]);
				DISK2_sectors[i] = 0xff;
				DISK2_tracks[i] = 0xff;
				buffer2.disk2.writebuf[i*350+2]=0;
			}
			DISK2_WrtBuffNum = 0;
			DISK2_writePtr = &(buffer2.disk2.writebuf[DISK2_WrtBuffNum*350+0]);
			break;
		}
	}
}

// set write pointer and write back if need
void DISK2_writeBuffering(void)
{	
	static uint8_t sec;

	if (buffer2.disk2.writebuf[DISK2_WrtBuffNum*350+2]==0xAD) {
		if (!DISK2_formatting) {
			DISK2_sectors[DISK2_WrtBuffNum]=DISK2_sector;
			DISK2_tracks[DISK2_WrtBuffNum]=(DISK2_ph_track[DISK2_currentDrive]>>2);		
			DISK2_sector=((((DISK2_sector==0xf)||(DISK2_sector==0xd))?(DISK2_sector+2):(DISK2_sector+1))&0xf);
			if (DISK2_WrtBuffNum == (DISK2_WRITE_BUF_NUM-1)) {
				DISK2_writeBack();				
			} else {
				DISK2_WrtBuffNum++;
				DISK2_writePtr = &(buffer2.disk2.writebuf[DISK2_WrtBuffNum*350+0]);
			}
		} else {
			DISK2_sector = sec;
			DISK2_formatting = 0;
		}
	} if (buffer2.disk2.writebuf[DISK2_WrtBuffNum*350+2]==0x96) {
		sec = (((buffer2.disk2.writebuf[DISK2_WrtBuffNum*350+7]&0x55)<<1) | (buffer2.disk2.writebuf[DISK2_WrtBuffNum*350+8]&0x55));
		DISK2_formatting = 1;
	}
}

// run the DISK2 emulator
int DISK2_run(uint8_t (*pf)(uint8_t))
{	
#ifndef SDISK2P
	uint8_t drvChange = 1;
	LCD_locate(0,0);
	LCD_print("DR  TR  ",8);
#endif	
	while (1) {			
		struct FILE *imgp = &buffer2.disk2.img[DISK2_currentDrive];
		// out SENSE (write protect) signal
		
#ifdef SDISK2P
		if (bit_is_set(PIND,6) || (imgp->valid && imgp->protect) || (PINC&2)) PORTD |= 0b00010000;
		else PORTD &= ~0b00010000;
#else
		if (WP || (imgp->valid && imgp->protect) || (PHASE&2)) PORTD.OUT |= PIN7_bm;
		else PORTD.OUT &= ~PIN7_bm;
#endif

#ifdef SDISK2P
		if (EN1) PORTD &= ~(1<<5);
		else PORTD |= (1<<5);
#endif
		if (pf(0)) {
			DISK2_clearBuffer();
			DISK2_prepare = 1;
			
#ifndef SDISK2P
			drvChange = 1;
			LCD_locate(0,0);
			LCD_print("DR  TR  ",8);
#endif
		}
#ifndef SDISK2P		
		if (UI_checkExecute()) {
			DISK2_writeBack();
			DISK2_clearBuffer();
			DISK2_prepare = 1;
			drvChange = 1;
			LCD_locate(0,0);
			LCD_print("DR  TR  ",8);
		}
#endif
		if (DISK2_prepare) {
			uint8_t err = 0;

			OFF_TIMER;

#if DISK2_DRIVENUM == 2
			if (!EN1) {if (DISK2_currentDrive!=0) { DISK2_currentDrive = 0; drvChange = 1; }}
			else if (!EN2) {if (DISK2_currentDrive!=1) { DISK2_currentDrive = 1; drvChange = 1; }}
#endif
			DISK2_sector = ((DISK2_sector+1)&0xf);		
			uint8_t trk = (DISK2_ph_track[DISK2_currentDrive]>>2);
	
			if (!(
				((DISK2_sectors[0]^DISK2_sector)|(DISK2_tracks[0]^trk))
#if DISK2_WRITE_BUF_NUM > 1		
				&((DISK2_sectors[1]^DISK2_sector)|(DISK2_tracks[1]^trk))
#endif
#if DISK2_WRITE_BUF_NUM > 2
				&((DISK2_sectors[2]^DISK2_sector)|(DISK2_tracks[2]^trk))
#endif
#if DISK2_WRITE_BUF_NUM > 3
				&((DISK2_sectors[3]^DISK2_sector)|(DISK2_tracks[3]^trk))
#endif
#if DISK2_WRITE_BUF_NUM > 4
				&((DISK2_sectors[4]^DISK2_sector)|(DISK2_tracks[4]^trk))
#endif
			)) DISK2_writeBack();			

			uint16_t long_sector = (uint16_t)trk*16+DISK2_sector;

#ifndef SDISK2P
			
			LCD_locate(6,0);
			LCD_printdec(trk,2);		
#endif
			struct FILE *imgp = &buffer2.disk2.img[DISK2_currentDrive];
			if (imgp->valid) {
#ifndef SDISK2P	
				if (drvChange) {
					LCD_locate(2,0);
					LCD_printdec(DISK2_currentDrive+1,1);
					LCD_locate(0,1);
					LCD_print(imgp->name, 8);
					drvChange = 0;
				}
#endif
				FILE_readBegin(imgp, long_sector, &err);
				if (!err) {
					for (uint16_t i = 0; i != 412; i++) buffer1[i] = SPI_readByte(&err);
					for (uint8_t i = 0; i != 102; i++) SPI_readByte(&err);
					FILE_readEnd(&err);
					DISK2_prepare = 0;
					DISK2_ptrByte = buffer1;
					DISK2_posBit = 1;

					ON_TIMER;
				}
			}
		}
		if (DISK2_doBuffering) {
			DISK2_doBuffering = 0;
			OFF_TIMER;
			DISK2_writeBuffering();
			ON_TIMER;
		}
	}
}

#ifdef SDISK2P
// should be called before SDISK2P eject reset
void DISK2_eject()
{
	// record track number on eeprom
	eeprom_busy_wait();
	eeprom_write_byte (EEP_PH_TRACK, DISK2_ph_track[0]);
	eeprom_busy_wait();
}
#endif

// convert a DSK file to a NIC file
void DISK2_dsk2Nic(struct FILE *nicFile, struct FILE *dskFile, uint8_t volume, uint8_t *err)
{
	uint16_t i;
	uint8_t *dst = buffer2.disk2.writebuf+512;

	for (i=0; i<0x16; i++) dst[i]=0xff;

	// sync header
	dst[0x16]=0x03;
	dst[0x17]=0xfc;
	dst[0x18]=0xff;
	dst[0x19]=0x3f;
	dst[0x1a]=0xcf;
	dst[0x1b]=0xf3;
	dst[0x1c]=0xfc;
	dst[0x1d]=0xff;
	dst[0x1e]=0x3f;
	dst[0x1f]=0xcf;
	dst[0x20]=0xf3;
	dst[0x21]=0xfc;
	
	// address header
	dst[0x22]=0xd5;
	dst[0x23]=0xaa;
	dst[0x24]=0x96;
	dst[0x2d]=0xde;
	dst[0x2e]=0xaa;
	dst[0x2f]=0xeb;
	
	// sync header
	for (i=0x30; i<0x35; i++) dst[i]=0xff;
	
	// data
	dst[0x35]=0xd5;
	dst[0x36]=0xaa;
	dst[0x37]=0xad;
	dst[0x18f]=	0xde;
	dst[0x190]=0xaa;
	dst[0x191]=0xeb;
	for (i=0x192; i<0x1a0; i++) dst[i]=0xff;
	for (i=0x1a0; i<0x200; i++) dst[i]=0x00;
	
#ifndef SDISK2P
	LCD_locate(0,0);
	LCD_print("Convert ", 8);
	LCD_locate(0,1);
	LCD_print("TR  SC", 6);
#endif
	for (uint8_t trk = 0; trk < 35; trk++) {
		for (uint8_t sector = 0; sector < 16; sector++) {
#ifndef SDISK2P
			LCD_locate(2,1);
			LCD_printdec(trk,2);
			LCD_locate(6,1);
			LCD_printdec(sector,2);
#endif
			uint8_t *src;
			uint16_t ph_sector = (uint16_t)pgm_read_byte_near(DISK2_interwieve+sector);

			if ((sector&1)==0) {
				uint32_t long_sector = (uint32_t)trk*8+(sector/2);
				
				FILE_readBegin(dskFile, long_sector, err);
				if (*err) return;
				for (i=0; i<512; i++) {
					buffer2.disk2.writebuf[i]=SPI_readByte(err);
					if (*err) return;
				}
				FILE_readEnd(err);
				if (*err) return;

				src = buffer2.disk2.writebuf;
			} else {
				src = buffer2.disk2.writebuf+256;
			}
			{
				uint8_t c, x, ox = 0;

				dst[0x25]=((volume>>1)|0xaa);
				dst[0x26]=(volume|0xaa);
				dst[0x27]=((trk>>1)|0xaa);
				dst[0x28]=(trk|0xaa);
				dst[0x29]=((ph_sector>>1)|0xaa);
				dst[0x2a]=(ph_sector|0xaa);
				c = (volume^trk^ph_sector);
				dst[0x2b]=((c>>1)|0xaa);
				dst[0x2c]=(c|0xaa);
				for (i = 0; i < 86; i++) {
					x = (pgm_read_byte_near(DISK2_flipBit1+(src[i]&3)) |
					pgm_read_byte_near(DISK2_flipBit2+(src[i+86]&3)) |
					((i<=83)?pgm_read_byte_near(DISK2_flipBit3+(src[i+172]&3)):0));
					dst[i+0x38] = pgm_read_byte_near(DISK2_encTable+(x^ox));
					ox = x;
				}
				for (i = 0; i < 256; i++) {
					x = (src[i] >> 2);
					dst[i+0x8e] = pgm_read_byte_near(DISK2_encTable+(x^ox));
					ox = x;
				}
				dst[0x18e]=pgm_read_byte_near(DISK2_encTable+ox);
			}
			{
				uint32_t long_sector = (uint32_t)trk*16+ph_sector;

				FILE_writeBegin(nicFile, long_sector, err);
				if (*err) return;
				for (i=0; i<512; i++) {
					SPI_writeByte(dst[i], err);
					if (*err) return;
				}
				FILE_writeEnd(err);
				if (*err) return;
			}
		}
	}
}

// convert a NIC file to a DSK file
void DISK2_nic2Dsk(struct FILE *dskFile, struct FILE *nicFile, uint8_t *err)
{
	uint8_t track, sector;
	uint8_t *src, *dst;
	
#ifndef SDISK2P
	LCD_locate(0,0);
	LCD_print("WR back ", 8);
	LCD_locate(0,1);
	LCD_print("TR  SC", 6);
#endif
	for (track = 0; track < 35; track++) {
		for (sector = 0; sector < 16; sector++) {
#ifndef SDISK2P
			LCD_locate(2,1);
			LCD_printdec(track,2);
			LCD_locate(6,1);
			LCD_printdec(sector,2);
#endif
			uint16_t i, j;
			uint8_t x, ox = 0;
			uint8_t ph_sector = pgm_read_byte_near(DISK2_interwieve+sector);

			FILE_readBegin(nicFile, (uint16_t)track*16+ph_sector, err);
			if (*err) return;
			for (i=0; i<512; i++) {
				buffer2.disk2.writebuf[i] = SPI_readByte(err);
				if (*err) return;
			}
			FILE_readEnd(err);
			if (*err) return;

			src = buffer2.disk2.writebuf;
			dst = ((sector&1)?(buffer2.disk2.writebuf+512+256):(buffer2.disk2.writebuf+512));

			for (j=0, i=0x38; i<0x8e; i++, j++) {
				x = ((ox^pgm_read_byte_near(DISK2_decTable+src[i]))&0x3f);
				dst[j+172] = pgm_read_byte_near(DISK2_flipBit+((x>>4)&3));
				dst[j+86] = pgm_read_byte_near(DISK2_flipBit+((x>>2)&3));
				dst[j] = pgm_read_byte_near(DISK2_flipBit+((x)&3));
				ox = x;
			}
			for (j=0, i=0x8e; i<0x18e; i++, j++) {
				x = ((ox^pgm_read_byte_near(DISK2_decTable+src[i]))&0x3f);
				dst[j]|=(x<<2);
				ox = x;
			}
			if (sector&1) {
				FILE_writeBegin(dskFile, (uint32_t)track*8+sector/2, err);
				if (*err) return;
				for (i=0; i<512; i++) {
					SPI_writeByte(buffer2.disk2.writebuf[512+i], err);
					if (*err) return;
				}
				FILE_writeEnd(err);
				if (*err) return;
			}
		}
	}
}