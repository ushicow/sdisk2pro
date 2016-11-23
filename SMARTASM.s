
/*
 * UNIDISKASM.s
 *
 * Apple // Smartport I/F
 * Original Written by Robert Justice
 * Ported to SDISK2P/UNISDISK by Koichi Nishida
 *
 * Created: 2013/10/17 19:49:33
 *  Author: Koichi Nishida
 */ 
 
#include <avr/io.h>				

#ifdef SDISK2P
#define ACK _SFR_IO_ADDR(PORTD),4
#define REQ _SFR_IO_ADDR(PINC),0
#define WDAT _SFR_IO_ADDR(PINB),1
#define RDR _SFR_IO_ADDR(PORTC),4
#else
#define ACK VPORT2_OUT,PIN7_bp
#define REQ VPORT0_IN,PIN0_bp
#define WDAT VPORT0_IN,6
#define RDR VPORT2_OUT,PIN0_bp
#endif

// receive SmartPort packet
.global SMART_ReceivePacket
                         ;XMEGA  ;MEGA  ;CYCLES
SMART_ReceivePacket:
          mov  XL,r24                   ;mov buffer pointer into X 
          mov  XH,r25

          ldi  r24,0
		  ldi  r25,0
start:	  
          ldi  r22,1                    ;1   remember tx line status when previously sampled
          sbis WDAT       ;0     ;0     ;X2/3M1/2 wait for txd line to go low
          rjmp 2f         ;2     ;2     ;2   txd cleared, start of packet data

		  sbiw r24,1
		  brne start
		  clr  r25                      ;return 1
          ldi  r24,1
          ret

#ifdef SDISK2P
2:        ldi  r24,16            ;4     ;1   wait for half a bit, 2us (54 cycles total)
3:        dec  r24                      ;1   this is so we sample mid point 
          brne 3b                       ;1/2
#else
2:        ldi  r24,19;22  ;4            ;1   wait for half a bit, 2us (64 cycles total)
3:        dec  r24                      ;1   this is so we sample mid point 
          brne 3b                       ;1/2
          nop             ;61           ;1
          nop             ;62           ;1
#endif
 
nxtbyte:                                ;    full cycle time for each byte is 32us
          ldi  r25,8      ;0     ;0     ;1   8bits to read

nxtbit:	  sbic WDAT       ;1     ;1     ;X2/3M1/2 now read a bit, cycle time is 4us
          rjmp bitset     ;3     ;2     ;2   bit is set
          rjmp bitclr     ;4     ;3     ;2   bit is clr
bitset:   sbrc r22,0      ;5     ;4     ;1/2 test previous bit recv'd
          rjmp carryclr   ;6     ;5     ;2   bit set, then we have a zero
          ldi  r22,1      ;7     ;6     ;1   remember prev tx bit is set
     	  sec             ;8     ;7     ;1   else we have a one
          nop             ;9     ;8     ;1
          nop             ;10    ;9     ;1
          rjmp loadbit    ;11    ;10    ;2
bitclr:   sbrc r22,0      ;6     ;5     ;1/2 test previous bit recv'd
          rjmp carryset   ;7     ;6     ;2   bit set, then we have a one
          ldi  r22,0      ;8     ;7     ;1   remember prev tx bit is clr
          clc             ;9     ;8     ;1   else we have a zero
          nop             ;10    ;9     ;1
	      rjmp loadbit    ;11    ;10    ;2
carryset: ldi  r22,0      ;9     ;8     ;1
          sec             ;10    ;9     ;1   remember prev tx bit is clr
          rjmp loadbit    ;11    ;10    ;2
carryclr: ldi  r22,1      ;8     ;7     ;1   remember prev tx bit is set
          clc             ;9     ;8     ;1
          nop             ;10    ;9     ;1
          nop             ;11    ;10    ;1
          nop             ;12    ;11    ;1

loadbit:  rol  r23        ;13    ;12    ;1   shift bit(carry) into r23
          dec  r25        ;14    ;13    ;1   dec bit counter
          breq havebyte   ;15    ;14    ;1/2 

#ifdef SDISK2P
          nop                    ;15    ;1
          nop                    ;16    ;1
          ldi  r24,30     ;16    ;17    ;1   delay to make up the rest of the 4us
#else
          ldi  r24,36 ;37 ;16           ;1   delay to make up the rest of the 4us (adjust)
#endif
3:        dec  r24                      ;1   
          brne 3b                       ;1/2

          rjmp nxtbit     ;127   ;107   ;2   get next bit	

havebyte:
          st   x+,r23	  ;17    ;16    ;X1M2   save byte in buffer
          ldi  r25,100    ;18    ;18    ;1   timeout counter if we are at the end
          cpi  r22,1      ;19    ;19    ;1   check for status of last bit    
          breq wasset     ;20    ;20    ;1/2
wasclr:   sbic WDAT       ;0     ;0     ;X2/3M1/2 now read a bit, wait for transition to 1         
          rjmp havesbit   ;2     ;1     ;2   now set, lets get the next byte
          dec  r25        ;3     ;2     ;1
          breq endpkt     ;4     ;3     ;1/2 we have timed out, must be end of packet
          rjmp wasclr     ;5     ;4     ;2   lets test again
          
