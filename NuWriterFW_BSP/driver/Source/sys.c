/**************************************************************************//**
 * @file     sys.c
 * @brief    NUC980 SYS driver source file
 *
 * @copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/
#include <stdio.h>
#include "nuc980.h"
#include "sys.h"

/// @cond HIDDEN_SYMBOLS

#define SYS_MIN_INT_SOURCE       1
#define SYS_MAX_INT_SOURCE       63
#define SYS_NUM_OF_AICREG        16

/* Global variables */
BOOL volatile _sys_bIsAICInitial = FALSE;

/* declaration the function prototype */
void SYS_IRQ_Shell(void);
void SYS_FIQ_Shell(void);

/* Interrupt Handler Table */
//typedef void (*sys_pvFunPtr)();   /* function pointer */
sys_pvFunPtr sysIrqHandlerTable[] = { 0,           /* 0 */
                                      SYS_IRQ_Shell,   /* 1 */
                                      SYS_IRQ_Shell,   /* 2 */
                                      SYS_IRQ_Shell,   /* 3 */
                                      SYS_IRQ_Shell,   /* 4 */
                                      SYS_IRQ_Shell,   /* 5 */
                                      SYS_IRQ_Shell,   /* 6 */
                                      SYS_IRQ_Shell,   /* 7 */
                                      SYS_IRQ_Shell,   /* 8 */
                                      SYS_IRQ_Shell,   /* 9 */
                                      SYS_IRQ_Shell,   /* 10 */
                                      SYS_IRQ_Shell,   /* 11 */
                                      SYS_IRQ_Shell,   /* 12 */
                                      SYS_IRQ_Shell,   /* 13 */
                                      SYS_IRQ_Shell,   /* 14 */
                                      SYS_IRQ_Shell,   /* 15 */
                                      SYS_IRQ_Shell,   /* 16 */
                                      SYS_IRQ_Shell,   /* 17 */
                                      SYS_IRQ_Shell,   /* 18 */
                                      SYS_IRQ_Shell,   /* 19 */
                                      SYS_IRQ_Shell,   /* 20 */
                                      SYS_IRQ_Shell,   /* 21 */
                                      SYS_IRQ_Shell,   /* 22 */
                                      SYS_IRQ_Shell,   /* 23 */
                                      SYS_IRQ_Shell,   /* 24 */
                                      SYS_IRQ_Shell,   /* 25 */
                                      SYS_IRQ_Shell,   /* 26 */
                                      SYS_IRQ_Shell,   /* 27 */
                                      SYS_IRQ_Shell,   /* 28 */
                                      SYS_IRQ_Shell,   /* 29 */
                                      SYS_IRQ_Shell,   /* 30 */
                                      SYS_IRQ_Shell,   /* 31 */
                                      SYS_IRQ_Shell,   /* 32 */
                                      SYS_IRQ_Shell,   /* 33 */
                                      SYS_IRQ_Shell,   /* 34 */
                                      SYS_IRQ_Shell,   /* 35 */
                                      SYS_IRQ_Shell,   /* 36 */
                                      SYS_IRQ_Shell,   /* 37 */
                                      SYS_IRQ_Shell,   /* 38 */
                                      SYS_IRQ_Shell,   /* 39 */
                                      SYS_IRQ_Shell,   /* 40 */
                                      SYS_IRQ_Shell,   /* 41 */
                                      SYS_IRQ_Shell,   /* 42 */
                                      SYS_IRQ_Shell,   /* 43 */
                                      SYS_IRQ_Shell,   /* 44 */
                                      SYS_IRQ_Shell,   /* 45 */
                                      SYS_IRQ_Shell,   /* 46 */
                                      SYS_IRQ_Shell,   /* 47 */
                                      SYS_IRQ_Shell,   /* 48 */
                                      SYS_IRQ_Shell,   /* 49 */
                                      SYS_IRQ_Shell,   /* 50 */
                                      SYS_IRQ_Shell,   /* 51 */
                                      SYS_IRQ_Shell,   /* 52 */
                                      SYS_IRQ_Shell,   /* 53 */
                                      SYS_IRQ_Shell,   /* 54 */
                                      SYS_IRQ_Shell,   /* 55 */
                                      SYS_IRQ_Shell,   /* 56 */
                                      SYS_IRQ_Shell,   /* 57 */
                                      SYS_IRQ_Shell,   /* 58 */
                                      SYS_IRQ_Shell,   /* 59 */
                                      SYS_IRQ_Shell,   /* 60 */
                                      SYS_IRQ_Shell,   /* 61 */
                                      SYS_IRQ_Shell,   /* 62 */
                                      SYS_IRQ_Shell    /* 63 */
                                    };

