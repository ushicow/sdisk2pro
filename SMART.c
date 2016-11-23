/*
 * SMART.c
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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include "COMMON.h"
#include "SD.h"
#include "BUFFER.h"
#include "DISK2.h"

#ifndef SDISK2P
#include "LCD.h"
#include "UI.h"
#endif

//number of prodos partions

#ifdef SDISK2P
#define SMART_PARTITION_MAX 1
#else
#define SMART_PARTITION_MAX 4
#endif

#ifdef SDISK2P
#define REQ PINC&(1<<0)
#define ACK_HIGH PORTD|=(1<<4)
#define ACK_LOW PORTD&=~(1<<4)
#else
#define REQ VPORT0_IN&PIN0_bm
#define ACK_HIGH VPORT2_OUT|=PIN7_bm
#define ACK_LOW VPORT2_OUT&=~PIN7_bm
#endif

#ifdef SDISK2P
const uint8_t UI_running = 0;
#endif

uint8_t SMART_is2mg[4];

void SMART_encodePacket (uint8_t source, uint8_t type, uint8_t status, uint16_t length);	//encode SmartPort packet
uint8_t SMART_decodePacket (uint16_t length);			//decode SmartPort packet

extern uint8_t SMART_ReceivePacket(uint8_t*);			//receive SmartPort packet assembler function
extern uint8_t SMART_SendPacket(uint8_t*);				//send SmartPort packet assembler function

struct FILE *SMART_image[SMART_PARTITION_MAX];			//disk images

uint8_t SMART_device_id[SMART_PARTITION_MAX];			//to hold assigned device id's for the partitions
uint8_t SMART_partition_num;
uint8_t SMART_number_partitions_initialised;
uint8_t SMART_partition_num;
uint8_t SMART_desktop;

// initialize SmartPort device emulator
void SMART_init()
{	
	for (uint8_t i=0 ; i < SMART_PARTITION_MAX; i++)
		SMART_image[i] = 0;

#ifdef SDISK2P
	// OFF WREQ
	EIMSK &= ~0b00000001;
#else
	// OFF WREQ
	PORTD.INTMASK = 0;
#endif
}

// run the SmartPort device emulator
void SMART_run(uint8_t (*pf)(uint8_t))
{	
	while (1) {
		pf(0);
#ifndef SDISK2P
		UI_checkExecute();
#endif
	}	
}

// execute SmartPort protocol
//void SMART_protocol()
#ifdef SDISK2P
ISR(PCINT1_vect)
#else
ISR(PORTA_INT_vect)
#endif
{
#ifndef SDISK2P
	PORTA.INTFLAGS |= PHASE_bm;
#endif
	if (mode == SMARTMODE) {
		uint8_t source, status, partition;
		uint8_t phase = PHASE;

		if (!(((phase&0b1010)==0b1010)||((phase&0b0101)==0b0101))) {
			return;
		}

		// read phase lines to check for SmartPort reset or enable
		switch (phase) {
			// phase lines for SmartPort bus reset
			// ph3=0 ph2=1 ph1=0 ph0=1
			case 0x05:
#ifndef SDISK2P
				if (!UI_running) {
					LCD_printAll("RESET           ");
				}
#endif
				// monitor phase lines for reset to clear
				while (PHASE == 0x05);
				SMART_number_partitions_initialised = 0;  // reset number of partitions init'd
				for (partition = 0; partition < SMART_PARTITION_MAX; partition++) // clear device_id table
					SMART_device_id[partition] = 0;
				break;
			// phase lines for SmartPort bus enable
			// ph3=1 ph2=x ph1=1 ph0=x
			case 0x0a:
			case 0x0b:
			case 0x0e:
			case 0x0f:
				{
					uint16_t i = 0;

					ACK_HIGH;
					while (1) {
						if (REQ) break;
						if (++i == 10000) { // timeout...
							ACK_LOW;
							return;
						}
					}
				}
				status = SMART_ReceivePacket(buffer2.smart.buf);
				if (status) return; // timeout

				// assume it's a cmd packet, cmd code is in byte 14
				switch (buffer2.smart.buf[14]) {
					case 0x80:  //is a status cmd
						source = buffer2.smart.buf[6];
						SMART_decodePacket(5);
						uint8_t flg = (buffer1[2]==0x20);

						for (partition = 0; partition < SMART_PARTITION_MAX; partition++) {	// Check if its one of ours
							if (SMART_device_id[partition] == source) {					// yes it is, then reply
								uint32_t partition_block_num = (buffer2.smart.img[partition].valid&&!UI_running)?((SMART_is2mg[partition]?(buffer2.smart.img[partition].length-64+511):(buffer2.smart.img[partition].length+511))/512):0;
		
								buffer1[0]=0xe8 // BlockDevice,ReadWriteAllowed,FormatAllowed
								| ((buffer2.smart.img[partition].valid&&!UI_running)?0x10:0)						// DiskInDrive
								| ((buffer2.smart.img[partition].valid)?((buffer2.smart.img[partition].protect||WP)?0x04:0):0x04);	// WriteProtect
								buffer1[1] = (partition_block_num&0xff);
								buffer1[2] = ((partition_block_num>>8)&0xff);
								buffer1[3] = ((partition_block_num>>16)&0xff);
								buffer1[4]=0;
								memcpy(&buffer1[5], "                ", 16);
								buffer1[21]=0x02; // hard disk
								buffer1[22]=0x00; // removable media
								buffer1[23]=0x00;
								buffer1[24]=0x00;
								SMART_encodePacket(source, 0x01, 0x00, flg?25:4);
								status = SMART_SendPacket(buffer2.smart.buf);
							}
						}
						break;
					case 0x81:	// is a readblock cmd
						if (UI_running) break;		
						source = buffer2.smart.buf[6];
						for (partition = 0; partition < SMART_PARTITION_MAX; partition++) {	// Check if its one of ours
							if (!buffer2.smart.img[partition].valid) break;

							if ((SMART_device_id[partition] == source)&&buffer2.smart.img[partition].valid&&!UI_running) {	// yes it is, then do the read
								uint8_t err;
								uint32_t block_num; 
								// block num 1st byte
								block_num = ((buffer2.smart.buf[19] & 0x7f) | ((buffer2.smart.buf[16] << 3) & 0x80));
								// block num second byte
								block_num += ((uint32_t)((buffer2.smart.buf[20] & 0x7f) | ((buffer2.smart.buf[16] << 4) & 0x80))*256);
								// block num third byte
								block_num += ((uint32_t)((buffer2.smart.buf[21] & 0x7f) | ((buffer2.smart.buf[16] << 5) & 0x80))*65536);
#ifdef SDISK2P
								LED_ON;
#else
								if (!UI_running) {
									LCD_locate(0,0);
									LCD_print("RD", 2);
									LCD_printhex(block_num,6);	
									LCD_locate(0,1);
									LCD_print(buffer2.smart.img[partition].name,8);
								}
#endif
								if (SMART_is2mg[partition]) {
									SD_changeBuff(buffer2.smart.buf);
									FILE_readBegin(&buffer2.smart.img[partition], block_num, &err);
									for (uint16_t i=0; i<64; i++) {
										SPI_readByte(&err);
									}
									for (uint16_t i=0; i<448; i++) {
										buffer1[i] = SPI_readByte(&err);
									}
									FILE_readEnd(&err);
									FILE_readBegin(&buffer2.smart.img[partition], block_num+1, &err);
									for (uint16_t i=0; i<64; i++) {
										buffer1[i+448] = SPI_readByte(&err);
									}
									for (uint16_t i=0; i<448; i++) {
										SPI_readByte(&err);
									}
									FILE_readEnd(&err);
									SD_changeBuff(buffer1);
								} else {
									FILE_readBegin(&buffer2.smart.img[partition], block_num, &err);
									for (uint16_t i=0; i<512; i++) {
										buffer1[i]=SPI_readByte(&err);
									}
									FILE_readEnd(&err);
								}
#ifdef SDISK2P
								LED_OFF;
#endif
								SMART_encodePacket(source, 0x02, 0x00, 512);
								status = SMART_SendPacket(buffer2.smart.buf);
							}
						}				
						break;
					case 0x82:  // is a writeblock cmd
						source = buffer2.smart.buf[6];
						for (partition = 0; partition < SMART_PARTITION_MAX; partition++) {	// Check if its one of ours
							if ((SMART_device_id[partition] == source)&&buffer2.smart.img[partition].valid&&!UI_running) {	// yes it is, then do the write
							// if (SMART_device_id[partition] == source) {		// yes it is, then do the write
								uint8_t err;
								uint32_t block_num;
	
								// block num 1st byte
								block_num = ((buffer2.smart.buf[19] & 0x7f) | ((buffer2.smart.buf[16] << 3) & 0x80));
								// block num second byte
								block_num += ((uint32_t)((buffer2.smart.buf[20] & 0x7f) | ((buffer2.smart.buf[16] << 4) & 0x80))*256);
								// block num third byte
								block_num += ((uint32_t)((buffer2.smart.buf[21] & 0x7f) | ((buffer2.smart.buf[16] << 5) & 0x80))*65536);
	
								// get write data packet
								ACK_HIGH;
								while (!REQ) ;
	
								status = SMART_ReceivePacket(buffer2.smart.buf);
								status = SMART_decodePacket(512);

								if (status==0) { // ok	
#ifdef SDISK2P
									LED_ON;
#else			
									if (!UI_running) {
										LCD_locate(0,0);
										LCD_print("WR", 2);
										LCD_printhex(block_num,6);
										LCD_locate(0,1);
										LCD_print(buffer2.smart.img[partition].name,8);
									}
#endif
#ifndef SDISK2P
									if (SMART_is2mg[partition]) {
										SD_changeBuff(buffer2.smart.buf);
										FILE_readBegin(&buffer2.smart.img[partition], block_num, &err);
										for (uint16_t i=0; i<512; i++) {
											buffer2.smart.buf2[i]=SPI_readByte(&err);
										}
										FILE_readEnd(&err);
										FILE_writeBegin(&buffer2.smart.img[partition], block_num, &err);
										for (uint16_t i=0; i<64; i++) {
											SPI_writeByte(buffer2.smart.buf2[i], &err);
										}
										for (uint16_t i=0; i<448; i++) {
											SPI_writeByte(buffer1[i], &err);
										}
										FILE_writeEnd(&err);
										FILE_readBegin(&buffer2.smart.img[partition], block_num+1, &err);
										for (uint16_t i=0; i<512; i++) {
											buffer2.smart.buf2[i]=SPI_readByte(&err);
										}
										FILE_readEnd(&err);
										FILE_writeBegin(&buffer2.smart.img[partition], block_num+1, &err);
										for (uint16_t i=0; i<64; i++) {
											SPI_writeByte(buffer1[448+i], &err);
										}
										for (uint16_t i=0; i<448; i++) {
											SPI_writeByte(buffer2.smart.buf2[i+64], &err);
										}
										FILE_writeEnd(&err);
										SD_changeBuff(buffer1);
									} else
#endif
									{
										// write block
										SD_changeBuff(buffer2.smart.buf);
										FILE_writeBegin(&buffer2.smart.img[partition], block_num, &err);
										SD_changeBuff(buffer1);
										for (uint16_t i=0; i<512; i++) {
											SPI_writeByte(buffer1[i], &err);
										}
										FILE_writeEnd(&err);
									}
#ifdef SDISK2P
									LED_OFF;
#endif							
								}
								SMART_encodePacket (source,0x01,status, 0);
								status = SMART_SendPacket(buffer2.smart.buf);
							}
						}
						break;
					case 0x83:  // is a format cmd
						source = buffer2.smart.buf[6];
						for (partition = 0; partition < SMART_PARTITION_MAX; partition++) {	// Check if its one of ours
							if ((SMART_device_id[partition] == source)&&buffer2.smart.img[partition].valid&&!UI_running) {	// yes it is, then do the read
							// if (SMART_device_id[partition] == source) {		// yes it is, then reply to the format cmd
#ifndef SDISK2P
								if (!UI_running) {
									LCD_locate(0,0);
									LCD_print("FORMAT  ",8);
									LCD_locate(0,1);
									LCD_print("DRV",3);
									LCD_printdec(partition+1,1);
									LCD_print("    ", 4);
								}
#endif								
								SMART_encodePacket(source,0x00, 0x0, 0);	// just send back a successful response
								status = SMART_SendPacket(buffer2.smart.buf);
#ifndef SDISK2P
								LCD_cls();
#endif
							}
						}
						break;
					case 0x85:  // is an init cmd
						if (UI_running) break;
						source = buffer2.smart.buf[6];
						SMART_device_id[SMART_number_partitions_initialised] = source;				// remember source id for partition
						SMART_number_partitions_initialised++;
						if (SMART_number_partitions_initialised < (SMART_desktop?SMART_partition_num:SMART_PARTITION_MAX)) {			// are all init'd yet
							status = 0x80;          // no, so status=0
						} else {					// the last one
							status = 0xff;          // yes, so status=non zero
						}
						SMART_encodePacket(source,0x00, status, 0);
						status = SMART_SendPacket(buffer2.smart.buf);
						break;
					default:
						break;
				}
			}
	} else {
		DISK2_moveHead();
	}
}

// encode a SmartPort packet. 
void SMART_encodePacket (uint8_t source, uint8_t type, uint8_t status, uint16_t length)
{
    uint8_t grpmsb;
	uint8_t oddlen = length%7;
	uint8_t grp7len = length/7;

    buffer2.smart.buf[0] =0xff;			// sync bytes
    buffer2.smart.buf[1] =0x3f;
    buffer2.smart.buf[2] =0xcf;
    buffer2.smart.buf[3] =0xf3;
    buffer2.smart.buf[4] =0xfc;
    buffer2.smart.buf[5] =0xff;

	buffer2.smart.buf[6] =0xc3;			// PBEGIN - start byte
    buffer2.smart.buf[7] =0x80;			// DEST - dest id - host
    buffer2.smart.buf[8] =0x80|source;	// SRC - source id - us
    buffer2.smart.buf[9] =0x80|type;		// TYPE
    buffer2.smart.buf[10]=0x80;			// AUX
    buffer2.smart.buf[11]=0x80|status;	// STAT
	
    buffer2.smart.buf[12]=oddlen|0x80;	// ODDCNT
	buffer2.smart.buf[13]=grp7len|0x80;	// GRP7CNT

    // odd bytes
	if (oddlen != 0) {
		uint8_t pch = 0x80;
		for (uint8_t oddcount=0; oddcount<oddlen; oddcount++) {
			pch += ((buffer1[oddcount]>>(oddcount+1))&0x40);
			buffer2.smart.buf[15+oddcount]= buffer1[oddcount] | 0x80;
		}
		buffer2.smart.buf[14]=pch;
	}

    // grps of 7
    for (uint8_t grpcount=0; grpcount<grp7len; grpcount++)
    {
        // add group msb byte
        grpmsb=0;
        for (uint8_t grpbyte=0; grpbyte<7; grpbyte++)
            grpmsb = (grpmsb | ((buffer1[oddlen+(grpcount*7)+grpbyte] >> (grpbyte+1)) & (0x80 >> (grpbyte+1))));
        buffer2.smart.buf[(oddlen?15+oddlen:14)+(grpcount*8)] = grpmsb | 0x80;   // set msb to one

        // now add the group data bytes bits 6-0
        for (uint8_t grpbyte=0; grpbyte<7; grpbyte++)
            buffer2.smart.buf[(oddlen?16+oddlen:15)+(grpcount*8)+grpbyte] = buffer1[oddlen+(grpcount*7)+grpbyte]| 0x80;

    }
    // add checksum
	{
		uint8_t checksum = 0;
		uint16_t at_check_sum = (oddlen?23+oddlen:22)+(((int16_t)grp7len-1)*8);

		for (uint16_t count=0; count<length; count++)   // xor all the data bytes
			checksum = checksum ^ buffer1[count];
		for (uint8_t count=7; count<14; count++)		// now xor the packet header bytes
			checksum = checksum ^ buffer2.smart.buf[count];

	    buffer2.smart.buf[at_check_sum] = checksum | 0xaa;		// 1 c6 1 c4 1 c2 1 c0
		buffer2.smart.buf[at_check_sum+1] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

		//end bytes
		buffer2.smart.buf[at_check_sum+2] = 0xc8;  // pkt end
		buffer2.smart.buf[at_check_sum+3] = 0x00;  // mark the end of the packet_buffer
	}
}

// decode a SmartPort packet. 
uint8_t SMART_decodePacket(uint16_t length)
{
    uint8_t bit0to6, bit7, oddbits, evenbits;

	uint8_t oddlen = length%7;
	uint8_t grp7len = length/7;

	if (oddlen != 0) {		
		for (uint8_t oddcount=0; oddcount<oddlen; oddcount++)			
			buffer1[oddcount] = ((buffer2.smart.buf[13] << (1+oddcount)) & 0x80) | (buffer2.smart.buf[14+oddcount] & 0x7f);
	}

    for (uint8_t grpcount=0; grpcount<grp7len; grpcount++)
    {
        for (uint8_t grpbyte=0; grpbyte<7; grpbyte++) {
            bit7 = (buffer2.smart.buf[(oddlen?14+oddlen:13)+(grpcount*8)] << (grpbyte+1)) & 0x80;
            bit0to6 = (buffer2.smart.buf[(oddlen?15+oddlen:14)+(grpcount*8)+grpbyte]) & 0x7f;
            buffer1[oddlen+(grpcount*7)+grpbyte] = bit7 | bit0to6;
        }
    }

	{
		uint16_t at_check_sum = (oddlen?22+oddlen:21)+(((int16_t)grp7len-1)*8);
		uint8_t checksum=0;
		
		// verify checksum
		for (uint16_t count=0; count<length; count++)   // xor all the data bytes
			checksum = checksum ^ buffer1[count];
		for (uint8_t count=6; count<13; count++)		// now xor the packet header bytes
			checksum = checksum ^ buffer2.smart.buf[count];

		evenbits = buffer2.smart.buf[at_check_sum] & 0x55;
		oddbits = (buffer2.smart.buf[at_check_sum+1] & 0x55 ) << 1;

		if (checksum == (oddbits | evenbits))
			return 0; // noerror
		else
			return 6; // SmartPort bus error code
	}
}
