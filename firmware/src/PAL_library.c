/*
 * file         : PAL_library.c
 * project      : PIC PAL SOFTWARE VIDEO GENERATOR
 * author       : Bruno Gavand
 * compiler     : mikroC V6.2
 * date         : January 17, 2006
 * updated      : April 30, 2008
 *                      minor adjustment for mikroC V8.1
 *
 * description  :
 *      PAL library for PIC18 MCU
 *      This library turns your PIC18 into a B&W PAL software video generator
 *
 * schematic :
                              ___
        RB0 (LUM)        +---|___|---+
                            100     |
                               ____   |
        RE0 (SYNC)        +---|____|--+--------> VIDEO OUT (to TV video input)
                            1K

        GND                 +--------------------> GND

 *
 * target devices :
 *      Tested with PIC18F452 and PIC18F4620 @ 32 Mhz (8 Mhz crystal + HS PLL)
 *
 * Licence :
 *      Feel free to use this source code at your own risks.
 *
 * history :
 *
 * see more details on http://www.micro-examples.com/
 */

#include        "PAL_Library.h"

/*
 * I/O definition
 */
#define PAL_BSYNC   PORTE.F0    // bit for synchronization
#define PAL_BVID    PORTD.F0    // bit for luminance. Note : full PORT is reserved !

#define PAL_DELAY4  1           // real delay in microsecond for 4 �s pulse
#define PAL_DELAY28 20          // real delay in microsecond for 28 �s pulse

/********************
 * MACRO DEFINITIONS
 ********************/

/*
 * create picture border
 */
#define PAL_MAKE_BORDER asm MOVFF _PAL_border, PORTD

/*
 * header to load first group of 8 pixels
 */
#define PAL_BLOCK_HEAD\
                        asm { MOVFF        PAL_ISR_ptr_L2, FSR0L }\
                        asm { MOVFF        PAL_ISR_ptr_L2+1, FSR0H }\
                        asm { MOVFF        POSTINC0, PORTD }

/*
 * process 8 pixels
 * pre-fetch next 8 pixels
 */
#define PAL_BLOCK_SHIFT\
                        asm { INFSNZ        PAL_ISR_ptr_L2, 1, 1 }\
                        asm { INCF        PAL_ISR_ptr_L2+1, 1, 1 }\
                        asm { RRCF        PORTD, 1, 0 }\
                        asm { MOVFF        PAL_ISR_ptr_L2, FSR0L }\
                        asm { RRCF        PORTD, 1, 0 }\
                        asm { MOVFF        PAL_ISR_ptr_L2+1, FSR0H }\
                        asm { RRCF        PORTD, 1, 0 }\
                        asm { nop }\
                        asm { RRCF        PORTD, 1, 0 }\
                        asm { nop }\
                        asm { RRCF        PORTD, 1, 0 }\
                        asm { nop }\
                        asm { RRCF        PORTD, 1, 0 }\
                        asm { nop }\
                        asm { RRCF        PORTD, 1, 0 }\
                        asm { nop }

/*
 * load 8 pixel bloc
 */
#define PAL_BLOCK_LOAD\
                        asm { MOVFF        POSTINC0, PORTD }

/*
 * set video signal to SYNC or BLACK for d �s
 */
#define PAL_HSYNC(d)        PAL_BVID = 0 ; PAL_BSYNC = 0 ; Delay_us(d)
#define PAL_BLACK(d)        PAL_BVID = 0 ; PAL_BSYNC = 1 ; Delay_us(d)

/*                                                                       g
 * vertical sync line is made of 4 levels of UP and LOW pulses of 28 and 4 �s
 */
#define PAL_LOW28   0b00
#define PAL_UP28    0b01
#define PAL_LOW4    0b10
#define PAL_UP4     0b11

/*
 * a vertical sync line is coded on one byte
 */
#define PAL_P1(x)   (x)
#define PAL_P2(x)   (x << 2)
#define PAL_P3(x)   (x << 4)
#define PAL_P4(x)   (x << 6)

/*
 * vertical sync lines codification
 */
#define PAL_L1      PAL_P1(PAL_LOW4)  | PAL_P2(PAL_UP28) | PAL_P3(PAL_LOW4)  | PAL_P4(PAL_UP28)
#define PAL_L2      PAL_P1(PAL_LOW4)  | PAL_P2(PAL_UP28) | PAL_P3(PAL_LOW4)  | PAL_P4(PAL_UP28)
#define PAL_L3      PAL_P1(PAL_LOW4)  | PAL_P2(PAL_UP28) | PAL_P3(PAL_LOW4)  | PAL_P4(PAL_UP28)
#define PAL_L4      PAL_P1(PAL_LOW28) | PAL_P2(PAL_UP4)  | PAL_P3(PAL_LOW28) | PAL_P4(PAL_UP4)
#define PAL_L5      PAL_P1(PAL_LOW28) | PAL_P2(PAL_UP4)  | PAL_P3(PAL_LOW28) | PAL_P4(PAL_UP4)
#define PAL_L6      PAL_P1(PAL_LOW28) | PAL_P2(PAL_UP4)  | PAL_P3(PAL_LOW4)  | PAL_P4(PAL_UP28)
#define PAL_L7      PAL_P1(PAL_LOW4)  | PAL_P2(PAL_UP28) | PAL_P3(PAL_LOW4)  | PAL_P4(PAL_UP28)
#define PAL_L8      PAL_P1(PAL_LOW4)  | PAL_P2(PAL_UP28) | PAL_P3(PAL_LOW4)  | PAL_P4(PAL_UP28)

