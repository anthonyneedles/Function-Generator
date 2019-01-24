/*
 * TSI.h
 * Touch Sensing Input
 *
 * Contains all the code reqired to use the K65's Touch Pads using the Cortex M4 TSI hardware
 *
 *  Created on: Dec 4, 2017
 *      Author: mesecar
 *
 */

#ifndef BOARD_TSI_H_
#define BOARD_TSI_H_

// Initializes the touch Sensor Module
void TSIInit(void);

// When called, pends on the TSItouchBufferKey and returns a copy of the TsiFlags and clears TsiFlags.
// 0 = None; 1 = Electrode 1; 2 = Electrode 2; 3 = both Electrode 1 & 2
INT8U TouchPend(OS_ERR *os_err);

#endif /* BOARD_TSI_H_ */
