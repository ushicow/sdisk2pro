/*-------------------------------

	DISK II Emulator (2 of 2)

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

#ifdef SDISK2P
#define RDR	_SFR_IO_ADDR(PORTC),4
#else
#define RDR VPORT2_OUT,PIN0_bp
#endif

.global DISK2_prepare
.global DISK2_doBuffering
.global DISK2_writePtr
.global DISK2_readPulse
.global DISK2_posBit
.global DISK2_ptrByte
.global DISK2_byteData
.global buffer1

#ifdef SDISK2P
.global __vector_14
.global __vector_1
#else
.global TCC4_OVF_vect
.global PORTD_INT_vect
#endif

#ifdef SDISK2P
__vector_14:
#else
TCC4_OVF_vect:
#endif
	push	r18
	in		r18,_SFR_IO_ADDR(SREG)
	push	r18

#ifndef SDISK2P
	ldi		r18,0b00000001
	sts		TCC4_INTFLAGS, r18
#endif

	; prepare check
	lds		r18,DISK2_prepare
	cpi		r18,1
	brne	TC_NOSKIP

	pop		r18
	out		_SFR_IO_ADDR(SREG),r18	
	pop		r18
	reti

TC_NOSKIP:
	; EN1/EN2 check

#ifdef SDISK2P
	sbic	_SFR_IO_ADDR(PIND),7
	rjmp	TC_SKIP
#else
	in		r18,VPORT2_IN
	andi	r18,(PIN5_bm|PIN6_bm)
	cpi		r18,(PIN5_bm|PIN6_bm)
	breq	TC_SKIP
#endif

	lds		r18,DISK2_readPulse

	sbrc	r18,0				; 1/2/3
	sbi		RDR					; X1M2

	push	R19					; X1M2
	push	R26					; X1M2
	push	R27					; X1M2

	nop
#ifdef SDISK2P
	ldi		r26,5
#else
	ldi		r26,8
#endif
1:
	dec		r26
	brne	1b

	cbi		RDR					; X1M2

	lds		r18,DISK2_posBit
	dec		r18
	sts		DISK2_posBit,r18

	brne	ROLBYTE

	ldi		r18,8
	sts		DISK2_posBit,r18

	lds		r27,DISK2_ptrByte+1
	lds		r26,DISK2_ptrByte

	ld		r18,X
	sts		DISK2_byteData,r18

	adiw	r26,1
	sts		DISK2_ptrByte+1,r27
	sts		DISK2_ptrByte,r26
	
	cpi		r27,hi8(buffer1+412)
	brne	ROLBYTE
	cpi		r26,lo8(buffer1+412)
	brne	ROLBYTE

	ldi		r18,1
	sts		DISK2_prepare,r18

	ldi		r18,lo8(buffer1)
	sts		DISK2_ptrByte,r18
	ldi		r18,hi8(buffer1)
	sts		DISK2_ptrByte+1,r18

ROLBYTE:
	lds		r18, DISK2_byteData
	ldi		r19,0
	rol		r18
	sts		DISK2_byteData,r18

	rol		r19

	sts		DISK2_readPulse,r19
	pop		R27
	pop		R26
	pop		r19
TC_SKIP:
	pop		r18
	out		_SFR_IO_ADDR(SREG),r18	
	pop		r18
	reti

; WREQ interrupt

#ifdef SDISK2P
__vector_1:
#else
PORTD_INT_vect:
#endif

	push	r18
	in		r18, _SFR_IO_ADDR(SREG)
	push	r18

	ldi r18,0
	sts DISK2_magState,r18
	
#ifdef SDISK2P
	sbic	_SFR_IO_ADDR(PIND),7
	rjmp	NOT_ENABLE
#else
	in		r18,VPORT2_IN
	andi	r18,(PIN5_bm|PIN6_bm)
	cpi		r18,(PIN5_bm|PIN6_bm)
	brne	ENABLE
	jmp		NOT_ENABLE
ENABLE:

#endif

	push	r19						; X1M2
	lds		r19,DISK2_magState		; 2

	; wait start bit 1
1:
#ifdef SDISK2P
	in		r18,_SFR_IO_ADDR(PINB)	; 1
	andi	r18,2					; 1
#else
	in		r18,VPORT0_IN			; 1
	andi	r18,64					; 1
#endif
	eor		r18,r19					; 1
	breq	1b						; 2/1

	; from here 192

#ifdef SDISK2P
	in		r18,_SFR_IO_ADDR(PINB)
	andi	r18,2
#else
	in		r18,VPORT0_IN			; 1
	andi	r18,64					; 1
#endif

	sts		DISK2_magState,r18		; X3M2

#ifdef SDISP2P
	ldi		r18,8					; 1   *
#else
	ldi		r18, 16					; 1   *
#endif

1:									;     * 48
	dec		r18						; 1	  *
	brne	1b						; 1/2 *

	nop								; 1

	push	r20						; X1M2
	push	r21						; X1M2
	push	r22						; X1M2
	push	r23						; X1M2
	push	r24						; X1M2
	push	r30						; X1M2
	push	r31						; X1M2
	ldi		r22,0					; 1 start storing
	lds		r30,(DISK2_writePtr)	; X3M2
	lds		r31,(DISK2_writePtr+1)	; X3M2
	ldi		r19,lo8(349)			; 1
	ldi		r20,hi8(349)			; 1 
	rjmp	ENTR					; 2

WLP2:
	lds		r21,DISK2_magState		; X3M2

	; wait start bit 1
1:
#ifdef SDISK2P
	in		r23,_SFR_IO_ADDR(PINB)	; 1
	andi	r23,2					; 1
#else
	in		r23,VPORT0_IN			; 1
	andi	r23,64					; 1
#endif

	eor		r23,r21					; 1
	breq	1b						; 2/1

	; from here 192 clock

#ifdef SDISK2P
	in		r23,_SFR_IO_ADDR(PINB)	; 1
	andi	r23,2					; 1
#else
	in		r23,VPORT0_IN			; 1
	andi	r23,64					; 1
#endif

	sts		DISK2_magState,r23		; X3M2

#ifdef SDISK2P
	ldi		r23, 	14				; 1 *
#else
	ldi		r23, 	24				; 1 *
#endif
1:									;   * 72
	dec		r23						; 1 *
	brne	1b						; 2 *

ENTR:
	ldi		r18,7					; 1
	ldi		r24,1					; 1

	; loop : 128 clock
WLP1:

#ifdef SDISK2P
	in		r23,_SFR_IO_ADDR(PIND)	; 1
#else
	in		r23,VPORT2_IN			; 1
#endif

	andi	r23,4					; 1
	brne	WRITE_END				; 1
		
	nop								; 1

#ifdef SDISK2P
	ldi		r23, 30					; 1
#else
	nop								; 1
	ldi		r23, /*36 adjust */35	; 1 *
