#include <string.h>
#include <stdlib.h>
#include "NUC980.h"
#include "sys.h"
#include "etimer.h"
#include "usbd.h"
#include "fmi.h"
#include "sd.h"

/*-----------------------------------------------------------------------------
 * Define some constants for BCH
 *---------------------------------------------------------------------------*/
// define the total padding bytes for 512/1024 data segment
#define BCH_PADDING_LEN_512     32
#define BCH_PADDING_LEN_1024    64
// define the BCH parity code lenght for 512 bytes data pattern
#define BCH_PARITY_LEN_T4  8
#define BCH_PARITY_LEN_T8  15
#define BCH_PARITY_LEN_T12 23
#define BCH_PARITY_LEN_T15 29
// define the BCH parity code lenght for 1024 bytes data pattern
#define BCH_PARITY_LEN_T24 45

#define ERASE_WITH_0XF0

#define NAND_EXTRA_512          16
#define NAND_EXTRA_2K           64
#define NAND_EXTRA_4K           128
#define NAND_EXTRA_8K           376
/*-----------------------------------------------------------------------------*/

// global variables
UCHAR _fmi_ucBaseAddr1=0, _fmi_ucBaseAddr2=0;
FMI_SM_INFO_T SMInfo, *pSM;
FW_UPDATE_INFO_T FWInfo;
unsigned char volatile gu_fmiSM_IsOnfi;

extern int volatile _usbd_IntraROM;
extern void SendAck(UINT32 status);
extern void SetTimer(unsigned int count);
extern void DelayMicrosecond(unsigned int count);
extern UINT32 g_uIsUserConfig;

INT fmiSMCheckRB()
{
    SetTimer(3000);
    while(1) {
        if((inpw(REG_NANDINTSTS) & 0x400)) { /* RB0_IF */
            //while(! (inpw(REG_NANDINTSTS) & 0x40000) );
            outpw(REG_NANDINTSTS, 0x400);
            return 1;
        }

        if (inpw(REG_ETMR0_ISR) & 0x1) {
            outpw(REG_ETMR0_ISR, 0x1);
            return 0;
        }
    }
}

// SM functions
INT fmiSM_Reset(void)
{
    outpw(REG_NANDINTSTS, 0x400);
    outpw(REG_NANDCMD, 0xff);
    /* delay for NAND flash tWB time */
    DelayMicrosecond(100);
    if (!fmiSMCheckRB()) {
        return Fail;
    }
    return Successful;
}

VOID fmiSM_Initial(FMI_SM_INFO_T *pSM)
{
    outpw(REG_NANDCTL,  inpw(REG_NANDCTL) | 0x800080);  // enable ECC

    //--- Set register to disable Mask ECC feature
    outpw(REG_NANDRACTL, inpw(REG_NANDRACTL) & ~0xffff0000);

    //--- Set registers that depend on page size. According to FA95 sepc, the correct order is
    //--- 1. SMCR_BCH_TSEL  : to support T24, MUST set SMCR_BCH_TSEL before SMCR_PSIZE.
    //--- 2. SMCR_PSIZE     : set SMCR_PSIZE will auto change SMRE_REA128_EXT to default value.
    //--- 3. SMRE_REA128_EXT: to use non-default value, MUST set SMRE_REA128_EXT after SMCR_PSIZE.
    outpw(REG_NANDCTL, (inpw(REG_NANDCTL) & ~0x7c0000) | pSM->uNandECC);
    if (pSM->uPageSize == 8192)
        outpw(REG_NANDCTL, (inpw(REG_NANDCTL)&(~0x30000)) | 0x30000);
    else if (pSM->uPageSize == 4096)
        outpw(REG_NANDCTL, (inpw(REG_NANDCTL)&(~0x30000)) | 0x20000);
    else if (pSM->uPageSize == 2048)
        outpw(REG_NANDCTL, (inpw(REG_NANDCTL)&(~0x30000)) | 0x10000);
    else    // Page size should be 512 bytes
        outpw(REG_NANDCTL, (inpw(REG_NANDCTL)&(~0x30000)) | 0x00000);
    outpw(REG_NANDRACTL, (inpw(REG_NANDRACTL) & ~0x1ff) | pSM->uSpareSize);

    //TODO: if need protect --- config register for Region Protect
    outpw(REG_NANDCTL, inpw(REG_NANDCTL) & ~0x20);   // disable Region Protect

    // Disable Write Protect, NAND flash is not write-protected and is writeable
    outpw(REG_NANDECTL, 0x01);
}

static UINT16 onfi_crc16(UINT16 crc, UINT8 const *p, UINT32 len)
{
    int i;
    while (len--) {
        crc ^= *p++ << 8;
        for (i = 0; i < 8; i++)
            crc = (crc << 1) ^ ((crc & 0x8000) ? 0x8005 : 0);
    }

    return crc;
}

UINT32 fmiSM_ReadOnfi(FMI_SM_INFO_T *pSM)
{
    unsigned char tempID[256];
    int volatile i;
    unsigned int extendlen, parampages, offset;

    /* set READ_ID command */
    outpw(REG_NANDCMD, 0x90);           // read ID command
    outpw(REG_NANDADDR, 0x80000020);    // address 0x20;

    for (i=0; i<4; i++)
        tempID[i] = inpb(REG_NANDDATA);

    if ((tempID[0] == 'O') && (tempID[1] == 'N') && (tempID[2] == 'F') && (tempID[3] == 'I')) {
        /* read parameter */
        outpw(REG_NANDCMD, 0xec);
        outpw(REG_NANDADDR, 0x80000000);
        fmiSMCheckRB();
        for (i=0; i<256; i++)
            tempID[i] = inpb(REG_NANDDATA);
        if (onfi_crc16(0x4F4E, (UINT8 *)tempID, 254) == (tempID[254]|(tempID[255]<<8))) {
            pSM->uPageSize = tempID[80]|(tempID[81]<<8)|(tempID[82]<<16)|(tempID[83]<<24);
            pSM->uSpareSize = tempID[84]|(tempID[85]<<8);
            pSM->uPagePerBlock = tempID[92]|(tempID[93]<<8)|(tempID[94]<<16)|(tempID[95]<<24);
            if (tempID[112] <= 8)
                pSM->uNandECC = BCH_T8;
            else if (tempID[112] <= 12)
                pSM->uNandECC = BCH_T12;
            else if (tempID[112] <= 24)
                pSM->uNandECC = BCH_T24;
            else if (tempID[112] == 0xff) {
                /* Read out the Extended Parameter Page */
                extendlen = (tempID[12]|(tempID[13]<<8)) * 16;
                parampages = tempID[14] * 256;
                /* read parameter */
                outpw(REG_NANDCMD, 0xec);
                outpw(REG_NANDADDR, 0x80000000);
                fmiSMCheckRB();
                outpw(REG_NANDCMD, 0x05);
                outpw(REG_NANDADDR, parampages & 0xFF);
                outpw(REG_NANDADDR, ((parampages >> 8) & 0xFF) | 0x80000000); // PA8 - PA15
                outpw(REG_NANDCMD, 0xE0);
                for (i=0; i<100; i++);
                for (i=0; i<extendlen; i++)
                    tempID[i] = inpb(REG_NANDDATA);
                if (onfi_crc16(0x4F4E, (UINT8 *)&tempID[2], extendlen-2) == (tempID[0]|(tempID[1]<<8))) {
                    /* Find the Extended Parameter Page */
                    if ((tempID[2] == 'E') && (tempID[3] == 'P') && (tempID[4] == 'P') && (tempID[5] == 'S')) {
                        /* Search the ECC section */
                        for (i=0; i<16; i+=2) {   /* extend maximum section is 8 */
                            if (tempID[16+i] == 2) {  /* get ECC section */
                                offset = (tempID[17+i]+1) * 16;
                                if (tempID[offset] <= 8)
                                    pSM->uNandECC = BCH_T8;
                                else if (tempID[offset] <= 12)
                                    pSM->uNandECC = BCH_T12;
                                else if (tempID[offset] <= 24)
                                    pSM->uNandECC = BCH_T24;
                                break;
                            }
                        }
                    }
                }
            }
            gu_fmiSM_IsOnfi = 1;
            return 0;   /* OK */
        }
    }
    return 1;   /* not ONFI */
}

