#include <string.h>
#include <stdlib.h>
#include "NUC980.h"
#include "sys.h"
#include "etimer.h"
#include "usbd.h"
#include "fmi.h"
#include "sd.h"
#include "spinandflash.h"
#include "gpio.h"

#define TIMEOUT  1000000   /* unit of timeout is micro second */
#define QSPI_FLASH_PORT    QSPI0


extern void SetTimer(unsigned int count);
extern void DelayMicrosecond(unsigned int count);

extern SPINAND_INFO_T SNInfo, *pSN;
extern INFO_T info;

typedef struct {
    char PID;
    int (*SpiFlashWrite)(UINT32 address, UINT32 len, UCHAR *data);
} spiflash_t;



/********************
Function: Serial NAND continuous read to buffer
Argument:
addh~addl: Read data address
count: read count
return:
*********************/
void spiNAND_Continuous_Normal_Read(uint8_t* buff, uint32_t count)
{
    uint32_t volatile i = 0;
    spiNAND_CS_LOW();
    SPIin(0x03); // dummy
    SPIin(0x00); // dummy
    SPIin(0x00); // dummy
    SPIin(0x00); // dummy
    for( i = 0; i < count; i++) {
        *(buff+i) = SPIin(0x00);
    }
    spiNAND_CS_HIGH();
    return;
}

/********************
Function: W25M series (SPISTACK) die select
Argument:
    Select_die
        0x00  (power on default states)
        0x01
return:
*********************/
void spiNAND_Die_Select(uint8_t select_die)
{
    /*  reference code to avoid incorrect select die ID input
        switch(select_die){
          case 0x00:
            break;
          case 0x01:
            break;
          default:
            return;
        }
    */

    spiNAND_CS_LOW();
    SPIin(0xC2);        // Software Die Select
    SPIin(select_die);
    spiNAND_CS_HIGH();

    return;
}

/********************
Function: Serial NAND BBM Set LUT
Argument:
  LBA                   Logical block address
  PBA                   Physical block address
  * LBA or PBA          0       1       2
  * Page address        0       0x40   0x80
  return:
*********************/
void spiNAND_LUT_Set(uint16_t LBA, uint16_t PBA)
{
    spiNAND_CS_LOW();
    SPIin(0x06);
    spiNAND_CS_HIGH();

    spiNAND_CS_LOW();
    SPIin(0xA1);
    SPIin((LBA/0x100));
    SPIin((LBA%0x100));
    SPIin((PBA/0x100));
    SPIin((PBA%0x100));
    spiNAND_CS_HIGH();
}

/********************
Function: Serial NAND LUT read
Argument:
  uint16_t LBA[20];             Logical block address
  uint16_t PBA[20];             Physical block address
  For Winbond 1Gb NAND, LBA[9:0] & PBA [9:0] are effective block address. LBA[15:14] is used for additional information
return:
*********************/
void spiNAND_LUT_Read(uint16_t* LBA, uint16_t* PBA)
{
    uint16_t volatile i, buf1, buf2;
    spiNAND_CS_LOW();
    SPIin(0xA5);
    SPIin(0x00);                  // Dummy
    for(i = 0; i < 20; i++) {
        buf1 = SPIin(0x00);
        buf2 = SPIin(0x00);
        *(LBA+i) = (buf1 << 8) | buf2;
        buf1 = SPIin(0x00);
        buf2 = SPIin(0x00);
        *(PBA+i) = (buf1 << 8) | buf2;
    }
    spiNAND_CS_HIGH();
    return;
}