sys_pvFunPtr sysFiqHandlerTable[] = { 0,
                                      SYS_FIQ_Shell,   /* 1 */
                                      SYS_FIQ_Shell,   /* 2 */
                                      SYS_FIQ_Shell,   /* 3 */
                                      SYS_FIQ_Shell,   /* 4 */
                                      SYS_FIQ_Shell,   /* 5 */
                                      SYS_FIQ_Shell,   /* 6 */
                                      SYS_FIQ_Shell,   /* 7 */
                                      SYS_FIQ_Shell,   /* 8 */
                                      SYS_FIQ_Shell,   /* 9 */
                                      SYS_FIQ_Shell,   /* 10 */
                                      SYS_FIQ_Shell,   /* 11 */
                                      SYS_FIQ_Shell,   /* 12 */
                                      SYS_FIQ_Shell,   /* 13 */
                                      SYS_FIQ_Shell,   /* 14 */
                                      SYS_FIQ_Shell,   /* 15 */
                                      SYS_FIQ_Shell,   /* 16 */
                                      SYS_FIQ_Shell,   /* 17 */
                                      SYS_FIQ_Shell,   /* 18 */
                                      SYS_FIQ_Shell,   /* 19 */
                                      SYS_FIQ_Shell,   /* 20 */
                                      SYS_FIQ_Shell,   /* 21 */
                                      SYS_FIQ_Shell,   /* 22 */
                                      SYS_FIQ_Shell,   /* 23 */
                                      SYS_FIQ_Shell,   /* 24 */
                                      SYS_FIQ_Shell,   /* 25 */
                                      SYS_FIQ_Shell,   /* 26 */
                                      SYS_FIQ_Shell,   /* 27 */
                                      SYS_FIQ_Shell,   /* 28 */
                                      SYS_FIQ_Shell,   /* 29 */
                                      SYS_FIQ_Shell,   /* 30 */
                                      SYS_FIQ_Shell,   /* 31 */
                                      SYS_FIQ_Shell,   /* 32 */
                                      SYS_FIQ_Shell,   /* 33 */
                                      SYS_FIQ_Shell,   /* 34 */
                                      SYS_FIQ_Shell,   /* 35 */
                                      SYS_FIQ_Shell,   /* 36 */
                                      SYS_FIQ_Shell,   /* 37 */
                                      SYS_FIQ_Shell,   /* 38 */
                                      SYS_FIQ_Shell,   /* 39 */
                                      SYS_FIQ_Shell,   /* 40 */
                                      SYS_FIQ_Shell,   /* 41 */
                                      SYS_FIQ_Shell,   /* 42 */
                                      SYS_FIQ_Shell,   /* 43 */
                                      SYS_FIQ_Shell,   /* 44 */
                                      SYS_FIQ_Shell,   /* 45 */
                                      SYS_FIQ_Shell,   /* 46 */
                                      SYS_FIQ_Shell,   /* 47 */
                                      SYS_FIQ_Shell,   /* 48 */
                                      SYS_FIQ_Shell,   /* 49 */
                                      SYS_FIQ_Shell,   /* 50 */
                                      SYS_FIQ_Shell,   /* 51 */
                                      SYS_FIQ_Shell,   /* 52 */
                                      SYS_FIQ_Shell,   /* 53 */
                                      SYS_FIQ_Shell,   /* 54 */
                                      SYS_FIQ_Shell,   /* 55 */
                                      SYS_FIQ_Shell,   /* 56 */
                                      SYS_FIQ_Shell,   /* 57 */
                                      SYS_FIQ_Shell,   /* 58 */
                                      SYS_FIQ_Shell,   /* 59 */
                                      SYS_FIQ_Shell,   /* 60 */
                                      SYS_FIQ_Shell,   /* 61 */
                                      SYS_FIQ_Shell,   /* 62 */
                                      SYS_FIQ_Shell    /* 63 */
                                    };