#define PAL_L311    PAL_P1(PAL_LOW4)  | PAL_P2(PAL_UP28) | PAL_P3(PAL_LOW4)  | PAL_P4(PAL_UP28)
#define PAL_L312    PAL_P1(PAL_LOW4)  | PAL_P2(PAL_UP28) | PAL_P3(PAL_LOW4)  | PAL_P4(PAL_UP28)
#define PAL_L313    PAL_P1(PAL_LOW4)  | PAL_P2(PAL_UP28) | PAL_P3(PAL_LOW28) | PAL_P4(PAL_UP4)
#define PAL_L314    PAL_P1(PAL_LOW28) | PAL_P2(PAL_UP4)  | PAL_P3(PAL_LOW28) | PAL_P4(PAL_UP4)
#define PAL_L315    PAL_P1(PAL_LOW28) | PAL_P2(PAL_UP4)  | PAL_P3(PAL_LOW28) | PAL_P4(PAL_UP4)
#define PAL_L316    PAL_P1(PAL_LOW4)  | PAL_P2(PAL_UP28) | PAL_P3(PAL_LOW4)  | PAL_P4(PAL_UP28)
#define PAL_L317    PAL_P1(PAL_LOW4)  | PAL_P2(PAL_UP28) | PAL_P3(PAL_LOW4)  | PAL_P4(PAL_UP28)

/*
 * these info lines are not visible on screen
 */
#define PAL_LINFO   PAL_P1(PAL_LOW4)  | PAL_P2(PAL_UP4)  | PAL_P3(PAL_UP28)  | PAL_P4(PAL_UP28)

/*
 * this is the code of a visible video line
 */
#define PAL_LVIDEO  0

/*
 * now, the full table of a 625 lines frame
 * it does not start with first sync line for my own convenience (spares a test in ISR)
 */
const unsigned char   PAL_ltype[] =
        {
        PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO,
        PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO,

        PAL_LVIDEO, // 286 times

        // synchro 311 to 317
        PAL_L311, PAL_L312, PAL_L313, PAL_L314, PAL_L315, PAL_L316, PAL_L317,

        PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO,
        PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO, PAL_LINFO,

        PAL_LVIDEO, // 286 times

        // synchro 1 to 8
        PAL_L1, PAL_L2, PAL_L3, PAL_L4, PAL_L5, PAL_L6, PAL_L7, PAL_L8
        } ;

/***************
 * RAM variables
 ***************/
const   char    *PAL_linePtr = PAL_ltype ;      // pointer to the current line type
unsigned char   PAL_lineIdx = 0 ;               // index of the current line type
unsigned char   PAL_lineCode = 0 ;              // current line code
unsigned int    PAL_lineVideo = 0 ;             // current visible video line number
unsigned int    PAL_border = 0 ;                // border flag : 1=white border, 0=black border
unsigned char   PAL_y ;                         // number of vertical pixels
unsigned int    PAL_max_y ;                     // last video line number
unsigned char   PAL_shift_y ;                   // first video line number (vertical centering)
unsigned char   PAL_render ;                    // start rendering flag
unsigned long   PAL_frameCtr = 0 ;              // frame counter, 25 per second

/*****************
 * ROM tables
 *****************/
/*
 * TERMINAL fonts
 * 6x8 ASCII character table, starts from 0 (32 = space) up to 255
 *
 */
