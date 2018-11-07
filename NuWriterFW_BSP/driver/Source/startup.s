;/**************************************************************************//**
; * @file     startup.s
; * @brief    NUC980 startup code
; *
; * @copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
; *****************************************************************************/


    AREA NUC_INIT, CODE, READONLY

;--------------------------------------------
; Mode bits and interrupt flag (I&F) defines
;--------------------------------------------
USR_MODE    EQU     0x10
FIQ_MODE    EQU     0x11
IRQ_MODE    EQU     0x12
SVC_MODE    EQU     0x13
ABT_MODE    EQU     0x17
UDF_MODE    EQU     0x1B
SYS_MODE    EQU     0x1F

I_BIT       EQU     0x80
F_BIT       EQU     0x40

AIC_INTDIS0 EQU	    0xB0042138  ; Interrupt Disable Control Register 0
AIC_INTDIS1 EQU	    0xB004213C  ; Interrupt Disable Control Register 1

;----------------------------
; System / User Stack Memory
;----------------------------
; TODO: Read bounding option to decide memory size
RAM_Limit       EQU     0x2000000           ; For unexpanded hardware board

UND_Stack       EQU     RAM_Limit
Abort_Stack     EQU     RAM_Limit-256
FIQ_Stack       EQU     RAM_Limit-512       ; followed by IRQ stack
SVC_Stack       EQU     RAM_Limit-1024      ; SVC stack at top of memory
IRQ_Stack       EQU     RAM_Limit-4092      ; followed by IRQ stack
USR_Stack       EQU     RAM_Limit-8192

    ENTRY
    EXPORT  Reset_Go
    EXPORT  Undefined_Handler
    EXPORT  SWI_Handler1
    EXPORT  Prefetch_Handler
    EXPORT  Abort_Handler
    EXPORT  IRQ_Handler
    EXPORT  FIQ_Handler

Reset_Go
    ; Disable Interrupt in case code is load by ICE while other firmware is executing
    LDR    r0, =AIC_INTDIS0
    LDR    r1, =0xFFFFFFFF
    STR    r1, [r0]
    LDR    r0, =AIC_INTDIS1
    STR    r1, [r0]
    ;--------------------------------
    ; Initial Stack Pointer register
    ;--------------------------------
    ;INIT_STACK
    MSR    CPSR_c, #UDF_MODE :OR: I_BIT :OR: F_BIT
    LDR    SP, =UND_Stack

    MSR    CPSR_c, #ABT_MODE :OR: I_BIT :OR: F_BIT
    LDR    SP, =Abort_Stack

    MSR    CPSR_c, #IRQ_MODE :OR: I_BIT :OR: F_BIT
    LDR    SP, =IRQ_Stack

    MSR    CPSR_c, #FIQ_MODE :OR: I_BIT :OR: F_BIT
    LDR    SP, =FIQ_Stack

    MSR    CPSR_c, #SYS_MODE :OR: I_BIT :OR: F_BIT
    LDR    SP, =USR_Stack

    MSR    CPSR_c, #SVC_MODE :OR: I_BIT :OR: F_BIT
    LDR    SP, =SVC_Stack

    ;------------------------------------------------------
    ; Set the normal exception vector of CP15 control bit
    ;------------------------------------------------------
    MRC p15, 0, r0 , c1, c0     ; r0 := cp15 register 1
    BIC r0, r0, #0x2000         ; Clear bit13 in r1
    MCR p15, 0, r0 , c1, c0     ; cp15 register 1 := r0


    IMPORT  __main
    ;-----------------------------
    ;   enter the C code
    ;-----------------------------
    B   __main

    ; ************************
    ; Exception Handlers
    ; ************************

    ; The following dummy handlers do not do anything useful in this example.
    ; They are set up here for completeness.

Undefined_Handler
    B       Undefined_Handler
SWI_Handler1
    B       SWI_Handler1
Prefetch_Handler
    B       Prefetch_Handler
Abort_Handler
    B       Abort_Handler
IRQ_Handler
    B       IRQ_Handler
FIQ_Handler
    B       FIQ_Handler


    END




