/*******************************************************************************
* DMA.c -   A module of initializing DMA0, DAC0, and PIT0 to feed 12bit values
*           for a sinusoidal wave of 300Hz with 4 harmonies at 600Hz, 900Hz,
*           1200Hz, and 1500Hz. These values are taken by the DMA to the DAC
*           with triggers from the PIT.
*
* Created on: Dec 7, 2017
* Author: Anthony Needles
*******************************************************************************/
#include "MCUType.h"
#include "K65TWR_GPIO.h"
#include "app_cfg.h"
#include "os.h"
#include "Wave.h"
#include "DMA.h"

static INT8U dmaBufferRdyIndex;
static OS_SEM dmaBufferDoneFlag;

INT16U wavCurSamples[DMA_TWOBLOCKS][DMA_64SAMPLES_PERBLOCK];

/********************************************************************
* DMAInit - Initializes DMA0
*
* Description:  Enables DMA for use with transferring data in dmaWaveTable to
*               DAC0. Uses PIT0 for triggering. Disables the DMAMUX, the gives
*               the source table address, 16bit data size, 2byte increments, -128
*               end address change (return to original address). For destination
*               the address is the DAC0 data register, 0 byte offset, 16 bit size.
*               Minor loop size increment of 2 byes, with 64 samples total.
*               Scatter/gather processing turned on. The DMAMUX is then reenabled
*               with DMAMUX 0 selected.
*
*
*
* Return value: None
*
* Arguments:    None
********************************************************************/
void DMAInit(INT16U *out_block){
    OS_ERR os_err;
    OSSemCreate(&dmaBufferDoneFlag, "Buffer Done Flag", 0,&os_err);
    while(os_err != OS_ERR_NONE){}

    dmaBufferRdyIndex = 1;

    SIM_SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
    SIM_SCGC7 |= SIM_SCGC7_DMA_MASK;

    DMAMUX_CHCFG(0) |= (DMAMUX_CHCFG_ENBL(0)|DMAMUX_CHCFG_TRIG(0));
    DMA_SADDR(0) = DMA_SADDR_SADDR(out_block);
    DMA_ATTR(0) = (DMA_ATTR_SSIZE(DMA_16BIT_SAMPLES) | DMA_ATTR_SMOD(0) | DMA_ATTR_DMOD(0) | DMA_ATTR_DSIZE(DMA_16BIT_SAMPLES));
    DMA_SOFF(0) = DMA_SOFF_SOFF(DMA_2BYTES_PERSAMPLE);
    DMA_TCD0_NBYTES_MLNO = DMA_NBYTES_MLNO_NBYTES(DMA_2BYTES_PERSAMPLE);
    DMA_CITER_ELINKNO(0) = DMA_CITER_ELINKNO_ELINK(0)|DMA_CITER_ELINKNO_CITER(DMA_TWOBLOCKS*DMA_64SAMPLES_PERBLOCK);
    DMA_BITER_ELINKNO(0) = DMA_BITER_ELINKNO_ELINK(0)|DMA_BITER_ELINKNO_BITER(DMA_TWOBLOCKS*DMA_64SAMPLES_PERBLOCK);
    DMA_SLAST(0) = DMA_SLAST_SLAST(-(DMA_256BYTES_PERBUFFER));
    DMA_DADDR(0) = DMA_DADDR_DADDR(&DAC0_DAT0L);
    DMA_DOFF(0) = DMA_DOFF_DOFF(0);
    DMA_DLAST_SGA(0) = DMA_DLAST_SGA_DLASTSGA(0);
    DMA_TCD0_CSR = DMA_CSR_ESG(0) | DMA_CSR_MAJORELINK(0) | DMA_CSR_BWC(3) | DMA_CSR_INTHALF(1) | DMA_CSR_INTMAJOR(1) | DMA_CSR_DREQ(0);
    DMAMUX_CHCFG(0) = DMAMUX_CHCFG_ENBL(1)|DMAMUX_CHCFG_TRIG(1)|DMAMUX_CHCFG_SOURCE(60);
    NVIC_EnableIRQ(0);
    DMA_SERQ = DMA_SERQ_SERQ(0);

}
/********************************************************************
* DMADAC0Init - Initializes DAC0
*
* Description:  Enables DAC system, software trigger, and VDDA reference. Also
*               enables DMA and DAC buffer.
*
* Return value: None
*
* Arguments:    None
********************************************************************/
void DMADAC0Init(void){
    SIM_SCGC2 = (SIM_SCGC2 | SIM_SCGC2_DAC0(1));
    DAC0_C0 |= DAC_C0_DACEN(1) | DAC_C0_DACRFS(0) | DAC_C0_DACTRGSEL(1);
    DAC0_C1 |= (DAC_C1_DMAEN(1) | DAC_C1_DACBFEN(1));
    VREF_SC = VREF_SC_VREFEN(1)| VREF_SC_MODE_LV(1);
}
/********************************************************************
* DMAPIT0Init - Initializes PIT0
*
* Description:  Enables PIT clock 0. Enables all standard timers. Enables
*               PIT0 timer and PIT0 timer interrupt. Triggers at 300Hz.
*
* Return value: None
*
* Arguments:    None
********************************************************************/
void DMAPIT0Init(void){
    SIM_SCGC6 = (SIM_SCGC6 | SIM_SCGC6_PIT(1));
    PIT_MCR = PIT_MCR_MDIS(0);
    PIT_TCTRL0 = (PIT_TCTRL0 | PIT_TCTRL_TIE(1) | PIT_TCTRL_TEN(1));
    PIT_LDVAL0 = PIT0_TIMER_VALUE;
}

void DMA0_DMA16_IRQHandler(void){
    OS_ERR os_err;

    OSIntEnter();
    DMA_CINT = DMA_CINT_CINT(0);
    if(dmaBufferRdyIndex == 1){
        dmaBufferRdyIndex = 0;
    } else{
        dmaBufferRdyIndex = 1;
    }
    (void)OSSemPost(&dmaBufferDoneFlag, OS_OPT_POST_1, &os_err);
    while(os_err != OS_ERR_NONE){}

    OSIntExit();
}

INT8U DMABlockDonePend(OS_ERR *os_err){
    (void)OSSemPend(&dmaBufferDoneFlag,0,OS_OPT_PEND_BLOCKING,(CPU_TS *)0, os_err);
    return dmaBufferRdyIndex;
}