const unsigned char     PAL_charTable[] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x3E, 0x5B, 0x4F, 0x5B, 0x3E, 0x00,
                0x3E, 0x6B, 0x4F, 0x6B, 0x3E, 0x00,
                0x1C, 0x3E, 0x7C, 0x3E, 0x1C, 0x00,
                0x18, 0x3C, 0x7E, 0x3C, 0x18, 0x00,
                0x1C, 0x57, 0x7D, 0x57, 0x1C, 0x00,
                0x1C, 0x5E, 0x7F, 0x5E, 0x1C, 0x00,
                0x00, 0x18, 0x3C, 0x18, 0x00, 0x00,
                0xFF, 0xE7, 0xC3, 0xE7, 0xFF, 0x00,
                0x00, 0x18, 0x24, 0x18, 0x00, 0x00,
                0xFF, 0xE7, 0xDB, 0xE7, 0xFF, 0x00,
                0x30, 0x48, 0x3A, 0x06, 0x0E, 0x00,
                0x26, 0x29, 0x79, 0x29, 0x26, 0x00,
                0x40, 0x7F, 0x05, 0x05, 0x07, 0x00,
                0x40, 0x7F, 0x05, 0x25, 0x3F, 0x00,
                0x5A, 0x3C, 0xE7, 0x3C, 0x5A, 0x00,
                0x7F, 0x3E, 0x1C, 0x1C, 0x08, 0x00,
                0x08, 0x1C, 0x1C, 0x3E, 0x7F, 0x00,
                0x14, 0x22, 0x7F, 0x22, 0x14, 0x00,
                0x5F, 0x5F, 0x00, 0x5F, 0x5F, 0x00,
                0x06, 0x09, 0x7F, 0x01, 0x7F, 0x00,
                0x00, 0x66, 0x89, 0x95, 0x6A, 0x00,
                0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
                0x94, 0xA2, 0xFF, 0xA2, 0x94, 0x00,
                0x08, 0x04, 0x7E, 0x04, 0x08, 0x00,
                0x10, 0x20, 0x7E, 0x20, 0x10, 0x00,
                0x08, 0x08, 0x2A, 0x1C, 0x08, 0x00,
                0x08, 0x1C, 0x2A, 0x08, 0x08, 0x00,
                0x1E, 0x10, 0x10, 0x10, 0x10, 0x00,
                0x0C, 0x1E, 0x0C, 0x1E, 0x0C, 0x00,
                0x30, 0x38, 0x3E, 0x38, 0x30, 0x00,
                0x06, 0x0E, 0x3E, 0x0E, 0x06, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x5F, 0x00, 0x00, 0x00,
                0x00, 0x07, 0x00, 0x07, 0x00, 0x00,
                0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00,
                0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00,
                0x23, 0x13, 0x08, 0x64, 0x62, 0x00,
                0x36, 0x49, 0x56, 0x20, 0x50, 0x00,
                0x00, 0x08, 0x07, 0x03, 0x00, 0x00,
                0x00, 0x1C, 0x22, 0x41, 0x00, 0x00,
                0x00, 0x41, 0x22, 0x1C, 0x00, 0x00,
                0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x00,
                0x08, 0x08, 0x3E, 0x08, 0x08, 0x00,
                0x00, 0x80, 0x70, 0x30, 0x00, 0x00,
                0x08, 0x08, 0x08, 0x08, 0x08, 0x00,
                0x00, 0x00, 0x60, 0x60, 0x00, 0x00,
                0x20, 0x10, 0x08, 0x04, 0x02, 0x00,
                0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00,
                0x00, 0x42, 0x7F, 0x40, 0x00, 0x00,
                0x72, 0x49, 0x49, 0x49, 0x46, 0x00,
                0x21, 0x41, 0x49, 0x4D, 0x33, 0x00,
                0x18, 0x14, 0x12, 0x7F, 0x10, 0x00,
                0x27, 0x45, 0x45, 0x45, 0x39, 0x00,
                0x3C, 0x4A, 0x49, 0x49, 0x31, 0x00,
                0x41, 0x21, 0x11, 0x09, 0x07, 0x00,
                0x36, 0x49, 0x49, 0x49, 0x36, 0x00,
                0x46, 0x49, 0x49, 0x29, 0x1E, 0x00,
                0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
                0x00, 0x40, 0x34, 0x00, 0x00, 0x00,
                0x00, 0x08, 0x14, 0x22, 0x41, 0x00,
                0x14, 0x14, 0x14, 0x14, 0x14, 0x00,
                0x00, 0x41, 0x22, 0x14, 0x08, 0x00,
                0x02, 0x01, 0x59, 0x09, 0x06, 0x00,
                0x3E, 0x41, 0x5D, 0x59, 0x4E, 0x00,
                0x7C, 0x12, 0x11, 0x12, 0x7C, 0x00,
                0x7F, 0x49, 0x49, 0x49, 0x36, 0x00,
                0x3E, 0x41, 0x41, 0x41, 0x22, 0x00,
                0x7F, 0x41, 0x41, 0x41, 0x3E, 0x00,
                0x7F, 0x49, 0x49, 0x49, 0x41, 0x00,
                0x7F, 0x09, 0x09, 0x09, 0x01, 0x00,
                0x3E, 0x41, 0x41, 0x51, 0x73, 0x00,
                0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00,
                0x00, 0x41, 0x7F, 0x41, 0x00, 0x00,
                0x20, 0x40, 0x41, 0x3F, 0x01, 0x00,
                0x7F, 0x08, 0x14, 0x22, 0x41, 0x00,
                0x7F, 0x40, 0x40, 0x40, 0x40, 0x00,
                0x7F, 0x02, 0x1C, 0x02, 0x7F, 0x00,
                0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00,
                0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00,
                0x7F, 0x09, 0x09, 0x09, 0x06, 0x00,
                0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00,
                0x7F, 0x09, 0x19, 0x29, 0x46, 0x00,
                0x26, 0x49, 0x49, 0x49, 0x32, 0x00,
                0x03, 0x01, 0x7F, 0x01, 0x03, 0x00,
                0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00,
                0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00,
                0x3F, 0x40, 0x38, 0x40, 0x3F, 0x00,
                0x63, 0x14, 0x08, 0x14, 0x63, 0x00,
                0x03, 0x04, 0x78, 0x04, 0x03, 0x00,
                0x61, 0x59, 0x49, 0x4D, 0x43, 0x00,
                0x00, 0x7F, 0x41, 0x41, 0x41, 0x00,
                0x02, 0x04, 0x08, 0x10, 0x20, 0x00,
                0x00, 0x41, 0x41, 0x41, 0x7F, 0x00,
                0x04, 0x02, 0x01, 0x02, 0x04, 0x00,
                0x40, 0x40, 0x40, 0x40, 0x40, 0x00,
                0x00, 0x03, 0x07, 0x08, 0x00, 0x00,
                0x20, 0x54, 0x54, 0x78, 0x40, 0x00,
                0x7F, 0x28, 0x44, 0x44, 0x38, 0x00,
                0x38, 0x44, 0x44, 0x44, 0x28, 0x00,
                0x38, 0x44, 0x44, 0x28, 0x7F, 0x00,
                0x38, 0x54, 0x54, 0x54, 0x18, 0x00,
                0x00, 0x08, 0x7E, 0x09, 0x02, 0x00,
                0x18, 0xA4, 0xA4, 0x9C, 0x78, 0x00,
                0x7F, 0x08, 0x04, 0x04, 0x78, 0x00,
                0x00, 0x44, 0x7D, 0x40, 0x00, 0x00,
                0x20, 0x40, 0x40, 0x3D, 0x00, 0x00,
                0x7F, 0x10, 0x28, 0x44, 0x00, 0x00,
                0x00, 0x41, 0x7F, 0x40, 0x00, 0x00,
                0x7C, 0x04, 0x78, 0x04, 0x78, 0x00,
                0x7C, 0x08, 0x04, 0x04, 0x78, 0x00,
                0x38, 0x44, 0x44, 0x44, 0x38, 0x00,
                0xFC, 0x18, 0x24, 0x24, 0x18, 0x00,
                0x18, 0x24, 0x24, 0x18, 0xFC, 0x00,
                0x7C, 0x08, 0x04, 0x04, 0x08, 0x00,
                0x48, 0x54, 0x54, 0x54, 0x24, 0x00,
                0x04, 0x04, 0x3F, 0x44, 0x24, 0x00,
                0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00,
                0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00,
                0x3C, 0x40, 0x30, 0x40, 0x3C, 0x00,
                0x44, 0x28, 0x10, 0x28, 0x44, 0x00,
                0x4C, 0x90, 0x90, 0x90, 0x7C, 0x00,
                0x44, 0x64, 0x54, 0x4C, 0x44, 0x00,
                0x00, 0x08, 0x36, 0x41, 0x00, 0x00,
                0x00, 0x00, 0x77, 0x00, 0x00, 0x00,
                0x00, 0x41, 0x36, 0x08, 0x00, 0x00,
                0x02, 0x01, 0x02, 0x04, 0x02, 0x00,
                0x3C, 0x26, 0x23, 0x26, 0x3C, 0x00,
                0x1E, 0xA1, 0xA1, 0x61, 0x12, 0x00,
                0x3A, 0x40, 0x40, 0x20, 0x7A, 0x00,
                0x38, 0x54, 0x54, 0x55, 0x59, 0x00,
                0x21, 0x55, 0x55, 0x79, 0x41, 0x00,
                0x21, 0x54, 0x54, 0x78, 0x41, 0x00,
                0x21, 0x55, 0x54, 0x78, 0x40, 0x00,
                0x20, 0x54, 0x55, 0x79, 0x40, 0x00,
                0x0C, 0x1E, 0x52, 0x72, 0x12, 0x00,
                0x39, 0x55, 0x55, 0x55, 0x59, 0x00,
                0x39, 0x54, 0x54, 0x54, 0x59, 0x00,
                0x39, 0x55, 0x54, 0x54, 0x58, 0x00,
                0x00, 0x00, 0x45, 0x7C, 0x41, 0x00,
                0x00, 0x02, 0x45, 0x7D, 0x42, 0x00,
                0x00, 0x01, 0x45, 0x7C, 0x40, 0x00,
                0xF0, 0x29, 0x24, 0x29, 0xF0, 0x00,
                0xF0, 0x28, 0x25, 0x28, 0xF0, 0x00,
                0x7C, 0x54, 0x55, 0x45, 0x00, 0x00,
                0x20, 0x54, 0x54, 0x7C, 0x54, 0x44,
                0x7C, 0x0A, 0x09, 0x7F, 0x49, 0x41,
                0x32, 0x49, 0x49, 0x49, 0x32, 0x00,
                0x32, 0x48, 0x48, 0x48, 0x32, 0x00,
                0x32, 0x4A, 0x48, 0x48, 0x30, 0x00,
                0x3A, 0x41, 0x41, 0x21, 0x7A, 0x00,
                0x3A, 0x42, 0x40, 0x20, 0x78, 0x00,
                0x00, 0x9D, 0xA0, 0xA0, 0x7D, 0x00,
                0x39, 0x44, 0x44, 0x44, 0x39, 0x00,
                0x3D, 0x40, 0x40, 0x40, 0x3D, 0x00,
                0x3C, 0x24, 0xFF, 0x24, 0x24, 0x00,
                0x48, 0x7E, 0x49, 0x43, 0x66, 0x00,
                0x2B, 0x2F, 0xFC, 0x2F, 0x2B, 0x00,
                0xFF, 0x09, 0x29, 0xF6, 0x20, 0x00,
                0xC0, 0x88, 0x7E, 0x09, 0x03, 0x00,
                0x20, 0x54, 0x54, 0x79, 0x41, 0x00,
                0x00, 0x00, 0x44, 0x7D, 0x41, 0x00,
                0x30, 0x48, 0x48, 0x4A, 0x32, 0x00,
                0x38, 0x40, 0x40, 0x22, 0x7A, 0x00,
                0x00, 0x7A, 0x0A, 0x0A, 0x72, 0x00,
                0x7D, 0x0D, 0x19, 0x31, 0x7D, 0x00,
                0x26, 0x29, 0x29, 0x2F, 0x28, 0x00,
                0x26, 0x29, 0x29, 0x29, 0x26, 0x00,
                0x30, 0x48, 0x4D, 0x40, 0x20, 0x00,
                0x38, 0x08, 0x08, 0x08, 0x08, 0x00,
                0x08, 0x08, 0x08, 0x08, 0x38, 0x00,
                0x2F, 0x10, 0xC8, 0xAC, 0xBA, 0x00,
                0x2F, 0x10, 0x28, 0x34, 0xFA, 0x00,
                0x00, 0x00, 0x7B, 0x00, 0x00, 0x00,
                0x08, 0x14, 0x2A, 0x14, 0x22, 0x00,
                0x22, 0x14, 0x2A, 0x14, 0x08, 0x00,
                0xAA, 0x00, 0x55, 0x00, 0xAA, 0x00,
                0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55,
                0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA,
                0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
                0x10, 0x10, 0x10, 0xFF, 0x00, 0x00,
                0x14, 0x14, 0x14, 0xFF, 0x00, 0x00,
                0x10, 0x10, 0xFF, 0x00, 0xFF, 0x00,
                0x10, 0x10, 0xF0, 0x10, 0xF0, 0x00,
                0x14, 0x14, 0x14, 0xFC, 0x00, 0x00,
                0x14, 0x14, 0xF7, 0x00, 0xFF, 0x00,
                0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
                0x14, 0x14, 0xF4, 0x04, 0xFC, 0x00,
                0x14, 0x14, 0x17, 0x10, 0x1F, 0x00,
                0x10, 0x10, 0x1F, 0x10, 0x1F, 0x00,
                0x14, 0x14, 0x14, 0x1F, 0x00, 0x00,
                0x10, 0x10, 0x10, 0xF0, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x1F, 0x10, 0x10,
                0x10, 0x10, 0x10, 0x1F, 0x10, 0x10,
                0x10, 0x10, 0x10, 0xF0, 0x10, 0x10,
                0x00, 0x00, 0x00, 0xFF, 0x10, 0x10,
                0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
                0x10, 0x10, 0x10, 0xFF, 0x10, 0x10,
                0x00, 0x00, 0x00, 0xFF, 0x14, 0x14,
                0x00, 0x00, 0xFF, 0x00, 0xFF, 0x10,
                0x00, 0x00, 0x1F, 0x10, 0x17, 0x14,
                0x00, 0x00, 0xFC, 0x04, 0xF4, 0x14,
                0x14, 0x14, 0x17, 0x10, 0x17, 0x14,
                0x14, 0x14, 0xF4, 0x04, 0xF4, 0x14,
                0x00, 0x00, 0xFF, 0x00, 0xF7, 0x14,
                0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
                0x14, 0x14, 0xF7, 0x00, 0xF7, 0x14,
                0x14, 0x14, 0x14, 0x17, 0x14, 0x14,
                0x10, 0x10, 0x1F, 0x10, 0x1F, 0x10,
                0x14, 0x14, 0x14, 0xF4, 0x14, 0x14,
                0x10, 0x10, 0xF0, 0x10, 0xF0, 0x10,
                0x00, 0x00, 0x1F, 0x10, 0x1F, 0x10,
                0x00, 0x00, 0x00, 0x1F, 0x14, 0x14,
                0x00, 0x00, 0x00, 0xFC, 0x14, 0x14,
                0x00, 0x00, 0xF0, 0x10, 0xF0, 0x10,
                0x10, 0x10, 0xFF, 0x10, 0xFF, 0x10,
                0x14, 0x14, 0x14, 0xFF, 0x14, 0x14,
                0x10, 0x10, 0x10, 0x1F, 0x00, 0x00,
                0x00, 0x00, 0x00, 0xF0, 0x10, 0x10,
                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
                0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
                0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                0x38, 0x44, 0x44, 0x38, 0x44, 0x00,
                0x7C, 0x2A, 0x2A, 0x3E, 0x14, 0x00,
                0x7E, 0x02, 0x02, 0x06, 0x06, 0x00,
                0x02, 0x7E, 0x02, 0x7E, 0x02, 0x00,
                0x63, 0x55, 0x49, 0x41, 0x63, 0x00,
                0x38, 0x44, 0x44, 0x3C, 0x04, 0x00,
                0x40, 0x7E, 0x20, 0x1E, 0x20, 0x00,
                0x06, 0x02, 0x7E, 0x02, 0x02, 0x00,
                0x99, 0xA5, 0xE7, 0xA5, 0x99, 0x00,
                0x1C, 0x2A, 0x49, 0x2A, 0x1C, 0x00,
                0x4C, 0x72, 0x01, 0x72, 0x4C, 0x00,
                0x30, 0x4A, 0x4D, 0x4D, 0x30, 0x00,
                0x30, 0x48, 0x78, 0x48, 0x30, 0x00,
                0xBC, 0x62, 0x5A, 0x46, 0x3D, 0x00,
                0x3E, 0x49, 0x49, 0x49, 0x00, 0x00,
                0x7E, 0x01, 0x01, 0x01, 0x7E, 0x00,
                0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x00,
                0x44, 0x44, 0x5F, 0x44, 0x44, 0x00,
                0x40, 0x51, 0x4A, 0x44, 0x40, 0x00,
                0x40, 0x44, 0x4A, 0x51, 0x40, 0x00,
                0x00, 0x00, 0xFF, 0x01, 0x03, 0x00,
                0xE0, 0x80, 0xFF, 0x00, 0x00, 0x00,
                0x08, 0x08, 0x6B, 0x6B, 0x08, 0x08,
                0x36, 0x12, 0x36, 0x24, 0x36, 0x00,
                0x06, 0x0F, 0x09, 0x0F, 0x06, 0x00,
                0x00, 0x00, 0x18, 0x18, 0x00, 0x00,
                0x00, 0x00, 0x10, 0x10, 0x00, 0x00,
                0x30, 0x40, 0xFF, 0x01, 0x01, 0x00,
                0x00, 0x1F, 0x01, 0x01, 0x1E, 0x00,
                0x00, 0x19, 0x1D, 0x17, 0x12, 0x00,
                0x00, 0x3C, 0x3C, 0x3C, 0x3C, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                } ;