/********************\
Function: Serial NAND Bad block mark check
Argument:
return:
1: Check block is bad block.
0: Check block is not bad block.
update: V.1.0.8 // correct the bad block mark address
*********************/
uint8_t spiNAND_bad_block_check(uint32_t page_address)
{
    uint8_t volatile *read_buf;

    spiNAND_PageDataRead(page_address/0x100, page_address%0x100); // Read the first page of a block

    spiNAND_Normal_Read(0x8, 0x0, read_buf, 1); // Read bad block mark at 0x800 update at v.1.0.8
    if(*read_buf != 0xFF) { // update at v.1.0.7
        return 1;
    }
    spiNAND_PageDataRead((page_address+1)/0x100, (page_address+1)%0x100); // Read the second page of a block


    spiNAND_Normal_Read(0x8, 0x0, read_buf, 1); // Read bad block mark at 0x800 update at v.1.0.8
    if(*read_buf != 0xFF) { // update at v.1.0.7
        return 1;
    }
    return 0;
}

/********************
Function: Program data verify
return:
0: no mismatch
1: mismatch
*********************/
uint8_t Program_verify(uint8_t* buff1, uint8_t* buff2, uint32_t count)
{
    uint32_t volatile i = 0;
    for( i = 0; i < count; i++) {
        if( *(buff1+i) != *(buff2+i)) {
            return 1;
        }
    }
    return 0;
}

/********************
Function: Serial NAND page program
Argument:
addh, addl: input address
program_buffer: input data
count: program count
return:
*********************/
void spiNAND_Pageprogram_Pattern(uint8_t addh, uint8_t addl, uint8_t* program_buffer, uint32_t count)
{
    uint32_t volatile i = 0;

    spiNAND_CS_LOW();
    SPIin(0x06);
    spiNAND_CS_HIGH();

    spiNAND_CS_LOW();
    SPIin(0x02);
    SPIin(addh);
    SPIin(addl);
    for(i = 0; i < count; i++) {
        SPIin(*(program_buffer+i));
    }
    spiNAND_CS_HIGH();

    return;
}

/********************
Function: Serial NAND page program
Argument:
addh, addl: input address
pattern: program data
count: program count
return: ready busy count
*********************/
void spiNAND_Program_Excute(uint8_t addh, uint8_t addl)
{
    spiNAND_CS_LOW();
    /* Send command : Page program */
    SPIin(0x10);
    SPIin(0x00); // dummy
    SPIin(addh);
    SPIin(addl);
    spiNAND_CS_HIGH();
    spiNAND_ReadyBusy_Check();

    return;
}

/********************
Function: Do whole Flash protect
Argument:
*********************/
void spiNAND_Protect()
{
    uint8_t volatile SR;
    SR = spiNAND_StatusRegister(1); // Read status register 2
    SR|=0x7C; // Enable ECC-E bit
    spiNAND_StatusRegister_Write_SR1(SR);
    return;
}

/********************
Function: Do whole Flash unprotect
Argument:
*********************/
void spiNAND_Unprotect()
{
    uint8_t volatile SR;
    SR = spiNAND_StatusRegister(1); // Read status register 2
    SR&=0x83; // Enable ECC-E bit
    spiNAND_StatusRegister_Write_SR1(SR);
    return;
}

/********************
Function: Check ECC-E status
Argument:
Comment: Change function name at V1.0.3
*********************/
uint8_t spiNAND_Check_Embedded_ECC_Enable()
{
    uint8_t volatile SR;
    SR = spiNAND_StatusRegister(2);  // Read status register 2
    return (SR&0x10)>>4;
}

/********************
Function: Check P-FAIL\E-FAIL status
Argument:
Comment: Modify this function at V1.0.3
*********************/
uint8_t spiNAND_Check_Program_Erase_Fail_Flag()
{
    uint8_t volatile SR;
    SR = spiNAND_StatusRegister(3); // Read status register 3
    return (SR&0x0C)>>2; // Check P-Fail, E-Fail bit
}

/********************
Function: Check ECC-1, ECC-0 status
Argument:
Comment: Add this function at V1.0.3
*********************/
uint8_t spiNAND_Check_Embedded_ECC_Flag()
{
    uint8_t volatile SR;
    SR = spiNAND_StatusRegister(3); // Read status register 3
    return (SR&0x30)>>4; // Check ECC-1, ECC0 bit
}

