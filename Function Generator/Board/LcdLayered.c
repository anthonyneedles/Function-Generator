/****************************************************************************************
* LcdLayered.c - A MicroC/OS driver for for a Hitachi-type LCD Display
*                                                                        
*                This LCD driver implements the concept of layers.       
*                This allows asynchronous application tasks to write to  
*                a single LCD display without interfering with each      
*                other.                                                  
*                                                                        
*                Requires the following be defined in app_cfg.h:         
*                   APP_CFG_LCD_TASK_PRIO
*                   APP_CFG_LCD_TASK_STK_SIZE
*                                                                        
*                It is derived from the work of Matthew Cohn, 2/26/2008
*                
*                Cursor code is derived from Keegan Morrow, 02/22/2013  
*                                                                        
* Todd Morton, 02/26/2013, First Revised Release
* 01/22/2015, Added to git repo, general clean up. TDM
* 02/03/2016, More cleanup. TDM
* 01/13/2017 Changed name to LcdLayered (was LayeredLcd), fixed bugs. TDM
* 01/18/2018 Changed to replace includes.h TDM
* 02/01/2018 Brian Willis changed lcdLayeredTask() DB Bit from 3 to 4
*****************************************************************************************
* Header Files - Dependencies
*****************************************************************************************/
#include "MCUType.h"
#include "app_cfg.h"
#include "os.h"
#include "LcdLayered.h"
#include "K65TWR_GPIO.h"

/*****************************************************************************************
* LCD Port Defines 
*****************************************************************************************/
#define LCD_RS_BIT     0x2
#define LCD_E_BIT      0x4
#define LCD_DB_MASK    0x78
#define LCD_PORT       GPIOD_PDOR
#define LCD_PORT_DIR   GPIOD_PDDR
#define INIT_BIT_DIR() (LCD_PORT_DIR |= (LCD_RS_BIT|LCD_E_BIT|LCD_DB_MASK))
#define LCD_SET_RS()   GPIOD_PSOR = LCD_RS_BIT
#define LCD_CLR_RS()   GPIOD_PCOR = LCD_RS_BIT
#define LCD_SET_E()    GPIOD_PSOR = LCD_E_BIT
#define LCD_CLR_E()    GPIOD_PCOR = LCD_E_BIT
#define LCD_WR_DB(nib) (GPIOD_PDOR = (GPIOD_PDOR & ~LCD_DB_MASK)|((nib)<<3))


/*****************************************************************************************
* LCD Defines                                                                            *
*****************************************************************************************/
// LCD Configuration
#define LCD_NUM_ROWS   2
#define LCD_NUM_COLS   16

#define LCD_ENABLE     0x04
#define LCD_CLEAR_BYTE 0x20    //SPACE is set as the transparent character

// LCD Cursor typedef
typedef struct {
    INT8U col;
    INT8U row;
    INT8U on;
    INT8U blink;
}LCD_CURSOR;

// LCD layer and buffer typdedef
typedef struct {
    INT8C lcd_char[LCD_NUM_ROWS][LCD_NUM_COLS];
    INT8U hidden;
    LCD_CURSOR cursor;
} LCD_BUFFER;

/*************************************************************************
  Private Local Functions
*************************************************************************/
static void lcdDlyus(INT16U us);
static void lcdDly500ns(void);
static void lcdWrite(INT16U data);
static void lcdClear(LCD_BUFFER *buffer);

static void lcdFlattenLayers(LCD_BUFFER *dest_buffer,
                             LCD_BUFFER *src_layers);
static void lcdWriteBuffer(LCD_BUFFER *buffer);
static void lcdMoveCursor(INT8U row, INT8U col);

/*************************************************************************
  MicroC/OS Resources
*************************************************************************/
static OS_TCB lcdLayeredTaskTCB;
static void lcdLayeredTask(void *p_arg);
static OS_MUTEX lcdLayersKey;
static CPU_STK lcdLayeredTaskStk[APP_CFG_LCD_TASK_STK_SIZE];

