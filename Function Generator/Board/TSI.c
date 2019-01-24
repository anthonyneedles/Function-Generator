/*
 * TSI.c
 *
 * Touch Sensing Interface
 *
 * Contains all the code reqired to use the K65's Touch Pads using the Cortex M4 TSI hardware
 *
 *
 *  Created on: Dec 4, 2017
 *      Author: mesecar
 *
 * 	Modified to work with Micro C/OS
 * 	~Rod Mesecar, 2/12/18
 *
 * 	Added TouchPend function
 * 	~Rod Mesecar. 2/13/18
 */
#include "MCUType.h"
#include "app_cfg.h"
#include "os.h"
#include "TSI.h"
#include "K65TWR_GPIO.h" // Here for debug bit

// Defined Constants
#define ELEC1 0U
#define ELEC2 1U
#define ELEC1_LEVEL_OFFSET 500U // Offset needed for detecting a touch to electrode 1
#define ELEC2_LEVEL_OFFSET 500U// Offset needed for detecting a touch to electrode 2
#define SENSOR_TASK_DELAY 10U // Delay for the TSI Task (about every 18ms required for sensing input)
#define ELECTRODE_BASE_VALUE 12U

// Micro C/OS stuff
static OS_TCB TSITaskTCB;
static CPU_STK TSITaskStk[APP_CFG_TSI_TASK_STK_SIZE];
static OS_SEM TSINewTouchFlag;

// Private Prototypes
void TSISensorTask(void *p_arg);

// Variables
static INT16U tsiTouchLevel[2]; // Holds the threshold for what is considered a touch
static INT8U TsiFlags =0; // Flags indicate which of the sensors are currently active
                  //1 = Electrode 1; 2 = Electrode 2; 3 = both Electrode 1 & 2

// Touch Sensing Input Initialization code
// Sets up the system to be able to use TSI.
void TSIInit(void){
	INT16U tsibaselevel[2];
	OS_ERR os_err;

	// Clock
    SIM_SCGC5 |= SIM_SCGC5_PORTB_MASK |SIM_SCGC5_TSI_MASK;

    //Electrode Bit Muxes
    PORTB_PCR18 |= PORT_PCR_MUX(0);
    PORTB_PCR19 |= PORT_PCR_MUX(0);

    //Configure
    TSI0_GENCS |= TSI_GENCS_REFCHRG(5)|TSI_GENCS_DVOLT(1)|TSI_GENCS_PS(5)|TSI_GENCS_NSCN(15)|TSI_GENCS_EXTCHRG(5);

    //Enable
    TSI0_GENCS |= TSI_GENCS_TSIEN_MASK;

    //Perform Calculations
    //ELEC1
    TSI0_DATA= TSI_DATA_TSICH(12);                                              // Designate Electrode 1 for scan
    TSI0_DATA |= TSI_DATA_SWTS(1);                                              // Scan
    while(!(TSI0_GENCS & TSI_GENCS_EOSF_MASK)){}                                // Blocking code - Wait for scan to complete
    TSI0_GENCS |= TSI_GENCS_EOSF(1);                                            //Clear the flag
    tsibaselevel[ELEC1] = (INT16U)(TSI0_DATA & TSI_DATA_TSICNT_MASK);           // Set Base level for Electrode 1
    tsiTouchLevel[ELEC1] = tsibaselevel[ELEC1] + (INT16U)ELEC1_LEVEL_OFFSET;    // Set trigger level for Electrode 1

    //ELEC2
    TSI0_DATA= TSI_DATA_TSICH(11);                                              // Designate Electrode 2
    TSI0_DATA |= TSI_DATA_SWTS(1);                                              // Scan
    while(!(TSI0_GENCS & TSI_GENCS_EOSF_MASK)){}                                // Blocking code - Wait for scan to complete
    TSI0_GENCS |= TSI_GENCS_EOSF(1);                                            // Clear flag
    tsibaselevel[ELEC2] = (INT16U)(TSI0_DATA & TSI_DATA_TSICNT_MASK);           // Set base level for Electrode 2
    tsiTouchLevel[ELEC2] = tsibaselevel[ELEC2] + (INT16U)ELEC2_LEVEL_OFFSET;    // Set base level for Electrode 2

    OSSemCreate(&TSINewTouchFlag, "New Touch Flag", 0,&os_err);
    while(os_err != OS_ERR_NONE){}              //Error Trap

    // Create Micro C/OS task
    OSTaskCreate(&TSITaskTCB,
                     "TSI Sensor Task",
					 TSISensorTask,
                     (void *) 0,
					 APP_CFG_TSI_TASK_PRIO,
                     &TSITaskStk[0],
                     (APP_CFG_TSI_TASK_STK_SIZE / 10u),
                     APP_CFG_TSI_TASK_STK_SIZE,
                     0,
                     0,
                     (void *) 0,
                     (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                     &os_err);
    while(os_err != OS_ERR_NONE){}              //Error Trap
}


// Handles all the Scanning and value setting.
void TSISensorTask(void *p_arg){
    (void)p_arg;
    OS_ERR os_err;
    static INT8U which_electrode = ELEC1; // Initialize with Electrode 1 First
    static INT8U last_touch = 0;
    static INT8U second_to_last_touch = 0;
    while(1){
        //Wait
        DB2_TURN_OFF();
        OSTimeDly(SENSOR_TASK_DELAY,OS_OPT_TIME_PERIODIC,&os_err);
        DB2_TURN_ON();

        if ((TSI0_GENCS & TSI_GENCS_SCNIP_MASK) == 0){ // No Scan Active
            // Check for touch
            if ((INT16U)(TSI0_DATA & TSI_DATA_TSICNT_MASK)>tsiTouchLevel[which_electrode]){ //Touch Detected
                if(which_electrode == ELEC1){
                    TsiFlags = 1;
                }else {
                    TsiFlags = 2;
                }
            }else{// No touch
                TsiFlags =0;
            }

            // Check for 'deboucne' (must remove finger from sensor)
            if((last_touch != TsiFlags) && (TsiFlags != 0)&& (second_to_last_touch != TsiFlags)){
            	OSSemPost(&TSINewTouchFlag, OS_OPT_POST_1, &os_err);
            }else{
            	// Do nothing.
            }
            second_to_last_touch =last_touch;
            last_touch = TsiFlags;

            // Change the Electrode to Scan.
            if (which_electrode == ELEC1){
                which_electrode = ELEC2;
            }else if (which_electrode == ELEC2){
                which_electrode = ELEC1;
            }else{ // ERROR, SHOULD NEVER BE THE CASE
            // Default to Electrode 1
                which_electrode = ELEC1;
            }
            // Start next scan
            TSI0_DATA= TSI_DATA_TSICH((ELECTRODE_BASE_VALUE - which_electrode)); // if which_electrode is ELECT1, 12-0 = 12: if which_electrode is ELECT2, 12-1 = 11
            TSI0_DATA |= TSI_DATA_SWTS(1);
        }else{ // Scan is in Progress
        // Do Nothing
        }
    }
}

// When called, pends on the TSItouchBufferKey and returns a copy of the TsiFlags
//1 = Electrode 1; 2 = Electrode 2;
//Design Inspired by Prof. Morten's KeyPend code in uCOSKEY module
INT8U TouchPend(OS_ERR *os_err){
    OSSemPend(&TSINewTouchFlag, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, os_err);
    return TsiFlags;
}
