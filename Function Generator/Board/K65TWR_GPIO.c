/*****************************************************************************************
* K65TWR_GPIO.c - K65TWR GPIO support package
* Todd Morton, 10/08/2015
* Todd Morton, 11/25/2015 Modified for new Debug bits. See EE344, Lab5, 2015
* Todd Morton, 10/16/2017 Modified to remove includes.h
 ****************************************************************************************/
#include "MCUType.h"
#include "K65TWR_GPIO.h"

/*****************************************************************************************
* GpioSw3Init - Initialization for SW3 on the TWR-K65 board
* Parameters:
*   irqc - pass the desired value for the IRQC bits to control the interrupt configuration.
*       0 -> Interrupts disabled
*       1 DMA on rising-edge
*       2 DMA on falling-edge
*       3 DMA on either edge
*       4-7 Reserved
*       8 Interrupt when 0
*       9 Interrupt on rising-edge
*       10 Interrupt on falling-edge
*       11Interrupts on either edge
*       12 INterrupt when 1
*       13-15 Reserved
* 10/16/2017, TDM
 ****************************************************************************************/
void GpioSw3Init(INT8U irqc){

    SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK; /* Enable clock gate for PORTA */
    PORTA_PCR10 = PORT_PCR_MUX(1)|PORT_PCR_PE(1)|PORT_PCR_PS(1)|PORT_PCR_IRQC(irqc);
}

/*****************************************************************************************
* GpioSw2Init - Initialization for SW2 on the TWR-K65 board
* Parameters:
*   irqc - pass the desired value for the IRQC bits to control the interrupt configuration.
*       0 -> Interrupts disabled
*       1 DMA on rising-edge
*       2 DMA on falling-edge
*       3 DMA on either edge
*       4-7 Reserved
*       8 Interrupt when 0
*       9 Interrupt on rising-edge
*       10 Interrupt on falling-edge
*       11Interrupts on either edge
*       12 INterrupt when 1
*       13-15 Reserved
* 10/16/2017, TDM
 ****************************************************************************************/
void GpioSw2Init(INT8U irqc){
    SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK; /* Enable clock gate for PORTA */
    PORTA_PCR4 = PORT_PCR_MUX(1)|PORT_PCR_PE(1)|PORT_PCR_PS(1)|PORT_PCR_IRQC(irqc);
}

/*****************************************************************************************
* GpioLED8Init - Initialization for LED8 port to an output, LED off.
* 10/16/2017, TDM
 ****************************************************************************************/
void GpioLED8Init(void){

    SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK; /* Enable clock gate for PORTA */
    PORTA_PCR28 = PORT_PCR_MUX(1);
    GPIOA_PSOR |= GPIO_PIN(LED8_BIT);     /* Initialize off, activelow */
    GPIOA_PDDR |= GPIO_PIN(LED8_BIT);
}

/*****************************************************************************************
* GpioLED9Init - Initialization for LED9 port to an output, LED off.
* 10/16/2017, TDM
 ****************************************************************************************/
void GpioLED9Init(void){

    SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK; /* Enable clock gate for PORTA */
    PORTA_PCR29 = PORT_PCR_MUX(1);
    GPIOA_PSOR |= GPIO_PIN(LED9_BIT);     /* Initialize off, activelow */
    GPIOA_PDDR |= GPIO_PIN(LED9_BIT);
}
/*****************************************************************************************
* GpioDBugBitsInit - Initialization for Debug bits, each bit is cleared to 0.
* 10/16/2017, TDM
 ****************************************************************************************/
void GpioDBugBitsInit(void){
    SIM_SCGC5 |= SIM_SCGC5_PORTB(1);
    SIM_SCGC5 |= SIM_SCGC5_PORTC(1);
    PORTC_PCR15 = PORT_PCR_MUX(1);
    PORTC_PCR14 = PORT_PCR_MUX(1);
    PORTC_PCR13 = PORT_PCR_MUX(1);
    PORTC_PCR12 = PORT_PCR_MUX(1);
    PORTB_PCR23 = PORT_PCR_MUX(1);
    PORTB_PCR22 = PORT_PCR_MUX(1);
    PORTB_PCR21 = PORT_PCR_MUX(1);
    PORTB_PCR20 = PORT_PCR_MUX(1);
    GPIOC_PCOR = GPIO_PIN(DB0_BIT)|GPIO_PIN(DB1_BIT)|GPIO_PIN(DB2_BIT)|GPIO_PIN(DB3_BIT);
    GPIOB_PCOR = GPIO_PIN(DB4_BIT)|GPIO_PIN(DB5_BIT)|GPIO_PIN(DB6_BIT)|GPIO_PIN(DB7_BIT);
    GPIOC_PDDR = GPIO_PIN(DB0_BIT)|GPIO_PIN(DB1_BIT)|GPIO_PIN(DB2_BIT)|GPIO_PIN(DB3_BIT);
    GPIOB_PDDR = GPIO_PIN(DB4_BIT)|GPIO_PIN(DB5_BIT)|GPIO_PIN(DB6_BIT)|GPIO_PIN(DB7_BIT);
}

