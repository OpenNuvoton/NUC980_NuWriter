/*
 * standalone.c - minimal bootstrap for C library
 * Copyright (C) 2000 ARM Limited.
 * All rights reserved.
 */


/*
 * This code defines a run-time environment for the C library.
 * Without this, the C startup code will attempt to use semi-hosting
 * calls to get environment information.
 */

extern unsigned int Image$$ROM1$$ZI$$Limit;


#if 1
__value_in_regs struct R0_R3 {unsigned heap_base, stack_base, heap_limit, stack_limit;} 
    __user_setup_stackheap(unsigned int R0, unsigned int SP, unsigned int R2, unsigned int SL)

{

    struct R0_R3 config;


    config.heap_base = (unsigned int)&Image$$ROM1$$ZI$$Limit;
    config.stack_base = SP;


    return config;
}
#endif
/* end of file standalone.c */