/********************
Function: Enable embedded ECC
Argument:
*********************/
void spiNAND_Enable_Embedded_ECC()
{
    uint8_t volatile SR;
    SR = spiNAND_StatusRegister(2); // Read status register 2
    SR|=0x10; // Enable ECC-E bit
    spiNAND_StatusRegister_Write_SR2(SR);
    return;
}

/********************
Function: Disable embedded ECC
Argument:
*********************/
void spiNAND_Disable_Embedded_ECC()
{
    uint8_t volatile SR;
    SR = spiNAND_StatusRegister(2); // Read status register 2
    SR&= 0xEF; // Disable ECC-E bit
    spiNAND_StatusRegister_Write_SR2(SR);
    return;
}

/********************
Function: Enable buffer mode
Argument:
*********************/
void spiNAND_Enable_Buffer_mode()
{
    uint8_t volatile SR;
    SR = spiNAND_StatusRegister(2); // Read status register 2
    SR|=0x08; // Enable BUF bit
    spiNAND_StatusRegister_Write_SR2(SR);
    return;
}

/********************
Function: Disable buffer mode
Argument:
*********************/
void spiNAND_Disable_Buffer_mode()
{
    uint8_t volatile SR;
    SR = spiNAND_StatusRegister(2); // Read status register 2
    SR&= 0xF7; // Disable BUF bit
    spiNAND_StatusRegister_Write_SR2(SR);
    return;
}

/********************
Function: Serial NAND Write Status register 1
Argument:
SR1: program SR1 data
*********************/
void spiNAND_StatusRegister_Write_SR1(uint8_t SR1)
{
    spiNAND_CS_LOW();
    if(pSN->SPINand_ID == 0xEFAA21)   /* winbond */
        SPIin(0x01);
    else
        SPIin(0x1f);
    SPIin(0xA0);
    SPIin(SR1);
    spiNAND_CS_HIGH();
    spiNAND_ReadyBusy_Check();
    return;
}

/********************
Function: Serial NAND Write Status register 2
Argument:
SR2: program SR2 data
*********************/
void spiNAND_StatusRegister_Write_SR2(uint8_t SR2)
{
    spiNAND_CS_LOW();
    SPIin(0x01);
    SPIin(0xB0);
    SPIin(SR2);
    spiNAND_CS_HIGH();
    spiNAND_ReadyBusy_Check();
    return;
}

/********************
Function: Serial NAND Write Status register 3
Argument:
 : select socket
SR3: program SR3 data
*********************/
void spiNAND_StatusRegister_Write_SR3(uint8_t SR3)
{
    spiNAND_CS_LOW();
    SPIin(0x01);
    SPIin(0xC0);
    SPIin(SR3);
    spiNAND_CS_HIGH();
    spiNAND_ReadyBusy_Check();
    return;
}

/********************
Function: Serial NAND Block erase
Argument:
PA_H, PA_L: Page address
return:
*********************/
void spiNAND_BlockErase(uint8_t PA_H, uint8_t PA_L)
{
    spiNAND_CS_LOW();
    SPIin(0x06);
    spiNAND_CS_HIGH();

    //__nop();
    spiNAND_CS_LOW();
    SPIin(0xD8);
    SPIin(0x00); // dummy
    SPIin(PA_H);
    SPIin(PA_L);
    spiNAND_CS_HIGH();

    spiNAND_ReadyBusy_Check();
}

