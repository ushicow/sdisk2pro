/*
 * BUFFER.h
 *
 * Created: 2013/10/29 22:49:54
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

#ifndef BUFFER_H_
#define BUFFER_H_

#include "SD.h"

#ifdef SDISK2P
union BUFFER2 {
	struct {
		uint8_t writebuf[350*3];
		struct FILE img[1];
	} disk2;
	struct {
		uint8_t buf[350*3];
		struct FILE img[1];
	} smart;
	struct {
		uint8_t buf[350*3];
		struct FILE img[1];
	} sd;
};
#else
// 2564 bytes in total
union BUFFER2 {
	struct {
		uint8_t buf[512];	
		uint8_t buf2[2052];
	} sd;
	struct {
		uint8_t buf[768];
		uint8_t buf2[632];
		struct FILE img[4];
	} smart;
	struct {
		uint8_t writebuf[350*5];
		uint8_t buf[232];
		struct FILE img[2];
	} disk2;
	struct {
		uint8_t buf[256];
		uint8_t filelist[6*180];
		uint8_t fullpath[64];
		struct FILE img[4];
	} ui;
	struct {
		uint8_t buf[256];
		uint8_t buf2[632];
		uint8_t ini[512];
		struct FILE img[4];
	} ini;
};
#endif

extern uint8_t buffer1[];
extern union BUFFER2 buffer2;

#endif /* BUFFER_H_ */