/*************************************************************************
* LayeredLcd.h - A MicroC/OS driver for for the Seiko LCD Display        *
*                                                                        *
*                This LCD driver implements the concept of layers.       *
*                This allows asynchronous application tasks to write to  *
*                a single LCD display without interfering with each      *
*                other.                                                  *
*                                                                        *
*                It is derived from the work of Matthew Cohn, 2/26/2008  *
*                                                                        *
* Todd Morton, 02/26/2013, First Revised Release
* 01/22/2015, Added to git repo, general clean up. TDM
* 02/03/2016, More cleanup. TDM
* 01/13/2017 Changed name to LcdLayered (was LayeredLcd), fixed bugs. TDM
*************************************************************************/

#ifndef LCD_DEF
#define LCD_DEF
/*************************************************************************
* LCD Layers - Define all layer values here                              *
*              Range from 0 to (LCD_NUM_LAYERS - 1)                      *
*              Arranged from largest number on top, down to 0 on bottom. *
*************************************************************************/
#define LCD_NUM_LAYERS 1

#define WAVE_LAYER 0


/*************************************************************************
  Public Functions
*************************************************************************/

void LcdInit(void);

void LcdDispChar(INT8U row,INT8U col,INT8U layer,INT8C c);

void LcdDispString(INT8U row,INT8U col,INT8U layer,
                          const INT8C *string);
                          
void LcdDispTime(INT8U row,INT8U col,INT8U layer,
                        INT8U hrs,INT8U mins,INT8U secs);
                        
void LcdDispByte(INT8U row,INT8U col,INT8U layer,INT8U byte);
                        
void LcdDispDecByte(INT8U row,INT8U col,INT8U layer,
                           INT8U byte,INT8U lzeros);
                        
void LcdDispClear(INT8U layer);

void LcdDispClrLine(INT8U row, INT8U layer);
INT8U LcdCursor(INT8U row, INT8U col, INT8U layer, INT8U on, INT8U blink);
void LcdCursorDispMode(INT8U on, INT8U blink);
void LcdHideLayer(INT8U layer);
void LcdShowLayer(INT8U layer);
void LcdToggleLayer(INT8U layer);
#endif