/********************
Function: Serial NAND Status register read
Argument:
sr_sel: select register
return: status register value
*********************/
uint8_t spiNAND_StatusRegister(uint8_t sr_sel)
{
    uint8_t volatile SR = 0;  // status register data
    switch(sr_sel) {
    case 0x01:
        spiNAND_CS_LOW();
        if(pSN->SPINand_ID == 0xEFAA21)   /* winbond */
            SPIin(0x05);
        else
            SPIin(0x0f);
        SPIin(0xA0); // SR1
        SR = SPIin(0x00);
        spiNAND_CS_HIGH();
        break;
    case 0x02:
        spiNAND_CS_LOW();
        SPIin(0x0F);
        SPIin(0xB0); // SR2
        SR = SPIin(0x00);
        spiNAND_CS_HIGH();
        break;
    case 0x03:
        spiNAND_CS_LOW();
        if(pSN->SPINand_ID == 0xEFAA21)   /* winbond */
            SPIin(0x05);
        else
            SPIin(0x0f);
        SPIin(0xC0); // SR3
        SR = SPIin(0x00);
        spiNAND_CS_HIGH();
        break;
    default:
        SR = 0xFF;
        break;
    }
    return SR;
}

/********************
Function: SPINAND page data read
Argument:
PA_H, page address
PA_L, page address
return:
*********************/
void spiNAND_PageDataRead(uint8_t PA_H, uint8_t PA_L)
{
    spiNAND_CS_LOW();
    /* Send command 0x13: Read data */
    SPIin(0x13); //
    SPIin(0x00); // dummy
    SPIin(PA_H); // Page address
    SPIin(PA_L); // Page address
    spiNAND_CS_HIGH();
    spiNAND_ReadyBusy_Check(); // Need to wait for the data transfer.
    return;
}

/********************
Function: Serial NAND Normal read to buffer
Argument:
addh~addl: Read data address
count: read count
return:
*********************/
void spiNAND_Normal_Read(uint8_t addh, uint8_t addl, uint8_t* buff, uint32_t count)
{
    uint32_t volatile i = 0;
    spiNAND_CS_LOW();
    /* Send command 0x03: Read data */
    SPIin(0x03);
    SPIin(addh);
    SPIin(addl);
    SPIin(0x00); // dummy
    for( i = 0; i < count; i++) {
        *(buff+i) = SPIin(0x00);
    }
    spiNAND_CS_HIGH();
    return;
}

/********************
Function: Serial NAND Quad IO read to buffer
Argument:
addh~addl: Read data address
count: read count
return:
*********************/
void spiNAND_QuadIO_Read(uint8_t addh, uint8_t addl, uint8_t* buff, uint32_t count)
{
    uint32_t volatile i = 0;
    //printf("pSN->SPINand_QuadReadCmd=0x%x\n", pSN->SPINand_QuadReadCmd);
    spiNAND_CS_LOW();

    SPIin(pSN->SPINand_QuadReadCmd);
    SPIin(addh);
    SPIin(addl);
    SPIin(0x00); // dummy
    outpw(REG_SYS_GPD_MFPL, (inpw(REG_SYS_GPD_MFPL)& ~(0xFF000000)) | 0x11000000);

    QSPI0->CTL &= ~0x1;
    while (QSPI0->STATUS & 0x8000);
    QSPI_ENABLE_QUAD_INPUT_MODE(QSPI0);
    QSPI0->CTL |= 0x1;
    while ((QSPI0->STATUS & 0x8000) == 0);

    for( i = 0; i < count; i++) {
        *(buff+i) = SPIin(0x00);
    }
    spiNAND_CS_HIGH();

    QSPI0->CTL &= ~0x1;
    while (QSPI0->STATUS & 0x8000);
    QSPI_DISABLE_QUAD_MODE(QSPI0);
    QSPI0->CTL |= 0x1;
    while ((QSPI0->STATUS & 0x8000) == 0);

    outpw(REG_SYS_GPD_MFPL, inpw(REG_SYS_GPD_MFPL)& ~(0x11000000));

    PD->MODE = (PD->MODE & 0xFFFF0FFF) | 0x5000; /* Configure PD6 and PD7 as output mode */
    PD6 = 1; /* PD6: SPI0_MOSI1 or SPI flash /WP pin */
    PD7 = 1; /* PD7: SPI0_MISO1 or SPI flash /HOLD pin */

    return;
}

