#define PAL_X      128

#define PAL_CNTL_STOP           0
#define PAL_CNTL_START          1

#define PAL_CNTL_BLANK          0
#define PAL_CNTL_RENDER         1

#define PAL_COLOR_BLACK         0
#define PAL_COLOR_WHITE         1
#define PAL_COLOR_REVERSE       2

#define PAL_CHAR_STANDARD       0x11
#define PAL_CHAR_DWIDTH         0x12
#define PAL_CHAR_DHEIGHT        0x21
#define PAL_CHAR_DSIZE          0x22

extern  unsigned char   PAL_screen[] ;
extern  unsigned long   PAL_frameCtr ;

void    PAL_ISR() ;
void    PAL_init(unsigned char y) ;
void    PAL_control(unsigned char st, unsigned char rd) ;
void    PAL_fill(unsigned char c) ;
void    PAL_setBorder(unsigned char border) ;
void    PAL_setPixel(char x, char y, unsigned char mode) ;
void    PAL_line(char x0, char y0, char x1, char y1, unsigned char pcolor) ;
void    PAL_circle(char x, char y, char r, unsigned char pcolor) ;
void    PAL_box(char x0, char y0, char x1, char y1, unsigned char pcolor) ;
void    PAL_rectangle(char x0, char y0, char x1, char y1, unsigned char pcolor) ;
void    PAL_char(unsigned char x, unsigned char y, unsigned char c, unsigned char size) ;
void    PAL_write(unsigned char lig, unsigned char col, unsigned char *s, unsigned char size) ;
void    PAL_constWrite(unsigned char lig, unsigned char col, const unsigned char *s, unsigned char size) ;
void    PAL_picture(unsigned char x, unsigned char y, const unsigned char *bm, unsigned char sx, unsigned char sy) ;
//void    PAL_pictureAlign(unsigned char x, unsigned char y, const unsigned char *bm, unsigned char sx, unsigned char sy) ;
