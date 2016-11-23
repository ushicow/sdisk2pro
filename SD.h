/*
 * SD.h
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

#ifndef SD_H_
#define SD_H_

#include <avr/io.h>

#ifdef SDISK2P
#define SPI_SS_bm	0b00000100
#define SPI_MOSI_bm	0b00001000
#define SPI_MISO_bm	0b00010000
#define SPI_SCK_bm	0b00100000
#define EJECT (PIND&(1<<3))
#define WP (PIND&(1<<6))
#else
#define EJECT (/*PORTA_IN*/VPORT0_IN&PIN5_bm)
#define WP (/*PORTC_IN*/VPORT1_IN&PIN3_bm)
#define SPI_SS_bm	0b00010000
#define SPI_MOSI_bm	0b10000000
#define SPI_MISO_bm	0b01000000
#define SPI_SCK_bm	0b00100000
#endif

// SD card properties
struct SD {
	uint8_t *buff;
	uint8_t *buff2;
	volatile uint8_t inited;
	uint32_t bpbAddr;
	uint8_t blkAdrAccs;			// 1 if block address access
	uint16_t bytesPerSector;
	uint8_t sectorsPerCluster;
	uint32_t fatAddr;			// the beginning of FAT
	uint32_t fatSectors;
	uint32_t fatSz;
	uint8_t numFats;
	uint32_t rootAddr;			// the beginning of RDE
	uint32_t rootSectors;
	uint32_t userAddr;			// the beginning of user area
	uint8_t fat32;				// 0 : fat16, 1 : fat32
};
extern struct SD SD_p;

#ifdef SDISK2P
#define FAT_ELEMS 32
#else
#define FAT_ELEMS 64
#endif

// FILE properties
// 291 bytes for UNISDISK, 163 bytes for SDISP2P
struct FILE {
	uint8_t valid;
	uint32_t startCluster;		// start cluster
	uint32_t fat[FAT_ELEMS];	// the fat for the file
	uint32_t prevFatNum;		// previous Fat number for the file
	uint32_t prevClstNum;		// previous cluster number for the file
	uint8_t protect;			// write protect
	uint32_t length;			// file length
	uint32_t isDir;
	char name[12];				// file name and extension
	uint8_t written;
};

// used by UI
struct FILELST {
	uint32_t blkAdr;
	uint16_t offset;
};

// write a byte data to spi
void SPI_writeByte(uint8_t c, uint8_t *err);

// read data from spi
uint8_t SPI_readByte(uint8_t *err);

// initialization SD card
void SD_init(uint8_t *err);

// change buffer
void SD_changeBuff(uint8_t *b);

// wait until SD initialized
// return 1 if newly detected
uint8_t SD_detect(uint8_t start);

// write byte data one by one to the SD card
void SD_writeBytes(uint32_t adr, uint16_t ofs, uint8_t *ptr, uint16_t length, uint8_t *err);

// write a word one by one to the SD card
void SD_writeWord(uint32_t adr, uint16_t ofs, uint16_t d, uint8_t *err);

// write a double word one by one to the SD card
void SD_writeDWord(uint32_t adr, uint16_t ofs, uint32_t d, uint8_t *err);

// open the file
void FILE_open(struct FILE *filep, int32_t dir_cluster, char *name, char *exts, uint8_t *err);

// open the file with the absolute path
// name should be <8 character directory name><3 character extension><8...><3...>...0
void FILE_openAbs(struct FILE *filep, char *name, uint8_t *err);

// read a sector from the file
void FILE_read(struct FILE *filep, uint32_t long_sector, uint8_t *err);

// prepare reading a sector from the file
void FILE_readBegin(struct FILE *filep, uint32_t long_sector, uint8_t *err);

// end the reading
void FILE_readEnd(uint8_t *err);

// prepare writing a sector to the file
void FILE_writeBegin(struct FILE *filep, uint32_t long_sector, uint8_t *err);

// end the writing
void FILE_writeEnd(uint8_t *err);

// get file name, extension, attribute and start cluster from an file list entry
// used in UI.c
void FILE_getEntry(struct FILELST *e, char *name, char *ext, uint8_t *attr, uint32_t *stclst, uint8_t *err);

// make file list and sort
// used in UI.c
uint16_t FILE_makeList(int32_t dir_cluster, struct FILELST *list, char *targExt, uint8_t *err);

// create a file
void FILE_create(int32_t dir_cluster, const char *name, uint32_t length, uint8_t *err);

// create a file with the absolute path
// name should be <8 character directory name><3 character extension><8...><3...>...0
void FILE_createAbs(struct FILE *filep, char *name, uint32_t length, uint8_t *err);

// substitute the extension of the full path file name with the given new extension
void FILE_substituteFullpathExtwith(char *fullpath, char *ext);

#endif /* SD_H_ */