/*********************
 * LIBRARY FUNCTIONS
 *********************/

/************************************
 * Interrupt service routine
 * This function MUST be included by user in the interrupt() routine
 * like this :
 * void interrupt() { PAL_ISR() } ;
 *
 * All PAL video generation is done here
 * one call is one video or vertical sync line of 64 �s
 ***********************************/
void    PAL_ISR()
        {
        if(INTCON.TMR0IF)
                {
                unsigned char d ;      // temporary register to save TABLAT

                PAL_BVID = 0 ;                  // end of previous line
                PAL_BSYNC = 0 ;

                d = TABLAT ;

                PAL_lineCode = *PAL_linePtr ;   // get curret line code

                if(PAL_lineCode == PAL_LVIDEO)  // is it a visible video line ?
                        {
                        unsigned char   *ptr ;  // temporary pointer
                        volatile char   dummy ; // dummy char to please compiler

                        /*
                         * start of video line
                         */

                        PAL_HSYNC(3) ;          // horizontal sync : low level
                        PAL_BSYNC = 1 ;         // ultra black

                        /*
                         * during ultra black level :
                         */
                        PAL_lineVideo++ ;               // next line number
                        if(PAL_lineVideo == 287)        // last line ?
                                {
                                // next step in line table
                                PAL_linePtr++ ;
                                PAL_lineIdx++ ;
                                }

                        /*
                         * points to the first 8 pixels of the line
                         */
                        ptr = PAL_screen + (((PAL_lineVideo - PAL_shift_y) & 0b11111110) << 3) ;

                        /*
                         * make left border
                         */
                        PAL_MAKE_BORDER ;

                        /*
                         * is rendering enabled and line number within limits ?
                         * if not, a border color line will be displayed
                         */
                        if(PAL_render && ((PAL_lineVideo >= PAL_shift_y) && (PAL_lineVideo < PAL_max_y)))
                                {
                                /*
                                 * yes, display one video line
                                 * one line is 16 bytes ( x 8 = 128 pixels)
                                 */
                                dummy = *ptr ;                        // dummy assignement lo let the compiler know PAL_ISR_ptr

                                PAL_BLOCK_HEAD ;                        // header

                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;      // shift bits, preload and load
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ; PAL_BLOCK_LOAD ;
                                PAL_BLOCK_SHIFT ;

                                PAL_MAKE_BORDER ;                       // make right border
                                }
                        }
                else
                        {
                        /*
                         * vertical synchronization
                         * decode the PAL_lineCode and build the 4 parts of the sync line
                         */
                        PAL_BSYNC = (PAL_lineCode & 1) ? 1 : 0 ;        // first part
                        if(PAL_lineCode & 2) Delay_us(PAL_DELAY4) ; else Delay_us(PAL_DELAY28 - 6) ;

                        PAL_linePtr++ ;
                        PAL_lineIdx++ ;
                        PAL_lineVideo = 0 ;
                        /*
                         * points to the next line type
                         */
                        if(PAL_lineIdx == sizeof(PAL_ltype))    // end of table ?
                                {
                                PAL_lineIdx = 0 ;               // back to start
                                PAL_linePtr = PAL_ltype ;
                                PAL_frameCtr++ ;
                                }

                        PAL_BSYNC = (PAL_lineCode & 4) ? 1 : 0 ;        // second part
                        if(PAL_lineCode != PAL_LINFO)
                                {
                                if(PAL_lineCode & 8) Delay_us(PAL_DELAY4) ; else Delay_us(PAL_DELAY28) ;

                                PAL_BSYNC = (PAL_lineCode & 16) ? 1 : 0 ;       // third part
                                if(PAL_lineCode & 32) Delay_us(PAL_DELAY4) ; else Delay_us(PAL_DELAY28) ;

                                PAL_BSYNC = (PAL_lineCode & 64) ? 1 : 0 ;       // fourth part (no delay is needed)
                                }

                        }

                TABLAT = d ;

                INTCON.TMR0IF = 0 ;     //      clear TIMER 0 interrupt flag
                }
        }

