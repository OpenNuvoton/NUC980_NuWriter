/**************************************************************************//**
 * @file     system_NUC980.c
 * @brief    NUC980 cache/MMU control code
 *
 * @copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/

#include "nuc980.h"
#include "sys.h"

/// @cond HIDDEN_SYMBOLS

BOOL volatile _sys_IsCacheOn = FALSE;
INT32 volatile _sys_CacheMode;

#define  _CoarsePageSize    64  //MB

/** \brief  Structure type of Coarse Table
 */
typedef struct _coarse_table {
    unsigned int page[256];
} _CTable;

__align(0x4000) unsigned int _mmuSectionTable[4096];
__align(1024) static _CTable _mmuCoarsePageTable[_CoarsePageSize];          // maximum 64MB for coarse pages
__align(1024) static _CTable _mmuCoarsePageTable_NonCache[_CoarsePageSize]; // Shadow SDRAM area for non-cacheable

static BOOL _IsInitMMUTable = FALSE;

extern INT32 sysGetSdramSizebyMB(void);


__asm void sys_flush_and_clean_dcache(void)
{
tci_loop
    MRC p15, 0, r15, c7, c14, 3 // test clean and invalidate
    BNE tci_loop
    BX  lr
}

__asm  void sysSetupCP15(unsigned int addr)
{
    MOV     r1, r0                 // _mmuSectionTable
    MCR     p15, 0, r1, c2, c0, 0  // write translation table base register c2

    MOV     r1, #0x40000000
    MCR     p15, 0, r1, c3, c0, 0  // domain access control register c3

    MRC     p15, 0, r1, c1, c0, 0  // read control register c1
    ORR     r1, r1, #0x1000        // enable I cache bit
    ORR     r1, r1, #0x5           // enable D cache and MMU bits
    MCR     p15, 0, r1, c1, c0, 0  // write control register c1
    BX      lr
}

unsigned int sysGetPhyPageAddr(unsigned int vaddr)
{
    int table_num, page_num;
    unsigned int base_addr, page_base, page_offset, phy_addr;
    volatile _CTable *PageTabPtr;

    if (vaddr & 0x80000000)
        PageTabPtr = (_CTable *) _mmuCoarsePageTable_NonCache; //non-cacheable virtual address
    else
        PageTabPtr = (_CTable *) _mmuCoarsePageTable;   //cache-able virtual address

    if (sysGetCacheState() == TRUE)
        PageTabPtr = (_CTable *) ((unsigned int)PageTabPtr | 0x80000000); //If cache is enable, must write page tables directly into SDRAM

    base_addr = vaddr & 0x7FFFF000;
    table_num = base_addr / 0x100000;
    page_num = (base_addr & 0xFF000) >> 12;

    page_base = (*(PageTabPtr+table_num)).page[page_num] & 0xFFFFF000;
    page_offset = vaddr & 0xFFF;
    phy_addr = page_base + page_offset;

    return phy_addr;

} /* end sysGetPHYAddr */


int sysInitPageTable(unsigned int vaddr, unsigned int phy_addr, int size, int cache_flag)
{
    int i, cnt, table_num, page_num, cache_mode, addr_offset;
    unsigned volatile int phy_base_addr, vbase_addr, temp;
    volatile _CTable *PageTabPtr;

    if (vaddr & 0x80000000)
        PageTabPtr = (_CTable *) _mmuCoarsePageTable_NonCache; //non-cacheable virtual address
    else
        PageTabPtr = (_CTable *) _mmuCoarsePageTable;   //cache-able virtual address

    if (sysGetCacheState() == TRUE)
        PageTabPtr = (_CTable *) ((unsigned int)PageTabPtr | 0x80000000); //If cache is enable, must write page tables directly into SDRAM

    //if ( _IsInitMMUTable == FALSE ) return -1;
    vaddr &= 0x7FFFFFFF;    //ignore the non-cacheable bit 31
    if ((vaddr + size) > (_CoarsePageSize << 20)) return -1;
    if (vaddr & 0xFFFFF)    return -1;  /* MUST 1M Boundary */
    if (size % 4096)        return -1;  /* MUST 4K multiple size */

    /* Pages count */
    cnt = size / 4096;

    if (cache_flag == CACHE_WRITE_BACK) /* write back mode */
        cache_mode = 0x0C;
    else if (cache_flag == CACHE_WRITE_THROUGH) /* write through mode */
        cache_mode = 0x08;
    else
        cache_mode = 0; /* Non-cacheable, non-buffered */


    phy_base_addr = phy_addr;


    addr_offset = 4096;
    for (i=0; i<cnt; i++) {
        vbase_addr = vaddr + i * 4096;
        table_num = vbase_addr / 0x100000;
        page_num =  (vbase_addr & 0xFF000) >> 12; /* bits [19:12] for level two table index */

        temp = phy_base_addr & 0xFFFFF000;
        temp |= 0xFF0; /* access permission, 11 for read/write */
        temp |= cache_mode; /* cache mode */
        temp |= 0x02;  /* small page */

        (*(PageTabPtr+table_num)).page[page_num] = temp;

        phy_base_addr += addr_offset;

    }

    return 0;

} /* end sysInitPageTable */