/********************
Function: Serial NAND Quad Output read to buffer
Argument:
addh~addl: Read data address
count: read count
return:
*********************/
void spiNAND_QuadOutput_Read(uint8_t addh, uint8_t addl, uint8_t* buff, uint32_t count)
{
    uint32_t u32RxCounter, u32TxCounter;

    /* Configure SPI0 related multi-function pins */
    outpw(REG_SYS_GPD_MFPL, (inpw(REG_SYS_GPD_MFPL) & (~0xFF000000)) | 0x11000000);

    /* /CS: active */
    spiNAND_CS_LOW();

    /* Send command */
    outpw(REG_QSPI0_TX, 0x6b);
    outpw(REG_QSPI0_TX, 0x00); // dummy
    outpw(REG_QSPI0_TX, 0x00); // dummy
    outpw(REG_QSPI0_TX, 0x00); // dummy

    /* Reset SPI RX */
    outpw(REG_QSPI0_FIFOCTL, inpw(REG_QSPI0_FIFOCTL)|0x1);
    while(inpw(REG_QSPI0_STATUS) & 0x800000);

    /* Enable Quad IO input mode */
    QSPI0->CTL &= ~0x1;
    while (QSPI0->STATUS & 0x8000);
    QSPI_ENABLE_QUAD_INPUT_MODE(QSPI0);
    QSPI0->CTL |= 0x1;
    while ((QSPI0->STATUS & 0x8000) == 0);

    outpw(REG_QSPI0_TX, 0);
    outpw(REG_QSPI0_TX, 0);
    u32RxCounter=0;
    u32TxCounter=2;
    while(u32RxCounter<count) {
        while((inpw(REG_QSPI0_STATUS)&0x100)==0) { // RX empty
            buff[u32RxCounter] = inpw(REG_QSPI0_RX);
            u32RxCounter++;
        }
        if( ((inpw(REG_QSPI0_STATUS)&0x20000)==0)&&(u32TxCounter<count)  ) { // TX full
            outpw(REG_QSPI0_TX, 0);
            u32TxCounter++;
        }
    }
    /* Check the BUSY flag */
    spiNAND_CS_HIGH();

    /* Disable Quad IO mode */
    QSPI0->CTL &= ~0x1;
    while (QSPI0->STATUS & 0x8000);
    QSPI_DISABLE_QUAD_MODE(QSPI0);
    QSPI0->CTL |= 0x1;

    outpw(REG_SYS_GPD_MFPL, inpw(REG_SYS_GPD_MFPL)& ~(0xFF000000));
    PD->MODE = (PD->MODE & 0xFFFF0FFF) | 0x5000; /* Configure PD6 and PD7 as output mode */
    PD6 = 1; /* PD6: SPI0_MOSI1 or SPI flash /WP pin */
    PD7 = 1; /* PD7: SPI0_MISO1 or SPI flash /HOLD pin */

    return;
}

/********************
Function: Serial NAND reset command
Argument:
return:
*********************/
void spiNAND_Reset()
{
    spiNAND_CS_LOW();
    SPIin(0xFF);
    spiNAND_CS_HIGH();
    spiNAND_ReadyBusy_Check();
}


/********************
Function: SPI NAND Ready busy check
Argument:
return:
*********************/
int8_t spiNAND_ReadyBusy_Check()
{
    uint8_t volatile SR = 0xFF;

    SetTimer(5000);
    if(pSN->SPINand_ID == 0xEFAA21) { /* winbond */
    //if(1){
        while((SR & 0x1) != 0x00) {
            spiNAND_CS_LOW();
            SPIin(0x0F);
            SPIin(0xC0);
            SR = SPIin(0x00);
            spiNAND_CS_HIGH();
            if (inpw(REG_ETMR0_ISR) & 0x1) {
                outpw(REG_ETMR0_ISR, 0x1);
                printf("Error winbond spiNAND_ReadyBusy_Check timeout\n");
                return 1;
            }
        }
    } else { // MXIC
        while((SR & 0x1) != 0x00) {
            spiNAND_CS_LOW();
            SPIin(0x0F);
            SPIin(0xC0);
            /* Check the BUSY flag */
            //while(inpw(REG_QSPI0_STATUS) & 0x1);
            SR = SPIin(0x00);
            spiNAND_CS_HIGH();

            if (inpw(REG_ETMR0_ISR) & 0x1) {
                outpw(REG_ETMR0_ISR, 0x1);
                printf("Error MXIC spiNAND_ReadyBusy_Check timeout\n");
                return 1;
            }
        }
    }
			
    return 0;
}