/* Interrupt Handler */
__irq void sysIrqHandler()
{
    UINT32 volatile num;

    num = inpw(REG_AIC_IRQNUM);
    if (num != 0)
        (*sysIrqHandlerTable[num])();
    outpw(REG_AIC_EOIS, 1);
}

__irq void sysFiqHandler()
{
    UINT32 volatile num;

    num = inpw(REG_AIC_FIQNUM);
    if (num != 0)
        (*sysIrqHandlerTable[num])();
    outpw(REG_AIC_EOFS, 1);
}

void SYS_IRQ_Shell(void)
{
    printf("ISR not found! ISNR=%d\n", inpw(REG_AIC_IRQNUM));
}

void SYS_FIQ_Shell(void)
{
    printf("ISR not found! ISNR=%d\n", inpw(REG_AIC_FIQNUM));
}

void sysInitializeAIC()
{
    *(unsigned int volatile *)0x38 = (unsigned int)sysIrqHandler;

    *(unsigned int volatile *)0x3C = (unsigned int)sysFiqHandler;
}
/// @endcond HIDDEN_SYMBOLS


/* Interrupt library functions */
/**
 *  @brief  system AIC - disable interrupt
 *
 *  @param[in]  eIntNo  Select interrupt source.  \ref IRQn_Type
 *
 *  @return   0
 */
INT32 sysDisableInterrupt(IRQn_Type eIntNo)
{
    if ((eIntNo > SYS_MAX_INT_SOURCE) || (eIntNo < SYS_MIN_INT_SOURCE))
        return Fail;

    if (eIntNo < 32)
        outpw(REG_AIC_INTDIS0, (1 << eIntNo));
    else
        outpw(REG_AIC_INTDIS1, (1 << (eIntNo - 32)));

    return Successful;
}


/**
 *  @brief  system AIC - enable interrupt
 *
 *  @param[in]  eIntNo  Select interrupt source.  \ref IRQn_Type
 *
 *  @return   0
 */
INT32 sysEnableInterrupt(IRQn_Type eIntNo)
{
    if ((eIntNo > SYS_MAX_INT_SOURCE) || (eIntNo < SYS_MIN_INT_SOURCE))
        return Fail;

    if (eIntNo < 32)
        outpw(REG_AIC_INTEN0, (1 << eIntNo));
    else
        outpw(REG_AIC_INTEN1, (1 << (eIntNo - 32)));

    return Successful;
}


/**
 *  @brief  system AIC - install exception handler
 *
 *  @param[in]  nExceptType  exception type. ( \ref SYS_SWI / \ref SYS_D_ABORT / \ref SYS_I_ABORT / \ref SYS_UNDEFINE)
 *  @param[in]  pvNewHandler  own exception handler
 *
 *  @return   old handler
 */
PVOID sysInstallExceptionHandler(INT32 nExceptType, PVOID pvNewHandler)
{
    PVOID _mOldVect = NULL;

    switch (nExceptType) {
        case SYS_SWI:
            _mOldVect = *(PVOID volatile *)0x28;
            *(PVOID volatile *)0x28 = pvNewHandler;
            break;

        case SYS_D_ABORT:
            _mOldVect = *(PVOID volatile *)0x30;
            *(PVOID volatile *)0x30 = pvNewHandler;
            break;

        case SYS_I_ABORT:
            _mOldVect = *(PVOID volatile *)0x2C;
            *(PVOID volatile *)0x2C = pvNewHandler;
            break;

        case SYS_UNDEFINE:
            _mOldVect = *(PVOID volatile *)0x24;
            *(PVOID volatile *)0x24 = pvNewHandler;
            break;

        default:
            ;
    }
    return _mOldVect;
}