/************************************************************
 * PAL_init : PAL initialization
 * parameters :
 *      y : number of vertical lines in pixels
 *              the more vertical lines you have :
 *                      - the less RAM you have
 *                      - the less MCU time you have
 * returns :
 *      nothing
 * requires :
 *      the PIC MUST be clocked at 32 Mhz
 * notes :
 *      this function must be called to prepare for PAL video generation
 *      this function takes control of TIMER 0 and associated interrupt
 */
void    PAL_init(unsigned char y)
        {
        INTCON.T0IF = 0 ;       // clear TIMER 0 overflow interrupt flag
        INTCON.T0IE = 0 ;       // disable TIMER 0 overflow interrupt

        T0CON = 0b11000000 ;    // TIMER 0 is 8 bits, prescaler = 2

        PAL_y = y ;                     // save vertical definition
        PAL_shift_y = 286 / 2 - y ;     // compute vertical offset for centering
        PAL_max_y = y * 2 + PAL_shift_y ;       // compute last video line number
        }

/**********************************************
 * PAL_start : start PAL video generation
 * parameters :
 *      st : start/stop PAL video generation, start = 1, stop = 0
 *      rd : start/stop rendering, start = 1, stop = 0
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      none
 */
void    PAL_control(unsigned char st, unsigned char rd)
        {
        if(st)
                {
                INTCON.GIE = 1 ;        // enable global interrupts
                INTCON.T0IE = 1 ;       // enable TIMER 0 overflow interrupt
                }
        else
                {
                INTCON.T0IE = 0 ;       // dsable TIMER 0 overflow interrupt
                }

        PAL_render = rd ;
        }

