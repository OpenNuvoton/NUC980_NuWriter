#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nuc980.h"
#include "sys.h"
#include "fmi.h"
#include "qspi.h"
#include "etimer.h"
#include "gpio.h"

typedef unsigned          char uint8_t;
typedef unsigned short     int uint16_t;
typedef unsigned           int uint32_t;
typedef unsigned       __int64 uint64_t;


/* SPI NAND information in header */
typedef struct spinand_info
{
    UINT32   SPINand_ID;
    UINT16   SPINand_PageSize;
    UINT16   SPINand_SpareArea;
    UINT8    SPINand_QuadReadCmd;
    UINT8    SPINand_ReadStatusCmd;
    UINT8    SPINand_WriteStatusCmd;
    UINT8    SPINand_StatusValue;
    UINT8    SPINand_dummybyte;
    UINT32   SPINand_BlockPerFlash;
    UINT32   SPINand_PagePerBlock;
    //UINT32   SPINand_BadBlockNum;
    //UINT32   SPINand_RecBadBlock[16];
} SPINAND_INFO_T;

/* program function */
uint8_t Program_verify(uint8_t* buff1, uint8_t* buff2, uint32_t count);
void spiNAND_Pageprogram_Pattern(uint8_t addh, uint8_t addl, uint8_t* program_buffer, uint32_t count);
void spiNAND_Program_Excute(uint8_t addh, uint8_t addl);

/* status check */
uint8_t spiNAND_Check_Embedded_ECC(void);
uint8_t spiNAND_Check_Embedded_ECC_Flag(void);
uint8_t spiNAND_Check_Program_Erase_Fail_Flag(void);
uint8_t spiNAND_StatusRegister(uint8_t sr_sel);
int8_t spiNAND_ReadyBusy_Check(void);
uint32_t spiNAND_Read_JEDEC_ID(void);
uint8_t spiNAND_bad_block_check(uint32_t page_address);
void spiNAND_LUT_Read(uint16_t* LBA, uint16_t* PBA);

/* Stack function for W25M series */
void spiNAND_Die_Select(uint8_t select_die);

/* status set */
void spiNAND_Enable_Embedded_ECC(void);
void spiNAND_Disable_Embedded_ECC(void);
void spiNAND_Enable_Buffer_mode(void);
void spiNAND_Disable_Buffer_mode(void);
void spiNAND_StatusRegister_Write_SR1(uint8_t SR1);
void spiNAND_StatusRegister_Write_SR2(uint8_t SR2);
void spiNAND_StatusRegister_Write_SR3(uint8_t SR3);
void spiNAND_Reset(void);
void spiNAND_Protect(void);
void spiNAND_Unprotect(void);
void spiNAND_LUT_Set(uint16_t LBA, uint16_t PBA);

/* erase function */
void spiNAND_BlockErase(uint8_t PA_H, uint8_t PA_L);

/* read function */
void spiNAND_PageDataRead(uint8_t PA_H, uint8_t PA_L);
void spiNAND_Normal_Read(uint8_t addh, uint8_t addl, uint8_t* buff, uint32_t count);
void spiNAND_Continuous_Normal_Read(uint8_t* buff, uint32_t count);
void spiNAND_QuadIO_Read(uint8_t addh, uint8_t addl, uint8_t* buff, uint32_t count);
void spiNAND_QuadOutput_Read(uint8_t addh, uint8_t addl, uint8_t* buff, uint32_t count);

/* Hardware Control */
void spiNAND_CS_LOW(void);
void spiNAND_CS_HIGH(void);
uint8_t SPIin(uint8_t DI);

int spiNANDInit(void);
INT spiNAND_ReadINFO(SPINAND_INFO_T *pSN);
void spiNANDMarkBadBlock(uint32_t page_address);