/**
 *  @brief  system AIC - install FIQ handler
 *
 *  @param[in]  pvNewISR  own fiq handler
 *
 *  @return   old handler
 */
PVOID sysInstallFiqHandler(PVOID pvNewISR)
{
    PVOID _mOldVect;

    _mOldVect = *(PVOID volatile *)0x3C;
    *(PVOID volatile *)0x3C = pvNewISR;
    return _mOldVect;
}

/**
 *  @brief  system AIC - install IRQ handler
 *
 *  @param[in]  pvNewISR  own irq handler
 *
 *  @return   old handler
 */
PVOID sysInstallIrqHandler(PVOID pvNewISR)
{
    PVOID _mOldVect;

    _mOldVect = *(PVOID volatile *)0x38;
    *(PVOID volatile *)0x38 = pvNewISR;
    return _mOldVect;
}


/**
 *  @brief  system AIC - install Own IRQ service routine
 *
 *  @param[in]  nIntTypeLevel   Interrupt Level. ( \ref FIQ_LEVEL_0 / \ref IRQ_LEVEL_1 / \ref IRQ_LEVEL_2 / \ref IRQ_LEVEL_3 /
 *                                                 \ref IRQ_LEVEL_4 / \ref IRQ_LEVEL_5 / \ref IRQ_LEVEL_6 / \ref IRQ_LEVEL_7 )
 *  @param[in]  eIntNo  Interrupt number. \ref IRQn_Type
 *  @param[in]  pvNewISR  own IRQ handler
 *
 *  @return   old handler
 */
PVOID sysInstallISR(INT32 nIntTypeLevel, IRQn_Type eIntNo, PVOID pvNewISR)
{
    PVOID   _mOldVect;
    UINT32  _mRegAddr;//, _mRegValue;
    INT     shift;

    if (!_sys_bIsAICInitial) {
        sysInitializeAIC();
        _sys_bIsAICInitial = TRUE;
    }

    _mRegAddr = REG_AIC_SRCCTL0 + ((eIntNo / 4) * 4);
    shift = (eIntNo % 4) * 8;
    nIntTypeLevel &= 0xff;
    outpw(_mRegAddr, (inpw(_mRegAddr) & ~(0x0f << shift)) | (nIntTypeLevel << shift));

    if ((nIntTypeLevel & 0x7) == FIQ_LEVEL_0) {
        _mOldVect = (PVOID) sysFiqHandlerTable[eIntNo];
        sysFiqHandlerTable[eIntNo] = (sys_pvFunPtr)pvNewISR;
    } else {
        _mOldVect = (PVOID) sysIrqHandlerTable[eIntNo];
        sysIrqHandlerTable[eIntNo] = (sys_pvFunPtr)pvNewISR;
    }
    return _mOldVect;
}


INT32 sysSetGlobalInterrupt(INT32 nIntState)
{
    switch (nIntState) {
        case ENABLE_ALL_INTERRUPTS:
            outpw(REG_AIC_INTEN0, 0xFFFFFFFF);
            outpw(REG_AIC_INTEN1, 0xFFFFFFFF);
            break;

        case DISABLE_ALL_INTERRUPTS:
            outpw(REG_AIC_INTDIS0, 0xFFFFFFFF);
            outpw(REG_AIC_INTDIS1, 0xFFFFFFFF);
            break;

        default:
            ;
    }
    return Successful;
}


/**
 *  @brief  system AIC - Change interrupt level
 *
 *  @param[in]  eIntNo  Interrupt number. \ref IRQn_Type
 *  @param[in]  uIntLevel   Interrupt Level. ( \ref FIQ_LEVEL_0 / \ref IRQ_LEVEL_1 / \ref IRQ_LEVEL_2 / \ref IRQ_LEVEL_3 /
 *                                             \ref IRQ_LEVEL_4 / \ref IRQ_LEVEL_5 / \ref IRQ_LEVEL_6 / \ref IRQ_LEVEL_7 )
 *
 *  @return   0
 */