/************************************************
 * PAL_fill : fill video screen with pattern
 * parameters :
 *      c : filling pattern
 *              for example : 0 for black screen, 0xff for white screen
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      none
 */
void    PAL_fill(unsigned char c)
        {
        memset(PAL_screen, c, PAL_X * PAL_y / 8) ;
        }

/**********************************************
 * PAL_setBorder : set video border color
 * parameters :
 *      border : PAL_COLOR_WHITE or PAL_COLOR_BLACK
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      none
 */
void    PAL_setBorder(unsigned char border)
        {
        PAL_border = border ;
        }

/********************************************
 * PAL_setPixel : direct access to pixel
 * parameters :
 *      x : pixel row
 *      y : pixel column
 *      mode : PAL_COLOR_WHITE or PAL_COLOR_BLACK or PAL_COLOR_REVERSE
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      none
 */
void    PAL_setPixel(char x, char y, unsigned char mode)
        {
        unsigned char *ptr ;
        unsigned char mask ;

        /*
         * do nothing if pixel is out of bounds
         */
        if(x < 0) return ;
        if(y < 0) return ;
        if(x >= PAL_X) return ;
        if(y >= PAL_y) return ;

        ptr = PAL_screen + (((y << 7) + x) >> 3) ;      // points to byte in screen map
        mask = 1 << (x & 7) ;                           // pixel bit mask

        switch(mode)
                {
                case PAL_COLOR_BLACK:
                        *ptr &= ~mask ;                 // clear bit
                        break ;
                case PAL_COLOR_WHITE:                   // set bit
                        *ptr |= mask ;
                        break ;
                default:
                        *ptr ^= mask ;                  // toggle bit
                        break ;
                }
        }