/*************************************************************************
  Global Variables
*************************************************************************/
// Stored Constants
static const INT8U lcdRowAddress[LCD_NUM_ROWS] = {0x00, 0x40};

// Static Globals
static LCD_BUFFER lcdBuffer;
static LCD_BUFFER lcdPreviousBuffer;
static LCD_BUFFER lcdLayers[LCD_NUM_LAYERS];

/*************************************************************************
  LCD Command Macros
*************************************************************************/
/*                                                    R R D D D D D D D D
                                                      / S B B B B B B B B
                                                      W   7 6 5 4 3 2 1 0
*/
// Clear Display                                      0 0 0 0 0 0 0 0 0 1
#define LCD_CLR_DISP()         (0x0001)      
// Return Home                                        0 0 0 0 0 0 0 0 1 *
#define LCD_CUR_HOME()         (0x0002)      
// Entry Mode Set                                     0 0 0 0 0 0 0 1 ids
#define LCD_ENTRY_MODE(id, s)  (0x0004                       \
                                | ((INT16U)id ? 0x0002 : 0)  \
                                | ((INT16U)s  ? 0x0001 : 0))
// Display ON/OFF Control                             0 0 0 0 0 0 1 d c b
#define LCD_ON_OFF(d, c, b)    (0x0008                       \
                                | ((INT16U)d  ? 0x0004 : 0)  \
                                | ((INT16U)c  ? 0x0002 : 0)  \
                                | ((INT16U)b  ? 0x0001 : 0))
// Cursor or Display Shift                            0 0 0 0 0 1 scrl* *
#define LCD_SHIFT(sc, rl)      (0x0010                       \
                                | ((INT16U)sc ? 0x0008 : 0)  \
                                | ((INT16U)rl ? 0x0004 : 0))
// Function Set                                       0 0 0 0 1 dln f * *
#define LCD_FUNCTION(dl, n, f) (0x0020                       \
                                | ((INT16U)dl ? 0x0010 : 0)  \
                                | ((INT16U)n  ? 0x0008 : 0)  \
                                | ((INT16U)f  ? 0x0004 : 0))
// Set CG RAM Address                                 0 0 0 1 ----acg-----
#define LCD_CG_RAM(acg)        (0x0040                       \
                                | ((INT16U)agc  & 0x003F))
// Set DD RAM Address                                 0 0 1 -----add------
#define LCD_DD_RAM(add)        (0x0080                       \
                                | (((INT16U)add)  & 0x007F))
// Write Data to CG or DD RAM                         0 1 ------data------
#define LCD_WRITE(data)        (0x0100                       \
                                | ((INT16U)data & 0x00FF))
                                                                       
/******************************************************************************
  lcdLayeredTask() - Handles writing to the LCD module      (Private Task)
  
        When writing to the LCD, will block until thescreen is updated.  
        This is worst-case x.xms, but will be much lower if not every character 
        on the screen is changing.
******************************************************************************/
static void lcdLayeredTask(void *p_arg) {
    OS_ERR os_err;
    
    // Avoid compiler warning
    (void)p_arg;
    
    while(1) {
    
        // Wait for an lcd layer to be modified
    	DB4_TURN_OFF();
        OSTaskSemPend(0,OS_OPT_PEND_BLOCKING,(CPU_TS *)0, &os_err);
    	DB4_TURN_ON();
        
        lcdFlattenLayers(&lcdBuffer, (LCD_BUFFER *)&lcdLayers);
        lcdWriteBuffer(&lcdBuffer);
    }
}

