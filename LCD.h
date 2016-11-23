/*
 * LCD.h
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

#ifndef LCD_H_
#define LCD_H_

#include <avr/io.h>

void LCD_init(void);

void LCD_locate(uint8_t x, uint8_t y);

void LCD_putchar(char c);

void LCD_print(char str[], uint8_t len);

void LCD_printhex(uint32_t a, int8_t digits);

void LCD_printdec(uint32_t a, int8_t digits);

void LCD_printAll(char *str);

void LCD_cls(void);

// for debug
void LCD_marker(char *str);
void LCD_markerL(char *str);

#endif /* LCD_H */