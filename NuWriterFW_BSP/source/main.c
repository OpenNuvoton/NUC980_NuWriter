/******************************************************************************
 * @file     main.c
 * @brief    XUB.bin  code
 * @version  1.0.1
 * @date     01, November, 2018
 *
 * @note
 * Copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "nuc980.h"
#include "sys.h"
#include "etimer.h"
#include "config.h"
#include "usbd.h"
#include "sdglue.h"
#include "filesystem.h"  //for eMMC format test

extern void ParseFlashType(void);
extern unsigned int eMMCBlockSize;

#define WDT_RSTCNT    outpw(REG_WDT_RSTCNT, 0x5aa5)

/***********************************************/
/* Set ETimer0 : unit is micro-second */
void SetTimer(unsigned int count)
{
    /* Set timer0 */
    outpw(REG_CLK_PCLKEN0, inpw(REG_CLK_PCLKEN0) | 0x100);  /* enable timer engine clock */
    outpw(REG_ETMR0_ISR, 0x1);
    outpw(REG_ETMR0_CMPR, count);       /* set timer init counter value */
    outpw(REG_ETMR0_PRECNT, 0xB);
    outpw(REG_ETMR0_CTL, 0x01);         /* one-shot mode, prescale = 12 */
}

void DelayMicrosecond(unsigned int count)
{
    SetTimer(count);
    /* wait timeout */
    while(1) {
        if (inpw(REG_ETMR0_ISR) & 0x1) {
            outpw(REG_ETMR0_ISR, 0x1);
            break;
        }
    }
}

UINT32 PLL_Get(UINT32 reg,UINT srcclk)
{
    UINT32 N,M,P;
    N =((inpw(reg) & 0x007F)>>0)+1;
    M =((inpw(reg) & 0x1F80)>>7)+1;
    P =((inpw(reg) & 0xE000)>>13)+1;
    return (srcclk*N/(M*P));
}
void CPU_Info(void)
{
    UINT32 system=12;
    UINT32 cpu,pclk;
    MSG_DEBUG("inpw(REG_CLK_HCLKEN) = 0x%x, inpw(REG_CLK_DIVCTL0) = 0x%x\n", inpw(REG_CLK_HCLKEN), inpw(REG_CLK_DIVCTL0));
    switch( ((inpw(REG_CLK_DIVCTL0) & (0x3<<3))>>3)) {
    case 0:
        system=12;
        break;
    case 1:
        return;
    case 2: /* ACLKout */
        system=PLL_Get(REG_CLK_APLLCON,system);
        break;
    case 3: /* UCLKout */
        system=PLL_Get(REG_CLK_UPLLCON,system);
        break;
    }
    system= system / (((inpw(REG_CLK_DIVCTL0) & (0x1<<8))>>8)+1);
    cpu   = system / (((inpw(REG_CLK_DIVCTL0) & (0x1<<16))>>16)+1);
    pclk  = system / 2;
    printf("CPU: %dMHz, DDR: %dMHz, SYS: %dMHz, PCLK: %dMHz\n",cpu,system,system,pclk);
    //printf("CLK_DIVCTL8 =0x%x\n",inpw(REG_CLK_DIVCTL8));// XIN/512 = 23kHz

}

void UART_Init()
{
    /* enable UART0 clock */
    outpw(REG_CLK_PCLKEN0, inpw(REG_CLK_PCLKEN0) | 0x10000);

    /* GPF11, GPF12 */
    outpw(REG_SYS_GPF_MFPH, (inpw(REG_SYS_GPF_MFPH) & 0xfff00fff) | 0x11000);   // UART0 multi-function

    /* UART0 line configuration for (115200,n,8,1) */
    outpw(REG_UART0_LCR, inpw(REG_UART0_LCR) | 0x07);
    outpw(REG_UART0_BAUD, 0x30000066); /* 12MHz reference clock input, 115200 */
}

/*----------------------------------------------------------------------------
  MAIN function
 *----------------------------------------------------------------------------*/
int main()
{
    UINT32 StartAddr=0,ExeAddr=EXEADDR;

    SYS_UnlockReg();

    *((volatile unsigned int *)REG_AIC_INTDIS0)=0xFFFFFFFF;  // disable all interrupt channel
    *((volatile unsigned int *)REG_AIC_INTDIS1)=0xFFFFFFFF;  // disable all interrupt channel
    /* copy arm vctor table to 0x0 */
    memcpy((unsigned char *)StartAddr,(unsigned char *)ExeAddr,0x40);

    WDT_RSTCNT;
    //printf("WDT: 0x08%x/0x08%x/0x08%x\n",inpw(REG_WDT_CTL),inpw(REG_WDT_ALTCTL),inpw(REG_WDT_RSTCNT));
    outpw(REG_WDT_CTL, (inpw(REG_WDT_CTL) & ~(0xf << 8))|(0x8<<8));// timeout 2^20 * (12M/512) = 44 sec

    outpw(REG_SYS_AHBIPRST,1<<19);  //USBD reset
    outpw(REG_SYS_AHBIPRST,0<<19);
    outpw(REG_USBD_PHYCTL, inpw(REG_USBD_PHYCTL) & ~0x100);
    outpw(REG_CLK_HCLKEN, inpw(REG_CLK_HCLKEN) & ~0x80000);

    outpw(REG_CLK_HCLKEN,  inpw(REG_CLK_HCLKEN)  | (1<<11)); //Enable GPIO clock
    outpw(REG_CLK_HCLKEN,  inpw(REG_CLK_HCLKEN)  | (1<<20)); //Enable FMI clock
    outpw(REG_CLK_HCLKEN,  inpw(REG_CLK_HCLKEN)  | (1<<22)); //Enable SD0 clock
    outpw(REG_CLK_HCLKEN,  inpw(REG_CLK_HCLKEN)  | (1<<21)); //Enable NAND clock
    outpw(REG_CLK_HCLKEN,  inpw(REG_CLK_HCLKEN)  | (1<<30)); //Enable SD1 clock
    outpw(REG_CLK_PCLKEN0, inpw(REG_CLK_PCLKEN0) | (1<<16)); //Enable UART0 clock
    outpw(REG_CLK_PCLKEN1, inpw(REG_CLK_PCLKEN1) | (1<<4 )); //Enable QSPI0 clock

    UART_Init();
    MSG_DEBUG("after WDT: 0x08%x/0x08%x/0x08%x\n",inpw(REG_WDT_CTL),inpw(REG_WDT_ALTCTL),inpw(REG_WDT_RSTCNT));
    MSG_DEBUG("0x%x\n", inpw(REG_USBD_PHY_CTL));
    //printf("after WDT: 0x08%x/0x08%x/0x08%x\n",inpw(REG_WDT_CTL),inpw(REG_WDT_ALTCTL),inpw(REG_WDT_RSTCNT));
    printf("=======================================\n");
    printf("Run firmware code\n");
    CPU_Info();

    /* enable USB engine */
    udcInit();

#if 1
    // Enable ETIMER0 engine clock
    outpw(REG_CLK_PCLKEN0, inpw(REG_CLK_PCLKEN0) | (1 << 8));
    ETIMER_Open(0, ETIMER_PERIODIC_MODE, 100);
#endif

    WDT_RSTCNT;
    fmiHWInit();

    printf("Parse NuWriter command line\n");
    printf("=======================================\n");


    while(1) {
        WDT_RSTCNT;
        ParseFlashType();
    }

}