wasset:   sbis WDAT       ;0     ;0     ;X2/3M1/2 now read a bit, wait for transition to 0         
          rjmp havesbit   ;2     ;1     ;2   now clr, lets get the next byte
          dec  r25        ;3     ;2     ;1
          breq endpkt     ;4     ;3     ;1/2 we have timed out, must be end of packet
          rjmp wasset     ;5     ;4     ;2   lets test again

#ifdef SDISK2P
havesbit: ldi  r24,16            ;3     ;1   wait for half a bit, 2us (54 cycles total)
#else    
havesbit: ldi  r24,19     ;4            ;1   wait for half a bit, 2us (64 cycles total)
#endif
3:        dec  r24        ;             ;1   this is so we sample mid point 
          brne 3b         ;             ;1/2

          rjmp nxtbyte    ;61    ;51    ;2   get next byte

endpkt:   clr  r23        ;27
          st   x+,r23	                ;save zero byte in buffer to mark end
          
          cbi  ACK                      ;set ACK(BSY) low to signal we have recv'd the pkt
          
1:        sbis REQ                      ;wait for REQ line to go low
          rjmp finish                   ;this indicates host has acknowledged ACK   
          rjmp 1b
          
finish:
          clr  r25                      ;return no error (for now)
          clr  r24
          ret

// send SmartPort packet
.global SMART_SendPacket
SMART_SendPacket:
          mov  XL,r24                    ;mov buffer pointer into X 
          mov  XH,r25

          sbi  ACK                       ;set ACK high to signal we are ready to send
         
1:        sbic REQ                       ;wait for req line to go high
          rjmp contin                    ;this indicates host is ready to receive packet   
          rjmp 1b
contin:
                          ;XMEGA  ;MEGA  ;CYCLES

nxtsbyte: ld   r23,x+     ;0      ;0     ;2   get first byte from buffer
          cpi  r23,0      ;2      ;2     ;1   zero marks end of data
          breq endspkt    ;3      ;3     ;1/2

          ldi  r25,8      ;4      ;4     ;1   8bits to read

nxtsbit:  sbrs r23,7      ;5      ;5     ;1/2 send bit 7 first       
          rjmp sbitclr    ;6      ;6     ;2   bit is clear
          sbi  RDR        ;7      ;7     ;X1M2   set bit for 1us (32/27 cycles)

#ifdef SDISK2P
          ldi  r24,8      ;       ;9     ;1   delay
#else
          ldi  r24,10     ;8             ;1   delay
#endif
3:        dec  r24                       ;1    
          brne 3b                        ;1/2

		  nop             ;38     ;33    ;1
          cbi  RDR        ;39     ;34    ;X1M2   clr bit for 3us
          dec  r25        ;40     ;36    ;1   dec bit counter
          breq nxtsbyt1   ;41     ;37    ;1/2
          rol  r23        ;42     ;38    ;1

#ifdef SDISK2P
          ldi  r24,24     ;       ;39    ;1   delay
#else
          nop             ;43
          ldi  r24,29     ;44            ;1   delay
#endif
3:        dec  r24                       ;1    
          brne 3b                        ;1/2
          rjmp nxtsbit    ;131    ;111   ;2

#ifdef SDISK2P
nxtsbyt1: ldi  r24,22     ;       ;39    ;1   delay to makeup 3us (81 cycles total)
#else
nxtsbyt1: ldi  r24,26 ;27 ;43            ;1   delay to makeup 3us (96 cycles total) (adjust)
          nop             ;44            ;1
		  nop ;(adjust)
#endif
3:        dec  r24                       ;1    
          brne 3b                        ;1/2

          nop             ;125    ;105   ;1
          rjmp nxtsbyte   ;126    ;106   ;2

; bit is clr, we need to check if its the last one, otherwise delay for 4us before next bit
sbitclr:  dec  r25        ;8      ;8     ;1   
          breq nxtsbycl   ;9      ;9     ;1/2 end of byte, delay then get nxt
          rol  r23        ;10     ;10    ;1

#ifdef SDISK2P
          ldi  r24,33     ;       ;11
		  nop             ;       ;12
#else
          ldi  r24,39 ;40 ;11            ;1   delay to makeup 4us (128 cycles total) (adjust)
#endif

3:        dec  r24                       ;1    
          brne 3b                        ;1/2

          rjmp nxtsbit    ;131    ;111   ;2
          
#ifdef SDISK2P
nxtsbycl: ldi  r24,31     ;       ;11    ;1
          nop             ;       ;12    ;1
#else
nxtsbycl: ldi  r24,38     ;11            ;1   delay to makeup 4us (128 cycles total)
#endif

3:        dec  r24                       ;1    
          brne 3b                        ;1/2

          nop             ;125    ;105   ;1
          rjmp nxtsbyte   ;126    ;106   ;2

endspkt:  
          cbi  ACK                       ;set ACK high to signal we are ready to send
1:        sbis REQ                       ;wait for REQ line to go low
          rjmp finishs                   ;this indicates host has acknowledged ACK   
          rjmp 1b

finishs:
          clr  r25
          clr  r24                       ;return no error
          ret