UINT32 Custom_uBlockPerFlash;
UINT32 Custom_uPagePerBlock;
INT fmiSM_ReadID(FMI_SM_INFO_T *pSM)
{
    UINT32 tempID[5],u32PowerOn,IsID=0;
    UINT8 name[6][16] = {"T24","T4","T8","T12","T15","XXX"};
    UINT8 BCHAlgoIdx;

    if (pSM->bIsInResetState == FALSE) {
        if (fmiSM_Reset() < 0)
            return Fail;
        pSM->bIsInResetState = TRUE;
    }
    pSM->bIsInResetState = FALSE;
    outpw(REG_NANDCMD, 0x90);     // read ID command
    outpw(REG_NANDADDR, 0x80000000);  // address 0x00

    tempID[0] = inpw(REG_NANDDATA);
    tempID[1] = inpw(REG_NANDDATA);
    tempID[2] = inpw(REG_NANDDATA);
    tempID[3] = inpw(REG_NANDDATA);
    tempID[4] = inpw(REG_NANDDATA);

    printf("\nNAND ID=[%x][%x][%x][%x]\n", tempID[0], tempID[1], tempID[2], tempID[3]);
    MSG_DEBUG("ID[4]=0x%2x\n",tempID[4]);

    /* Using PowerOn setting*/
    u32PowerOn = inpw(REG_SYS_PWRON);
    printf("PowerOn setting 0x%x\n",u32PowerOn);

    /* Without Power-On-Setting for NAND */
    pSM->uPagePerBlock = 64;
    pSM->uPageSize = 2048;
    pSM->uNandECC = BCH_T8;
    pSM->bIsMulticycle = TRUE;
    pSM->uSpareSize = 8;
    pSM->uBlockPerFlash  = Custom_uBlockPerFlash-1; // block index with 0-base. = physical blocks - 1
    pSM->uPagePerBlock   = Custom_uPagePerBlock;
    MSG_DEBUG("Default[0x%x 0x%x] -> BlockPerFlash=%d, PagePerBlock=%d, PageSize=%d\n", tempID[1], tempID[3], pSM->uBlockPerFlash, pSM->uPagePerBlock, pSM->uPageSize);

    switch (tempID[1]) {
        /* page size 512B */
    case 0x79:  // 128M
        pSM->uBlockPerFlash = 8191;
        pSM->uPagePerBlock = 32;
        pSM->uSectorPerBlock = 32;
        pSM->uPageSize = 512;
        pSM->uNandECC = BCH_T8;
        pSM->bIsMulticycle = TRUE;
        pSM->uSpareSize = 16;
        break;

    case 0x76:  // 64M
        pSM->uBlockPerFlash = 4095;
        pSM->uPagePerBlock = 32;
        pSM->uSectorPerBlock = 32;
        pSM->uPageSize = 512;
        pSM->uNandECC = BCH_T8;
        pSM->bIsMulticycle = TRUE;
        pSM->uSpareSize = 16;
        break;

    case 0x75:  // 32M
        pSM->uBlockPerFlash = 2047;
        pSM->uPagePerBlock = 32;
        pSM->uSectorPerBlock = 32;
        pSM->uPageSize = 512;
        pSM->uNandECC = BCH_T8;
        pSM->bIsMulticycle = FALSE;
        pSM->uSpareSize = 16;
        break;

    case 0x73:  // 16M
        pSM->uBlockPerFlash = 1023;
        pSM->uPagePerBlock = 32;
        pSM->uSectorPerBlock = 32;
        pSM->uPageSize = 512;
        pSM->uNandECC = BCH_T8;
        pSM->bIsMulticycle = FALSE;
        pSM->uSpareSize = 16;
        break;

        /* page size 2KB */
    case 0xf1:  // 128M
    case 0xd1:  // 128M
    case 0xa1:
        pSM->uBlockPerFlash = 1023;
        pSM->uPagePerBlock = 64;
        pSM->uSectorPerBlock = 256;
        pSM->uPageSize = 2048;
        pSM->uNandECC = BCH_T8;
        pSM->bIsMulticycle = FALSE;
        pSM->uSpareSize = 64;
        MSG_DEBUG("[case 0xf1] %d   %d   %d   %d   0x%x   %d   %d\n", pSM->uBlockPerFlash, pSM->uPagePerBlock, pSM->uPageSize, pSM->uSectorPerBlock, pSM->uNandECC, pSM->uSpareSize, pSM->bIsMLCNand);
        break;

    case 0xda:  // 256M
        if ((tempID[3] & 0x33) == 0x11) {
            pSM->uBlockPerFlash = 2047;
            pSM->uPagePerBlock = 64;
            pSM->uSectorPerBlock = 256;
        } else if ((tempID[3] & 0x33) == 0x21) {
            pSM->uBlockPerFlash = 1023;
            pSM->uPagePerBlock = 128;
            pSM->uSectorPerBlock = 512;
        }
        pSM->uPageSize = 2048;
        pSM->uNandECC = BCH_T8;
        pSM->bIsMulticycle = TRUE;
        pSM->uSpareSize = 64;
        break;

    case 0xdc:  // 512M
        pSM->uBlockPerFlash = 64;
        if((tempID[0]==0x98) && (tempID[1]==0xDC) &&(tempID[2]==0x90)&&(tempID[3]==0x26)&&(tempID[4]==0x76)) {
            pSM->uBlockPerFlash = 2047;
            pSM->uPagePerBlock = 64;
            pSM->uSectorPerBlock = 256;
            pSM->uPageSize = 4096;
            pSM->uNandECC = BCH_T12;
            pSM->bIsMLCNand = TRUE;
            pSM->uSpareSize = 192;
            pSM->bIsMulticycle = TRUE;
            break;
        } else if ((tempID[3] & 0x33) == 0x11) {
            pSM->uBlockPerFlash = 4095;
            pSM->uPagePerBlock = 64;
            pSM->uSectorPerBlock = 256;
        } else if ((tempID[3] & 0x33) == 0x21) {
            pSM->uBlockPerFlash = 2047;
            pSM->uPagePerBlock = 128;
            pSM->uSectorPerBlock = 512;
        }
        pSM->uPageSize = 2048;
        pSM->uNandECC = BCH_T8;
        pSM->bIsMulticycle = TRUE;
        pSM->uSpareSize = 64;
        break;

    case 0xd3:  // 1024M
        if ((tempID[3] & 0x33) == 0x32) {
            pSM->uBlockPerFlash = 2047;
            pSM->uPagePerBlock = 128;
            pSM->uSectorPerBlock = 1024;    /* 128x8 */
            pSM->uPageSize = 4096;
            pSM->uNandECC = BCH_T8;
            pSM->uSpareSize = 128;
        } else if ((tempID[3] & 0x33) == 0x11) {
            pSM->uBlockPerFlash = 8191;
            pSM->uPagePerBlock = 64;
            pSM->uSectorPerBlock = 256;
            pSM->uPageSize = 2048;
            pSM->uNandECC = BCH_T8;
            pSM->uSpareSize = 64;
        } else if ((tempID[3] & 0x33) == 0x21) {
            pSM->uBlockPerFlash = 4095;
            pSM->uPagePerBlock = 128;
            pSM->uSectorPerBlock = 512;
            pSM->uPageSize = 2048;
            pSM->uNandECC = BCH_T8;
            pSM->uSpareSize = 64;
        } else if ((tempID[3] & 0x3) == 0x3) {
            pSM->uBlockPerFlash = 4095;//?
            pSM->uPagePerBlock = 128;
            pSM->uSectorPerBlock = 512;//?
            pSM->uPageSize = 8192;
            pSM->uNandECC = BCH_T12;
            pSM->uSpareSize = 368;
        } else if ((tempID[3] & 0x33) == 0x22) {
            pSM->uBlockPerFlash = 4095;
            pSM->uPagePerBlock = 64;
            pSM->uSectorPerBlock = 512; /* 64x8 */
            pSM->uPageSize = 4096;
            pSM->uNandECC = BCH_T8;
            pSM->uSpareSize = 128;
        }
        pSM->bIsMulticycle = TRUE;
        break;

    case 0xd5:  // 2048M
        // H27UAG8T2A
        if ((tempID[0]==0xAD)&&(tempID[2] == 0x94)&&(tempID[3] == 0x25)) {
            pSM->uBlockPerFlash = 4095;
            pSM->uPagePerBlock = 128;
            pSM->uSectorPerBlock = 1024;    /* 128x8 */
            pSM->uPageSize = 4096;
            pSM->uNandECC = BCH_T12;
            pSM->bIsMulticycle = TRUE;
            pSM->uSpareSize = 224;
            break;
        }
        // 2011/7/28, To support Hynix H27UAG8T2B NAND flash
        else if ((tempID[0]==0xAD)&&(tempID[2]==0x94)&&(tempID[3]==0x9A)) {
            pSM->uBlockPerFlash = 1023;        // block index with 0-base. = physical blocks - 1
            pSM->uPagePerBlock = 256;
            pSM->uPageSize = 8192;
            pSM->uSectorPerBlock = pSM->uPageSize / 512 * pSM->uPagePerBlock;
            pSM->uNandECC = BCH_T24;
            pSM->bIsMulticycle = TRUE;
            pSM->uSpareSize = 448;
            break;
        }
        // 2011/7/28, To support Toshiba TC58NVG4D2FTA00 NAND flash
        else if ((tempID[0]==0x98)&&(tempID[2]==0x94)&&(tempID[3]==0x32)) {
            pSM->uBlockPerFlash = 2075;        // block index with 0-base. = physical blocks - 1
            pSM->uPagePerBlock = 128;
            pSM->uPageSize = 8192;
            pSM->uSectorPerBlock = pSM->uPageSize / 512 * pSM->uPagePerBlock;
            pSM->uNandECC = BCH_T24;
            pSM->bIsMulticycle = TRUE;
            pSM->uSpareSize = 376;
            break;
        } else if ((tempID[3] & 0x33) == 0x32) {
            pSM->uBlockPerFlash = 4095;
            pSM->uPagePerBlock = 128;
            pSM->uSectorPerBlock = 1024;    /* 128x8 */
            pSM->uPageSize = 4096;
            pSM->uNandECC = BCH_T8;
            pSM->bIsMLCNand = TRUE;
            pSM->uSpareSize = 128;
        } else if ((tempID[3] & 0x33) == 0x11) {
            pSM->uBlockPerFlash = 16383;
            pSM->uPagePerBlock = 64;
            pSM->uSectorPerBlock = 256;
            pSM->uPageSize = 2048;
            pSM->uNandECC = BCH_T8;;
            pSM->bIsMLCNand = FALSE;
            pSM->uSpareSize = 64;
        } else if ((tempID[3] & 0x33) == 0x21) {
            pSM->uBlockPerFlash = 8191;
            pSM->uPagePerBlock = 128;
            pSM->uSectorPerBlock = 512;
            pSM->uPageSize = 2048;
            pSM->uNandECC = BCH_T8;
            pSM->bIsMLCNand = TRUE;
            pSM->uSpareSize = 64;
        }
        pSM->bIsMulticycle = TRUE;
        break;

    default:
        // 2017/3/21, To support Micron MT29F32G08CBACA NAND flash
        if ((tempID[0]==0x2C)&&(tempID[1]==0x68)&&(tempID[2]==0x04)&&(tempID[3]==0x4A)&&(tempID[4]==0xA9)) {
            pSM->uBlockPerFlash  = 4095;        // block index with 0-base. = physical blocks - 1
            pSM->uPagePerBlock   = 256;
            pSM->uSectorPerBlock = pSM->uPageSize / 512 * pSM->uPagePerBlock;
            pSM->uPageSize       = 4096;
            pSM->uNandECC        = BCH_T24;
            pSM->bIsMulticycle   = TRUE;
            //pSM->uSpareSize      = 224;
            pSM->uSpareSize      = 188;
            pSM->bIsMLCNand      = TRUE;
            break;
        }

        /* ONFI */
        if (fmiSM_ReadOnfi(pSM) == 1) {
            if((u32PowerOn & 0xC0) == 0xC0) {   /* [7:6] Ignore Power-On-Setting for NAND Page */
                pSM->uNandECC = BCH_T8;
                pSM->uPagePerBlock   = 64;
                pSM->uPageSize = 2048;
                return 1;
            }
            IsID=1;
        }
    }

    //if((u32PowerOn&0x3C0)!=0x3C0 ) {
    if ((u32PowerOn & 0xC0) != 0xC0) { /* PageSize PWRON[7:6] */
        const UINT16 BCH12_SPARE[3] = { 92,184,368};/* 2K, 4K, 8K */
        //const UINT16 BCH15_SPARE[3] = {116,232,464};/* 2K, 4K, 8K */
        const UINT16 BCH24_SPARE[3] = { 90,180,360};/* 2K, 4K, 8K */
        unsigned int volatile gu_fmiSM_PageSize;
        unsigned int volatile g_u32ExtraDataSize;

        printf("Using PowerOn setting(0x%x): ", (u32PowerOn>>6)&0xf);
        gu_fmiSM_PageSize = 1024 << (((u32PowerOn >> 6) & 0x3) + 1);
        switch(gu_fmiSM_PageSize) {
        case 2048:
            printf(" PageSize = 2KB ");
            pSM->uPagePerBlock   = 64;
            pSM->uNandECC        = BCH_T8;
            break;
        case 4096:
            printf(" PageSize = 4KB ");
            pSM->uPagePerBlock   = 128;
            pSM->uNandECC        = BCH_T8;
            break;
        case 8192:
            printf(" PageSize = 8KB ");
            pSM->uPagePerBlock   = 128;
            pSM->uNandECC        = BCH_T12;
            break;
        }

        if((u32PowerOn & 0x300) != 0x300) { /* ECC PWRON[9:8] */
            switch((u32PowerOn & 0x300)) {
            case 0x000:
                printf(" No ECC\n");
                g_u32ExtraDataSize = 0;
                pSM->uNandECC = 0;
                break;
            case 0x100:
                printf(" ECC = T12\n");
                g_u32ExtraDataSize = BCH12_SPARE[gu_fmiSM_PageSize >> 12] + 8;
                pSM->uNandECC = BCH_T12;
                break;
            case 0x200:
                printf(" ECC = T24\n");
                g_u32ExtraDataSize = BCH24_SPARE[gu_fmiSM_PageSize >> 12] + 8;
                pSM->uNandECC = BCH_T24;
                break;
            case 0x300:
                printf(" ECC = T8\n");
                g_u32ExtraDataSize = 60;
                pSM->uNandECC = BCH_T8;
                break;
            }
        } else if (gu_fmiSM_IsOnfi == 0) {
            printf(" ECC = XXX\n");
            switch(gu_fmiSM_PageSize) {
            case 2048:
                g_u32ExtraDataSize = NAND_EXTRA_2K;
                break;
            case 4096:
                g_u32ExtraDataSize = NAND_EXTRA_4K;
                break;
            case 8192:
                g_u32ExtraDataSize = NAND_EXTRA_8K;
                break;
            default:
                ;
            }
        }

        if(g_uIsUserConfig == 1) {
            pSM->uBlockPerFlash = Custom_uBlockPerFlash-1;
            pSM->uPagePerBlock = Custom_uPagePerBlock;
            printf("Custom_uBlockPerFlash= %d, Custom_uPagePerBlock= %d\n", Custom_uBlockPerFlash, Custom_uPagePerBlock);
        }

        pSM->uPageSize       = gu_fmiSM_PageSize;
        pSM->uSectorPerBlock = pSM->uPageSize / 512 * pSM->uPagePerBlock;
        pSM->bIsMulticycle   = TRUE;
        pSM->uSpareSize      = g_u32ExtraDataSize;
        pSM->bIsMLCNand      = TRUE;
        printf("User Configure:\nBlockPerFlash= %d, PagePerBlock= %d\n", pSM->uBlockPerFlash, pSM->uPagePerBlock);

    } else {
        if(IsID==1) {
            printf("NAND ID not support!! [%x][%x][%x][%x]\n", tempID[0], tempID[1], tempID[2], tempID[3]);
            return Fail;
        } else {
            if(g_uIsUserConfig == 1) {
                pSM->uBlockPerFlash = Custom_uBlockPerFlash-1;
                pSM->uPagePerBlock = Custom_uPagePerBlock;
                printf("Custom_uBlockPerFlash= %d, Custom_uPagePerBlock= %d\n", Custom_uBlockPerFlash, Custom_uPagePerBlock);
            }
            printf("Auto Detect:\nBlockPerFlash= %d, PagePerBlock= %d\n", pSM->uBlockPerFlash, pSM->uPagePerBlock);
        }
    }

    switch(pSM->uNandECC) {
    case BCH_T24:
        BCHAlgoIdx = 0;
        break;
    case BCH_T8:
        BCHAlgoIdx = 2;
        break;
    case BCH_T12:
        BCHAlgoIdx = 3;
        break;
    case BCH_T15:
        BCHAlgoIdx = 4;
        break;
    default:
        BCHAlgoIdx = 5;
        break;
    }
    printf("PageSize= %d, ECC= %s, ExtraDataSize= %d, SectorPerBlock= %d\n\n", pSM->uPageSize, name[BCHAlgoIdx], pSM->uSpareSize, pSM->uSectorPerBlock);
    MSG_DEBUG("NAND ID [%x][%x][%x][%x]\n", tempID[0], tempID[1], tempID[2], tempID[3]);
    return Successful;
}

