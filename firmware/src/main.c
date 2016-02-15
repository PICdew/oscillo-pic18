/* Headers */
#include "PAL_Library.h"  //PAL Library header

/* Definitions */
#define PAL_Y               120                 // number of vertical pixels
#define CURVE_CENTER_Y      (PAL_Y >> 1)        // center of the curve (px)
#define CURVE_AMPLITUDE     ((PAL_Y >> 1) - 2)  // amplitude of the curve (px)

#define DISPLAY_MODE_LINE   0                   // wave display mode : line
#define DISPLAY_MODE_DOT    1                   // wave display mode : dot
#define DISPLAY_MODE_FILLED 2                   // wave display mode : filled under

/* RAM Variables */
unsigned char PAL_screen[PAL_X * PAL_Y / 8];    // screen memory
unsigned long frequency = 0L;                   // frequency (Hz) [0;2^32-1]
unsigned int tension = 0;                       // tension, (mV) [0;65335]

char frequency_display[] = "1.5625e-1 Hz";      // output format : "FREQ : %d"
char tension_display[] = "2.5 V";               // output form "TENS : %d"

unsigned char pixel_x = 0;                      // current sample x coord (px)
unsigned char pixel_y = 0;                      // current sample y coord (px)
unsigned char last_pixel_x = 0;                 // previous sample x coord (px)
unsigned char last_pixel_y = 0;                 // previous sample y coord (px)

unsigned char display_mode = 0;                 // current display mode

/* Functions */
void interrupt(void) {
    //Do PAL related business
    PAL_ISR();
}

// redraw the screen
void paint() {
    // clear screen
    PAL_fill(0);
    // draw frequency & tension
    PAL_write(0, 1, frequency_display, PAL_CHAR_STANDARD);
    PAL_write(1, 1, tension_display, PAL_CHAR_STANDARD);
    // start rendering
    PAL_control(PAL_CNTL_START, PAL_CNTL_RENDER);
    // start drawing curve
    while(pixel_x < PAL_X)
    {
      /* Getting a sample
       * Trimming the sample requires mult. by 128 / 1024,
       * which is like dividing by 2^3. Bitsift division
       * is then faster, bc E(x/2^3) = x >> 3;
       */
      pixel_y = 64 - (ADC_Read(0) >> 4);
      // saving pixel coords
      last_pixel_y = pixel_x == 0 ? pixel_y : last_pixel_y;
      last_pixel_x = pixel_x == 0 ? pixel_x : last_pixel_x;
      // setting the pixel
      switch(display_mode)
      {
          // dot mode on
          case DISPLAY_MODE_DOT:
               // draw the pixel
               PAL_setPixel(pixel_x, pixel_y, PAL_COLOR_WHITE);
               break;

          // filled mode on
          case DISPLAY_MODE_FILLED:
               // draw the pixel
               PAL_setPixel(pixel_x, pixel_y, PAL_COLOR_WHITE);
               // if pixel is not at center
               if(pixel_y == CURVE_CENTER_Y)
                   break;
               // fill along y axis
               PAL_line(pixel_x,
                   pixel_y - (pixel_y > CURVE_CENTER_Y ? 1 : -1),
                   pixel_x,
                   CURVE_CENTER_Y,
                   PAL_COLOR_REVERSE);
               break;

          // line mode on (default mode)
          case DISPLAY_MODE_LINE:
          default:
               // draw a line btwn the 2 newest pixels
               PAL_line(last_pixel_x,
                   last_pixel_y,
                   pixel_x,
                   pixel_y,
                   PAL_COLOR_WHITE);
               break;

      }

      // TODO : REMOVE
      if (PORTC) { display_mode = 1 - display_mode; }
      // wait for some time (depends on the frequency)
      Delay_ms(10);
      // drawing next pixel
      last_pixel_x = pixel_x;
      last_pixel_y = pixel_y;
      ++pixel_x;
      // start rendering
      PAL_control(PAL_CNTL_START, PAL_CNTL_RENDER);
    }
    // resetting pixel count
    pixel_x = 0;
    last_pixel_x = -1;
}


// entry point
void main(void) {
    // I/O configuration
    TRISA = 0xFF;
    TRISC = 0xFF;
    TRISD = 0;
    TRISE = 0;
    PORTD = 0;
    PORTE = 0;
    // ADC configuration (see VCFG1/2)
    ADCON1 = 0x01;

    // init ADC
    ADC_Init();
    // init PAL library
    PAL_init(PAL_Y);
    // paint picture
    while(1) { paint(); }
}