int sysInitMMUTable(int cache_mode)
{
    unsigned volatile int temp;
    int i, size, ramsize;

    if (_IsInitMMUTable == FALSE) {
        ramsize = sysGetSdramSizebyMB();

        //flat mapping for 4GB, 4096 section table, each size is 1MB
        temp = 0xC00;   /* (11:10) access permission, R/W */
        temp |= 0x1E0;  /* (8:5) domain 15 */
        temp |= 0x10;   /* bit 4 must be 1 */
        temp |= 0x00;   /* bit 3:2 for cache control bits, cache disabled */
        temp |= 0x02;   /* set as 1Mb section */

        for (i=0; i<4096; i++) {
            _mmuSectionTable[i] = (unsigned int)(temp | (i << 20));
        }

        //Inside SDRAM, divide each section into 256 small pages, each page size is 4KB
        if (ramsize > _CoarsePageSize) size = _CoarsePageSize;  //maximum 64MB
        else                           size = ramsize;

        /* first 1M always direct mapping */
        sysInitPageTable(0, 0, 0x100000, cache_mode);
        temp = ((unsigned int)_mmuCoarsePageTable  & 0xFFFFFC00); /*  coarse table base address */
        temp |= 0x1E0;  /* (8:5) domain 15 */
        temp |= 0x10;   /* bit 4 must be 1 */
        temp |= 0x01;   /* Coarse page table */
        _mmuSectionTable[0] = temp;

        /* Create a shadow area at 0x80000000 for non-cacheable region */
        sysInitPageTable(0x80000000, 0x0, 0x100000, CACHE_DISABLE);
        temp = ((unsigned int)_mmuCoarsePageTable_NonCache  & 0xFFFFFC00); /*  coarse table base address */
        temp |= 0x1E0;  /* (8:5) domain 15 */
        temp |= 0x10;   /* bit 4 must be 1 */
        temp |= 0x01;   /* Coarse page table */
        _mmuSectionTable[0x800] = temp;

        /* Mapping the other memory */
        for (i=1; i< size; i++) {
            temp = (((unsigned int)_mmuCoarsePageTable + (unsigned int)i*1024) & 0xFFFFFC00); /*  coarse table base address */
            //temp = ((unsigned int)(0x604000 + i*1024) & 0xFFFFFC00); /* coarse table base address */
            temp |= 0x1E0;  /* (8:5) domain 15 */
            temp |= 0x10;   /* bit 4 must be 1 */
            temp |= 0x01;   /* Coarse page table */

            sysInitPageTable((i << 20), (i << 20), 0x100000, cache_mode); /* direct mapping */

            _mmuSectionTable[i] = temp;
        }

        //Create shadow non-cacheable region
        for (i=1; i< size; i++) {
            temp = (((unsigned int)_mmuCoarsePageTable_NonCache + (unsigned int)i*1024) & 0xFFFFFC00); /*  coarse table base address */
            //temp = ((unsigned int)(0x604000 + i*1024) & 0xFFFFFC00); /* coarse table base address */
            temp |= 0x1E0;  /* (8:5) domain 15 */
            temp |= 0x10;   /* bit 4 must be 1 */
            temp |= 0x01;   /* Coarse page table */

            sysInitPageTable(((i << 20) | 0x80000000), (i << 20), 0x100000, CACHE_DISABLE); /* direct mapping */

            _mmuSectionTable[0x800+i] = temp;
        }

        _IsInitMMUTable = TRUE;
    }

    //moved here by cmn [2007/01/27]
    //set CP15 registers
    sysSetupCP15((unsigned int)_mmuSectionTable);

    return 0;

} /* end sysInitMMUTable */


