/*
 * main
 *
 * Created: 2013/09/21 20:00:03
 *  Author: Koichi Nishida
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include "COMMON.h"
#include "SD.h"
#include "DISK2.h"
#include "SMART.h"
#include "BUFFER.h"

uint8_t exchangeSD(uint8_t start)
{
	uint8_t err = 0;

	if (EJECT || start) {
		buffer2.smart.img[0].valid = 0;
		SD_p.inited = 0;
	}
	if (!SD_detect(start)) return 0;

	FILE_open(&buffer2.smart.img[0], 0, 0, "PO NIC", &err);
	return 1;
}

int main(void)
{
	COMMON_init();
	exchangeSD(1);
	sei();

	if (memcmp(buffer2.smart.img[0].name+8, "PO ", 3)==0) {
		mode = SMARTMODE;
		SMART_init();
		SMART_run(exchangeSD);
	} else if (memcmp(buffer2.smart.img[0].name+8, "NIC", 3)==0) {
		mode = DISK2MODE;
		DISK2_init();
		DISK2_run(exchangeSD);
	}
}
