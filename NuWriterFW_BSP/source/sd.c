/******************************************************************************
 * @file     SD.c
 * @version  V0.10
 * $Revision: 1 $
 * $Date: 18/11/01 1:17p $
 * @brief    NUC980 series SD driver source file
 *
 * @note
 * Copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/

#include "sys.h"
#include "sd.h"
#include "usbd.h"


#define SD_BLOCK_SIZE   512
#define SD_RSTCNT       0x100

// global variables
// For response R3 (such as ACMD41, CRC-7 is invalid; but SD controller will still
//      calculate CRC-7 and get an error result, software should ignore this error and clear SDISR [CRC_IF] flag
//      _sdio_uR3_CMD is the flag for it. 1 means software should ignore CRC-7 error
UINT32 _sd_uR3_CMD=0;
UINT32 _sd_uR7_CMD=0;

UINT8 *_sd_pSDHCBuffer;
UINT32 _sd_ReferenceClock;

__align(4096) UINT8 _sd_ucSDHCBuffer[64];
unsigned char _fmi_uceMMCBuffer[512];

void SD_CheckRB()
{
    UINT32 volatile i;

    while(1) {
        outpw(REG_FMI_EMMCCTL, inpw(REG_FMI_EMMCCTL)|SD_CSR_CLK8_OE);
        while(inpw(REG_FMI_EMMCCTL) & SD_CSR_CLK8_OE);
        if (inpw(REG_FMI_EMMCINTSTS) & SD_ISR_DATA0)
            break;
    }
    MSG_DEBUG("SD_CheckRB()\n");
}


int SD_SDCommand(FMI_SD_INFO_T *pSD, UINT8 ucCmd, UINT32 uArg)
{
    outpw(REG_FMI_EMMCCMD, uArg);
    outpw(REG_FMI_EMMCCTL, (inpw(REG_FMI_EMMCCTL)&(~SD_CSR_CMD_MASK))|(ucCmd << 8)|(SD_CSR_CO_EN));

    while(inpw(REG_FMI_EMMCCTL) & SD_CSR_CO_EN) {
        if (pSD->bIsCardInsert == FALSE)
            return SD_NO_SD_CARD;
    }

    return Successful;
}


int SD_SDCmdAndRsp(FMI_SD_INFO_T *pSD, UINT8 ucCmd, UINT32 uArg, int ntickCount)
{
    outpw(REG_FMI_EMMCCMD, uArg);
    outpw(REG_FMI_EMMCCTL, (inpw(REG_FMI_EMMCCTL)&(~SD_CSR_CMD_MASK))|(ucCmd << 8)|(SD_CSR_CO_EN | SD_CSR_RI_EN));

    if (ntickCount > 0) {
        while(inpw(REG_FMI_EMMCCTL) & SD_CSR_RI_EN) {
            if(ntickCount-- == 0) {
                outpw(REG_FMI_EMMCCTL, inpw(REG_FMI_EMMCCTL)|SD_CSR_SWRST);   // reset SD engine
                return 2;
            }
            if (pSD->bIsCardInsert == FALSE)
                return SD_NO_SD_CARD;
        }
    } else {
        while(inpw(REG_FMI_EMMCCTL) & SD_CSR_RI_EN);
    }

    if (_sd_uR7_CMD) {
        if (((inpw(REG_FMI_EMMCRESP1) & 0xff) != 0x55) && ((inpw(REG_FMI_EMMCRESP0) & 0xf) != 0x01)) {
            _sd_uR7_CMD = 0;
            return SD_CMD8_ERROR;
        }
    }

    if (!_sd_uR3_CMD) {
        if (inpw(REG_FMI_EMMCINTSTS) & SD_ISR_CRC7_OK)      // check CRC7
            return Successful;
        else
            return SD_CRC7_ERROR;
    } else { // ignore CRC error for R3 case
        _sd_uR3_CMD = 0;
        outpw(REG_FMI_EMMCINTSTS, SD_ISR_CRC_IF);
        return Successful;
    }
}


int SD_Swap32(int val)
{
#if 1
    int buf;

    buf = val;
    val <<= 24;
    val |= (buf<<8)&0xff0000;
    val |= (buf>>8)&0xff00;
    val |= (buf>>24)&0xff;
    return val;

#else
    return ((val<<24) | ((val<<8)&0xff0000) | ((val>>8)&0xff00) | (val>>24));
#endif
}

// Get 16 bytes CID or CSD
int SD_SDCmdAndRsp2(FMI_SD_INFO_T *pSD, UINT8 ucCmd, UINT32 uArg, UINT32 *puR2ptr)
{
    unsigned int i;
    unsigned int tmpBuf[5];

    outpw(REG_FMI_EMMCCMD, uArg);
    outpw(REG_FMI_EMMCCTL, (inpw(REG_FMI_EMMCCTL)&(~SD_CSR_CMD_MASK))|(ucCmd << 8)|(SD_CSR_CO_EN | SD_CSR_R2_EN));

    while(inpw(REG_FMI_EMMCCTL) & SD_CSR_R2_EN) {
        if (pSD->bIsCardInsert == FALSE)
            return SD_NO_SD_CARD;
    }

    if (inpw(REG_FMI_EMMCINTSTS) & SD_ISR_CRC7_OK) {
        for (i=0; i<5; i++)
            tmpBuf[i] = SD_Swap32(inpw(REG_EMMC_BUFFER+i*4));

        for (i=0; i<4; i++)
            *puR2ptr++ = ((tmpBuf[i] & 0x00ffffff)<<8) | ((tmpBuf[i+1] & 0xff000000)>>24);
        return Successful;
    } else
        return SD_CRC7_ERROR;
}


int SD_SDCmdAndRspDataIn(FMI_SD_INFO_T *pSD, UINT8 ucCmd, UINT32 uArg)
{
    volatile int buf;

    outpw(REG_FMI_EMMCCMD, uArg);
    outpw(REG_FMI_EMMCCTL, (inpw(REG_FMI_EMMCCTL)&(~SD_CSR_CMD_MASK))|(ucCmd << 8)|(SD_CSR_CO_EN | SD_CSR_RI_EN | SD_CSR_DI_EN));

    while (inpw(REG_FMI_EMMCCTL) & SD_CSR_RI_EN) {
        if (pSD->bIsCardInsert == FALSE)
            return SD_NO_SD_CARD;
    }
    while (inpw(REG_FMI_EMMCCTL) & SD_CSR_DI_EN) {
        if (pSD->bIsCardInsert == FALSE)
            return SD_NO_SD_CARD;
    }

    if (!(inpw(REG_FMI_EMMCINTSTS) & SD_ISR_CRC7_OK)) {     // check CRC7
        return SD_CRC7_ERROR;
    }

    if (!(inpw(REG_FMI_EMMCINTSTS) & SD_ISR_CRC16_OK)) {    // check CRC16
        return SD_CRC16_ERROR;
    }
    return Successful;
}

void SD_Set_clock(UINT32 sd_clock_khz)
{
    UINT32 div;
    if(sd_clock_khz<2000) {
        outpw(REG_CLK_DIVCTL9, (inpw(REG_CLK_DIVCTL9) & ~0x18) | (0x0 << 3)); 	    // SD clock from XIN [4:3]
        outpw(REG_CLK_DIVCTL3, (inpw(REG_CLK_DIVCTL3) & ~0x18) | (0x0 << 3)); 	    // SD clock from XIN [4:3]
        div=(12000/sd_clock_khz)-1;
    } else {
        outpw(REG_CLK_DIVCTL9, (inpw(REG_CLK_DIVCTL9) & ~0x18) | (0x3 << 3)); 	    // SD clock from UPLL [4:3]
        outpw(REG_CLK_DIVCTL3, (inpw(REG_CLK_DIVCTL3) & ~0x18) | (0x3 << 3)); 	    // SD clock from XIN [4:3]
        div=(300000/sd_clock_khz)-1;
    }
    outpw(REG_CLK_DIVCTL9, (inpw(REG_CLK_DIVCTL9) & ~0xff00) | ((div) << 8)); 	// SD clock divided by CLKDIV9[SD_N] [15:8]
    outpw(REG_CLK_DIVCTL3, (inpw(REG_CLK_DIVCTL3) & ~0xff00) | ((div) << 8)); 	// SD clock divided by CLKDIV9[SD_N] [15:8]
    MSG_DEBUG("clock: sd_clock_khz= %d   div = %d, REG_CLK_DIVCTL3=0x%x   REG_CLK_DIVCTL9=0x%x\n", sd_clock_khz, div, inpw(REG_CLK_DIVCTL3), inpw(REG_CLK_DIVCTL9));

    return;
}

// Initial
int SD_Init(FMI_SD_INFO_T *pSD)
{
    int volatile i, status;
    unsigned int resp;
    unsigned int CIDBuffer[4];
    unsigned int volatile u32CmdTimeOut;

    // set the clock to 300KHz for SD Initial
    SD_Set_clock(300);

    // power ON 74 clock
    outpw(REG_FMI_EMMCCTL, inpw(REG_FMI_EMMCCTL) | SD_CSR_CLK74_OE);
    while(inpw(REG_FMI_EMMCCTL) & SD_CSR_CLK74_OE) {
        if (pSD->bIsCardInsert == FALSE)
            return SD_NO_SD_CARD;
    }

    SD_SDCommand(pSD, 0, 0);        // reset all cards
    for (i=SD_RSTCNT; i>0; i--);

    // initial SDHC
    _sd_uR7_CMD = 1;
    u32CmdTimeOut = 5000;

    i = SD_SDCmdAndRsp(pSD, 8, 0x00000155, u32CmdTimeOut);
    if (i == Successful) {
        // SD 2.0
        SD_SDCmdAndRsp(pSD, 55, 0x00, u32CmdTimeOut);
        _sd_uR3_CMD = 1;
        SD_SDCmdAndRsp(pSD, 41, 0x40ff8000, u32CmdTimeOut); // 2.7v-3.6v
        resp = inpw(REG_FMI_EMMCRESP0);

        while (!(resp & 0x00800000)) {      // check if card is ready
            SD_SDCmdAndRsp(pSD, 55, 0x00, u32CmdTimeOut);
            _sd_uR3_CMD = 1;
            SD_SDCmdAndRsp(pSD, 41, 0x40ff8000, u32CmdTimeOut); // 3.0v-3.4v
            resp = inpw(REG_FMI_EMMCRESP0);
        }

        if ((resp & 0x00400000ul) == 0x00400000ul)
            pSD->uCardType = SD_TYPE_SD_HIGH;
        else
            pSD->uCardType = SD_TYPE_SD_LOW;
    } else {
        // SD 1.1
        SD_SDCommand(pSD, 0, 0);        // reset all cards
        for (i=0x100ul; i>0ul; i--);

        i = SD_SDCmdAndRsp(pSD, 55, 0x00, u32CmdTimeOut);
        if (i == 2) { // MMC memory
            SD_SDCommand(pSD, 0, 0);        // reset
            for (i=0x100ul; i>0ul; i--);

            _sd_uR3_CMD = 1;
            if (SD_SDCmdAndRsp(pSD, 1, 0x40ff8000ul, u32CmdTimeOut) != 2) { // eMMC memory
                resp = inpw(REG_FMI_EMMCRESP0);

                while ((resp & 0x00800000ul) != 0x00800000ul) { // check if card is ready
                    _sd_uR3_CMD = 1;
                    SD_SDCmdAndRsp(pSD, 1, 0x40ff8000ul, u32CmdTimeOut);      // high voltage
                    resp = inpw(REG_FMI_EMMCRESP0);
                }
                if ((resp & 0x00400000ul) == 0x00400000ul)
                    pSD->uCardType = SD_TYPE_EMMC;
                else
                    pSD->uCardType = SD_TYPE_MMC;
            } else {
                pSD->bIsCardInsert = FALSE;
                pSD->uCardType = SD_TYPE_UNKNOWN;
                return SD_ERR_DEVICE;
            }
        } else if (i == 0) { // SD Memory
            unsigned int u32ReadyTimeOut = 0xfful;
            _sd_uR3_CMD = 1;
            SD_SDCmdAndRsp(pSD, 41, 0x00ff8000ul, u32CmdTimeOut); // 3.0v-3.4v
            resp = inpw(REG_FMI_EMMCRESP0);
            while ((resp & 0x00800000ul) != 0x00800000ul) {
                if(u32ReadyTimeOut-- == 0ul)
                {
                    pSD->bIsCardInsert = FALSE;
                    pSD->uCardType = SD_TYPE_UNKNOWN;
                    return SD_INIT_ERROR;
                }
                SD_SDCmdAndRsp(pSD, 55, 0x00,u32CmdTimeOut);
                _sd_uR3_CMD = 1;
                SD_SDCmdAndRsp(pSD, 41, 0x00ff8000ul, u32CmdTimeOut); // 3.0v-3.4v
                resp = inpw(REG_FMI_EMMCRESP0);
            }
            pSD->uCardType = SD_TYPE_SD_LOW;
        } else {
            pSD->bIsCardInsert = FALSE;
            pSD->uCardType = SD_TYPE_UNKNOWN;
            return SD_INIT_ERROR;
        }
    }

    // CMD2, CMD3
    if (pSD->uCardType != SD_TYPE_UNKNOWN) {
        SD_SDCmdAndRsp2(pSD, 2, 0x00, CIDBuffer);
        if ((pSD->uCardType == SD_TYPE_MMC) || (pSD->uCardType == SD_TYPE_EMMC)) {
            if ((status = SD_SDCmdAndRsp(pSD, 3, 0x10000, 0)) != Successful)        // set RCA
                return status;
            pSD->uRCA = 0x10000ul;
        } else {
            if ((status = SD_SDCmdAndRsp(pSD, 3, 0x00, 0)) != Successful)       // get RCA
                return status;
            else
                pSD->uRCA = (inpw(REG_FMI_EMMCRESP0) << 8) & 0xffff0000;
        }
    }

    SD_Set_clock(24000);
    if (pSD->uCardType == SD_TYPE_SD_HIGH)
        MSG_DEBUG("This is high capacity SD memory card\n");
    if (pSD->uCardType == SD_TYPE_SD_LOW)
        MSG_DEBUG("This is standard capacity SD memory card\n");
    if (pSD->uCardType == SD_TYPE_MMC) {
        SD_Set_clock(20000);
        MSG_DEBUG("This is MMC memory card\n");
    }
    if (pSD->uCardType == SD_TYPE_EMMC) {
        SD_Set_clock(20000);
        MSG_DEBUG("This is eMMC memory card\n");
    }

    //SD_Set_clock(1000);

    return Successful;
}

int SD_SwitchToHighSpeed(FMI_SD_INFO_T *pSD)
{
    int volatile status=0;
    UINT16 current_comsumption, busy_status0;

    outpw(REG_EMMC_DMASA, (UINT32)_sd_pSDHCBuffer);   // set DMA transfer starting address
    outpw(REG_FMI_EMMCBLEN, 63); // 512 bit

    if ((status = SD_SDCmdAndRspDataIn(pSD, 6, 0x00ffff01)) != Successful)
        return Fail;

    current_comsumption = _sd_pSDHCBuffer[0]<<8 | _sd_pSDHCBuffer[1];
    if (!current_comsumption)
        return Fail;

    busy_status0 = _sd_pSDHCBuffer[28]<<8 | _sd_pSDHCBuffer[29];

    if (!busy_status0) { // function ready
        outpw(REG_EMMC_DMASA, (UINT32)_sd_pSDHCBuffer);   // set DMA transfer starting address
        outpw(REG_FMI_EMMCBLEN, 63); // 512 bit

        if ((status = SD_SDCmdAndRspDataIn(pSD, 6, 0x80ffff01)) != Successful)
            return Fail;

        // function change timing: 8 clocks
        outpw(REG_FMI_EMMCCTL, inpw(REG_FMI_EMMCCTL)|SD_CSR_CLK8_OE);
        while(inpw(REG_FMI_EMMCCTL) & SD_CSR_CLK8_OE);

        current_comsumption = _sd_pSDHCBuffer[0]<<8 | _sd_pSDHCBuffer[1];
        if (!current_comsumption)
            return Fail;

        return Successful;
    } else
        return Fail;
}


int SD_SelectCardType(FMI_SD_INFO_T *pSD)
{
    int volatile status=0;
    uint32_t param;

    if ((status = SD_SDCmdAndRsp(pSD, 7, pSD->uRCA, 0)) != Successful)
        return status;

    SD_CheckRB();
    MSG_DEBUG("SD_SelectCardType #367\n");
    // if SD card set 4bit
    if (pSD->uCardType == SD_TYPE_SD_HIGH) {
        _sd_pSDHCBuffer = (UINT8 *)((UINT32)_sd_ucSDHCBuffer);
        outpw(REG_EMMC_DMASA, (UINT32)_sd_pSDHCBuffer);   // set DMA transfer starting address

        if ((status = SD_SDCmdAndRsp(pSD, 55, pSD->uRCA, 0)) != Successful)
            return status;

        outpw(REG_FMI_EMMCBLEN, 7);  // 64 bit

        if ((status = SD_SDCmdAndRspDataIn(pSD, 51, 0x00)) != Successful)
            return status;

        if ((_sd_ucSDHCBuffer[0] & 0xf) == 0x2) {
            status = SD_SwitchToHighSpeed(pSD);
            if (status == Successful) {
                /* divider */
                SD_Set_clock(24000);
            }
        }

        if ((status = SD_SDCmdAndRsp(pSD, 55, pSD->uRCA, 0)) != Successful) {
            printf("Error  SD_SelectCardType  #391  status =0x%x\n", status);
            return status;
        }

        if ((status = SD_SDCmdAndRsp(pSD, 6, 0x02, 0)) != Successful) { // set bus width
            printf("Error  SD_SelectCardType  #397  status =0x%x\n", status);
            return status;
        }

        outpw(REG_FMI_EMMCCTL, inpw(REG_FMI_EMMCCTL)|SD_CSR_DBW_4BIT);
    } else if (pSD->uCardType == SD_TYPE_SD_LOW) {
        _sd_pSDHCBuffer = (UINT8 *)((UINT32)_sd_ucSDHCBuffer);
        outpw(REG_EMMC_DMASA, (UINT32)_sd_pSDHCBuffer);   // set DMA transfer starting address
        outpw(REG_FMI_EMMCBLEN, 7);  // 64 bit

        if ((status = SD_SDCmdAndRsp(pSD, 55, pSD->uRCA, 0)) != Successful) {
            printf("Error  SD_SelectCardType  #409  status =0x%x\n", status);
            return status;
        }

        if ((status = SD_SDCmdAndRspDataIn(pSD, 51, 0x00)) != Successful) {
            printf("Error SD_SelectCardType  #413  status =0x%x\n", status);
            return status;
        }

        // set data bus width. ACMD6 for SD card, SDCR_DBW for host.
        if ((status = SD_SDCmdAndRsp(pSD, 55, pSD->uRCA, 0)) != Successful) {
            printf("Error  SD_SelectCardType  #419  status =0x%x\n", status);
            return status;
        }

        if ((status = SD_SDCmdAndRsp(pSD, 6, 0x02, 0)) != Successful) { // set bus width
            printf("Error  SD_SelectCardType  #424  status =0x%x\n", status);
            return status;
        }

        outpw(REG_FMI_EMMCCTL, inpw(REG_FMI_EMMCCTL)|SD_CSR_DBW_4BIT); // SD can't format issue.

    } else if ((pSD->uCardType == SD_TYPE_MMC) ||(pSD->uCardType == SD_TYPE_EMMC)) {

        if(pSD->uCardType == SD_TYPE_MMC) {
            outpw(REG_FMI_EMMCCTL, inpw(REG_FMI_EMMCCTL) & ~SD_CSR_DBW_4BIT);
        }

        /*--- sent CMD6 to MMC card to set bus width to 4 bits mode */
        /* set CMD6 argument Access field to 3, Index to 183, Value to 1 (4-bit mode) */
        param = (3ul << 24) | (183ul << 16) | (1ul << 8);
        if ((status = SD_SDCmdAndRsp(pSD, 6ul, param, 0ul)) != Successful) {
            return status;
        }
        SD_CheckRB();

        outpw(REG_FMI_EMMCCTL, inpw(REG_FMI_EMMCCTL)|SD_CSR_DBW_4BIT);
    }

    if ((status = SD_SDCmdAndRsp(pSD, 16, SD_BLOCK_SIZE, 0)) != Successful) { // set block length
        printf("Error SD_SelectCardType  #435  status =0x%x\n", status);
        return status;
    }

    outpw(REG_FMI_EMMCBLEN, SD_BLOCK_SIZE - 1);           // set the block size
    SD_SDCommand(pSD, 7, 0);

    outpw(REG_FMI_EMMCCTL, inpw(REG_FMI_EMMCCTL)|SD_CSR_CLK8_OE);
    while(inpw(REG_FMI_EMMCCTL) & SD_CSR_CLK8_OE);
    outpw(REG_FMI_EMMCINTEN, inpw(REG_FMI_EMMCINTEN)|SD_IER_BLKD_IE);
    pSD->bIsCardInsert = 1;
    MSG_DEBUG("SD_SelectCardType  Done. REG_FMI_EMMCCTL = 0x%x\n", inpw(REG_FMI_EMMCCTL));
    return Successful;
}

