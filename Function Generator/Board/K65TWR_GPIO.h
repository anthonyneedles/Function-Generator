/***************************************************************************************
* K65TWR_GPIO.h - K65TWR GPIO support package
* Todd Morton, 10/08/2015
* Todd Morton, 11/25/2015 Modified for new Debug bits. See EE344, Lab5, 2015
****************************************************************************************/

#ifndef GPIO_H_
#define GPIO_H_

void GpioSw3Init(INT8U irqc);
void GpioSw2Init(INT8U irqc);
void GpioLED8Init(void);
void GpioLED9Init(void);
void GpioDBugBitsInit(void);

/****************************************************************************************
 * Pin macro
 ***************************************************************************************/
#define GPIO_PIN(x) (((1)<<(x & 0x1FU)))
/****************************************************************************************
 * Switch defines for SW2 (PTA4), SW3 (PTA10), LED8 (PTA28), and LED9 (PTA29)
 ***************************************************************************************/
#define LED8_BIT 28U
#define LED9_BIT 29U

#define LED8_TURN_OFF()  (GPIOA_PSOR = GPIO_PIN(LED8_BIT))
#define LED8_TURN_ON() (GPIOA_PCOR = GPIO_PIN(LED8_BIT))
#define LED8_TOGGLE() (GPIOA_PTOR = GPIO_PIN(LED8_BIT))

#define LED9_TURN_OFF()  (GPIOA_PSOR = GPIO_PIN(LED9_BIT))
#define LED9_TURN_ON() (GPIOA_PCOR = GPIO_PIN(LED9_BIT))
#define LED9_TOGGLE() (GPIOA_PTOR = GPIO_PIN(LED9_BIT))

#define SW2_BIT 4U
#define SW3_BIT 10U
#define SW2_INPUT (GPIOA_PDIR & GPIO_PIN(SW2_BIT))
#define SW3_INPUT (GPIOA_PDIR & GPIO_PIN(SW3_BIT))

#define SW2_ISF (PORTA_ISFR & GPIO_PIN(SW2_BIT))

#define SW2_INIT_INT() (PORTA_PCR26=PORT_PCR_MUX(1)|PORT_PCR_PE_MASK|\
    PORT_PCR_PS_MASK|PORT_PCR_IRQC(9))
#define SW2_CLR_ISF() (PORTA_ISFR = GPIO_PIN(SW2_BIT))

/****************************************************************************************
 * #defines for debug bits
 ***************************************************************************************/
#define DB0_BIT 15
#define DB1_BIT 14
#define DB2_BIT 13
#define DB3_BIT 12
#define DB4_BIT 23
#define DB5_BIT 22
#define DB6_BIT 21
#define DB7_BIT 20

#define DB0_TURN_ON() (GPIOC_PSOR = GPIO_PIN(DB0_BIT))
#define DB1_TURN_ON() (GPIOC_PSOR = GPIO_PIN(DB1_BIT))
#define DB2_TURN_ON() (GPIOC_PSOR = GPIO_PIN(DB2_BIT))
#define DB3_TURN_ON() (GPIOC_PSOR = GPIO_PIN(DB3_BIT))
#define DB4_TURN_ON() (GPIOB_PSOR = GPIO_PIN(DB4_BIT))
#define DB5_TURN_ON() (GPIOB_PSOR = GPIO_PIN(DB5_BIT))
#define DB6_TURN_ON() (GPIOB_PSOR = GPIO_PIN(DB6_BIT))
#define DB7_TURN_ON() (GPIOB_PSOR = GPIO_PIN(DB7_BIT))

#define DB0_TURN_OFF() (GPIOC_PCOR = GPIO_PIN(DB0_BIT))
#define DB1_TURN_OFF() (GPIOC_PCOR = GPIO_PIN(DB1_BIT))
#define DB2_TURN_OFF() (GPIOC_PCOR = GPIO_PIN(DB2_BIT))
#define DB3_TURN_OFF() (GPIOC_PCOR = GPIO_PIN(DB3_BIT))
#define DB4_TURN_OFF() (GPIOB_PCOR = GPIO_PIN(DB4_BIT))
#define DB5_TURN_OFF() (GPIOB_PCOR = GPIO_PIN(DB5_BIT))
#define DB6_TURN_OFF() (GPIOB_PCOR = GPIO_PIN(DB6_BIT))
#define DB7_TURN_OFF() (GPIOB_PCOR = GPIO_PIN(DB7_BIT))

#define DB0_TOGGLE() (GPIOC_PTOR = GPIO_PIN(DB0_BIT))
#define DB1_TOGGLE() (GPIOC_PTOR = GPIO_PIN(DB1_BIT))
#define DB2_TOGGLE() (GPIOC_PTOR = GPIO_PIN(DB2_BIT))
#define DB3_TOGGLE() (GPIOC_PTOR = GPIO_PIN(DB3_BIT))
#define DB4_TOGGLE() (GPIOB_PTOR = GPIO_PIN(DB4_BIT))
#define DB5_TOGGLE() (GPIOB_PTOR = GPIO_PIN(DB5_BIT))
#define DB6_TOGGLE() (GPIOB_PTOR = GPIO_PIN(DB6_BIT))
#define DB7_TOGGLE() (GPIOB_PTOR = GPIO_PIN(DB7_BIT))
#endif /* DBUGBITS_H_ */
