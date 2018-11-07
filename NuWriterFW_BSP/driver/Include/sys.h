/**************************************************************************//**
 * @file     sys.h
 * @brief    NUC980 SYS driver header file
 *
 * @copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/

#ifndef __SYS_H__
#define __SYS_H__

#include "nuc980.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @addtogroup NUC980_Device_Driver NUC980 Device Driver
  @{
*/

/** @addtogroup NUC980_SYS_Driver SYS Driver
  @{
*/

/** @addtogroup NUC980_SYS_EXPORTED_CONSTANTS SYS Exported Constants
  @{
*/

/**
 * @details  Interrupt Number Definition.
 */
typedef enum IRQn {
    IRQ_WDT = 1,        // Watch Dog Timer
    IRQ_WWDT = 2,       // Windowed-WDT Interrupt
    IRQ_LVD = 3,        // LVD Interrupt
    IRQ_EXTI0 = 4,      // External Interrupt 0
    IRQ_EXTI1 = 5,      // External Interrupt 1
    IRQ_EXTI2 = 6,      // External Interrupt 2
    IRQ_EXTI3 = 7,      // External Interrupt 3
    IRQ_GPA = 8,        // GPA Interrupt
    IRQ_GPB = 9,        // GPB Interrupt
    IRQ_GPC = 10,       // GPC Interrupt
    IRQ_GPD = 11,       // GPD Interrupt
    IRQ_I2S = 12,       // I2S Interrupt
    IRQ_CAP0 = 14,      // Sensor Interface Controller Interrupt
    IRQ_RTC = 15,       // RTC interrupt
    IRQ_TIMER0 = 16,    // Timer 0 interrupt
    IRQ_TIMER1 = 17,    // Timer 1 interrupt
    IRQ_ADC = 18,       // ADC interrupt
    IRQ_EMC0_RX = 19,   // EMC 0 RX Interrupt
    IRQ_EMC1_RX = 20,   // EMC 1 RX Interrupt
    IRQ_EMC0_TX = 21,   // EMC 0 TX Interrupt
    IRQ_EMC1_TX = 22,   // EMC 1 TX Interrupt
    IRQ_EHCI = 23,      // USB 2.0 Host Controller Interrupt
    IRQ_OHCI = 24,      // USB 1.1 Host Controller Interrupt
    IRQ_PDMA0 = 25,     // PDMA Channel 0 Interrupt
    IRQ_PDMA1 = 26,     // PDMA Channel 1 Interrupt
    IRQ_SDH = 27,       // SD Host Interrupt
    IRQ_FMI = 28,       // NAND/eMMC Interrupt
    IRQ_UDC = 29,       // USB Device Controller Interrupt
    IRQ_TIMER2 = 30,    // Timer 2 interrupt
    IRQ_TIMER3 = 31,    // Timer 3 interrupt
    IRQ_TIMER4 = 32,    // Timer 4 interrupt
    IRQ_CAP1 = 33,      // VCAP1 Engine Interrupt
    IRQ_TIMER5 = 34,    // Timer 5 interrupt
    IRQ_CRYPTO = 35,    // CRYPTO Engine Interrupt
    IRQ_UART0 = 36,     // UART 0 interrupt
    IRQ_UART1 = 37,     // UART 1 interrupt
    IRQ_UART2 = 38,     // UART 2 interrupt
    IRQ_UART4 = 39,     // UART 4 interrupt
    IRQ_UART6 = 40,     // UART 6 interrupt
    IRQ_UART8 = 41,     // UART 8 interrupt
    IRQ_CAN3  = 42,     // CAN  3 interrupt
    IRQ_UART3 = 43,     // UART 3 interrupt
    IRQ_UART5 = 44,     // UART 5 interrupt
    IRQ_UART7 = 45,     // UART 7 interrupt
    IRQ_UART9 = 46,     // UART 9 interrupt
    IRQ_I2C2 = 47,      // I2C 2 interrupt
    IRQ_I2C3 = 48,      // I2C 3 interrupt
    IRQ_GPE = 49,       // GPE interrupt
    IRQ_SPI1 = 50,      // SPI 1 interrupt
    IRQ_QSPI0 = 51,     // QSPI 0 interrupt
    IRQ_SPI0 = 52,      // SPI 0 interrupt
    IRQ_I2C0 = 53,      // I2C 0 Interrupt
    IRQ_I2C1 = 54,      // I2C 1 Interrupt
    IRQ_SMC0 = 55,      // SmartCard 0 Interrupt
    IRQ_SMC1 = 56,      // SmartCard 1 Interrupt
    IRQ_GPF = 57,       // GPF interrupt
    IRQ_CAN0 = 58,      // CAN 0 interrupt
    IRQ_CAN1 = 59,      // CAN 1 interrupt
    IRQ_PWM0 = 60,      // PWM 0 interrupt
    IRQ_PWM1 = 61,      // PWM 1 interrupt
    IRQ_CAN2 = 62,      // CAN 2 interrupt
    IRQ_GPG = 63,       // GPG interrupt
}
IRQn_Type;