/*************************************************************************
  LcdCursor                                                       (Public)


  DESCRIPTION: Sets the cursor position, blinking, and visibility

  RETURNS: TRUE if no error, FALSE otherwise
*************************************************************************/
INT8U LcdCursor(INT8U row, INT8U col, INT8U layer, INT8U on, INT8U blink){
    INT8U noerr = TRUE;
    OS_ERR os_err;
    
    OSMutexPend(&lcdLayersKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);

    if ((layer < LCD_NUM_LAYERS) && (col <= LCD_NUM_COLS) && (row <= LCD_NUM_ROWS)){
        lcdLayers[layer].cursor.col = col;
        lcdLayers[layer].cursor.row = row;
        if ( blink ){
            lcdLayers[layer].cursor.blink = TRUE;
        }else{
            lcdLayers[layer].cursor.blink = FALSE;
        }
        if ( on ){
            lcdLayers[layer].cursor.on = TRUE;
        }else{
            lcdLayers[layer].cursor.on = FALSE;
        }
    }else{
        noerr = FALSE;
    }

    (void)OSMutexPost(&lcdLayersKey, OS_OPT_POST_NONE, &os_err);

    // We have modified a layer
    (void)OSTaskSemPost(&lcdLayeredTaskTCB, OS_OPT_POST_NONE, &os_err);

    return(noerr);
}
/*************************************************************************
  LcdDispClear() - Clears a layer                                 (Public)   

                   Pends on the lcdLayersKey mutex
                   Posts the lcdModifiedFlag semaphore
*************************************************************************/
void LcdDispClear(INT8U layer) {
    OS_ERR os_err;
    LCD_BUFFER *llayer = &lcdLayers[layer];

    OSMutexPend(&lcdLayersKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }

    lcdClear(llayer);

    (void)OSMutexPost(&lcdLayersKey, OS_OPT_POST_NONE, &os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }
    
    // We have modified a layer
    (void)OSTaskSemPost(&lcdLayeredTaskTCB, OS_OPT_POST_NONE, &os_err);
}


/*************************************************************************
  LcdDispClrLine() - Clears a line of a layer                     (Public)   

                     Pends on the lcdLayersKey mutex
                     Posts the lcdModifiedFlag semaphore
*************************************************************************/
void LcdDispClrLine(INT8U row, INT8U layer) {
    INT8U col;
    OS_ERR os_err;
    
    LCD_BUFFER *llayer = &lcdLayers[layer];
    
    OSMutexPend(&lcdLayersKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }
    
    // For each column...
    for(col = 0; col < LCD_NUM_COLS; col++) {

        // Clear the character at that position
        llayer->lcd_char[row-1][col] = LCD_CLEAR_BYTE;
    }
    
    (void)OSMutexPost(&lcdLayersKey, OS_OPT_POST_NONE, &os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }
    
    // We have modified a layer
    (void)OSTaskSemPost(&lcdLayeredTaskTCB,OS_OPT_POST_NONE,&os_err);
}


/*************************************************************************
  LcdDispString() - Writes a null terminated string to a layer    (Public)

                    Pends on the lcdLayersKey mutex
                    Posts the lcdModifiedFlag semaphore
*************************************************************************/
void LcdDispString(INT8U row,
                   INT8U col,
                   INT8U layer,
                   const INT8C *string) {

    OS_ERR os_err;
    INT8U cnt, row_index, col_index;
    LCD_BUFFER *llayer = &lcdLayers[layer];

    row_index = row - 1;
    col_index = col - 1;
    
    OSMutexPend(&lcdLayersKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }
    
    // Iterate through the string until we reach a null
    for(cnt = 0; string[cnt] != 0x00; cnt++) {
    
        if((col_index+cnt) < LCD_NUM_COLS){ // not at end of row
            // Copy from the passed paramater to the layer
            llayer->lcd_char[row_index][col_index+cnt] = string[cnt];
        }else{ //outside buffer
        }
    }
    
    (void)OSMutexPost(&lcdLayersKey, OS_OPT_POST_NONE, &os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }
    
    // We have modified a layer
    (void)OSTaskSemPost(&lcdLayeredTaskTCB,OS_OPT_POST_NONE,&os_err);
}