/*-----------------------------------------------------------------------------
 * sdioSD_Read_in_blksize(), To read data with black size "blksize"
 *---------------------------------------------------------------------------*/
int SD_Read_in_blksize(FMI_SD_INFO_T *pSD, UINT32 uSector, UINT32 uBufcnt, UINT32 uDAddr, UINT32 blksize)
{
    char volatile bIsSendCmd=FALSE, buf;
    unsigned int volatile reg;
    int volatile i, loop, status;

    //--- check input parameters
    if (uBufcnt == 0) {
        return SD_SELECT_ERROR;
    }

    if ((status = SD_SDCmdAndRsp(pSD, 7, pSD->uRCA, 0)) != Successful)
        return status;
    SD_CheckRB();

    outpw(REG_FMI_EMMCBLEN, blksize - 1);   // the actual byte count is equal to (SDBLEN+1)
    if ((pSD->uCardType == SD_TYPE_SD_HIGH) || (pSD->uCardType == SD_TYPE_EMMC))
        outpw(REG_FMI_EMMCCMD, uSector);
    else
        outpw(REG_FMI_EMMCCMD, uSector * blksize);

    outpw(REG_EMMC_DMASA, uDAddr);

    loop = uBufcnt / 255;
    for (i=0; i<loop; i++) {
        reg = (inpw(REG_FMI_EMMCCTL) & ~SD_CSR_CMD_MASK) | 0xff0000;
        if (bIsSendCmd == FALSE) {
            outpw(REG_FMI_EMMCCTL, reg|(18<<8)|(SD_CSR_CO_EN | SD_CSR_RI_EN | SD_CSR_DI_EN));
            bIsSendCmd = TRUE;
        } else
            outpw(REG_FMI_EMMCCTL, reg | SD_CSR_DI_EN);

        while(1) {
            if ((inpw(REG_FMI_EMMCINTSTS) & SD_ISR_BLKD_IF) && (!(inpw(REG_FMI_EMMCCTL) & SD_CSR_DI_EN))) {
                outpw(REG_FMI_EMMCINTSTS, SD_ISR_BLKD_IF);
                break;
            }

            if (pSD->bIsCardInsert == FALSE)
                return SD_NO_SD_CARD;
        }

        if (!(inpw(REG_FMI_EMMCINTSTS) & SD_ISR_CRC7_OK)) {     // check CRC7
            printf("Error sdioSD_Read_in_blksize(): response error!\n");
            return SD_CRC7_ERROR;
        }

        if (!(inpw(REG_FMI_EMMCINTSTS) & SD_ISR_CRC16_OK)) {    // check CRC16
            printf("Error sdioSD_Read_in_blksize() :read data error!\n");
            return SD_CRC16_ERROR;
        }
    }

    loop = uBufcnt % 255;
    if (loop != 0) {
        reg = inpw(REG_FMI_EMMCCTL) & (~SD_CSR_CMD_MASK);
        reg = reg & (~SD_CSR_BLK_CNT_MASK);
        reg |= (loop << 16);    // setup SDCR_BLKCNT

        if (bIsSendCmd == FALSE) {
            outpw(REG_FMI_EMMCCTL, reg|(18<<8)|(SD_CSR_CO_EN | SD_CSR_RI_EN | SD_CSR_DI_EN));
            bIsSendCmd = TRUE;
        } else
            outpw(REG_FMI_EMMCCTL, reg | SD_CSR_DI_EN);

        while(1) {
            if ((inpw(REG_FMI_EMMCINTSTS) & SD_ISR_BLKD_IF) && (!(inpw(REG_FMI_EMMCCTL) & SD_CSR_DI_EN))) {
                outpw(REG_FMI_EMMCINTSTS, SD_ISR_BLKD_IF);
                break;
            }
            if (pSD->bIsCardInsert == FALSE)
                return SD_NO_SD_CARD;
        }

        if (!(inpw(REG_FMI_EMMCINTSTS) & SD_ISR_CRC7_OK)) {     // check CRC7
            //printf("sdioSD_Read_in_blksize(): response error!\n");
            return SD_CRC7_ERROR;
        }

        if (!(inpw(REG_FMI_EMMCINTSTS) & SD_ISR_CRC16_OK)) {    // check CRC16
            //printf("sdioSD_Read_in_blksize(): read data error!\n");
            return SD_CRC16_ERROR;
        }
    }

    if (SD_SDCmdAndRsp(pSD, 12, 0, 0)) {    // stop command
        //printf("stop command fail !!\n");
        return SD_CRC7_ERROR;
    }

    SD_CheckRB();
    SD_SDCommand(pSD, 7, 0);

    return Successful;
}

