/********************************************************************
* KeyUcos.h - The header file for the keypad module, KeyUcos.c
* 02/15/2006 Todd Morton
*********************************************************************
* Public Resources
********************************************************************/
#ifndef UC_KEY_DEF
#define UC_KEY_DEF

INT8U KeyPend(INT16U tout, OS_ERR *os_err); /* Pend on key press*/
                             /* tout - semaphore timeout           */
                             /* *err - destination of err code     */
                             /* Error codes are identical to a semaphore */

void KeyInit(void);             /* Keypad Initialization    */

#endif
