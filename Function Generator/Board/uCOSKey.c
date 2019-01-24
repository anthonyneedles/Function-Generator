/********************************************************************
* uCOSKey.c - A keypad module that runs under MicroC/OS for a 4x4 
* matrix keypad.
* This version allows for multiple column keys to be pressed and 
* mapped to a different code by changing ColTable[] in keyScan().
* Multiple rows can not be resolved. The topmost button will be used.
* The keyCodeTable[] is currently set to generate ASCII codes.
*
* Requires the following be defined in app_cfg.h:
*                   APP_CFG_KEY_TASK_PRIO
*                   APP_CFG_KEY_TASK_STK_SIZE
*
* 02/20/2001 TDM Original key.c for 9S12
* 01/14/2013 TDM Modified for K70 custom tower board.
* 02/12/2013 TDM Modified to run under MicroC/OS-III
* 01/18/2018 Changed to replace includes.h TDM
* 02/01/2018 Brian Willis changed KeyTask() DB Bit from 4 to 1
*********************************************************************
* Header Files - Dependencies
********************************************************************/
#include "MCUType.h"
#include "app_cfg.h"
#include "os.h"
#include "uCOSKey.h"
#include "k65TWR_GPIO.h"
/********************************************************************
* Module Defines
* This version is designed for the custom LCD/Keypad board, which
* has the following mapping:
*  COL1->PTC3, COL2->PTC4, COL3->PTC5, COL4->PTC6
*  ROW1->PTC7, ROW2->PTC8, ROW3->PTC9, ROW4->PTC10
********************************************************************/
typedef enum{KEY_OFF,KEY_EDGE,KEY_VERF} KEYSTATES;
#define KEY_PORT_OUT   GPIOC_PDOR
#define KEY_PORT_DIR   GPIOC_PDDR
#define KEY_PORT_IN	   GPIOC_PDIR
#define COLS_MASK 0x00000078
#define ROWS_MASK 0x00000780
#define DC1 (INT8U)0x11     /*ASCII control code for the A button */
#define DC2 (INT8U)0x12     /*ASCII control code for the B button */
#define DC3 (INT8U)0x13     /*ASCII control code for the C button */
#define DC4 (INT8U)0x14     /*ASCII control code for the D button */
typedef struct{
    INT8U buffer;
    OS_SEM flag;
}KEY_BUFFER;
/********************************************************************
* Private Resources
********************************************************************/
static INT8U keyScan(void);         /* Makes a single keypad scan  */
static const INT8U keyCodeTable[16] =
   {'1','2','3',DC1,'4','5','6',DC2,'7','8','9',DC3,'*','0','#',DC4};
static void keyDly(void);  /* Added for GPIO to settle before read */
static void keyTask(void *p_arg);
static KEY_BUFFER keyBuffer;
/**********************************************************************************
* Allocate task control blocks
**********************************************************************************/
static OS_TCB keyTaskTCB;
/*************************************************************************
* Allocate task stack space.
*************************************************************************/
static CPU_STK keyTaskStk[APP_CFG_KEY_TASK_STK_SIZE];

/********************************************************************
* KeyPend() - A function to provide access to the key buffer via a
*             semaphore.
*    - Public
********************************************************************/
INT8U KeyPend(INT16U tout, OS_ERR *os_err){
    OSSemPend(&(keyBuffer.flag),tout, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, os_err);
    return(keyBuffer.buffer);
}