/*-----------------------------------------------------------------------------
 * sdioSD_Read_in(), To read data with default black size SD_BLOCK_SIZE
 *---------------------------------------------------------------------------*/
int SD_Read_in(FMI_SD_INFO_T *pSD, UINT32 uSector, UINT32 uBufcnt, UINT32 uDAddr)
{
    return SD_Read_in_blksize(pSD, uSector, uBufcnt, uDAddr, SD_BLOCK_SIZE);
}

/*-----------------------------------------------------------------------------
 * sdioSD_Write_in(), To write data with static black size SD_BLOCK_SIZE
 *---------------------------------------------------------------------------*/
int SD_Write_in(FMI_SD_INFO_T *pSD, UINT32 uSector, UINT32 uBufcnt, UINT32 uSAddr)
{
    char volatile bIsSendCmd = FALSE;
    unsigned int volatile reg;
    int volatile i, loop, status;

    //--- check input parameters
    if (uBufcnt == 0) {
        printf("#560  Error  SD_SELECT_ERROR\n");
        return SD_SELECT_ERROR;
    }

    if ((status = SD_SDCmdAndRsp(pSD, 7, pSD->uRCA, 0)) != Successful) {
        printf("#565  Error  status =0x%x\n", status);
        return status;
    }
    MSG_DEBUG("#568\n");
    SD_CheckRB();
    MSG_DEBUG("#570\n");

    // According to SD Spec v2.0, the write CMD block size MUST be 512, and the start address MUST be 512*n.
    outpw(REG_FMI_EMMCBLEN, SD_BLOCK_SIZE - 1);           // set the block size

    if ((pSD->uCardType == SD_TYPE_SD_HIGH) || (pSD->uCardType == SD_TYPE_EMMC))
        outpw(REG_FMI_EMMCCMD, uSector);
    else
        outpw(REG_FMI_EMMCCMD, uSector * SD_BLOCK_SIZE);  // set start address for SD CMD

    outpw(REG_EMMC_DMASA, uSAddr);
    loop = uBufcnt / 255;
    for (i=0; i<loop; i++) {
        reg = (inpw(REG_FMI_EMMCCTL) & 0xff00c080)|0xff0000;
        if (!bIsSendCmd) {
            outpw(REG_FMI_EMMCCTL, reg|(25<<8)|(SD_CSR_CO_EN | SD_CSR_RI_EN | SD_CSR_DO_EN));
            bIsSendCmd = TRUE;
        } else
            outpw(REG_FMI_EMMCCTL, reg | SD_CSR_DO_EN);

        while(1) {
            if ((inpw(REG_FMI_EMMCINTSTS) & SD_ISR_BLKD_IF) && (!(inpw(REG_FMI_EMMCCTL) & SD_CSR_DO_EN))) {
                outpw(REG_FMI_EMMCINTSTS, SD_ISR_BLKD_IF);
                break;
            }
            if (pSD->bIsCardInsert == FALSE)
                return SD_NO_SD_CARD;
        }

        if ((inpw(REG_FMI_EMMCINTSTS) & SD_ISR_CRC_IF) != 0) {      // check CRC
            outpw(REG_FMI_EMMCINTSTS, SD_ISR_CRC_IF);
            printf("#599 Error  SD_CRC_ERROR = 0x%x\n", SD_CRC_ERROR);
            return SD_CRC_ERROR;
        }
    }

    loop = uBufcnt % 255;
    if (loop != 0) {
        reg = (inpw(REG_FMI_EMMCCTL) & 0xff00c080) | (loop << 16);
        if (!bIsSendCmd) {
            outpw(REG_FMI_EMMCCTL, reg|(25<<8)|(SD_CSR_CO_EN | SD_CSR_RI_EN | SD_CSR_DO_EN));
            bIsSendCmd = TRUE;
        } else
            outpw(REG_FMI_EMMCCTL, reg | SD_CSR_DO_EN);

        while(1) {
            if ((inpw(REG_FMI_EMMCINTSTS) & SD_ISR_BLKD_IF) && (!(inpw(REG_FMI_EMMCCTL) & SD_CSR_DO_EN))) {
                outpw(REG_FMI_EMMCINTSTS, SD_ISR_BLKD_IF);
                break;
            }
            if (pSD->bIsCardInsert == FALSE)
                return SD_NO_SD_CARD;
        }

        if ((inpw(REG_FMI_EMMCINTSTS) & SD_ISR_CRC_IF) != 0) {      // check CRC
            printf("#623  Error  SD_CRC_ERROR = 0x%x\n", inpw(REG_FMI_EMMCINTSTS));
            outpw(REG_FMI_EMMCINTSTS, SD_ISR_CRC_IF);
            return SD_CRC_ERROR;
        }
    }
    outpw(REG_FMI_EMMCINTSTS, SD_ISR_CRC_IF);
    MSG_DEBUG("#628\n");
    if (SD_SDCmdAndRsp(pSD, 12, 0, 0)) {    // stop command
        printf("#630   Error  SD_CRC7_ERROR\n");
        return SD_CRC7_ERROR;
    }
    SD_CheckRB();
    MSG_DEBUG("#634\n");
    SD_SDCommand(pSD, 7, 0);
    MSG_DEBUG("#636\n");
    return Successful;
}