/*************************************************************************
  LcdDispChar() - Writes a character to a layer                   (Public)

                  Pends on the lcdLayersKey mutex
                  Posts the lcdModifiedFlag semaphore
*************************************************************************/
void LcdDispChar(INT8U row,
                 INT8U col,
                 INT8U layer,
                 INT8C character) {
    OS_ERR os_err;
    INT8U row_index, col_index;
    LCD_BUFFER *llayer = &lcdLayers[layer];

    row_index = row - 1;
    col_index = col - 1;
    
    if(col_index < LCD_NUM_COLS){
        OSMutexPend(&lcdLayersKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
        while(os_err != OS_ERR_NONE){           /* Error Trap                        */
        }
    
        // Copy from the passed paramater to the layer
        llayer->lcd_char[row_index][col_index] = character;
    
        (void)OSMutexPost(&lcdLayersKey, OS_OPT_POST_NONE, &os_err);
        while(os_err != OS_ERR_NONE){           /* Error Trap                        */
        }
    
        // We have modified a layer
        (void)OSTaskSemPost(&lcdLayeredTaskTCB,OS_OPT_POST_NONE,&os_err);
    }else{ //outside layer
    }
}



/*************************************************************************
  LcdDispByte - Writes the ASCII representation of a byte to a    (Public)
                layer in hex

                Pends on the lcdLayersKey mutex
                Posts the lcdModifiedFlag semaphore
*************************************************************************/
void LcdDispByte(INT8U row, INT8U col, INT8U layer, INT8U byte) {
    OS_ERR os_err;
    INT8U row_index, col_index;
    LCD_BUFFER *llayer = &lcdLayers[layer];
    
    // Convert row / col index 1 to index 0
    row_index = row - 1;
    col_index = col - 1;
    
    if(col < LCD_NUM_COLS){
        OSMutexPend(&lcdLayersKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
        while(os_err != OS_ERR_NONE){           /* Error Trap                        */
        }

        llayer->lcd_char[row_index][col_index+0] = (byte >> 4);   // MSB
        llayer->lcd_char[row_index][col_index+1] = (byte & 0x0F); // LSB
    
        // Convert MSB to ASCII character
        llayer->lcd_char[row_index][col_index+0] +=
            (llayer->lcd_char[row_index][col_index+0] <= 9 ? '0' : 'A' - 10);

        // Convert LSB to ASCII character
        llayer->lcd_char[row_index][col_index+1] +=
            (llayer->lcd_char[row_index][col_index+1] <= 9 ? '0' : 'A' - 10);


        (void)OSMutexPost(&lcdLayersKey, OS_OPT_POST_NONE, &os_err);
        while(os_err != OS_ERR_NONE){           /* Error Trap                        */
        }

        // We have modified a layer
        (void)OSTaskSemPost(&lcdLayeredTaskTCB,OS_OPT_POST_NONE,&os_err);
        while(os_err != OS_ERR_NONE){           /* Error Trap                        */
        }
    }else{ //outside layer
    }
}


/*************************************************************************
  LcdDispDecByte - Writes the ASCII representation of a byte to a (Public)
                   layer in decimal

                   Pends on the lcdLayersKey mutex
                   Posts the lcdModifiedFlag semaphore
*************************************************************************/
void LcdDispDecByte(INT8U row,
                    INT8U col,
                    INT8U layer,
                    INT8U byte,
                    INT8U lzeros) {
    
    OS_ERR os_err;
    INT8U row_index, col_index, hunds, tens, ones;
    LCD_BUFFER *llayer = &lcdLayers[layer];
    
    if((col + 1) < LCD_NUM_COLS){
        // Convert row / col index 1 to index 0
        row_index = row - 1;
        col_index = col - 1;
    
        hunds = byte / 100;
        tens = (byte / 10) % 10;
        ones = byte % 10;
    
        OSMutexPend(&lcdLayersKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
        while(os_err != OS_ERR_NONE){           /* Error Trap                        */
        }

        if(lzeros == 1 || hunds > 0) {
            llayer->lcd_char[row_index][col_index+0] = hunds; // Hundreds
            llayer->lcd_char[row_index][col_index+0] += '0';  //  --> ASCII
        }

        if(lzeros == 1 || hunds > 0 || tens > 0) {
            llayer->lcd_char[row_index][col_index+1] = tens;  // Tens
            llayer->lcd_char[row_index][col_index+1] += '0';  //  --> ASCII
        }
    
        llayer->lcd_char[row_index][col_index+2] = ones;      // Ones
        llayer->lcd_char[row_index][col_index+2] += '0';      //  --> ASCII
        

        (void)OSMutexPost(&lcdLayersKey, OS_OPT_POST_NONE, &os_err);
        while(os_err != OS_ERR_NONE){           /* Error Trap                        */
        }

        // We have modified a layer
        (void)OSTaskSemPost(&lcdLayeredTaskTCB,OS_OPT_POST_NONE,&os_err);
        while(os_err != OS_ERR_NONE){           /* Error Trap                        */
        }
    }else{ //outside layer
    }
}

/*************************************************************************
  LcdDispTime - Writes a time to a layer                          (Public)

                Pends on the lcdLayersKey mutex
                Posts the lcdModifiedFlag semaphore
*************************************************************************/
void LcdDispTime(INT8U row,
                 INT8U col,
                  INT8U layer,
                 INT8U hrs,
                 INT8U mins,
                 INT8U secs) {
    OS_ERR os_err;
    INT8U row_index, col_index;
    LCD_BUFFER *llayer = &lcdLayers[layer];

    if((col + 6) < LCD_NUM_COLS){
        // Convert row / col index 1 to index 0
        row_index = row - 1;
        col_index = col - 1;

    
        OSMutexPend(&lcdLayersKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
        while(os_err != OS_ERR_NONE){           /* Error Trap                        */
        }
    

        llayer->lcd_char[row_index][col_index+0] = hrs / 10 + '0';
        llayer->lcd_char[row_index][col_index+1] = hrs % 10 + '0';

        llayer->lcd_char[row_index][col_index+2] = ':';

        llayer->lcd_char[row_index][col_index+3] = mins / 10 + '0';
        llayer->lcd_char[row_index][col_index+4] = mins % 10 + '0';

        llayer->lcd_char[row_index][col_index+5] = ':';

        llayer->lcd_char[row_index][col_index+6] = secs / 10 + '0';
        llayer->lcd_char[row_index][col_index+7] = secs % 10 + '0';
    
           
        (void)OSMutexPost(&lcdLayersKey, OS_OPT_POST_NONE, &os_err);
        while(os_err != OS_ERR_NONE){           /* Error Trap                        */
        }
    
        // We have modified a layer
        (void)OSTaskSemPost(&lcdLayeredTaskTCB,OS_OPT_POST_NONE,&os_err);
    }else{ //outside layer
    }
}


/******************************************************************************
  LcdInit() - Initializes the LCD                                 (Public)

        Initializes the LCD hardware, sets up our semaphores/mutexes/task,
        clears all of our buffers and layers.  This needs to be run before
        any other function that accesses the LCD.
******************************************************************************/
void LcdInit(void) {
    INT8U layer_cnt;
    OS_ERR os_err;
    
    // Create mutex key, semaphore, and task
    OSMutexCreate(&lcdLayersKey,"LCD Layers Key", &os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }

    OSTaskCreate((OS_TCB     *)&lcdLayeredTaskTCB,
                (CPU_CHAR   *)"Layered LCD Task",
                (OS_TASK_PTR ) lcdLayeredTask,
                (void       *) 0,
                (OS_PRIO     ) APP_CFG_LCD_TASK_PRIO,
                (CPU_STK    *)&lcdLayeredTaskStk[0],
                (CPU_STK     )(APP_CFG_LCD_TASK_STK_SIZE / 10u),
                (CPU_STK_SIZE) APP_CFG_LCD_TASK_STK_SIZE,
                (OS_MSG_QTY  ) 0,
                (OS_TICK     ) 0,
                (void       *) 0,
                (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                (OS_ERR     *)&os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }

    // Perform LCD hardware initialisation
    SIM_SCGC5 |= SIM_SCGC5_PORTD_MASK;              /* Enable clock gate for PORTD */
    PORTD_PCR1=(0|PORT_PCR_MUX(1));
    PORTD_PCR2=(0|PORT_PCR_MUX(1));
    PORTD_PCR3=(0|PORT_PCR_MUX(1));
    PORTD_PCR4=(0|PORT_PCR_MUX(1));
    PORTD_PCR5=(0|PORT_PCR_MUX(1));
    PORTD_PCR6=(0|PORT_PCR_MUX(1));
    INIT_BIT_DIR();
    LCD_CLR_E(); 
    LCD_SET_RS();           /*Data select unless in LcdWrCmd()  */
    lcdDlyus(15000);           /* LCD requires 15ms delay at powerup */
   
    LCD_CLR_RS();           /*Send first command for RESET sequence*/
    LCD_WR_DB(0x3);
    LCD_SET_E();
    lcdDly500ns();
    LCD_CLR_E();
    lcdDlyus(4200);            /*Wait >4.1ms */
  
    LCD_WR_DB(0x3);         /*Repeat */
    LCD_SET_E();
    lcdDly500ns();
    LCD_CLR_E();
    lcdDlyus(101);            /*Wait >100us */
  
    LCD_WR_DB(0x3);         /* Repeat */
    LCD_SET_E();
    lcdDly500ns();
    LCD_CLR_E();
    lcdDlyus(41);           /*Wait >40us*/
  
    LCD_WR_DB(0x2);         /*Send last command for RESET sequence*/
    LCD_SET_E();
    lcdDly500ns();
    LCD_CLR_E();
    lcdDlyus(41);
  
    lcdWrite(LCD_FUNCTION(0, 1, 0));     /*Send command for 4-bit mode */
    lcdWrite(LCD_ENTRY_MODE(1, 0)); // Increment, no shift
    lcdWrite(LCD_ON_OFF(1, 0, 0));  // LCD on, cursor off, blink off
    lcdWrite(LCD_CLR_DISP());       // Clear display
    lcdDlyus(1650);
    lcdWrite(LCD_DD_RAM(0x0000));   // Reset cursor
    
    
    // Clear all of our layers
    for(layer_cnt = 0; layer_cnt < LCD_NUM_LAYERS; layer_cnt++) {
        lcdClear(&lcdLayers[layer_cnt]);
    }
    
    // Clear the current buffer
    // and the previous buffer
    lcdClear(&lcdBuffer);
    lcdClear(&lcdPreviousBuffer);
}


/*************************************************************************
  lcdFlattenLayers() - Combines *src_layers onto *dest_buffer    (Private)

        The src_layer with the lowest index will be on the bottom, the
        src_layer with the highest index will be on the top.  Treats the
        character defined as LCD_CLEAR_BYTE as a transparent byte.

                       Pends on the lcdLayersKey mutex
*************************************************************************/
static void lcdFlattenLayers(LCD_BUFFER *dest_buffer,
                             LCD_BUFFER *src_layers) {
    
    INT8U layer, row, col, current_char;
    OS_ERR os_err;
    // Clear the destination buffer
    lcdClear(dest_buffer);

//    DBUG_PORT &= ~DBUG_LCDTASK;
    OSMutexPend(&lcdLayersKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }
//    DBUG_PORT |= DBUG_LCDTASK;
    // Set the destination buffer cursor to false initially
    dest_buffer->cursor.on = FALSE;
    dest_buffer->cursor.blink = FALSE;

    // For each layer...
    for(layer = 0; layer < LCD_NUM_LAYERS; layer++) {

        // If that layer is not hidden...
        if((src_layers+layer)->hidden == 0) {
            // For each row...
            for(row = 0; row < LCD_NUM_ROWS; row++) {
                // For each column...
                for(col = 0; col < LCD_NUM_COLS; col++) {
                    current_char = (src_layers+layer)->lcd_char[row][col];
                
                    // If the source layer is not null
                    if(current_char != LCD_CLEAR_BYTE) {
                        // Copy from the source layer to the buffer
                        dest_buffer->lcd_char[row][col] = current_char;
                    }else{ //Do nothing - transparent
                    }
                    
                } // column
            } // row

            //Handle the cursor status
//            if ( (src_layers+layer)->cursor.on != FALSE){
                dest_buffer->cursor.col = (src_layers+layer)->cursor.col;
                dest_buffer->cursor.row = (src_layers+layer)->cursor.row;
                dest_buffer->cursor.on = (src_layers+layer)->cursor.on;
                dest_buffer->cursor.blink = (src_layers+layer)->cursor.blink;
//            }else{
                // This layer doesn't move the cursor, do nothing
//            }
        }else{ //Do nothing - layer is hidden
        }
    } // layer
    
    (void)OSMutexPost(&lcdLayersKey, OS_OPT_POST_NONE, &os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }

}


/*************************************************************************
  lcdWriteBuffer() - Sends an LCD_BUFFER buffer to lcdWrite()    (Private)
  
        The previous buffer lcdPreviousBuffer is a global variable
        containing a copy of the actual contents of the LCD module.  By 
        using the lcdPreviousBuffer and repos_flag, we are able to only
        write bytes that have changed.
                                                           
                     Blocks for as long as lcdWrite() blocks
*************************************************************************/
static void lcdWriteBuffer(LCD_BUFFER *buffer) {
    INT8U row, col, repos_flag;
    
    // For each row...
    for(row = 0; row < LCD_NUM_ROWS; row++) {
    
        // Set our cursor to the beginning of the row
        lcdWrite(LCD_DD_RAM(lcdRowAddress[row]));
        repos_flag = 0;
        
        // For each column...
        for(col = 0; col < LCD_NUM_COLS; col++) {

            // If the character at the current position has changed...
            if(lcdPreviousBuffer.lcd_char[row][col]
                != buffer->lcd_char[row][col]) {
                
                // If we need to reposition, do that now
                if(repos_flag == 1) {
                    lcdWrite(LCD_DD_RAM((lcdRowAddress[row] + col)));
                    repos_flag = 0;
                }
            
                // Write the character to the LCD
                lcdWrite(LCD_WRITE(buffer->lcd_char[row][col]));
             
                // And update the previous buffer
                lcdPreviousBuffer.lcd_char[row][col] =
                    buffer->lcd_char[row][col];
            } else {
                // Otherwise we don't write the character... but we need
                //   to set the reposition flag
                
                repos_flag = 1;
            }
            
        
        }
    }
    // At the end setup the cursor
    lcdMoveCursor(buffer->cursor.row,buffer->cursor.col);
    LcdCursorDispMode(buffer->cursor.on, buffer->cursor.blink);

}

/******************************************************************************
  lcdWrite() - Writes a command (both data and control busses)   (Private)
               to the LCD.
               data is a 16-bit value bits 9-15 are not used, bit 8 is the 
               register select, bits 0-7 is the character or command.
               
******************************************************************************/
static void lcdWrite(INT16U data) {
    INT8U c;
    // Set/Reset RS
    if((data & 0x0100) == 0x0100){
        LCD_SET_RS(); //data write
    }else{
        LCD_CLR_RS(); //command write
    }
    
    c = (INT8U)data;
    // Write character/command to LCD
    LCD_WR_DB((c>>4));
    LCD_SET_E();
    lcdDly500ns();
    LCD_CLR_E();
    lcdDly500ns();
    lcdDly500ns();
    LCD_WR_DB((c&0x0f));
    LCD_SET_E();
    lcdDly500ns();
    LCD_CLR_E();
    lcdDlyus(41);
}


/*************************************************************************
  lcdClear() - Clears a buffer or layer                          (Private)
*************************************************************************/
static void lcdClear(LCD_BUFFER *buffer) {
    INT8U row, col;
    
    // For each row...
    for(row = 0; row < LCD_NUM_ROWS; row++) {
        // For each column...
        for(col = 0; col < LCD_NUM_COLS; col++) {

            // Clear the character at that position
            buffer->lcd_char[row][col] = LCD_CLEAR_BYTE;

        }
    }
    
}

/********************************************************************
** lcdMoveCursor(INT8U row, INT8U col)
*
*  FILENAME: LCD.c
*
*  PARAMETERS: row - Destination row (1 or 2).
*              col - Destination column (1 - 16).
*
*  DESCRIPTION: Moves the cursor to [row,col].
*
*  RETURNS: None
********************************************************************/
static void lcdMoveCursor(INT8U row, INT8U col) {
   lcdWrite(LCD_DD_RAM((lcdRowAddress[(row-1)]) + (col-1)));
}

/********************************************************************
** LcdCursorDispMode(INT8U on, INT8U blink)
*
*  FILENAME: LCD.c
*
*  PARAMETERS: on - (Binary)Turn cursor on if TRUE, off if FALSE.
*              blink - (Binary)Cursor blinks if TRUE.
*
*  DESCRIPTION: Changes LCD cursor state.
*
*  RETURNS: None
********************************************************************/
void LcdCursorDispMode(INT8U on, INT8U blink) {
    lcdWrite(LCD_ON_OFF(1, on, blink));
}

/********************************************************************
** LcdHideLayer(INT8U layer)
*
*  FILENAME: LcdLayered.c
*
*  PARAMETERS: layer - The layer to be hidden
*
*  DESCRIPTION: Hides the specified layer
*
*  RETURNS: None
********************************************************************/
void LcdHideLayer(INT8U layer){
    lcdLayers[layer].hidden = 1;
}


/********************************************************************
** LcdShowLayer(INT8U layer)
*
*  FILENAME: LcdLayered.c
*
*  PARAMETERS: layer - The layer to be shown
*
*  DESCRIPTION: Shows the specified layer
*
*  RETURNS: None
********************************************************************/
void LcdShowLayer(INT8U layer){
    lcdLayers[layer].hidden = 0;
}

/********************************************************************
** LcdToggleLayer(INT8U layer)
*
*  FILENAME: LcdLayered.c
*
*  PARAMETERS: layer - The layer to be toggled
*
*  DESCRIPTION: Toggles the specified layer
*
*  RETURNS: None
********************************************************************/
void LcdToggleLayer(INT8U layer){
    if(lcdLayers[layer].hidden){
        lcdLayers[layer].hidden = 0;
    }else{
        lcdLayers[layer].hidden = 1;
    }
}

/*************************************************************************
  lcdDlyus() - Blocks for the passed number of microseconds      (Private)
*************************************************************************/
static void lcdDlyus(INT16U us) {
    INT16U cnt;
    
    for(cnt = 0; cnt <= us; cnt++) {
        lcdDly500ns();
        lcdDly500ns();
    }

}


/********************************************************************
** lcdDly500ns(void)
*   Delays, at least, 500ns
*   Designed for 120MHz or 150MHz clock.
 *  Tdly >= (66.5ns)i (at 150MHz)
 * Currently set to ~532ns with i=8.
 * TDM 01/20/2013
********************************************************************/
static void lcdDly500ns(void){
    INT32U i;
    for(i=0;i<8;i++){
    }
}
