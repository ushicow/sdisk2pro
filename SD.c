/*
 * SD.c
 * SD card and file manipulation
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

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "COMMON.h"
#include "SD.h"
#include "BUFFER.h"
#ifndef SDISK2P
#include "LCD.h"
#endif

// ========== SPI ==========

#ifndef SDISK2P
// initialize SPI
//   clk2x : SPI_CLK2X_bm | 0
//   dv : SPI_PRESCALER_DIV4_gc | SPI_PRESCALER_DIV128_gc
static void SPI_init(uint8_t clk2x, uint8_t dv, uint8_t *err)
{
	SPIC_CTRL = (clk2x | SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | dv);
	PORTC_DIRSET = (SPI_MOSI_bm | SPI_SCK_bm | SPI_SS_bm);
	PORTC_PIN6CTRL = PORT_OPC_PULLUP_gc; 
}
#endif

// write a byte data to spi
void SPI_writeByte(uint8_t c, uint8_t *err)
{
	uint16_t i=0;
#ifdef SDISK2P
	SPDR = c;
	// wait for transmission complete
	while (!(SPSR & (1<<SPIF))) if ((i++==1000) || EJECT) { *err = 1; return; }
#else	
	SPIC.DATA = c;
	// wait for transmission complete
	while (!(SPIC_STATUS & SPI_IF_bm)) if ((i++==1000) || EJECT) { *err = 1; return; }
#endif
}

// read data from spi
uint8_t SPI_readByte(uint8_t *err)
{
	uint16_t i=0;
#ifdef SDISK2P
	SPDR = 0xff;
	// wait for transmission complete
	while (!(SPSR & (1<<SPIF))) if ((i++==1000) || EJECT) { *err = 1; return 0; }
	return SPDR;
#else
	SPIC.DATA = 0xff;
	// wait for reception complete
	while (!(SPIC.STATUS & SPI_IF_bm)) if ((i++==1000) || EJECT) { *err = 1; return 0; }
	return SPIC.DATA;
#endif
}

// ========== SD card ==========
struct SD SD_p;

// wait until data is written to the SD card
void SD_waitFinish(uint8_t *err)
{
	while (SPI_readByte(err) != 0xff) {
		if (*err || EJECT) { *err = 1; return; }
	}
}

// get a command response from the SD card
unsigned char SD_getResp(uint8_t *err)
{
	uint16_t i = 0;
	uint8_t ch;
	do {
		ch = SPI_readByte(err);
		if ((i++==1000) || *err || EJECT) { *err = 1; return 0; }
	} while ((ch&0x80) != 0);

	return ch;
}

// issue a SD card command without getting response
void SD_cmd_(uint8_t cmd, uint32_t adr, uint8_t crc, uint8_t *err)
{
	SPI_writeByte(0xff, err);
	if (*err) return;
	SPI_writeByte(0x40+cmd, err);
	if (*err) return;
	SPI_writeByte(adr>>24, err);
	if (*err) return;
	SPI_writeByte((adr>>16)&0xff, err);
	if (*err) return;
	SPI_writeByte((adr>>8)&0xff, err);
	if (*err) return;
	SPI_writeByte(adr&0xff, err);
	if (*err) return;
	SPI_writeByte(crc, err);
	if (*err) return;
	SPI_writeByte(0xff, err);
}

// issue a SD card command and wait a normal response
void SD_cmd(uint8_t cmd, uint32_t adr, uint8_t *err)
{
	uint8_t res;
	do {
		if (EJECT) { *err = 1; return; }
		SPI_writeByte(0xff, err);
		if (*err) return;
		SPI_writeByte(0x40+cmd, err);
		if (*err) return;
		SPI_writeByte(adr>>24, err);
		if (*err) return;
		SPI_writeByte((adr>>16)&0xff, err);
		if (*err) return;
		SPI_writeByte((adr>>8)&0xff, err);
		if (*err) return;
		SPI_writeByte(adr&0xff, err);
		if (*err) return;
		SPI_writeByte(0x95, err);
		if (*err) return;
		SPI_writeByte(0xff, err);
		if (*err) return;
		res = SD_getResp(err);
		if (*err) return;
	} while ((res!=0) && (res!=0xff));
}

// issue command 17 and get ready for reading
void SD_cmd17(uint32_t adr, uint8_t *err)
{
	uint8_t ch;

	SD_cmd(17, adr, err);
	if (*err) return;
	do {
		if (EJECT) { *err = 1; return; }
		ch = SPI_readByte(err);
		if (*err) return;
	} while (ch != 0xfe);
}

void SD_readBlockBegin(uint32_t block_adr, uint8_t *err)
{
	ENABLE_CS;
	SD_cmd17(SD_p.blkAdrAccs?block_adr:(block_adr*512), err);
}

void SD_readBlockEnd(uint8_t *err)
{
	SPI_readByte(err);				// discard CRC
	if (*err) return;
	SPI_readByte(err);
	DISABLE_CS;
}

void SD_readBlock(uint32_t block_adr, uint8_t *err)
{
	uint16_t i;
	
	SD_readBlockBegin(block_adr, err);
	if (*err) return;
	for (i=0; i<512; i++) {
		if (EJECT) { *err = 1; return; }
		SD_p.buff[i] = SPI_readByte(err);
		if (*err) return;
	}
	SD_readBlockEnd(err);
}

void SD_writeBlockBegin(uint32_t block_adr, uint8_t *err)
{
	//DISABLE_CS;
	ENABLE_CS;

	SD_cmd(24, SD_p.blkAdrAccs?block_adr:(block_adr*512), err);
	if (*err) return;

	DISABLE_CS;
	ENABLE_CS;

	SPI_writeByte(0xff, err);
	if (*err) return;
	SPI_writeByte(0xfe, err);
}
	
void SD_writeBlockEnd(uint8_t *err)
{
	SPI_writeByte(0xff, err);
	if (*err) return;
	SPI_writeByte(0xff, err);
	if (*err) return;

	DISABLE_CS;
	ENABLE_CS;

	SD_waitFinish(err);
	
	DISABLE_CS;
	//ENABLE_CS;
}

// write bytes one by one to the SD card
// Notice : buffer2.sd.buf[512] is also used!
void SD_writeBytes(uint32_t adr, uint16_t ofs, uint8_t *ptr, uint16_t length, uint8_t *err)
{
	uint16_t i;
	
	SD_readBlockBegin(adr, err);
	if (*err) return;
	for (i = 0; i < 512; i++) {
		SD_p.buff2[i] = SPI_readByte(err);
		if (*err) return;
	}
	SD_readBlockEnd(err);
	
	for (i=0; i<length; i++) SD_p.buff2[ofs++] = *(ptr++);

	SD_writeBlockBegin(adr, err);
	if (*err) return;
	for (i = 0; i < 512; i++) {
		SPI_writeByte(SD_p.buff2[i], err);
		if (*err) return;
	}
	SD_writeBlockEnd(err);
}

// write a word one by one to the SD card
// Notice : buffer2.sd.buf[512] is also used!
void SD_writeWord(uint32_t adr, uint16_t ofs, uint16_t d, uint8_t *err)
{
	SD_writeBytes(adr, ofs, (uint8_t *)&d, 2, err);
}

// write a double word one by one to the SD card
// Notice : buffer2.sd.buf[512] is also used!
void SD_writeDWord(uint32_t adr, uint16_t ofs, uint32_t d, uint8_t *err)
{
	SD_writeBytes(adr, ofs, (uint8_t *)&d, 4, err);
}

// change buffer
void SD_changeBuff(uint8_t *b)
{
	SD_p.buff = b;
}

// initialization SD card
void SD_init(uint8_t *err)
{
	SD_p.buff = buffer1;
	SD_p.buff2 = buffer2.sd.buf;
	SD_p.inited = 0;
	
	uint8_t ch, ver;
	uint16_t resp7;
	uint16_t i;

#ifdef DEBUG
	LCD_marker("SD_init POINT1  ");
#endif

#ifdef SDISK2P
	// EJECT
	DDRD &= ~(1<<3);
	PORTD |= (1<<3);
	// WP
	DDRD &= ~(1<<6);
	PORTD |= (1<<6);
	//SPI
	PORTB &= ~(SPI_MOSI_bm | SPI_SCK_bm | SPI_SS_bm);
	PORTB |= SPI_MISO_bm;
	DDRB |= (SPI_MOSI_bm | SPI_SCK_bm | SPI_SS_bm);
	DDRB &= ~SPI_MISO_bm;
#else
	// EJECT
	PORTA.DIRCLR = PIN5_bm;
	PORTA.PIN5CTRL = PORT_OPC_PULLUP_gc;
	// WP
	PORTC.DIRCLR = PIN3_bm;
	PORTC.PIN3CTRL = PORT_OPC_PULLUP_gc;
#endif

	if (EJECT) { *err=1; return; }

#ifdef SDISK2P
	SPCR = ((1<<SPE)|(1<<MSTR)|(1<<SPR1)|(1<<SPR0));
	SPSR = 0;
#else
	SPI_init(0, SPI_PRESCALER_DIV128_gc, err);			// SPI = 250 KHz
#endif

	DISABLE_CS;
	for (i=0; i < 10; i++) {
		SPI_writeByte(0xFF, err);						// Send 10 * 8 = 80 clock
		if (*err) return;
	}

#ifdef DEBUG
	LCD_marker("SD_init POINT2  ");
#endif

	ENABLE_CS;
	for (i=0; i < 2; i++) {
		SPI_writeByte(0xFF, err);						// Send 2 * 8 = 16 clock
		if (*err) return;
	}
		
	SD_cmd_(0, 0, 0x95, err);							// command 0
	if (*err) return;

#ifdef DEBUG
	LCD_marker("SD_init POINT3  ");
#endif

	i=0;
	do {
		if ((i++==10000) || EJECT) { *err = 1; return; }
		ch = SPI_readByte(err);
		if (*err) return;
	} while (ch != 0x01);

	SD_cmd_(8, 0x1AA, 0x87, err);						// command 8
	if (*err) return;
	ch = SD_getResp(err);

	if (*err) return;
	SPI_readByte(err);
	if (*err) return;
	SPI_readByte(err);
	if (*err) return;
	resp7 = SPI_readByte(err);
	if (*err) return;
	resp7 = ((resp7<<8)|SPI_readByte(err))&0x0fff;
	if (*err) return;

	if (ch==1) {
		if (resp7!=0x1AA) { *err = 1; return; }
		ver = 2;
	} else {
		ver = 1;
	}

#ifdef DEBUG
	LCD_marker("SD_init POINT4  ");
#endif

	DISABLE_CS;

	i = 0;
	while (1) {
		if (i++==60000 || EJECT) { *err = 1; return; }
		ENABLE_CS;
		SD_cmd_(55, 0, 0, err);							// command 55
		if (*err) return;
		ch = SD_getResp(err);
		if (*err) return;
		if (ch == 0xff) {*err = 1; return; }
		if (ch & 0xfe) {*err = 1; return; }
		if (ch == 0x00) break;
		DISABLE_CS;
		ENABLE_CS;
		SD_cmd_(41, (ver==2)?0x40000000:0, 0, err);		// command 41
		if (*err) return;
		if (!(ch=SD_getResp(err))) break;
		if (*err) return;
		if (ch == 0xff) {*err = 1; return; }
		DISABLE_CS;
	}

#ifdef DEBUG
	LCD_marker("SD_init POINT5  ");
#endif

	SD_p.blkAdrAccs = 0;
	if (ver==2) {
		SD_cmd_(58, 0, 0, err);							// command 58
		if (*err) return;

		ch = SD_getResp(err);
		if (*err) return;
				
		if (SPI_readByte(err)&0x40) SD_p.blkAdrAccs = 1;
		if (*err) return;
		SPI_readByte(err);
		if (*err) return;
		SPI_readByte(err);
		if (*err) return;
		SPI_readByte(err);
		if (*err) return;
	}

#ifdef DEBUG
	LCD_marker("SD_init POINT6  ");
#endif	
	
#ifdef SDISK2P	
	//SPI enable, master mode, f/2
	SPCR = ((1<<SPE)|(1<<MSTR));
	SPSR = (1<<SPI2X);
#else	
	SPI_init(SPI_CLK2X_bm, SPI_PRESCALER_DIV4_gc, err);	// SPI = 16MHz
	if (*err) return;
#endif
	SD_cmd(16, 512, err);
	if (*err) return;

#ifdef DEBUG
	LCD_marker("SD_init POINT7  ");
#endif

	// read MBR
	SD_readBlock(0, err);
	if (*err) return;

#ifdef DEBUG
	LCD_marker("SD_init POINT8  ");
#endif

	SD_p.bpbAddr = 0;
	if (memcmp(&SD_p.buff[54], "FAT16", 5)&&memcmp(&SD_p.buff[82],"FAT32", 5)) {
		// read BPB
		memcpy(&(SD_p.bpbAddr), &SD_p.buff[0x1c6], 4);
		SD_readBlock(SD_p.bpbAddr, err);	

		if (*err) return;
	}

#ifdef DEBUG
	LCD_marker("SD_init POINT9  ");
#endif

	if (!((SD_p.buff[510]==0x55)&&(SD_p.buff[511]==0xaa))) { *err = 1; return; }

	{
		uint16_t BPB_RsvdSecCnt = readmem_word(&SD_p.buff[14]);
		SD_p.numFats = SD_p.buff[16];
		uint16_t BPB_RootEntCnt = readmem_word(&SD_p.buff[17]);
		uint16_t BPB_TotSec16 = readmem_word(&SD_p.buff[19]);
		uint16_t BPB_FATSz16 = readmem_word(&SD_p.buff[22]);
		uint32_t BPB_TotSec32 = readmem_long(&SD_p.buff[32]);
		uint32_t BPB_FATSz32 = readmem_long(&SD_p.buff[36]);
		uint32_t BPB_TotSec;

		SD_p.bytesPerSector = readmem_word(&SD_p.buff[11]);
		SD_p.sectorsPerCluster = SD_p.buff[13];

		if (SD_p.bytesPerSector != 512) { *err = 1; return; }

		BPB_TotSec = BPB_TotSec16?BPB_TotSec16:BPB_TotSec32;
		SD_p.fatSz = BPB_FATSz16?BPB_FATSz16:BPB_FATSz32;

		SD_p.fatAddr = BPB_RsvdSecCnt+SD_p.bpbAddr;
		SD_p.fatSectors = SD_p.fatSz * SD_p.numFats;
	
		SD_p.rootAddr = SD_p.fatAddr + SD_p.fatSectors;
		SD_p.rootSectors = (32 * BPB_RootEntCnt + SD_p.bytesPerSector - 1) / SD_p.bytesPerSector;

		SD_p.userAddr = SD_p.rootAddr + SD_p.rootSectors;

		{	
			uint32_t DataStartSector = SD_p.rootAddr + SD_p.rootSectors;
			uint32_t DataSectors = BPB_TotSec - DataStartSector;
			uint32_t CountofClusters = DataSectors / SD_p.sectorsPerCluster;
		
			if ((CountofClusters >= 4086) && (CountofClusters <= 65525)) SD_p.fat32 = 0;
			else if (CountofClusters >= 65526) SD_p.fat32 = 1;
			else { *err = 1; return; }
			SD_p.inited = 1;
			DISABLE_CS;	
		}
	}
}

// wait until SD initialized
// return 1 if newly detected
uint8_t SD_detect(uint8_t start)
{
	if (start || EJECT) {
		while (!SD_p.inited) {
			uint8_t err = 0;
		
#ifndef SDISK2P
			LCD_printAll("SD EJECT        ");
#endif
			{
				uint16_t i = 500;	
				while (i--) if (EJECT) i = 500;
			}
			cli();
			SD_init(&err);
			sei();
			if (err) continue;
#ifndef SDISK2P
			LCD_locate(0,0);
			LCD_print("SD MOUNT",8);
#endif
			return 1;
		}
	}
	return 0;
}

// ========== FILE ==========

void FILE_initFat(struct FILE *filep, uint8_t *err)
{
	uint32_t sectLen, clstLen, dltClst, ft, i, ofs1 = 0, ofs2 = 1;

	sectLen = (filep->length+SD_p.bytesPerSector-1)/SD_p.bytesPerSector;
	clstLen = (sectLen+SD_p.sectorsPerCluster-1)/SD_p.sectorsPerCluster;
	dltClst = (clstLen+FAT_ELEMS-1)/FAT_ELEMS;

	for (i = 0; i < FAT_ELEMS; i ++) {
		if (EJECT) { *err = 1; return; }
		filep->fat[i] = SD_p.fat32?0x0ffffff7:0xfff7;	// initialize
	}

	ft = filep->startCluster;

	filep->fat[0] = ft;
	filep->prevFatNum = ft;
	filep->prevClstNum = 0;

	for (i = 0; i < clstLen-1; i++) {
		if (EJECT) { *err = 1; return; }
		if (ofs1 != ofs2) {
			ofs1 = ft*(SD_p.fat32?4:2)/SD_p.bytesPerSector;
			SD_readBlock(SD_p.fatAddr+ofs1, err);
		}
		if (SD_p.fat32)
			ft = (readmem_long(&SD_p.buff[ft*4%SD_p.bytesPerSector])&0x0fffffff);
		else
			ft = readmem_word(&SD_p.buff[ft*2%SD_p.bytesPerSector]);

		if ((i+1)%dltClst == 0)
			filep->fat[(i+1)/dltClst] = ft;
		if (SD_p.fat32) {
			if (ft>0x0ffffff6) break;
		} else {
			if (ft>0xfff6) break;
		}
		ofs2 = ft*(SD_p.fat32?4:2)/SD_p.bytesPerSector;
	}
}

// open the file
void FILE_open(struct FILE *filep, int32_t dir_cluster, char *name, char *exts, uint8_t *err)
{
	uint8_t i, s;
	uint32_t max_entry_adr = 0, max_entry_offset = 0;
	uint16_t max_time = 0, max_date = 0;
	uint8_t isRoot = !dir_cluster;
	uint8_t isDir = 0, max_isDir = 0;
	uint8_t spc = (isRoot&&!SD_p.fat32)?64:SD_p.sectorsPerCluster;
	uint32_t ft = isRoot?2:dir_cluster;
	uint32_t entry_adr = isRoot?SD_p.rootAddr:(SD_p.userAddr+((ft-2)*SD_p.sectorsPerCluster));
	
	filep->valid = 0;
	do {
		for (s=0; s!=spc; s++) {
			uint16_t entry_offset = 0;

			SD_readBlock(entry_adr, err);
			for (i=0; i!=16; i++, entry_offset += 32) {
				if (EJECT) { *err = 1; return; }
				char ext_[4], name_[9], d;
				uint16_t f_time, f_date;

				memcpy(name_, &SD_p.buff[entry_offset+0], 8);
				name_[8] = 0;

				// check first char
				d = name_[0];
				if (d==0x00) goto FILE_OPEN_EXIT;
				if ((d==0x2e)||(d==0xe5)) continue;

				// check attribute
				d = SD_p.buff[entry_offset+11];
				if ((d==0xf) || (d==0x8)) continue;
				if (d&0x0e) continue;
				isDir = ((d&0x10)?1:0);

				// check extension & protect
				memcpy(ext_, &SD_p.buff[entry_offset+8], 3);
				ext_[3] = 0;

				uint8_t extFound = 0;
#ifdef SDISK2P
				if (exts) {
					char *targExt = exts;
					while (targExt[0]) {
						if (memcmp(ext_, targExt, 3)==0) {
						extFound=1;
						break;
						}
						targExt+=3;
					}
				}
#else
				extFound = (memcmp(ext_, exts, 3)==0);
#endif
				// check time stamp
				f_time = readmem_word(&SD_p.buff[entry_offset+22]);
				f_date = readmem_word(&SD_p.buff[entry_offset+24]);

				if ((!exts || extFound)&&(!name || !memcmp(name, name_, 8))) {
					uint16_t tm = f_time;
					uint16_t dt = f_date;

					if ((dt>max_date)||((dt==max_date)&&(tm>=max_time))) {
						max_time = tm;
						max_date = dt;
						max_entry_adr = entry_adr;
						max_entry_offset = entry_offset;
						max_isDir = isDir;
						memcpy(filep->name, name_, 8);
						memcpy(filep->name+8, ext_,3);
						*(filep->name+11)=0;
					}
				}		
			}
			entry_adr++;
		}
		if (!(isRoot&&!SD_p.fat32)) {
			SD_readBlock(SD_p.fatAddr+ft*(SD_p.fat32?4:2)/SD_p.bytesPerSector, err);
			if (SD_p.fat32) {
				ft = 0x0fffffff&readmem_long(&SD_p.buff[(ft*4)%SD_p.bytesPerSector]);
			} else {
				ft = readmem_word(&SD_p.buff[(ft*2)%SD_p.bytesPerSector]);
			}
			if ((SD_p.fat32&&(ft>=0x0ffffff7)) || ((!SD_p.fat32)&&(ft>=0xfff7))) break;
			entry_adr = SD_p.userAddr+((ft-2)*SD_p.sectorsPerCluster);
		}
	} while (!(isRoot&&!SD_p.fat32));

FILE_OPEN_EXIT:
	
	if (max_entry_adr) {
		SD_readBlock(max_entry_adr, err);
		filep->length = readmem_long(SD_p.buff+max_entry_offset+28);
		filep->startCluster = readmem_word(SD_p.buff+max_entry_offset+26)+(uint32_t)readmem_word(SD_p.buff+max_entry_offset+20)*0x10000;
		filep->protect = (SD_p.buff[max_entry_offset+11]&1);
		filep->isDir = max_isDir;
		FILE_initFat(filep, err);
		filep->valid = 1;
		filep->written = 0;
		
		return;
	}
	*err = 1;

	return;
}

// open the file with the absolute path
// name should be <8 character directory name><3 character extension><8...><3...>...0
void FILE_openAbs(struct FILE *filep, char *name, uint8_t *err)
{
	int32_t dir = 0;
	do {
		FILE_open(filep, dir, name, name+8, err);
		if (*err) return;
		name += 11;
		dir = filep->startCluster;
	} while (filep->isDir);
}

// get file name, extension, attribute and start cluster from a file list entry
// used in UI.c
void FILE_getEntry(struct FILELST *entry, char *name, char *ext, uint8_t *attr, uint32_t *stclst, uint8_t *err)
{
	SD_readBlock(entry->blkAdr, err);
	if (name) memcpy(name, &SD_p.buff[entry->offset+0], 8);
	if (ext) memcpy(ext, &SD_p.buff[entry->offset+8], 3);
	if (attr) *attr = SD_p.buff[entry->offset+11];
	if (stclst) *stclst = readmem_word(SD_p.buff+entry->offset+26)+(uint32_t)readmem_word(SD_p.buff+entry->offset+20)*0x10000;
}

static int compare(struct FILELST *a, struct FILELST *b)
{
	uint8_t err = 0;
	char name1[11], name2[11];
		
	FILE_getEntry(a, name1, name1+8, 0, 0, &err);
	FILE_getEntry(b, name2, name1+8, 0, 0, &err);
	return memcmp(name1, name2, 11);
}

static void combSort(struct FILELST* array, uint16_t size){
	uint16_t h = size;
	uint8_t is_swapped = 0;
	uint16_t i;
	struct FILELST temp;
	
	while (h > 1 || is_swapped){
		if (h > 1){
			h = (h*10)/13;
		}
		
		is_swapped = 0;
		for (i = 0; i < size-h; ++i){
			if (compare(&array[i], &array[i+h])>0){
				//swap
				temp = array[i];
				array[i] = array[i+h];
				array[i+h] = temp;
				is_swapped = 1;
			}
		}
	}
}

// make file list and sort
// used in UI.c
uint16_t FILE_makeList(int32_t dir_cluster, struct FILELST *list, char *exts, uint8_t *err)
{
	uint16_t entryNum = 0;
	uint16_t i, s;
	uint8_t isRoot = !dir_cluster;
	uint8_t spc = (isRoot&&!SD_p.fat32)?64:SD_p.sectorsPerCluster;
	uint32_t ft = isRoot?2:dir_cluster;
	uint32_t entry_adr = isRoot?SD_p.rootAddr:(SD_p.userAddr+((ft-2)*SD_p.sectorsPerCluster));

	// for "UNMOUNT" entry
	list[entryNum].blkAdr = 0;
	list[entryNum++].offset = 0;

	// find extension
	do {
		for (s=0; s!=spc; s++) {
			uint16_t entry_offset = 0;

			SD_readBlock(entry_adr, err);
			for (i=0; i!=16; i++, entry_offset += 32) {
				if (EJECT) { *err = 1; return 0; }
				char ext_[3], name_[2], d;

				memcpy(name_, &SD_p.buff[entry_offset+0], 2);
			
				// check first char
				d = name_[0];
				if (d==0x00) goto FILE_MFNL_EXIT;
				if (d==0xe5) continue;
				if ((d==0x2e)&&(name_[1]!=0x2e)) continue;

				// check attribute
				d = SD_p.buff[entry_offset+11];
				if (d&0x0e) continue;

				// check extension & protect
				memcpy(ext_, &SD_p.buff[entry_offset+8], 3);
				{
					uint8_t flg = 0;	
					char *targExt = exts;
					while (targExt[0]) {
						if (memcmp(ext_, targExt, 3)==0) {
							flg=1;
							break;
						}
						targExt+=3;
					}
					if (flg || (d&0b00010000)) {
						if (entryNum < 180) {
							list[entryNum].blkAdr = entry_adr;
							list[entryNum++].offset = entry_offset;
						}
					}
				}
			}
			entry_adr++;
		}
		if (!(isRoot&&!SD_p.fat32)) {
			SD_readBlock(SD_p.fatAddr+ft*(SD_p.fat32?4:2)/SD_p.bytesPerSector, err);
			if (SD_p.fat32) {
				ft = 0x0fffffff&readmem_long(&SD_p.buff[(ft*4)%SD_p.bytesPerSector]);
				} else {
				ft = readmem_word(&SD_p.buff[(ft*2)%SD_p.bytesPerSector]);
			}
			if ((SD_p.fat32 && (ft>=0x0ffffff7)) || ((!SD_p.fat32)&&(ft>=0xfff7))) break;
			entry_adr = SD_p.userAddr+((ft-2)*SD_p.sectorsPerCluster);
		}
	} while (!(isRoot&&!SD_p.fat32));

FILE_MFNL_EXIT:
	combSort(list, entryNum);
	
	return entryNum;
}

void FILE_prepareFat(struct FILE *filep, uint32_t *fat, uint32_t clstLen, uint32_t clstNum, uint8_t *err)
{
	uint16_t dltClst;
	uint16_t i;
	uint32_t ofs1=0, ofs2=1;

	dltClst = (clstLen+FAT_ELEMS-1)/FAT_ELEMS;
	*fat = filep->fat[clstNum / dltClst];

	for (i = 0; i < (clstNum%dltClst); i++) {
		if (EJECT) { *err = 1; return; }
		if (ofs1 != ofs2) {
			ofs1 = (*fat)*(SD_p.fat32?4:2)/SD_p.bytesPerSector;
			SD_readBlock(SD_p.fatAddr+ofs1, err);
		}
		if (SD_p.fat32) {
			*fat = (readmem_long(&SD_p.buff[(*fat)*4%SD_p.bytesPerSector])&0x0fffffff);
			if (*fat>0x0ffffff6) break;
		} else {
			*fat = readmem_word(&SD_p.buff[(*fat)*2%SD_p.bytesPerSector]);
			if (*fat>0xfff6) break;
		}
		ofs2 = (*fat)*(SD_p.fat32?4:2)/SD_p.bytesPerSector;
	}
}

void FILE_rw_sub(uint32_t long_sector, struct FILE *filep, uint8_t *err)
{
	uint32_t long_cluster = long_sector/SD_p.sectorsPerCluster;
	
	if (long_cluster != filep->prevClstNum) {
		uint32_t sectLen;
		uint32_t clstLen;
		uint32_t fat;
	
		filep->prevClstNum = long_cluster;
		sectLen = (filep->length+SD_p.bytesPerSector-1)/SD_p.bytesPerSector;
		clstLen = (sectLen+SD_p.sectorsPerCluster-1)/SD_p.sectorsPerCluster;
		FILE_prepareFat(filep, &fat, clstLen, long_cluster, err);
		filep->prevFatNum = fat;
	}
}

// prepare reading a sector from the file
void FILE_readBegin(struct FILE *filep, uint32_t long_sector, uint8_t *err)
{
	if (!filep->valid) {*err=1; return;}
	FILE_rw_sub(long_sector, filep, err);
	SD_readBlockBegin(SD_p.userAddr+(filep->prevFatNum-2)*SD_p.sectorsPerCluster+long_sector%SD_p.sectorsPerCluster, err);
}

// end the reading
void FILE_readEnd(uint8_t *err)
{
	SD_readBlockEnd(err);
}

// read a sector from the file
void FILE_read(struct FILE *filep, uint32_t long_sector, uint8_t *err)
{
	if (!filep->valid) {*err=1; return;}
	FILE_readBegin(filep, long_sector, err);
	for (uint16_t i = 0; i < 512; i++) SD_p.buff[i] = SPI_readByte(err);			
	FILE_readEnd(err);
}

// prepare writing a sector to the file
void FILE_writeBegin(struct FILE *filep, uint32_t long_sector, uint8_t *err)
{
	if (!filep->valid) {*err=1; return;}
	FILE_rw_sub(long_sector, filep, err);
	SD_writeBlockBegin(SD_p.userAddr+(filep->prevFatNum-2)*SD_p.sectorsPerCluster+long_sector%SD_p.sectorsPerCluster, err);
	if (!*err) filep->written = 1;
}

// end the writing
void FILE_writeEnd(uint8_t *err)
{
	SD_writeBlockEnd(err);
}
	
void SD_alloc(uint32_t adr, uint16_t ofsH, uint16_t ofsL, uint32_t clstLen, uint8_t isFat, uint8_t isClr, uint8_t *err)
{
	uint32_t clstNum = 0;
	uint8_t rdFat = 1;
	for (uint32_t ft=2; clstNum<=clstLen; ft++) {
		uint32_t d=0;
		if ((ft*(SD_p.fat32?4:2)%SD_p.bytesPerSector)==0) rdFat = 1;
		if (EJECT) { *err = 1; return; }
		if (clstNum < clstLen) {
			if (rdFat) {
				SD_readBlock(SD_p.fatAddr+ft*(SD_p.fat32?4:2)/SD_p.bytesPerSector, err);
				rdFat = 0;
			}
			if (*err) {return;}
			if (SD_p.fat32)
				d = (readmem_long(&SD_p.buff[ft*4%SD_p.bytesPerSector])&0x0fffffff);
			else
				d = readmem_word(&SD_p.buff[ft*2%SD_p.bytesPerSector]);
		} else ft = (SD_p.fat32?0xfffffff:0xffff);
		if ((clstNum==clstLen) || (d==0)) {
			if ((d==0)&&isClr&&(clstNum<clstLen)) {
				for (uint8_t s=0; s<SD_p.sectorsPerCluster; s++) {
					SD_writeBlockBegin(SD_p.userAddr+(ft-2)*SD_p.sectorsPerCluster+s, err);
					for (uint16_t j=0; j<512; j++) {
						SPI_writeByte(0, err);
					}
					SD_writeBlockEnd(err);
				}
			}
			clstNum++;
			SD_writeWord(adr, ofsL, ft&0xffff, err);
			if (*err) {return;}
			if (SD_p.fat32) {
				SD_writeWord(adr, ofsH, ft>>16, err);
				if (*err) {return;}
			}
			if (isFat) {
				if (SD_p.numFats == 2) {
					SD_writeWord(adr+SD_p.fatSz, ofsL, ft&0xffff, err);
					if (*err) {return;}
					if (SD_p.fat32) {
						SD_writeWord(adr+SD_p.fatSz, ofsH, ft>>16, err);
						if (*err) {return;}
					}
				}
			}
			isFat = 1;
			adr = SD_p.fatAddr+ft*(SD_p.fat32?4:2)/SD_p.bytesPerSector;
			ofsL = ft*(SD_p.fat32?4:2)%SD_p.bytesPerSector;
			ofsH = ofsL+2;
			rdFat = 1;
		}
	}
}

// create a file
// Notice : buffer2.sd.buf[512] is also used!
void FILE_create(int32_t dir_cluster, const char *name, uint32_t length, uint8_t *err)
{
	uint8_t i, s, found = 0;
	uint16_t entry_offset;
	uint8_t isRoot = !dir_cluster;
	uint8_t spc = (isRoot&&!SD_p.fat32)?64:SD_p.sectorsPerCluster;
	uint32_t ft = isRoot?2:dir_cluster;
	uint32_t entry_adr = isRoot?SD_p.rootAddr:(SD_p.userAddr+((ft-2)*SD_p.sectorsPerCluster));
	uint32_t sectNum = (length+SD_p.bytesPerSector-1)/SD_p.bytesPerSector;
	uint32_t adr;
	uint16_t ofsH, ofsL;

	do {
		for (s=0; s!=spc; s++) {
			entry_offset = 0;
			SD_readBlock(entry_adr, err);
			for (i=0; i!=16; i++, entry_offset += 32) {
				if (EJECT) { *err = 1; return; }
				// first char
				char d = SD_p.buff[entry_offset];
				// attribute
				char a = SD_p.buff[entry_offset+11];
				if (((d==0xe5)||(d==0x00))&&(a!=0xf)) {
					found = 1;	
					goto FILE_CREATE_EXIT;  // find a RDE!
				}
			}
			entry_adr++;
		}
		if (!(isRoot&&!SD_p.fat32)) {
			uint32_t oldft = ft;
			SD_readBlock(SD_p.fatAddr+ft*(SD_p.fat32?4:2)/SD_p.bytesPerSector, err);
			if (SD_p.fat32) ft = 0x0fffffff&readmem_long(&SD_p.buff[(ft*4)%SD_p.bytesPerSector]);
			else ft = readmem_word(&SD_p.buff[(ft*2)%SD_p.bytesPerSector]);
			if ((SD_p.fat32&&(ft>=0x0ffffff7)) || ((!SD_p.fat32)&&(ft>=0xfff7))) {
				ft = oldft;
				adr = SD_p.fatAddr+ft*(SD_p.fat32?4:2)/SD_p.bytesPerSector;
				ofsL = (ft*(SD_p.fat32?4:2))%SD_p.bytesPerSector;
				ofsH = ofsL+2;
				SD_alloc(adr, ofsH, ofsL, 1, 1, 1, err);
				SD_readBlock(SD_p.fatAddr+ft*(SD_p.fat32?4:2)/SD_p.bytesPerSector, err);
				if (SD_p.fat32) ft = 0x0fffffff&readmem_long(&SD_p.buff[(ft*4)%SD_p.bytesPerSector]);
				else ft = readmem_word(&SD_p.buff[(ft*2)%SD_p.bytesPerSector]);	
			}
			entry_adr = SD_p.userAddr+((ft-2)*SD_p.sectorsPerCluster);
		}		
	} while (!(isRoot&&!SD_p.fat32));

FILE_CREATE_EXIT:
	if (found) {
		uint32_t clstLen = ((sectNum+SD_p.sectorsPerCluster-1)/SD_p.sectorsPerCluster);
		uint8_t dirEntry[32];
		for (i=0; i<32; i++) dirEntry[i]=0;
		memcpy(dirEntry, name, 11);
		
		*(uint32_t *)(dirEntry+28) = length;
		SD_writeBytes(entry_adr, entry_offset, dirEntry, 32, err);
		if (*err) return;
		SD_alloc(entry_adr, entry_offset+20,  entry_offset+26, clstLen, 0, 0, err);		
	} else { *err = 1; return; }
}
	
// create a file with the absolute path
// name should be <8 character directory name><3 character extension><8...><3...>...0
// Notice : buffer2.sd.buf[512] is also used!
void FILE_createAbs(struct FILE *filep, char *name, uint32_t length, uint8_t *err)
{
	int32_t dir = 0;

	while (*(name+11)) {
		FILE_open(filep, dir, name, name+8, err);
		if (*err) return;
		name += 11;
		dir = filep->startCluster;
		if (!filep->isDir) {*err=1; return;}
	}
	FILE_create(dir, name, length, err);
}

// substitute the extension of the full path file name with the given new extension
void FILE_substituteFullpathExtwith(char *fullpath, char *ext)
{
	while (*(fullpath+11)) {
		fullpath += 11;
	}
	memcpy(fullpath+8, ext, 3);	
}