INT32 sysSetInterruptPriorityLevel(IRQn_Type eIntNo, UINT32 uIntLevel)
{
    UINT32  _mRegAddr;
    INT     shift;

    if ((eIntNo > SYS_MAX_INT_SOURCE) || (eIntNo < SYS_MIN_INT_SOURCE))
        return 1;

    _mRegAddr = REG_AIC_SRCCTL0 + ((eIntNo / 4) * 4);
    shift = (eIntNo % 4) * 8;
    uIntLevel &= 0x7;
    outpw(_mRegAddr, (inpw(_mRegAddr) & ~(0x07 << shift)) | (uIntLevel << shift));

    return 0;
}


/**
 *  @brief  system AIC - Set CP15 Interrupt Type
 *
 *  @param[in]  nIntState   Interrupt state. ( \ref ENABLE_IRQ / \ref ENABLE_FIQ / \ref ENABLE_FIQ_IRQ /
 *                                             \ref DISABLE_IRQ / \ref DISABLE_FIQ / \ref DISABLE_FIQ_IRQ)
 *
 *  @return   0
 */
INT32 sysSetLocalInterrupt(INT32 nIntState)
{
    INT32 temp;

    switch (nIntState) {
        case ENABLE_IRQ:
        case ENABLE_FIQ:
        case ENABLE_FIQ_IRQ:
            __asm {
                MRS    temp, CPSR
                AND    temp, temp, nIntState
                MSR    CPSR_c, temp
            }
            break;

        case DISABLE_IRQ:
        case DISABLE_FIQ:
        case DISABLE_FIQ_IRQ:
            __asm {
                MRS    temp, CPSR
                ORR    temp, temp, nIntState
                MSR    CPSR_c, temp
            }
            break;

        default:
            ;
    }
    return 0;
}

UINT32  sysGetInterruptEnableStatus(void)
{
    return (inpw(REG_AIC_INTMSK0));
}


UINT32  sysGetInterruptEnableStatusH(void)
{
    return (inpw(REG_AIC_INTMSK1));
}

/// @cond HIDDEN_SYMBOLS
BOOL sysGetIBitState()
{
    INT32 temp;

    __asm {
        MRS temp, CPSR
    }

    if (temp & 0x80)
        return FALSE;
    else
        return TRUE;
}

INT32 sysGetPLL(UINT32 reg)
{
    UINT32 N,M,P;

    N =((inpw(reg) & 0x007F)>>0)+1;
    M =((inpw(reg) & 0x1F80)>>7)+1;
    P =((inpw(reg) & 0xE000)>>13)+1;

    return (12*N/(M*P));    /* 12MHz HXT */
}
/// @endcond HIDDEN_SYMBOLS

/**
 *  @brief  system Timer - install WDT interrupt handler
 *
 *  @param[in]  clk   clock source. \ref CLK_Type
 *
 *  @return   MHz
 */