void sysFlushCache(INT32 nCacheType)
{
    int temp;

    switch (nCacheType) {
        case I_CACHE:
            __asm {
                /*----- flush I-cache -----*/
                MOV temp, 0x0
                MCR p15, 0, temp, c7, c5, 0 /* invalidate I cache */
            }
            break;

        case D_CACHE:
            sys_flush_and_clean_dcache();
            __asm {
                /*----- flush D-cache & write buffer -----*/
                MOV temp, 0x0
                MCR p15, 0, temp, c7, c10, 4 /* drain write buffer */
            }
            break;

        case I_D_CACHE:
            sys_flush_and_clean_dcache();
            __asm {
                /*----- flush I, D cache & write buffer -----*/
                MOV temp, 0x0
                MCR p15, 0, temp, c7, c5, 0 /* invalidate I cache */
                MCR p15, 0, temp, c7, c10, 4 /* drain write buffer */
            }
            break;

        default:
            ;
    }
}

void sysInvalidCache()
{
    int temp;

    __asm {
        MOV temp, 0x0
        MCR p15, 0, temp, c7, c7, 0 /* invalidate I and D cache */
    }
}

BOOL sysGetCacheState()
{
    return _sys_IsCacheOn;
}


INT32 sysGetCacheMode()
{
    return _sys_CacheMode;
}


INT32 _sysLockCode(UINT32 addr, INT32 size)
{
    int i, cnt, temp;

    __asm {
        /* use way3 to lock instructions */
        MRC p15, 0, temp, c9, c0, 1 ;
        ORR temp, temp, 0x07 ;
        MCR p15, 0, temp, c9, c0, 1 ;
    }

    if (size % 16)  cnt = (size/16) + 1;
    else            cnt = size / 16;

    for (i=0; i<cnt; i++) {
        __asm {
            MCR p15, 0, addr, c7, c13, 1;
        }

        addr += 16;
    }


    __asm {
        /* use way3 to lock instructions */
        MRC p15, 0, temp, c9, c0, 1 ;
        BIC temp, temp, 0x07 ;
        ORR temp, temp, 0x08 ;
        MCR p15, 0, temp, c9, c0, 1 ;
    }

    return 0;

}


INT32 _sysUnLockCode()
{
    int temp;

    /* unlock I-cache way 3 */
    __asm {
        MRC p15, 0, temp, c9, c0, 1;
        BIC temp, temp, 0x08 ;
        MCR p15, 0, temp, c9, c0, 1;

    }

    return 0;
}


/// @endcond HIDDEN_SYMBOLS

/**
 *  @brief  system Cache - Enable cache
 *
 *  @param[in]  uCacheOpMode    cache operation mode. ( \ref CACHE_WRITE_BACK / \ref CACHE_WRITE_THROUGH)
 *
 *  @return   0
 */
INT32 sysEnableCache(UINT32 uCacheOpMode)
{
    sysInitMMUTable(uCacheOpMode);
    _sys_IsCacheOn = TRUE;
    _sys_CacheMode = uCacheOpMode;

    return 0;
}

/**
 *  @brief  system Cache - Disable cache
 *
 *  @return   None
 */
void sysDisableCache(void)
{
    int temp;

    sys_flush_and_clean_dcache();
    __asm {
        /*----- flush I, D cache & write buffer -----*/
        MOV temp, 0x0
        MCR p15, 0, temp, c7, c5, 0 /* flush I cache */
        MCR p15, 0, temp, c7, c6, 0 /* flush D cache */
        MCR p15, 0, temp, c7, c10,4 /* drain write buffer */

        /*----- disable Protection Unit -----*/
        MRC p15, 0, temp, c1, c0, 0     /* read Control register */
        BIC temp, temp, 0x01
        MCR p15, 0, temp, c1, c0, 0     /* write Control register */
    }
    _sys_IsCacheOn = FALSE;
    _sys_CacheMode = CACHE_DISABLE;

}