#endif
1:									;   * 108
	dec		r23						; 1 *
	brne	1b						; 2 *

#ifdef SDISK2P
	in		r23,_SFR_IO_ADDR(PINB)
	andi	r23,2
#else
	in		r23,VPORT0_IN			; 1
	andi	r23,64					; 1
#endif

	lds		r21,DISK2_magState		; 2
	sts		DISK2_magState,r23		; 2
	eor		r23,r21					; 1
	lsr		r23						; 1
#ifdef SDISK2P
	nop								; 1
#else
	lsr		r23						; 1
	swap	r23						; 1
#endif

	lsl		r24						; 1
	or		r24,r23					; 1
	dec		r18						; 1
	brne	WLP1					; 2/1

	; detect first D5

	cpi		r24,0xD5				; 1
	brne	NOT_START				; 2/1
	ldi		r22,1					; 1
NOT_START:
	cpi		r22,0					; 1
	breq	WLP2					; 2/1

	st		Z+,r24					; X1M2
	subi	r19,1					; 1
	sbci	r20,0					; 1
	brne	WLP2					; 2/1

WRITE_END:
	ldi		r21,1
	sts		DISK2_doBuffering,r21
	pop		r31
	pop		r30
	pop		r24
	pop		r23
	pop		r22
	pop		r21
	pop		r20
	pop		r19

NOT_ENABLE:

#ifndef SDISK2P
	sbi VPORT2_INTFLAGS, PIN2_bp
#endif

	pop		r18
	out		_SFR_IO_ADDR(SREG), r18	
	pop		r18
	reti