/******************************
 * PAL_line : draw a line
 * parameters :
 *      x0, y0 : pixel start coordinates
 *      x1, y1 : pixel end coordinates
 *      pcolor : PAL_COLOR_WHITE or PAL_COLOR_BLACK or PAL_COLOR_REVERSE
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      uses Bresenham's line drawing algorithm
 */
void PAL_line(char x0, char y0, char x1, char y1, unsigned char pcolor)
        {
        int     dy ;
        int     dx ;
        int     stepx, stepy ;

        dy = y1 - y0 ;
        dx = x1 - x0 ;

        if(dy < 0)
                {
                dy = -dy ;
                stepy = -1 ;
                }
        else
                {
                stepy = 1 ;
                }

        if(dx < 0)
                {
                dx = -dx ;
                stepx = -1 ;
                }
        else
                {
                stepx = 1 ;
                }

        dy <<= 1 ;
        dx <<= 1 ;

        PAL_setPixel(x0, y0, pcolor) ;

        if(dx > dy)
                {
                int fraction;
                fraction = dy - (dx >> 1) ;

                while(x0 != x1)
                        {
                        if(fraction >= 0)
                                {
                                y0 += stepy ;
                                fraction -= dx ;
                                }
                        x0 += stepx ;
                        fraction += dy ;
                        PAL_setPixel(x0, y0, pcolor) ;
                        }
                }
        else
                {
                int fraction;
                fraction = dx - (dy >> 1) ;

                while(y0 != y1)
                        {
                        if(fraction >= 0)
                                {
                                x0 += stepx ;
                                fraction -= dy ;
                                }
                        y0 += stepy ;
                        fraction += dx ;
                        PAL_setPixel(x0, y0, pcolor) ;
                        }
                }
        }

/**********************************
 * PAL_circle : draw a circle
 * parameters :
 *      x, y : row and column of the center
 *      r : radius
 *      pcolor : PAL_COLOR_WHITE or PAL_COLOR_BLACK or PAL_COLOR_REVERSE
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      none
 */
void PAL_circle(char x, char y, char r, unsigned char pcolor)
        {
        long    px, py, opx, opy ;
        int     a ;

        for(a = 0 ; a <= 360 ; a += 15)             // for each angle from 0 to 360
                {
                /*
                 * pixel row
                 */
                px = (long)sinE3(a) * r ;
                px /= 1000 ;
                px += x ;

                /*
                 * pixel column
                 */
                py = (long)cosE3(a) * r ;
                py /= 1000 ;
                py += y ;

                /*
                 * if not first step
                 */
                if(a > 0)
                        {
                        PAL_line(opx, opy, px, py, pcolor) ;    // draw the line from old to new coordinates
                        }
                opx = px ;      // save previous coordinates
                opy = py ;
                }
        }

/**********************************
 * PAL_box : draw a solid box
 * parameters :
 *      x0, y0 : top left coordinates
 *      x1, y1 : bottom right coordinates
 *      pcolor : PAL_COLOR_WHITE or PAL_COLOR_BLACK or PAL_COLOR_REVERSE
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      none
 */
