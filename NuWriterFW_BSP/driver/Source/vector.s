;/**************************************************************************//**
; * @file     vector.s
; * @brief    NUC980 vector table
; *
; * @copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
; *****************************************************************************/

    AREA VECTOR, CODE, READONLY

    ENTRY

        EXPORT  Vector
Vector
        B       Reset_Go    ; Modified to be relative jump for external boot
        LDR     PC, Undefined_Addr
        LDR     PC, SWI_Addr
        LDR     PC, Prefetch_Addr
        LDR     PC, Abort_Addr
        DCD     0x0
        LDR     PC, IRQ_Addr
        LDR     PC, FIQ_Addr

        IMPORT  Reset_Go
        IMPORT  Undefined_Handler
        IMPORT  SWI_Handler1
        IMPORT  Prefetch_Handler
        IMPORT  Abort_Handler
        IMPORT  IRQ_Handler
        IMPORT  FIQ_Handler
Reset_Addr      DCD     Reset_Go
Undefined_Addr  DCD     Undefined_Handler
SWI_Addr        DCD     SWI_Handler1
Prefetch_Addr   DCD     Prefetch_Handler
Abort_Addr      DCD     Abort_Handler
                DCD     0
IRQ_Addr        DCD     IRQ_Handler
FIQ_Addr        DCD     FIQ_Handler



    END




