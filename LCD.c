/*
 * LCD.c
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

#include <string.h>
#include <avr/io.h>
#include "SYSTEM.h"
#include <util/delay.h>
#include "LCD.h"

/*! Defining an example slave address. */
#define LCD_SLAVE_ADDRESS    0x3e

/*! CPU speed 2MHz, BAUDRATE 400kHz and Baudrate Register Settings */
#define TWI_BAUDRATE	400000
#define TWI_BAUD(F_SYS, F_TWI) ((F_SYS / (2 * F_TWI)) - 5)
#define TWI_BAUDSETTING TWI_BAUD(F_CPU, TWI_BAUDRATE)

static void twi_init(void)
{
	TWIC.MASTER.STATUS = TWI_MASTER_BUSSTATE_UNKNOWN_gc;
	TWIC.MASTER.BAUD = TWI_BAUDSETTING;
	TWIC.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
	TWIC.MASTER.CTRLA = TWI_MASTER_ENABLE_bm;
}

static void twi_cmd(uint8_t addr, uint8_t cmd, uint8_t data)
{
	TWIC.MASTER.ADDR = addr<<1;	
	while (!(TWIC.MASTER.STATUS&TWI_MASTER_WIF_bm)) ;
	TWIC.MASTER.DATA = cmd;
	while (!(TWIC.MASTER.STATUS&TWI_MASTER_WIF_bm)) ;
	TWIC.MASTER.DATA = data;
	while (!(TWIC.MASTER.STATUS&TWI_MASTER_WIF_bm)) ;
	TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
}

void LCD_init(void)
{
	twi_init();

	_delay_ms(40);
	twi_cmd(LCD_SLAVE_ADDRESS, 0, 0x38);
	_delay_us(32);
	twi_cmd(LCD_SLAVE_ADDRESS, 0, 0x39);
	_delay_us(32);
	twi_cmd(LCD_SLAVE_ADDRESS, 0, 0x14);
	_delay_us(32);
	twi_cmd(LCD_SLAVE_ADDRESS, 0, 0x70);
	_delay_us(32);
	twi_cmd(LCD_SLAVE_ADDRESS, 0, 0x56);
	_delay_us(32);
	twi_cmd(LCD_SLAVE_ADDRESS, 0, 0x6c);
	_delay_ms(220);
	twi_cmd(LCD_SLAVE_ADDRESS, 0, 0x38);
	_delay_us(32);
	twi_cmd(LCD_SLAVE_ADDRESS, 0, 0x0c);
	_delay_us(32);
	twi_cmd(LCD_SLAVE_ADDRESS, 0, 0x01);
	_delay_ms(2);
}

void LCD_locate(uint8_t x, uint8_t y)
{
	twi_cmd(LCD_SLAVE_ADDRESS, 0, 0x80+x+y*0x40);
}

void LCD_putchar(char c)
{
	twi_cmd(LCD_SLAVE_ADDRESS, 0x40, c);
}

void LCD_print(char str[], uint8_t len)
{
	uint8_t i;
	
	for (i=0; i<len; i++) twi_cmd(LCD_SLAVE_ADDRESS, 0x40, str[i]);
}

void LCD_printhex(uint32_t a, int8_t digits)
{
	uint8_t i;
	char str[8];
	
	for (i=0; i<digits; i++) {
		uint8_t b = (a & 0xf);
		str[i] = (b<10)?b+'0':b+'A'-10;
		a>>=4;
	}
	for (i=0; i<digits; i++) LCD_putchar(str[digits-i-1]);
}

void LCD_printdec(uint32_t a, int8_t digits)
{
	uint8_t i;
	char str[8];
	
	for (i=0; i<digits; i++) {
		str[i] = (a%10)+'0';
		a/=10;
	}
	for (i=0; i<digits; i++) LCD_putchar(str[digits-i-1]);
}

void LCD_printAll(char *str)
{
	uint8_t len = strlen(str);
	LCD_locate(0,0);
	LCD_print(str, (len<8)?len:8);
	if (len<8) LCD_print("       ",8-len);
	LCD_locate(0,1);
	if (len>8) LCD_print(str+8,len-8);
	if (len<16) LCD_print("        ",16-len);
}

void LCD_cls()
{
	LCD_printAll("                ");
}

void LCD_marker(char *str)
{
	LCD_printAll(str);
}

void LCD_markerL(char *str)
{
	LCD_printAll(str);
	while (1) ;
}