/*-----------------------------------------------------------------------------
 * 2011/7/28 by CJChen1@nuvoton.com, To issue command and address to NAND flash chip
 *  to order NAND flash chip to prepare the data or RA data at chip side and wait FMI to read actually.
 *  Support large page size 2K / 4K / 8K.
 *  INPUT: ucColAddr = 0 means prepare data from begin of page;
 *                   = <page size> means prepare RA data from begin of spare area.
 *---------------------------------------------------------------------------*/
INT fmiSM2BufferM_large_page(UINT32 uPage, UINT32 ucColAddr)
{
    // clear R/B flag
    while(!(inpw(REG_NANDINTSTS) & 0x40000));
    outpw(REG_NANDINTSTS, 0x400);

    outpw(REG_NANDCMD, 0x00);       // read command
    outpw(REG_NANDADDR, ucColAddr);                 // CA0 - CA7
    outpw(REG_NANDADDR, (ucColAddr >> 8) & 0xFF);   // CA8 - CA11
    outpw(REG_NANDADDR, uPage & 0xff);              // PA0 - PA7

    if (!pSM->bIsMulticycle)
        outpw(REG_NANDADDR, ((uPage >> 8) & 0xff)|0x80000000);    // PA8 - PA15
    else {
        outpw(REG_NANDADDR, (uPage >> 8) & 0xff);                 // PA8 - PA15
        outpw(REG_NANDADDR, ((uPage >> 16) & 0xff)|0x80000000);   // PA16 - PA18
    }
    outpw(REG_NANDCMD, 0x30);       // read command

    if (!fmiSMCheckRB())
        return FMI_SM_RB_ERR;
    else
        return 0;
}