/********************
Function: read ID
Argument: cs sel
return: ID
*********************/
uint32_t spiNAND_ReadID()
{
    uint32_t JEDECID = 0;
    spiNAND_CS_LOW();
    SPIin(0x9F);
    SPIin(0x00); // dummy
    JEDECID += SPIin(0x00);

    if(JEDECID == 0xC2) { // mxic Read ID BYTE0, BYTE1
        JEDECID <<= 8;
        JEDECID += SPIin(0x00);
    } else {
        JEDECID <<= 8;
        JEDECID += SPIin(0x00);
        JEDECID <<= 8;
        JEDECID += SPIin(0x00);
    }
    spiNAND_CS_HIGH();

    return JEDECID;
}

/********************
Function: CS select low
Argument:
return:
*********************/
void spiNAND_CS_LOW()
{
    // /CS: active
    QSPI_SET_SS_LOW(QSPI0);
}

/********************
Function: CS select high
Argument:
return:
*********************/
void spiNAND_CS_HIGH()
{
    // /CS: de-active
    QSPI_SET_SS_HIGH(QSPI0);
}

/********************
Function: SPI hardware command send
Argument: DI: DI data
return: DO data
*********************/
uint8_t SPIin(uint8_t DI)
{
    QSPI_WRITE_TX(QSPI0, DI);
    while(QSPI_GET_RX_FIFO_EMPTY_FLAG(QSPI0));
    return (QSPI_READ_RX(QSPI0) & 0xff);
}

BOOL volatile _usbd_bIsSPINANDInit = FALSE;
int spiNANDInit()
{
    unsigned int volatile u32ReturnValue;

    /* select QSPI0 function pins */
    outpw(REG_CLK_PCLKEN1, (inpw(REG_CLK_PCLKEN1) | 0x10)); /* enable QSPI0 clock */
    outpw(REG_SYS_GPD_MFPL, 0x00111100);
    PD->MODE = (PD->MODE & 0xFFFF0FFF) | 0x5000; /* Configure PD6 and PD7 as output mode */
    PD6 = 1; /* PD6: SPI0_MOSI1 or SPI flash /WP pin */
    PD7 = 1; /* PD7: SPI0_MISO1 or SPI flash /HOLD pin */

    outpw(REG_QSPI0_CLKDIV, 15);   /* Set SPI0 clock to 9.375 MHz => PCLK(150)/(n+1) */

    /* Default setting: slave selection signal is active low; disable automatic slave selection function. */
    outpw(REG_QSPI0_SSCTL, 0); /* AUTOSS=0; low-active; de-select all SS pins. */
    /* Default setting: MSB first, disable unit transfer interrupt, SP_CYCLE = 15. */
    outpw(REG_QSPI0_CTL, 0x8F5); /* Data width 8 bits; MSB first; CLKP=0; TX_NEG=1; SPIEN=1. */

    memset((char *)&SNInfo, 0, sizeof(SPINAND_INFO_T));
    pSN = &SNInfo;

    //if (_usbd_bIsSPINANDInit == FALSE)
    {
        /* Disable auto SS function, control SS signal manually. */
        QSPI_DisableAutoSS(QSPI_FLASH_PORT);
        spiNAND_Reset();
        MSG_DEBUG("QSPI->CTL = 0x%x    QSPI->SSCTL = 0x%x   QSPI->CLKDIV = 0x%x\n", QSPI_FLASH_PORT->CTL, QSPI_FLASH_PORT->SSCTL, QSPI_FLASH_PORT->CLKDIV);

        if (spiNAND_ReadINFO(pSN)< 0)
            return Fail;

        // un-protect
        u32ReturnValue = spiNAND_StatusRegister(1);
        u32ReturnValue &= 0x83;
        spiNAND_StatusRegister_Write_SR1(u32ReturnValue);

        _usbd_bIsSPINANDInit = TRUE;
    }

    return 0;
}