void PAL_box(char x0, char y0, char x1, char y1, unsigned char pcolor)
        {
        /*
         * just draw lines from left to right
         */
        while(x0 != x1)
                {
                PAL_line(x0, y0, x0, y1, pcolor) ;
                x0++ ;
                }
        }

/************************************
 * PAL_rectangle : draw a solid rectangle
 * parameters :
 *      x0, y0 : top left coordinates
 *      x1, y1 : bottom right coordinates
 *      pcolor : PAL_COLOR_WHITE or PAL_COLOR_BLACK or PAL_COLOR_REVERSE
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      none
 */
void PAL_rectangle(char x0, char y0, char x1, char y1, unsigned char pcolor)
        {
        /*
         * just draw the four lines of the border
         */
        PAL_line(x0, y0, x1, y0, pcolor) ;
        PAL_line(x1, y0, x1, y1, pcolor) ;
        PAL_line(x1, y1, x0, y1, pcolor) ;
        PAL_line(x0, y1, x0, y0, pcolor) ;
        }

/********************************
 * PAL_char : draw a character
 * parameters :
 *      x, y : top left coordinates where to draw
 *      c : PAL_COLOR_WHITE or PAL_COLOR_BLACK or PAL_COLOR_REVERSE
 *      size : high nibble is height multiplier, low nibble is width multiplier
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      char is white on black background, please use PAL_box to reverse after PAL_char if you want to reverse video.
 */
void    PAL_char(unsigned char x, unsigned char y, unsigned char c, unsigned char size)
        {
        unsigned char   i, j ;
        unsigned char   px, py ;
        unsigned char   sx, sy ;
        unsigned char   mx, my ;
        const char *ptr ;

        /*
         * get height and width size
         */
        mx = size & 0x0f ;
        my = size >> 4 ;

        /*
         * pointer to pattern in character table
         */
        ptr = PAL_charTable + c * 6 ;

        /*
         * char is 6 pixels large
         */
        for(i = 0 ; i < 6 ; i++)
                {
                px = x + i * mx ;
                c = *ptr++ ;    // get byte pattern
                py = y ;

                /*
                 * char is 8 pixels high
                 */
                for(j = 0 ; j < 8 ; j++)
                        {
                        for(sx = 0 ; sx < mx ; sx++)
                                {
                                for(sy = 0 ; sy < my ; sy++)
                                        {
                                        PAL_setPixel(px + sx, py + sy, c & 1) ;        // paint pixel
                                        }
                                }
                        c >>= 1 ;       // next one
                        py += my ;
                        }
                }
        }

/*******************************
 * PAL_write : write a string
 * parameters :
 *      lig, col : line and column of the first char
 *      s : string to write
 *      size : high nibble is height multiplier, low nibble is width multiplier
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      no cursor, no automatic carriage return at end of line.
 *      string is painted white on black, use PAL_box to reverse video after writing if you need.
 */
void    PAL_write(unsigned char lig, unsigned char col, unsigned char *s, unsigned char size)
        {
        unsigned char c ;

        /*
         * coordinates calculations
         */
        lig <<= 3 ;     // a char is 8 pixels high
        col *= 6 ;      // and 6 pixel large

        while(c = *s++) // parse all string
                {
                PAL_char(col, lig, c, size) ; // print char
                col += 6 *(size & 0x0f) ;              // next row
                }
        }

/***************************************
 * PAL_write : write a constant string
 * parameters :
 *      lig, col : line and column of the first char
 *      s : string to write
 *      size : high nibble is height multiplier, low nibble is width multiplier
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      no cursor, no automatic carriage return at end of line.
 *      string is displayed white on black, use PAL_box to reverse video after writing if you need.
 */
void    PAL_constWrite(unsigned char lig, unsigned char col, const unsigned char *s, unsigned char size)
        {
        unsigned char c ;

        lig <<= 3 ;
        col *= 6 ;
        while(c = *s++)
                {
                PAL_char(col, lig, c, size) ;
                col += 6 *(size & 0x0f) ;              // next row
                }
        }

/***************************************
 * PAL_picture : draw a bitmap picture
 * parameters :
 *      x, y : coordinates of the top left location
 *      bm : pointer to picture bitmap
 *      sx, sy : size of the picture (in pixels)
 * returns :
 *      nothing
 * requires :
 *      PAL_init must have been called
 * notes :
 *      data is arranged as a monochrome bitmap
 *      mikroElektronika GLCD bitmap editor tool does it very well :
 *              select T6963 and 128x128 for example.
 */
void    PAL_picture(unsigned char x, unsigned char y, const unsigned char *bm, unsigned char sx, unsigned char sy)
        {
        unsigned char   i, j, k ;
        unsigned char   px ;
        unsigned int    py ;
        unsigned char   c ;

        sx /= 8 ;
        x /= 8 ;
        for(j = 0 ; j < sy ; j++)
                {
                py = (y + j) << 4 ;
                for(i = 0 ; i < sx ; i++)
                        {
                        unsigned char rc ;

                        px = x + i ;
                        c = *bm++ ;
                        for(k = 0 ; k < 8 ; k++)
                                {
                                rc <<= 1 ;
                                rc |= c & 1 ;
                                c >>= 1 ;
                                }

                        PAL_screen[px + py] = rc ;
                        }
                }
        }