/* Define constants for use AIC in service parameters.  */
#define SYS_SWI                     0
#define SYS_D_ABORT                 1
#define SYS_I_ABORT                 2
#define SYS_UNDEFINE                3

/* The parameters for sysSetInterruptPriorityLevel() and
   sysInstallISR() use */
#define FIQ_LEVEL_0     0       /*!< FIQ Level 0 */
#define IRQ_LEVEL_1     1       /*!< IRQ Level 1 */
#define IRQ_LEVEL_2     2       /*!< IRQ Level 2 */
#define IRQ_LEVEL_3     3       /*!< IRQ Level 3 */
#define IRQ_LEVEL_4     4       /*!< IRQ Level 4 */
#define IRQ_LEVEL_5     5       /*!< IRQ Level 5 */
#define IRQ_LEVEL_6     6       /*!< IRQ Level 6 */
#define IRQ_LEVEL_7     7       /*!< IRQ Level 7 */


/* The parameters for sysSetLocalInterrupt() use */
#define ENABLE_IRQ        0x7F  /*!< Enable I-bit of CP15  */
#define ENABLE_FIQ        0xBF  /*!< Enable F-bit of CP15  */
#define ENABLE_FIQ_IRQ    0x3F  /*!< Enable I-bit and F-bit of CP15  */
#define DISABLE_IRQ       0x80  /*!< Disable I-bit of CP15  */
#define DISABLE_FIQ       0x40  /*!< Disable F-bit of CP15  */
#define DISABLE_FIQ_IRQ   0xC0  /*!< Disable I-bit and F-bit of CP15  */

/* Define Cache type  */
#define CACHE_WRITE_BACK        0     /*!< Cache Write-back mode  */
#define CACHE_WRITE_THROUGH     1     /*!< Cache Write-through mode  */
#define CACHE_DISABLE           -1    /*!< Cache Disable  */

/** \brief  Structure type of clock source
 */
typedef enum CLKn {

    SYS_UPLL     = 1,   /*!< UPLL clock */
    SYS_APLL     = 2,   /*!< APLL clock */
    SYS_SYSTEM   = 3,   /*!< System clock */
    SYS_HCLK     = 4,   /*!< HCLK1 clock */
    SYS_PCLK01   = 5,   /*!< HCLK234 clock */
    SYS_PCLK2    = 6,   /*!< PCLK clock */
    SYS_CPU      = 7,   /*!< CPU clock */

}  CLK_Type;

/* The parameters for sysSetGlobalInterrupt() use */
#define ENABLE_ALL_INTERRUPTS      0
#define DISABLE_ALL_INTERRUPTS     1

#define MMU_DIRECT_MAPPING  0

/* Define constants for use Cache in service parameters.  */
#define I_CACHE         6
#define D_CACHE         7
#define I_D_CACHE       8


/// @endcond HIDDEN_SYMBOLS

/*@}*/ /* end of group NUC980_SYS_EXPORTED_CONSTANTS */


/** @addtogroup NUC980_SYS_EXPORTED_FUNCTIONS SYS Exported Functions
  @{
*/

/* Define system library AIC functions */
INT32   sysDisableInterrupt (IRQn_Type eIntNo);
INT32   sysEnableInterrupt (IRQn_Type eIntNo);
BOOL    sysGetIBitState(void);
UINT32  sysGetInterruptEnableStatus(void);
UINT32  sysGetInterruptEnableStatusH(void);
PVOID   sysInstallExceptionHandler (INT32 nExceptType, PVOID pvNewHandler);
PVOID   sysInstallFiqHandler (PVOID pvNewISR);
PVOID   sysInstallIrqHandler (PVOID pvNewISR);
PVOID   sysInstallISR (INT32 nIntTypeLevel, IRQn_Type eIntNo, PVOID pvNewISR);
INT32   sysSetGlobalInterrupt (INT32 nIntState);
INT32   sysSetInterruptPriorityLevel (IRQn_Type eIntNo, UINT32 uIntLevel);
INT32   sysSetInterruptType (IRQn_Type eIntNo, UINT32 uIntSourceType);
INT32   sysSetLocalInterrupt (INT32 nIntState);


/* Define system library Cache functions */
void    sysDisableCache(void);
INT32   sysEnableCache(UINT32 uCacheOpMode);
void    sysFlushCache(INT32 nCacheType);
BOOL    sysGetCacheState(void);
INT32   sysGetSdramSizebyMB(void);
void    sysInvalidCache(void);

UINT32 sysGetClock(CLK_Type clk);

typedef void (*sys_pvFunPtr)();   /* function pointer */
extern sys_pvFunPtr sysIrqHandlerTable[];
extern BOOL volatile _sys_bIsAICInitial;

void SYS_UnlockReg(void);
void SYS_LockReg(void);

#ifdef __cplusplus
}
#endif

/*@}*/ /* end of group NUC980_SYS_EXPORTED_FUNCTIONS */

/*@}*/ /* end of group NUC980_SYS_Driver */

/*@}*/ /* end of group NUC980_Device_Driver */

#endif //__SYS_H__