UINT32 sysGetClock(CLK_Type clk)
{
    UINT32 src, divS, divN, reg, div;

    switch(clk)
    {
        case SYS_UPLL:
            return sysGetPLL(REG_CLK_UPLLCON);

        case SYS_APLL:
            return sysGetPLL(REG_CLK_APLLCON);

        case SYS_SYSTEM:
        {
            reg = inpw(REG_CLK_DIVCTL0);
            switch (reg & 0x18)
            {
                case 0x0:
                    src = 12;   /* HXT */
                    break;
                case 0x10:
                    src = sysGetPLL(REG_CLK_APLLCON);
                    break;
                case 0x18:
                    src = sysGetPLL(REG_CLK_UPLLCON);
                    break;
                default:
                    return 0;
            }
            divS = (reg & 0x7) + 1;
            divN = ((reg & 0xf00) >> 8) + 1;
            return (src / divS / divN);
        }

        case SYS_HCLK:
        {
            reg = inpw(REG_CLK_DIVCTL0);
            switch (reg & 0x18)
            {
                case 0x0:
                    src = 12;   /* HXT */
                    break;
                case 0x10:
                    src = sysGetPLL(REG_CLK_APLLCON);
                    break;
                case 0x18:
                    src = sysGetPLL(REG_CLK_UPLLCON);
                    break;
                default:
                    return 0;
            }
            divS = (reg & 0x7) + 1;
            divN = ((reg & 0xf00) >> 8) + 1;
            return (src / divS / divN / 2);
        }

        case SYS_PCLK01:
        {
            reg = inpw(REG_CLK_DIVCTL0);
            switch (reg & 0x18)
            {
                case 0x0:
                    src = 12;   /* HXT */
                    break;
                case 0x10:
                    src = sysGetPLL(REG_CLK_APLLCON);
                    break;
                case 0x18:
                    src = sysGetPLL(REG_CLK_UPLLCON);
                    break;
                default:
                    return 0;
            }
            divS = (reg & 0x7) + 1;
            divN = ((reg & 0xf00) >> 8) + 1;
            div = ((reg & 0xf00000) >> 20) + 1;
            return (src / divS / divN / 2 / div);
        }

        case SYS_PCLK2:
        {
            reg = inpw(REG_CLK_DIVCTL0);
            switch (reg & 0x18)
            {
                case 0x0:
                    src = 12;   /* HXT */
                    break;
                case 0x10:
                    src = sysGetPLL(REG_CLK_APLLCON);
                    break;
                case 0x18:
                    src = sysGetPLL(REG_CLK_UPLLCON);
                    break;
                default:
                    return 0;
            }
            divS = (reg & 0x7) + 1;
            divN = ((reg & 0xf00) >> 8) + 1;
            div = ((reg & 0xf000000) >> 24) + 1;
            return (src / divS / divN / 2 / div);
        }
        case SYS_CPU:
        {
            reg = inpw(REG_CLK_DIVCTL0);
            switch (reg & 0x18)
            {
                case 0x0:
                    src = 12;   /* HXT */
                    break;
                case 0x10:
                    src = sysGetPLL(REG_CLK_APLLCON);
                    break;
                case 0x18:
                    src = sysGetPLL(REG_CLK_UPLLCON);
                    break;
                default:
                    return 0;
            }
            divS = (reg & 0x7) + 1;
            divN = ((reg & 0xf00) >> 8) + 1;
            div = ((reg & 0xf0000) >> 16) + 1;
            return (src / divS / divN / div);
        }

        default:
            ;
    }
    return 0;
}

INT32   sysGetSdramSizebyMB(void)
{
    unsigned int volatile reg, totalsize=0;

    reg = inpw(SDIC_BA+0x10) & 0x07;
    switch(reg)
    {
        case 1:
            totalsize += 2;
            break;

        case 2:
            totalsize += 4;
            break;

        case 3:
            totalsize += 8;
            break;

        case 4:
            totalsize += 16;
            break;

        case 5:
            totalsize += 32;
            break;

        case 6:
            totalsize += 64;
            break;

        case 7:
            totalsize += 128;
            break;
    }
    if((inpw(SDIC_BA+0x14) & (0xFF<<21)) !=0)
      totalsize +=totalsize;

    return totalsize;
}

/**
  * @brief      Disable register write-protection function
  * @param      None
  * @return     None
  * @details    This function disable register write-protection function.
  *             To unlock the protected register to allow write access.
  */
void SYS_UnlockReg(void)
{
    do {
			  outpw(REG_SYS_REGWPCTL, 0x59UL);
				outpw(REG_SYS_REGWPCTL, 0x16UL);
				outpw(REG_SYS_REGWPCTL, 0x88UL);
    } while(inpw(REG_SYS_REGWPCTL) == 0UL);
}

/**
  * @brief      Enable register write-protection function
  * @param      None
  * @return     None
  * @details    This function is used to enable register write-protection function.
  *             To lock the protected register to forbid write access.
  */
void SYS_LockReg(void)
{
		outpw(REG_SYS_REGWPCTL, 0UL);
}