INT fmiSM_Read_RA(UINT32 uPage, UINT32 ucColAddr)
{
    return fmiSM2BufferM_large_page(uPage, ucColAddr);
}

INT fmiCheckInvalidBlockExcept0xF0(FMI_SM_INFO_T *pSM, UINT32 BlockNo)
{
    int volatile status=0;
    unsigned int volatile sector;
    unsigned char volatile data512=0xff, data517=0xff, blockStatus=0xff;

    if (pSM->bIsMLCNand == TRUE)
        sector = (BlockNo+1) * pSM->uPagePerBlock - 1;
    else
        sector = BlockNo * pSM->uPagePerBlock;
    MSG_DEBUG("sector %d, BlockNo %d        pSM->uPagePerBlock = %d\n", sector, BlockNo, pSM->uPagePerBlock);
    status = fmiSM_Read_RA(sector, pSM->uPageSize);
    if (status < 0) {
        MSG_DEBUG("ERROR: fmiCheckInvalidBlock(), for block %d, return 0x%x\n", BlockNo, status);
        return -1;  // storage error
    }
    blockStatus = inpw(REG_NANDDATA) & 0xff;
    MSG_DEBUG("blockStatus= 0x%x\n", blockStatus);
    if (blockStatus == 0xFF || blockStatus == 0xF0) {
        fmiSM_Reset();
        status = fmiSM_Read_RA(sector+1, pSM->uPageSize);
        if (status < 0) {
            MSG_DEBUG("ERROR: fmiCheckInvalidBlock(), for block %d, return 0x%x\n", BlockNo, status);
            return -1;  // storage error
        }
        blockStatus = inpw(REG_NANDDATA) & 0xff;
        MSG_DEBUG("blockStatus= 0x%x\n", blockStatus);
        if (blockStatus != 0xFF && blockStatus != 0xF0 ) {
            MSG_DEBUG("ERROR: blockStatus != 0xFF(0x%2x)\n", blockStatus);
            fmiSM_Reset();
            return 1;   // invalid block
        }
    } else {
        fmiSM_Reset();
        return 1;   // invalid block
    }

    fmiSM_Reset();
    return 0;
}

INT fmiCheckInvalidBlock(FMI_SM_INFO_T *pSM, UINT32 BlockNo)
{
    int volatile status=0;
    unsigned int volatile sector;
    unsigned char volatile data512=0xff, data517=0xff, blockStatus=0xff;

    if (pSM->bIsMLCNand == TRUE)
        sector = (BlockNo+1) * pSM->uPagePerBlock - 1;
    else
        sector = BlockNo * pSM->uPagePerBlock;

    status = fmiSM_Read_RA(sector, pSM->uPageSize);
    if (status < 0) {
        MSG_DEBUG("ERROR: fmiCheckInvalidBlock(), for block %d, return 0x%x\n", BlockNo, status);
        return -1;  // storage error
    }
    blockStatus = inpw(REG_NANDDATA) & 0xff;
    if (blockStatus == 0xFF) {
        fmiSM_Reset();
        status = fmiSM_Read_RA(sector+1, pSM->uPageSize);
        if (status < 0) {
            MSG_DEBUG("ERROR: fmiCheckInvalidBlock(), for block %d, return 0x%x\n", BlockNo, status);
            return -1;  // storage error
        }
        blockStatus = inpw(REG_NANDDATA) & 0xff;
        if (blockStatus != 0xFF) {
            fmiSM_Reset();
            return 1;   // invalid block
        }
    } else {
        fmiSM_Reset();
        return 1;   // invalid block
    }

    fmiSM_Reset();
    return 0;
}


