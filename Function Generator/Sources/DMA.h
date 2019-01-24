/*******************************************************************************
* DMA.h -   Project header file for DMA.c
*
* Created on: Dec 7, 2017
* Author: Anthony Needles
*******************************************************************************/
#ifndef SOURCES_DMA_H_
#define SOURCES_DMA_H_

#define PIT0_TIMER_VALUE 1250
#define DMA_16BIT_SAMPLES 1
#define DMA_2BYTES_PERSAMPLE 2
#define DMA_64SAMPLES_PERBLOCK 64
#define DMA_256BYTES_PERBUFFER 256
#define DMA_TWOBLOCKS 2

extern INT16U wavCurSamples[DMA_TWOBLOCKS][DMA_64SAMPLES_PERBLOCK];

/********************************************************************
* DMAInit - Initializes DMA0
*
* Description:  Enables DMA for use with transferring data in dmaWaveTable to
*               DAC0. Uses PIT0 for triggering.
*
* Return value: None
*
* Arguments:    None
********************************************************************/
void DMAInit(INT16U *out_block);
/********************************************************************
* DMADAC0Init - Initializes DAC0
*
* Description:  Enables both DAC0 clocks. Enables DAC system, software trigger,
*               and VDDA reference. Also enables DMA and DAC buffer.
*
* Return value: None
*
* Arguments:    None
********************************************************************/
void DMADAC0Init(void);
/********************************************************************
* DMAPIT0Init - Initializes PIT0
*
* Description:  Enables PIT clock. Enables all standard timers. Enables
*               PIT0 timer and PIT0 timer interrupt. Triggers at 300Hz.
*
* Return value: None
*
* Arguments:    None
********************************************************************/
void DMAPIT0Init(void);

void DMA0_DMA16_IRQHandler(void);

INT8U DMABlockDonePend(OS_ERR *os_err);

#endif /* SOURCES_DMAV1_H_ */