/********************************************************************
* KeyInit() - Initialization routine for the keypad module
*             The columns are normally set as inputs and, since they 
*             are pulled high, they are one. Then to pull a row low
*             during scanning, the direction for that pin is changed
*             to an output.
********************************************************************/
void KeyInit(void){

    OS_ERR os_err;
	/* Key port init */
    SIM_SCGC5 |= SIM_SCGC5_PORTC_MASK;              /* Enable clock gate for PORTC */
    PORTC_PCR3=PORT_PCR_MUX(1)|PORT_PCR_PS_MASK|PORT_PCR_PE_MASK;
    PORTC_PCR4=PORT_PCR_MUX(1)|PORT_PCR_PS_MASK|PORT_PCR_PE_MASK;
    PORTC_PCR5=PORT_PCR_MUX(1)|PORT_PCR_PS_MASK|PORT_PCR_PE_MASK;
    PORTC_PCR6=PORT_PCR_MUX(1)|PORT_PCR_PS_MASK|PORT_PCR_PE_MASK;
	PORTC_PCR7=PORT_PCR_MUX(1);
	PORTC_PCR8=PORT_PCR_MUX(1);
	PORTC_PCR9=PORT_PCR_MUX(1);
	PORTC_PCR10=PORT_PCR_MUX(1);
    KEY_PORT_OUT &= ~ROWS_MASK;            /* Preset all rows to zero    */
    // Initialize the Key Buffer and semaphore
    keyBuffer.buffer = 0x00;           /* Init KeyBuffer      */
    OSSemCreate(&(keyBuffer.flag),"Key Semaphore",0,&os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }
    //Create the key task
    OSTaskCreate((OS_TCB     *)&keyTaskTCB,
                (CPU_CHAR   *)"uCOS Key Task ",
                (OS_TASK_PTR ) keyTask,
                (void       *) 0,
                (OS_PRIO     ) APP_CFG_KEY_TASK_PRIO,
                (CPU_STK    *)&keyTaskStk[0],
                (CPU_STK     )(APP_CFG_KEY_TASK_STK_SIZE / 10u),
                (CPU_STK_SIZE) APP_CFG_KEY_TASK_STK_SIZE,
                (OS_MSG_QTY  ) 0,
                (OS_TICK     ) 0,
                (void       *) 0,
                (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                (OS_ERR     *)&os_err);
    while(os_err != OS_ERR_NONE){           /* Error Trap                        */
    }

}

/********************************************************************
* KeyTask() - Read the keypad and updates KeyBuffer. 
*             A task decomposed into states for detecting and
*             verifying keypresses. This task should be called
*             periodically with a period greater than the worst case
*             switch bounce time and less than the shortest switch
*             activation time minus the bounce time. The switch must 
*             be released to have multiple acknowledged presses.
* (Public)
********************************************************************/
static void keyTask(void *p_arg) {

    OS_ERR os_err;
    INT8U cur_key;
    INT8U last_key = 0;
    KEYSTATES KeyState = KEY_OFF;
    (void)p_arg;
    while(1){
		DB1_TURN_OFF();
        OSTimeDly(8,OS_OPT_TIME_PERIODIC,&os_err);
		DB1_TURN_ON();
        while(os_err != OS_ERR_NONE){           /* Error Trap                        */
        }
        cur_key = keyScan();
        if(KeyState == KEY_OFF){    /* Key released state */
            if(cur_key != 0){
                KeyState = KEY_EDGE;
            }else{ /* wait for key press */
            }
        }else if(KeyState == KEY_EDGE){     /* Keypress detected state*/
            if(cur_key == last_key){        /* Keypress verified */
                KeyState = KEY_VERF;
                keyBuffer.buffer = keyCodeTable[cur_key - 1]; /*update buffer */
                (void)OSSemPost(&(keyBuffer.flag), OS_OPT_POST_1, &os_err);   /* Signal new data in buffer */
                while(os_err != OS_ERR_NONE){           /* Error Trap                        */
                }
            }else if( cur_key == 0){        /* Unvalidated, start over */
                KeyState = KEY_OFF;
            }else{                          /*Unvalidated, diff key edge*/
            }
        }else if(KeyState == KEY_VERF){     /* Keypress verified state */
            if((cur_key == 0) || (cur_key != last_key)){
                KeyState = KEY_OFF;
            }else{ /* wait for release or key change */
            }
        }else{ /* In case of error */
            KeyState = KEY_OFF;             /* Should never get here */
        }
        last_key = cur_key;                 /* Save key for next time */
    
    }
}

/********************************************************************
* keyScan() - Scans the keypad and returns a keycode.
*           - Designed for 4x4 keypad with columns pulled high.
*           - Current keycodes follow:
*               1->0x01,2->0x02,3->0x03,A->0x04
*               4->0x05,5->0x06,6->0x07,B->0x08
*               7->0x09,8->0x0A,9->0x0B,C->0x0C
*               *->0x0D,0->0x0E,#->0x0F,D->0x10
*           - Returns zero if no key is pressed.
*           - ColTable[] can be changed to distinguish multiple keys
*             pressed in the same row.
* (Private)
********************************************************************/
static INT8U keyScan(void) {

    INT8U kcode;
    INT8U roff;
    INT32U rbit;

    const INT8U ColTable[16] = {0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4};

    rbit = 0x00000080;
    roff = 0x00;
    while(rbit != 0){ /* Until all rows are scanned */
        KEY_PORT_OUT &= ~ROWS_MASK;
        KEY_PORT_DIR = (KEY_PORT_DIR & ~ROWS_MASK)|rbit;    /* Pull row low */
        keyDly();	// wait for direction and col inputs to settle
        kcode = (INT8U)(((~KEY_PORT_IN) & COLS_MASK)>>3);  /*Read columns */
        KEY_PORT_DIR = (KEY_PORT_DIR &~ROWS_MASK); 
        if(kcode != 0){        /* generate key code if key pressed */
            kcode = roff + ColTable[kcode];
            break;
        }
        rbit = ROWS_MASK & (rbit<<1);       /* setup for next row */
        roff += 4;
    }
    return (kcode); 
}
/********************************************************************
 * keyDly() a software delay for keyScan() to wait until port row
 * bit direction and column inputs are settled .
 * With debug bits included and a 120Mhz bus the delay is 
 * 	Tdly = (83.2ns)i + 128ns
 * Currently set to ~830ns with i=10.
 * TDM 01/20/2013
 *******************************************************************/
static void keyDly(void){
	INT32U i;
	for(i=0;i<10;i++){
	}
}