INT spiNAND_ReadINFO(SPINAND_INFO_T *pSN)
{
    pSN->SPINand_ID=spiNAND_ReadID();

    if(info.SPINand_uIsUserConfig == 1) {
        pSN->SPINand_ID = pSN->SPINand_ID;
        pSN->SPINand_PageSize = info.SPINand_PageSize;
        pSN->SPINand_SpareArea = info.SPINand_SpareArea ;
        pSN->SPINand_QuadReadCmd    = info.SPINand_QuadReadCmd;
        pSN->SPINand_ReadStatusCmd  = info.SPINand_ReadStatusCmd;
        pSN->SPINand_WriteStatusCmd = info.SPINand_WriteStatusCmd;
        pSN->SPINand_StatusValue    = info.SPINand_StatusValue;
        pSN->SPINand_dummybyte      = info.SPINand_dummybyte;
        pSN->SPINand_BlockPerFlash = info.SPINand_BlockPerFlash;
        pSN->SPINand_PagePerBlock = info.SPINand_PagePerBlock;
    } else {
        //printf("spiNAND_ReadINFO   ID = 0x%x\n", pSN->SPINand_ID);
        if(pSN->SPINand_ID == 0xEFAA21) { /* winbond */
            pSN->SPINand_ID = 0xEFAA21;
            pSN->SPINand_PageSize=0x800; // 2048 bytes per page
            pSN->SPINand_SpareArea=0x40; // 64 bytes per page spare area
            //CMD_READ_QUAD_IO_FAST     0xeb, dummy 3
            //CMD_READ_QUAD_OUTPUT_FAST 0x6b, dummy 1
            pSN->SPINand_QuadReadCmd = 0x6b;
            pSN->SPINand_ReadStatusCmd = 0xff;
            pSN->SPINand_WriteStatusCmd =0xff;
            pSN->SPINand_StatusValue = 0xff;
            pSN->SPINand_dummybyte = 0x1;
            pSN->SPINand_BlockPerFlash = 0x400;// 1024 blocks per 1G NAND
            pSN->SPINand_PagePerBlock = 64; // 64 pages per block

            info.SPINand_ID = 0xEFAA21;
            info.SPINand_PageSize=0x800; // 2048 bytes per page
            info.SPINand_SpareArea=0x40; // 64 bytes per page spare area
            info.SPINand_QuadReadCmd = 0x6b;
            info.SPINand_ReadStatusCmd = 0xff;
            info.SPINand_WriteStatusCmd =0xff;
            info.SPINand_StatusValue = 0xff;
            info.SPINand_dummybyte = 0x1;
            info.SPINand_BlockPerFlash = 0x400;// 1024 blocks per 1G NAND
            info.SPINand_PagePerBlock = 64; // 64 pages per block

        } else if(pSN->SPINand_ID == 0xC212) { /* mxic */
            pSN->SPINand_ID = 0xC212;
            pSN->SPINand_PageSize=0x800; // 2048 bytes per page
            pSN->SPINand_SpareArea=0x40; // 64 bytes per page spare area
            pSN->SPINand_QuadReadCmd = 0x6b;
            pSN->SPINand_ReadStatusCmd = 0x05;
            pSN->SPINand_WriteStatusCmd =0x01;
            pSN->SPINand_StatusValue = 0x40;
            pSN->SPINand_dummybyte = 0x1;
            pSN->SPINand_BlockPerFlash = 0x400;// 1024 blocks per 1G NAND
            pSN->SPINand_PagePerBlock = 64; // 64 pages per block

            info.SPINand_ID = 0xC212;
            info.SPINand_PageSize=0x800; // 2048 bytes per page
            info.SPINand_SpareArea=0x40;    // 64 bytes per page spare area
            info.SPINand_QuadReadCmd = 0x6b;
            info.SPINand_ReadStatusCmd = 0x05;
            info.SPINand_WriteStatusCmd =0x01;
            info.SPINand_StatusValue = 0x40;
            info.SPINand_dummybyte = 0x1;
            info.SPINand_BlockPerFlash = 0x400;// 1024 blocks per 1G NAND
            info.SPINand_PagePerBlock = 64; // 64 pages per block

        } else  if(pSN->SPINand_ID == 0xbe20b) { /* XTX  2G */
            pSN->SPINand_ID = 0xbe20b;
            pSN->SPINand_PageSize=0x800; // 2048 bytes per page
            pSN->SPINand_SpareArea=0x40;    // 64 bytes per page spare area
            pSN->SPINand_QuadReadCmd = 0x6b;
            pSN->SPINand_ReadStatusCmd = 0xff;
            pSN->SPINand_WriteStatusCmd =0xff;
            pSN->SPINand_StatusValue = 0xff;
            pSN->SPINand_dummybyte = 0x1;
            pSN->SPINand_BlockPerFlash = 0x800;// 2048 blocks per 2G NAND
            pSN->SPINand_PagePerBlock = 64; // 64 pages per block

            info.SPINand_ID = 0xbe20b;
            info.SPINand_PageSize=0x800; // 2048 bytes per page
            info.SPINand_SpareArea=0x40;    // 64 bytes per page spare area
            info.SPINand_QuadReadCmd = 0x6b;
            info.SPINand_ReadStatusCmd = 0xff;
            info.SPINand_WriteStatusCmd =0xff;
            info.SPINand_StatusValue = 0xff;
            info.SPINand_dummybyte = 0x1;
            info.SPINand_BlockPerFlash= 0x800;// 2048 blocks per 2G NAND
            info.SPINand_PagePerBlock = 64; // 64 pages per block

        } else  if(pSN->SPINand_ID == 0xbe10b || pSN->SPINand_ID == 0xd511d5 || pSN->SPINand_ID == 0xd51cd5) { /* XTX/MK  1G */
            //pSN->SPINand_ID = 0xbe10b;
            pSN->SPINand_PageSize=0x800; // 2048 bytes per page
            pSN->SPINand_SpareArea=0x40;    // 64 bytes per page spare area
            pSN->SPINand_QuadReadCmd = 0x6b;
            pSN->SPINand_ReadStatusCmd = 0xff;
            pSN->SPINand_WriteStatusCmd =0xff;
            pSN->SPINand_StatusValue = 0xff;
            pSN->SPINand_dummybyte = 0x1;
            pSN->SPINand_BlockPerFlash = 0x400;// 1024 blocks per 1G NAND
            pSN->SPINand_PagePerBlock = 64; // 64 pages per block

            info.SPINand_ID = pSN->SPINand_ID;//0xbe10b;
            info.SPINand_PageSize=0x800; // 2048 bytes per page
            info.SPINand_SpareArea=0x40;    // 64 bytes per page spare area
            info.SPINand_QuadReadCmd = 0x6b;
            info.SPINand_ReadStatusCmd = 0xff;
            info.SPINand_WriteStatusCmd =0xff;
            info.SPINand_StatusValue = 0xff;
            info.SPINand_dummybyte = 0x1;
            info.SPINand_BlockPerFlash= 0x400;// 1024 blocks per 1G NAND
            info.SPINand_PagePerBlock = 64; // 64 pages per block

        } else {
            printf("SPI NAND ID not support!! 0x%x\n", pSN->SPINand_ID);
            pSN->SPINand_ID = 0x0;//pSN->SPINand_ID;
        }
    }
    return 0;
}