void SD_Get_SD_info(FMI_SD_INFO_T *pSD, DISK_DATA_T *_info)
{
    unsigned int i;
    unsigned int R_LEN, C_Size, MULT, size;
    unsigned int Buffer[4];
    unsigned char *ptr;

    SD_SDCmdAndRsp2(pSD, 9, pSD->uRCA, Buffer);

    if ((pSD->uCardType == SD_TYPE_MMC) || (pSD->uCardType == SD_TYPE_EMMC))
    {
        /* for MMC/eMMC card */
        if ((Buffer[0] & 0xc0000000) == 0xc0000000)
        {
            /* CSD_STRUCTURE [127:126] is 3 */
            /* CSD version depend on EXT_CSD register in eMMC v4.4 for card size > 2GB */
            SD_SDCmdAndRsp(pSD, 7ul, pSD->uRCA, 0ul);

            outpw(REG_EMMC_DMASA, (uint32_t)_fmi_uceMMCBuffer);
            outpw(REG_FMI_EMMCBLEN, 511ul);

            if (SD_SDCmdAndRspDataIn(pSD, 8ul, 0x00ul) == Successful)
            {
                SD_SDCommand(pSD, 7ul, 0ul);
                outpw(REG_FMI_EMMCCTL, inpw(REG_FMI_EMMCCTL) | SD_CSR_CLK8_OE);
                while ( (inpw(REG_FMI_EMMCCTL) & SD_CSR_CLK8_OE) == SD_CSR_CLK8_OE)
                {
                }

                _info->totalSectorN = (uint32_t)_fmi_uceMMCBuffer[215]<<24;
                _info->totalSectorN |= (uint32_t)_fmi_uceMMCBuffer[214]<<16;
                _info->totalSectorN |= (uint32_t)_fmi_uceMMCBuffer[213]<<8;
                _info->totalSectorN |= (uint32_t)_fmi_uceMMCBuffer[212];
                _info->diskSize = _info->totalSectorN / 2ul;
                MSG_DEBUG("#723 %d KB,  %d sectors \n",_info->diskSize,_info->totalSectorN);
            }
        }
        else
        {
            /* CSD version v1.0/1.1/1.2 in eMMC v4.4 spec for card size <= 2GB */
            R_LEN = (Buffer[1] & 0x000f0000ul) >> 16;
            C_Size = ((Buffer[1] & 0x000003fful) << 2) | ((Buffer[2] & 0xc0000000ul) >> 30);
            MULT = (Buffer[2] & 0x00038000ul) >> 15;
            size = (C_Size+1ul) * (1ul<<(MULT+2ul)) * (1ul<<R_LEN);

            _info->diskSize = size / 1024ul;
            _info->totalSectorN = size / 512ul;
        }
    }
    else
    {
        if ((Buffer[0] & 0xc0000000) != 0x0ul)
        {
            C_Size = ((Buffer[1] & 0x0000003ful) << 16) | ((Buffer[2] & 0xffff0000ul) >> 16);
            size = (C_Size+1ul) * 512ul;    /* Kbytes */

            _info->diskSize = size;
            _info->totalSectorN = size << 1;
        }
        else
        {
            R_LEN = (Buffer[1] & 0x000f0000ul) >> 16;
            C_Size = ((Buffer[1] & 0x000003fful) << 2) | ((Buffer[2] & 0xc0000000ul) >> 30);
            MULT = (Buffer[2] & 0x00038000ul) >> 15;
            size = (C_Size+1ul) * (1ul<<(MULT+2ul)) * (1ul<<R_LEN);

            _info->diskSize = size / 1024ul;
            _info->totalSectorN = size / 512ul;
        }
    }
    _info->sectorSize = 512;

    SD_SDCmdAndRsp2(pSD, 10, pSD->uRCA, Buffer);

    _info->vendor[0] = (Buffer[0] & 0xff000000) >> 24;
    ptr = (unsigned char *)Buffer;
    ptr = ptr + 4;
    for (i=0; i<5; i++)
        _info->product[i] = *ptr++;
    ptr = ptr + 10;
    for (i=0; i<4; i++)
        _info->serial[i] = *ptr++;

    MSG_DEBUG("The size is %d KB\n", _info->diskSize);
    MSG_DEBUG("            %d bytes * %d sectors\n", _info->sectorSize, _info->totalSectorN);
}

void SD_SetReferenceClock(UINT32 uClock)
{
    _sd_ReferenceClock = uClock;    // kHz
}


int SD_ChipErase(FMI_SD_INFO_T *pSD, DISK_DATA_T *_info)
{
    int status=0;

    status = SD_SDCmdAndRsp(pSD, 32, 512, 6000);
    if (status < 0) {
        return status;
    }
    status = SD_SDCmdAndRsp(pSD, 33, _info->totalSectorN*512, 6000);
    if (status < 0) {
        return status;
    }
    status = SD_SDCmdAndRsp(pSD, 38, 0, 6000);
    if (status < 0) {
        return status;
    }
    SD_CheckRB();

    return 0;
}