INT fmiSM_BlockErase(FMI_SM_INFO_T *pSM, UINT32 uBlock)
{
    UINT32 page_no;

#ifndef ERASE_WITH_0XF0
    if (fmiCheckInvalidBlock(pSM, uBlock) != 1)
#else
    if (fmiCheckInvalidBlockExcept0xF0(pSM, uBlock) == 0)
#endif
    {
        page_no = uBlock * pSM->uPagePerBlock;      // get page address

        while(!(inpw(REG_NANDINTSTS) & 0x40000));
        outpw(REG_NANDINTSTS, 0x400);

        if (inpw(REG_NANDINTSTS) & 0x4) {
            MSG_DEBUG("erase: error sector !!\n");
            outpw(REG_NANDINTSTS, 0x4);
        }
        outpw(REG_NANDCMD, 0x60);     // erase setup command

        outpw(REG_NANDADDR, (page_no & 0xff));        // PA0 - PA7
        if (!pSM->bIsMulticycle)
            outpw(REG_NANDADDR, ((page_no  >> 8) & 0xff)|0x80000000);     // PA8 - PA15
        else {
            outpw(REG_NANDADDR, ((page_no  >> 8) & 0xff));        // PA8 - PA15
            outpw(REG_NANDADDR, ((page_no  >> 16) & 0xff)|0x80000000);        // PA16 - PA17
        }

        outpw(REG_NANDCMD, 0xd0);     // erase command
        if (!fmiSMCheckRB())
            return FMI_SM_RB_ERR;

        outpw(REG_NANDCMD, 0x70);     // status read command
        if (inpw(REG_NANDDATA) & 0x01)    // 1:fail; 0:pass
            return FMI_SM_STATUS_ERR;
    }
#ifndef ERASE_WITH_0XF0
    else if(fmiCheckInvalidBlock(pSM, uBlock) == -1)
#else
    else if(fmiCheckInvalidBlockExcept0xF0(pSM, uBlock) == -1)
#endif
    {
        MSG_DEBUG("ERROR: storage error\n");
        return -1;  // storage error
    } else {
        return Fail;
    }
    return Successful;
}

INT fmiSM_BlockEraseBad(FMI_SM_INFO_T *pSM, UINT32 uBlock)
{
    UINT32 page_no;

    page_no = uBlock * pSM->uPagePerBlock;      // get page address

    while(!(inpw(REG_NANDINTSTS) & 0x40000));
    outpw(REG_NANDINTSTS, 0x400);

    if (inpw(REG_NANDINTSTS) & 0x4) {
        MSG_DEBUG("erase: error sector !!\n");
        outpw(REG_NANDINTSTS, 0x4);
    }
    outpw(REG_NANDCMD, 0x60);     // erase setup command

    outpw(REG_NANDADDR, (page_no & 0xff));        // PA0 - PA7
    if (!pSM->bIsMulticycle)
        outpw(REG_NANDADDR, ((page_no  >> 8) & 0xff)|0x80000000);     // PA8 - PA15
    else {
        outpw(REG_NANDADDR, ((page_no  >> 8) & 0xff));        // PA8 - PA15
        outpw(REG_NANDADDR, ((page_no  >> 16) & 0xff)|0x80000000);        // PA16 - PA17
    }

    outpw(REG_NANDCMD, 0xd0);     // erase command
    if (!fmiSMCheckRB())
        return FMI_SM_RB_ERR;

    outpw(REG_NANDCMD, 0x70);     // status read command
    if (inpw(REG_NANDDATA) & 0x01)    // 1:fail; 0:pass
        return FMI_SM_STATUS_ERR;

    return Successful;
}


INT fmiMarkBadBlock(FMI_SM_INFO_T *pSM, UINT32 BlockNo)
{
    UINT32 uSector, ucColAddr;

    /* check if MLC NAND */
    if (pSM->bIsMLCNand == TRUE) {
        uSector = (BlockNo+1) * pSM->uPagePerBlock - 1; // write last page
        ucColAddr = pSM->uPageSize;

        // send command
        outpw(REG_NANDCMD, 0x80);       // serial data input command
        outpw(REG_NANDADDR, ucColAddr); // CA0 - CA7
        outpw(REG_NANDADDR, (ucColAddr >> 8) & 0xff);   // CA8 - CA11
        outpw(REG_NANDADDR, uSector & 0xff);    // PA0 - PA7
        if (!pSM->bIsMulticycle)
            outpw(REG_NANDADDR, ((uSector >> 8) & 0xff)|0x80000000);        // PA8 - PA15
        else {
            outpw(REG_NANDADDR, (uSector >> 8) & 0xff);     // PA8 - PA15
            outpw(REG_NANDADDR, ((uSector >> 16) & 0xff)|0x80000000);       // PA16 - PA17
        }
        outpw(REG_NANDDATA, 0xf0);  // mark bad block (use 0xf0 instead of 0x00 to differ from Old (Factory) Bad Blcok Mark)
        outpw(REG_NANDCMD, 0x10);

        if (! fmiSMCheckRB())
            return FMI_SM_RB_ERR;

        fmiSM_Reset();
        return 0;
    }
    /* SLC check the 2048 byte of 1st or 2nd page per block */
    else {  // SLC
        uSector = BlockNo * pSM->uPagePerBlock;     // write lst page
        if (pSM->uPageSize == 512) {
            ucColAddr = 0;          // write 4096th byte
            goto _mark_512;
        } else
            ucColAddr = pSM->uPageSize;

        // send command
        outpw(REG_NANDCMD, 0x80);       // serial data input command
        outpw(REG_NANDADDR, ucColAddr); // CA0 - CA7
        outpw(REG_NANDADDR, (ucColAddr >> 8) & 0xff);   // CA8 - CA11
        outpw(REG_NANDADDR, uSector & 0xff);    // PA0 - PA7
        if (!pSM->bIsMulticycle)
            outpw(REG_NANDADDR, ((uSector >> 8) & 0xff)|0x80000000);        // PA8 - PA15
        else {
            outpw(REG_NANDADDR, (uSector >> 8) & 0xff);     // PA8 - PA15
            outpw(REG_NANDADDR, ((uSector >> 16) & 0xff)|0x80000000);       // PA16 - PA17
        }
        outpw(REG_NANDDATA, 0xf0);  // mark bad block (use 0xf0 instead of 0x00 to differ from Old (Factory) Bad Blcok Mark)
        outpw(REG_NANDCMD, 0x10);

        if (! fmiSMCheckRB())
            return FMI_SM_RB_ERR;

        fmiSM_Reset();
        return 0;

_mark_512:

        outpw(REG_NANDCMD, 0x50);       // point to redundant area
        outpw(REG_NANDCMD, 0x80);       // serial data input command
        outpw(REG_NANDADDR, ucColAddr); // CA0 - CA7
        outpw(REG_NANDADDR, uSector & 0xff);    // PA0 - PA7
        if (!pSM->bIsMulticycle)
            outpw(REG_NANDADDR, ((uSector >> 8) & 0xff)|0x80000000);        // PA8 - PA15
        else {
            outpw(REG_NANDADDR, (uSector >> 8) & 0xff);     // PA8 - PA15
            outpw(REG_NANDADDR, ((uSector >> 16) & 0xff)|0x80000000);       // PA16 - PA17
        }

        outpw(REG_NANDDATA, 0xf0);  // 512
        outpw(REG_NANDDATA, 0xff);
        outpw(REG_NANDDATA, 0xff);
        outpw(REG_NANDDATA, 0xff);
        outpw(REG_NANDDATA, 0xf0);  // 516
        outpw(REG_NANDDATA, 0xf0);  // 517
        outpw(REG_NANDCMD, 0x10);
        if (! fmiSMCheckRB())
            return FMI_SM_RB_ERR;

        fmiSM_Reset();
        return 0;
    }
}

INT fmiSM_Erase(UINT32 uChipSel,UINT32 start, UINT32 len)
{
    int i, status;
    int volatile badBlock=0;

    // erase all chip
    for (i=0; i<len; i++) {
#ifndef ERASE_WITH_0XF0
        if (fmiCheckInvalidBlock(pSM, i+start) != 1)
#else
        if (fmiCheckInvalidBlockExcept0xF0(pSM, i+start) != 1)
#endif
        {
            status = fmiSM_BlockErase(pSM, i+start);
            if (status < 0) {
                fmiMarkBadBlock(pSM, i+start);
                badBlock++;
            }
        } else
            badBlock++;

        /* send status */
        SendAck(((i+1)*100) /(len));
    }
    return badBlock;
}

