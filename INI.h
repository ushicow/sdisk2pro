/*
 * INI.h
 *
 * Created: 2013/11/06 1:16:46
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

#ifndef INI_H_
#define INI_H_

// SDISK2.INI file
extern struct FILE iniFile;

// open or create UNISDISK.INI
void INI_openCreate(uint8_t *err);

// read INI file to a buffer
void INI_read(uint8_t *buff, uint8_t *err);

// substitute a file name in UNISDISK.INI
void INI_substitute(uint8_t *buff, uint8_t drv, char *name, uint8_t *err);

// read a file name in UNISDISK.INI
void INI_readFName(uint8_t *buff, uint8_t drv, char *name, uint8_t *err);

#endif /* INI_H_ */