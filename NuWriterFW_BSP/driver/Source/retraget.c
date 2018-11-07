/**************************************************************************//**
 * @file     retarget.c
 * @brief    NUC980 retarget code
 *
 * @copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/
#include <stdio.h>
#include <rt_misc.h>
#include "nuc980.h"
#include "sys.h"


#pragma import(__use_no_semihosting_swi)
/// @cond HIDDEN_SYMBOLS
int sendchar(int ch)
{
    while ((inpw(REG_UART0_FSR) & (1<<23))); //waits for TX_FULL bit is clear
    outpw(REG_UART0_THR, ch);
    if(ch == '\n')
    {
        while((inpw(REG_UART0_FSR) & (1<<23))); //waits for TX_FULL bit is clear
        outpw(REG_UART0_THR, '\r');
    }
    return (ch);
}

int recvchar(void)
{
    while(1)
    {
        if((inpw(REG_UART0_FSR) & (1 << 14)) == 0)  // waits RX not empty
        {
            return inpw(REG_UART0_RBR);
        }
    }
}

struct __FILE { int handle; /* Add whatever you need here */ };
FILE __stdout;
FILE __stdin;

int fputc(int ch, FILE *f) {
    return (sendchar(ch));
}

int fgetc(FILE *stream)
{
    return (recvchar());
}


int ferror(FILE *f)
{
    /* Your implementation of ferror */
    return EOF;
}


void _ttywrch(int ch)
{
    sendchar(ch);
}


void _sys_exit(int return_code)
{
label:  goto label;  /* No where to go, endless loop */
}


/// @endcond HIDDEN_SYMBOLS