INT fmiSM_EraseBad(UINT32 uChipSel,UINT32 start, UINT32 len)
{
    int i, status;
    int volatile badBlock=0;

    // erase all chip
    for (i=0; i<len; i++) {
        status = fmiSM_BlockEraseBad(pSM, i+start);
        if (status < 0) {
            fmiMarkBadBlock(pSM, i+start);
            badBlock++;
        }
        /* send status */
        SendAck(((i+1)*100) /(len));
    }
    return badBlock;
}

//-----------------------------------------------------
INT fmiSM_ChipErase(UINT32 uChipSel)
{
    int i, status;
    int volatile badBlock=0;

    // erase all chip
    for (i=0; i<=pSM->uBlockPerFlash; i++) {
#ifndef ERASE_WITH_0XF0
        if (fmiCheckInvalidBlock(pSM, i) == 0)
#else
        if (fmiCheckInvalidBlockExcept0xF0(pSM, i) == 0)
#endif
        {
            status = fmiSM_BlockErase(pSM, i);
            if (status < 0) {
                fmiMarkBadBlock(pSM, i);
                badBlock++;
            }
            /* send status */
            SendAck((i*100) / pSM->uBlockPerFlash);
        }
#ifndef ERASE_WITH_0XF0
        else if (fmiCheckInvalidBlock(pSM, i) == -1)
#else
        else if (fmiCheckInvalidBlockExcept0xF0(pSM, i) == -1)
#endif
        {
            badBlock = -1;
            /* send status */
            SendAck(0xffff);
        } else {
            badBlock++;
            /* send status */
            SendAck((i*100) / pSM->uBlockPerFlash);
        }
    }
    return badBlock;
}

INT fmiSM_ChipEraseBad(UINT32 uChipSel)
{
    int i, status;
    int volatile badBlock=0;

    // erase all chip
    for (i=0; i<=pSM->uBlockPerFlash; i++) {
        status = fmiSM_BlockEraseBad(pSM, i);
        if (status < 0) {
            fmiMarkBadBlock(pSM, i);
            badBlock++;
        }
        /* send status */
        SendAck((i*100) / pSM->uBlockPerFlash);
    }
    return badBlock;
}

/*-----------------------------------------------------------------------------
 * Really write data and parity code to 2K/4K/8K page size NAND flash by NAND commands.
 *---------------------------------------------------------------------------*/
INT fmiSM_Write_large_page_oob(UINT32 uSector, UINT32 ucColAddr, UINT32 uSAddr,UINT32 oobsize)
{
    int k;
    outpw(REG_FMI_DMASA, uSAddr);// set DMA transfer starting address

    // write the spare area configuration
    for(k=0; k<pSM->uSpareSize; k+=4)
        outpw(REG_NANDRA0+k, *(unsigned int *)(uSAddr+pSM->uPageSize+k));

    // clear R/B flag
    while(!(inpw(REG_NANDINTSTS) & 0x40000));
    outpw(REG_NANDINTSTS, 0x400);

    // send command
    outpw(REG_NANDCMD, 0x80);                       // serial data input command
    outpw(REG_NANDADDR, ucColAddr);             // CA0 - CA7
    outpw(REG_NANDADDR, (ucColAddr >> 8) & 0x3f);   // CA8 - CA12
    outpw(REG_NANDADDR, uSector & 0xff);            // PA0 - PA7

    if (!pSM->bIsMulticycle)
        outpw(REG_NANDADDR, ((uSector >> 8) & 0xff)|0x80000000);  // PA8 - PA15
    else {
        outpw(REG_NANDADDR, (uSector >> 8) & 0xff);         // PA8 - PA15
        outpw(REG_NANDADDR, ((uSector >> 16) & 0xff)|0x80000000); // PA16 - PA17
    }

    outpw(REG_NANDINTSTS, 0x1);         // clear DMA flag
    outpw(REG_NANDINTSTS, 0x4);         // clear ECC_FIELD flag
    outpw(REG_NANDINTSTS, 0x8);         // clear Region Protect flag
    outpw(REG_NANDCTL, inpw(REG_NANDCTL) | 0x10);    // auto write redundancy data to NAND after page data written
    outpw(REG_NANDCTL, inpw(REG_NANDCTL) | 0x4);     // begin to write one page data to NAND flash

    while(1) {
        if (inpw(REG_NANDINTSTS) & 0x1)         // wait to finish DMAC transfer.
            break;
    }

    outpw(REG_NANDINTSTS, 0x1);  // clear DMA flag
    outpw(REG_NANDCMD, 0x10);   // auto program command

    if (!fmiSMCheckRB())
        return FMI_SM_RB_ERR;

    //--- check Region Protect result
    if (inpw(REG_NANDINTSTS) & 0x8) {
        MSG_DEBUG("ERROR: fmiSM_Write_large_page(): region write protect detected!!\n");
        outpw(REG_NANDINTSTS, 0x8);      // clear Region Protect flag
        return Fail;
    }

    outpw(REG_NANDCMD, 0x70);           // status read command
    if (inpw(REG_NANDDATA) & 0x01) {    // 1:fail; 0:pass
        MSG_DEBUG("ERROR: fmiSM_Write_large_page(): data error!!\n");
        return FMI_SM_STATE_ERROR;
    }
    return 0;
}

INT fmiSM_Write_large_page_oob2(UINT32 uSector, UINT32 ucColAddr, UINT32 uSAddr)
{
    UINT32 readlen,i;

    readlen=pSM->uPageSize+pSM->uSpareSize;
    outpw(REG_NANDCMD,  0x80);  // serial data input command
    outpw(REG_NANDADDR, 0x00);  // CA0 - CA7
    outpw(REG_NANDADDR, 0x00);  // CA8 - CA12

    outpw(REG_NANDADDR, uSector & 0xff);                      // PA0 - PA7
    outpw(REG_NANDADDR, ((uSector >> 8) & 0xff));  // PA8 - PA15
    outpw(REG_NANDADDR, ((uSector >> 16) & 0xff));            // PA16 - PA17
    outpw(REG_NANDADDR, ((uSector >> 24) & 0xff)|0x80000000);
    MSG_DEBUG("readlen=%d\n",readlen);
    for (i=0; i<readlen; i++)
        outpw(REG_NANDDATA, ((unsigned char *)uSAddr)[i]);

    outpw(REG_NANDCMD, 0x10);               // auto program command

    if (!fmiSMCheckRB())
        return FMI_SM_RB_ERR;

    return 0;
}

