/*
 * Wave.c
 *  Source File for the Wave generator module.
 *
 *
 *  Created on: Feb 9, 2018
 *      Author: Mesecar
 */

#ifndef SOURCES_WAVE_H_
#define SOURCES_WAVE_H_

typedef enum {TRI, SIN} WAVE_TYPE;

typedef struct{
    INT16U freq;
    INT8U amp;
    WAVE_TYPE waveshape;
} WAVE_W;

/*
 * WaveInit()
 * Public Function
 *
 * Initializes the Wave module:
 *
 * ~Rod Mesecar, 2/9/18
 */
void WaveInit(void);

/*
 * WaveGet()
 * Public Function
 *
 * When Called, pends on WaveMutexKey and copies the contents of
 * CurrentSignal to the address of the passed pointer.
 *
 * ~Rod Mesecar, 2/9/18
 */
void WaveGet(WAVE_W *passwave);


/*
 * WaveSet()
 * Public Function
 *
 * When Called, pends on WaveMutexKey and copies the contents at
 * the address of the passed pointer to CurrentSignal
 *
 * ~Rod Mesecar, 2/9/18
 */
void WaveSet(WAVE_W *passwave);


/*
 * WaveUpdateIndex()
 * Public Function
 *
 * When called, copies the DMABufferIndex to the passed value to the  and posts the BufferDoneFlag
 *
 * ~Rod Mesecar, 2/10/18
 *
 *  UPDATE: Now copies DMABufferIndex to the pointer, was copies passed values to DMABufferIndex
 *  Header updated to match new function
 *
 *  ~Rod Mesecar 2/13/18
 *
 */
void WaveUpdateIndex (INT8U *blockIndex);


#endif /* SOURCES_WAVE_H_ */