INT fmiSM_Write_large_page(UINT32 uSector, UINT32 ucColAddr, UINT32 uSAddr)
{
    outpw(REG_FMI_DMASA, uSAddr);   // set DMA transfer starting address

    // set the spare area configuration
    memset((void *)REG_NANDRA0, 0xFF, 64);
    /* write byte 2050, 2051 as used page */
    outpw(REG_NANDRA0, 0x0000FFFF);

    // clear R/B flag
    while(!(inpw(REG_NANDINTSTS) & 0x40000));
    outpw(REG_NANDINTSTS, 0x400);

    // send command
    outpw(REG_NANDCMD, 0x80);                       // serial data input command
    outpw(REG_NANDADDR, ucColAddr);                 // CA0 - CA7
    outpw(REG_NANDADDR, (ucColAddr >> 8) & 0x3f);   // CA8 - CA12
    outpw(REG_NANDADDR, uSector & 0xff);            // PA0 - PA7

    if (!pSM->bIsMulticycle)
        outpw(REG_NANDADDR, ((uSector >> 8) & 0xff)|0x80000000);  // PA8 - PA15
    else {
        outpw(REG_NANDADDR, (uSector >> 8) & 0xff);               // PA8 - PA15
        outpw(REG_NANDADDR, ((uSector >> 16) & 0xff)|0x80000000); // PA16 - PA17
    }

    outpw(REG_NANDINTSTS, 0x1);         // clear DMA flag
    outpw(REG_NANDINTSTS, 0x4);         // clear ECC_FIELD flag
    outpw(REG_NANDINTSTS, 0x8);         // clear Region Protect flag
    outpw(REG_NANDCTL, inpw(REG_NANDCTL) | 0x10);    // auto write redundancy data to NAND after page data written
    outpw(REG_NANDCTL, inpw(REG_NANDCTL) | 0x4);     // begin to write one page data to NAND flash

    while(1) {
        if (inpw(REG_NANDINTSTS) & 0x1)         // wait to finish DMAC transfer.
            break;
    }

    outpw(REG_NANDINTSTS, 0x1);  // clear DMA flag
    outpw(REG_NANDCMD, 0x10);   // auto program command

    if (!fmiSMCheckRB())
        return FMI_SM_RB_ERR;

    //--- check Region Protect result
    if (inpw(REG_NANDINTSTS) & 0x8) {
        MSG_DEBUG("ERROR: fmiSM_Write_large_page(): region write protect detected!!\n");
        outpw(REG_NANDINTSTS, 0x8);      // clear Region Protect flag
        return Fail;
    }

    outpw(REG_NANDCMD, 0x70);           // status read command
    if (inpw(REG_NANDDATA) & 0x01) {    // 1:fail; 0:pass
        MSG_DEBUG("ERROR: fmiSM_Write_large_page(): data error!!\n");
        return FMI_SM_STATE_ERROR;
    }
    return 0;
}


static void fmiSM_CorrectData_BCH(UINT8 ucFieidIndex, UINT8 ucErrorCnt, UINT8* pDAddr)
{
    UINT32 uaData[24], uaAddr[24];
    UINT32 uaErrorData[4];
    UINT8  ii, jj;
    UINT32 uPageSize;
    UINT32 field_len, padding_len, parity_len;
    UINT32 total_field_num;
    UINT8  *smra_index;

    //--- assign some parameters for different BCH and page size
    switch (inpw(REG_NANDCTL) & 0x7c0000) {
    case BCH_T24:
        field_len   = 1024;
        padding_len = BCH_PADDING_LEN_1024;
        parity_len  = BCH_PARITY_LEN_T24;
        break;
    case BCH_T15:
        field_len   = 512;
        padding_len = BCH_PADDING_LEN_512;
        parity_len  = BCH_PARITY_LEN_T15;
        break;
    case BCH_T12:
        field_len   = 512;
        padding_len = BCH_PADDING_LEN_512;
        parity_len  = BCH_PARITY_LEN_T12;
        break;
    case BCH_T8:
        field_len   = 512;
        padding_len = BCH_PADDING_LEN_512;
        parity_len  = BCH_PARITY_LEN_T8;
        break;
//    case BCH_T4:
//        field_len   = 512;
//        padding_len = BCH_PADDING_LEN_512;
//        parity_len  = BCH_PARITY_LEN_T4;
//        break;
    default:
        printf("ERROR: fmiSM_CorrectData_BCH(): invalid SMCR_BCH_TSEL = 0x%08X\n", (UINT32)(inpw(REG_NANDCTL) & 0x7c0000));
        return;
    }

    uPageSize = inpw(REG_NANDCTL) & 0x30000;
    switch (uPageSize) {
    case 0x30000:
        total_field_num = 8192 / field_len;
        break;
    case 0x20000:
        total_field_num = 4096 / field_len;
        break;
    case 0x10000:
        total_field_num = 2048 / field_len;
        break;
    case 0x00000:
        total_field_num =  512 / field_len;
        break;
    default:
        MSG_DEBUG("ERROR: fmiSM_CorrectData_BCH(): invalid SMCR_PSIZE = 0x%08X\n", uPageSize);
        return;
    }

    //--- got valid BCH_ECC_DATAx and parse them to uaData[]
    // got the valid register number of BCH_ECC_DATAx since one register include 4 error bytes
    jj = ucErrorCnt/4;
    jj ++;
    if (jj > 6)
        jj = 6;     // there are 6 BCH_ECC_DATAx registers to support BCH T24

    for(ii=0; ii<jj; ii++) {
        uaErrorData[ii] = inpw(REG_NANDECCED0 + ii*4);
    }

    for(ii=0; ii<jj; ii++) {
        uaData[ii*4+0] = uaErrorData[ii] & 0xff;
        uaData[ii*4+1] = (uaErrorData[ii]>>8) & 0xff;
        uaData[ii*4+2] = (uaErrorData[ii]>>16) & 0xff;
        uaData[ii*4+3] = (uaErrorData[ii]>>24) & 0xff;
    }

    //--- got valid REG_BCH_ECC_ADDRx and parse them to uaAddr[]
    // got the valid register number of REG_BCH_ECC_ADDRx since one register include 2 error addresses
    jj = ucErrorCnt/2;
    jj ++;
    if (jj > 12)
        jj = 12;    // there are 12 REG_BCH_ECC_ADDRx registers to support BCH T24

    for(ii=0; ii<jj; ii++) {
        uaAddr[ii*2+0] = inpw(REG_NANDECCEA0 + ii*4) & 0x07ff;   // 11 bits for error address
        uaAddr[ii*2+1] = (inpw(REG_NANDECCEA0 + ii*4)>>16) & 0x07ff;
    }

    //--- pointer to begin address of field that with data error
    pDAddr += (ucFieidIndex-1) * field_len;

    //--- correct each error bytes
    for(ii=0; ii<ucErrorCnt; ii++) {
        // for wrong data in field
        if (uaAddr[ii] < field_len) {
            MSG_DEBUG("BCH error corrected for data: address 0x%08X, data [0x%02X] --> ", pDAddr+uaAddr[ii], *(pDAddr+uaAddr[ii]));
            *(pDAddr+uaAddr[ii]) ^= uaData[ii];

            MSG_DEBUG("[0x%02X]\n", *(pDAddr+uaAddr[ii]));
        }
        // for wrong first-3-bytes in redundancy area
        else if (uaAddr[ii] < (field_len+3)) {
            uaAddr[ii] -= field_len;
            uaAddr[ii] += (parity_len*(ucFieidIndex-1));    // field offset

            MSG_DEBUG("BCH error corrected for 3 bytes: address 0x%08X, data [0x%02X] --> ",
                      (UINT8 *)REG_NANDRA0+uaAddr[ii], *((UINT8 *)REG_NANDRA0+uaAddr[ii]));

            *((UINT8 *)REG_NANDRA0+uaAddr[ii]) ^= uaData[ii];

            MSG_DEBUG("[0x%02X]\n", *((UINT8 *)REG_NANDRA0+uaAddr[ii]));
        }
        // for wrong parity code in redundancy area
        else {
            // BCH_ERR_ADDRx = [data in field] + [3 bytes] + [xx] + [parity code]
            //                                   |<--     padding bytes      -->|
            // The BCH_ERR_ADDRx for last parity code always = field size + padding size.
            // So, the first parity code = field size + padding size - parity code length.
            // For example, for BCH T12, the first parity code = 512 + 32 - 23 = 521.
            // That is, error byte address offset within field is
            uaAddr[ii] = uaAddr[ii] - (field_len + padding_len - parity_len);

            // smra_index point to the first parity code of first field in register SMRA0~n
            smra_index = (UINT8 *)(REG_NANDRA0 + (inpw(REG_NANDRACTL) & 0x1ff) - // bottom of all parity code -
                                   (parity_len * total_field_num)             // byte count of all parity code
                                  );

            // final address = first parity code of first field +
            //                 offset of fields +
            //                 offset within field
            MSG_DEBUG("BCH error corrected for parity: address 0x%08X, data [0x%02X] --> ",
                      smra_index + (parity_len * (ucFieidIndex-1)) + uaAddr[ii],
                      *((UINT8 *)smra_index + (parity_len * (ucFieidIndex-1)) + uaAddr[ii]));
            *((UINT8 *)smra_index + (parity_len * (ucFieidIndex-1)) + uaAddr[ii]) ^= uaData[ii];
            MSG_DEBUG("[0x%02X]\n", *((UINT8 *)smra_index + (parity_len * (ucFieidIndex-1)) + uaAddr[ii]));
        }
    }   // end of for (ii<ucErrorCnt)
}

INT fmiSM_Read_move_data_ecc_check(UINT32 uDAddr)
{
    UINT32 uStatus;
    UINT32 uErrorCnt, ii, jj;
    volatile UINT32 uError = 0;
    UINT32 uLoop;

    //--- uLoop is the number of SM_ECC_STx should be check.
    //      One SM_ECC_STx include ECC status for 4 fields.
    //      Field size is 1024 bytes for BCH_T24 and 512 bytes for other BCH.
    switch (inpw(REG_NANDCTL) & 0x30000) {
    case 0x10000:
        uLoop = 1;
        break;
    case 0x20000:
        if (inpw(REG_NANDCTL) & 0x7c0000 == BCH_T24)
            uLoop = 1;
        else
            uLoop = 2;
        break;
    case 0x30000:
        if (inpw(REG_NANDCTL) & 0x7c0000 == BCH_T24)
            uLoop = 2;
        else
            uLoop = 4;
        break;
    default:
        return -1; // don't work for 512 bytes page
    }

    outpw(REG_FMI_DMASA, uDAddr); // set DMA transfer starting address
    outpw(REG_NANDINTSTS, 0x1); // clear DMA flag
    outpw(REG_NANDINTSTS, 0x4); // clear ECC_FIELD flag
    outpw(REG_NANDCTL, inpw(REG_NANDCTL) | 0x2); // begin to move data by DMA

    //--- waiting for DMA transfer stop since complete or ECC error
    // IF no ECC error, DMA transfer complete and make SMCR[DRD_EN]=0
    // IF ECC error, DMA transfer suspend     and make SMISR[ECC_FIELD_IF]=1 but keep keep SMCR[DRD_EN]=1
    //      If we clear SMISR[ECC_FIELD_IF] to 0, DMA transfer will resume.
    // So, we should keep wait if DMA not complete (SMCR[DRD_EN]=1) and no ERR error (SMISR[ECC_FIELD_IF]=0)
    while((inpw(REG_NANDCTL) & 0x2) && ((inpw(REG_NANDINTSTS) & 0x4)==0))
        ;

    //--- DMA transfer completed or suspend by ECC error, check and correct ECC error
    while(1) {
        if (inpw(REG_NANDINTSTS) & 0x4) {
            for (jj=0; jj<uLoop; jj++) {
                uStatus = inpw(REG_NANDECCES0+jj*4);
                if (!uStatus)
                    continue;   // no error on this register for 4 fields
                // ECC error !! Check 4 fields. Each field has 512 bytes data
                for (ii=1; ii<5; ii++) {
                    if (!(uStatus & 0x3)) { // no error for this field
                        uStatus >>= 8; // next field
                        continue;
                    }

                    if ((uStatus & 0x3)==0x01) { // correctable error in field (jj*4+ii)
                        // 2011/8/17 by CJChen1@nuvoton.com, mask uErrorCnt since Fx_ECNT just has 5 valid bits
                        uErrorCnt = (uStatus >> 2) & 0x1F;
                        fmiSM_CorrectData_BCH(jj*4+ii, uErrorCnt, (UINT8*)uDAddr);
                        MSG_DEBUG("Warning: Field %d have %d BCH error. Corrected!!\n", jj*4+ii, uErrorCnt);
                        break;
                    } else if (((uStatus & 0x3)==0x02) ||
                               ((uStatus & 0x3)==0x03)) { // uncorrectable error or ECC error in 1st field
                        MSG_DEBUG("ERROR: Field %d encountered uncorrectable BCH error!!\n", jj*4+ii);
                        uError = 1;
                        break;
                    }
                    uStatus >>= 8;  // next field
                }
            }
            outpw(REG_NANDINTSTS, 0x4);     // clear ECC_FIELD_IF to resume DMA transfer
        }

        if (inpw(REG_NANDINTSTS) & 0x1) {    // wait to finish DMAC transfer.
            if ( !(inpw(REG_NANDINTSTS) & 0x4) )
                break;
        }
    } // end of while(1)

    if (uError)
        return -1;
    else
        return 0;
}

INT fmiSM_Read_large_page(FMI_SM_INFO_T *pSM, UINT32 uPage, UINT32 uDAddr)
{
    INT result;

    result = fmiSM2BufferM_large_page(uPage, 0);
    if (result != 0)
        return result;  // fail for FMI_SM_RB_ERR
    result = fmiSM_Read_move_data_ecc_check(uDAddr);
    if(result<0) {
        MSG_DEBUG("  ==> page_no=%d,address=0x%08x\n",uPage,uDAddr);
    }
    return result;
}


BOOL volatile _usbd_bIsFMIInit = FALSE;
INT fmiHWInit(void)
{
    if (_usbd_bIsFMIInit == FALSE) {
        // Enable SD Card Host Controller operation and driving clock.
        outpw(REG_CLK_HCLKEN, (inpw(REG_CLK_HCLKEN) | 0x700000)); /* enable FMI, NAND, SD clock */
        // SD Initial
        outpw(REG_EMMC_DMACTL, DMAC_CSR_SWRST | DMAC_CSR_EN);
        while(inpw(REG_EMMC_DMACTL) & DMAC_CSR_SWRST);
        // NAND Initial
        outpw(REG_FMI_DMACTL, DMAC_CSR_SWRST | DMAC_CSR_EN);
        while(inpw(REG_FMI_DMACTL) & DMAC_CSR_SWRST);
        // reset FMI engine
        outpw(REG_FMI_CTL, inpw(REG_FMI_CTL) |FMI_CSR_SWRST);
        while(inpw(REG_FMI_CTL) & FMI_CSR_SWRST);
        _usbd_bIsFMIInit = TRUE;
        MSG_DEBUG(" fmiHWInit=0x%08x\n",_usbd_bIsFMIInit);
    }
    return 0;
} /* end fmiHWInit */

INT fmiNandInit(void)
{
    MSG_DEBUG("REG_CLK_HCLKEN = 0x%x\n", inpw(REG_CLK_HCLKEN));
    /* select NAND function pins */
    /* Set GPC for NAND */
    outpw(REG_SYS_GPC_MFPL, 0x33333330);
    outpw(REG_SYS_GPC_MFPH, 0x33333333);

    // Enable NAND flash functionality of FMI
    outpw(REG_FMI_CTL, FMI_CSR_SM_EN);

    /* set page size = 512B (default) enable CS0, disable CS1 */
    outpw(REG_NANDCTL, inpw(REG_NANDCTL) & ~0x02030000 | 0x04000000);
    MSG_DEBUG(">>>>>> REG_NANDCTL = 0x%x\n", inpw(REG_NANDCTL));

    outpw(REG_NANDTMCTL, 0x20305);
    outpw(REG_NANDCTL, (inpw(REG_NANDCTL) & ~0x30000) | 0x00000); //512 byte
    outpw(REG_NANDCTL, inpw(REG_NANDCTL) |  0x100); //protect RA 3 byte
    outpw(REG_NANDCTL, inpw(REG_NANDCTL) | 0x10);// Enable auto write redundant data out to NAND flash

    memset((char *)&SMInfo, 0, sizeof(FMI_SM_INFO_T));
    pSM = &SMInfo;
    // NAND information for setting page size, BCH, spare size , etc
    if (fmiSM_ReadID(pSM) < 0)
        return Fail;
    fmiSM_Initial(pSM);

    return 0;
} /* end fmiHWInit */
