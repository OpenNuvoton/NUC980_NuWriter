#ifndef __FMI_H__
#define __FMI_H__
#include <stdio.h>

#include "nuc980.h"

#define SPI_HEAD_ADDR 4 //Blocks

//static volatile unsigned char Enable4ByteFlag=0;
#define FMI_SD_CARD0	0
#define FMI_SD_CARD1	1
#define FMI_SM_CARD		2
#define FMI_MS_CARD0	3
#define FMI_MS_CARD1	4

#define FMI_MS_TYPE		0
#define FMI_MSPRO_TYPE	1

#define FMI_SD_WRITE_PROTECT	-1
#define FMI_NO_SD_CARD			-2
#define FMI_NO_SM_CARD			-3
#define FMI_SD_INIT_ERROR		-4
#define FMI_SM_INIT_ERROR		-5
#define FMI_SD_CRC7_ERROR		-6
#define FMI_SD_CRC16_ERROR		-7
#define FMI_SD_CRC_ERROR		-8
#define FMI_SM_STATE_ERROR		-9
#define FMI_SM_ECC_ERROR		-10
#define FMI_ERR_DEVICE			-11
#define FMI_TIMEOUT				-12
#define FMI_SD_INIT_TIMEOUT		-13
#define FMI_SD_SELECT_ERROR		-14
#define FMI_MS_INIT_ERROR		-15
#define FMI_MS_INT_TIMEOUT		-16
#define FMI_MS_BUSY_TIMEOUT		-17
#define FMI_MS_CRC_ERROR		-18
#define FMI_MS_INT_CMDNK		-19
#define FMI_MS_INT_ERR			-20
#define FMI_MD_INT_BREQ			-21
#define FMI_MS_INT_CED_ERR		-22
#define FMI_MS_READ_PAGE_ERROR	-23
#define FMI_MS_COPY_PAGE_ERR	-24
#define FMI_MS_ALLOC_ERR		-25
#define FMI_MS_WRONG_SEGMENT	-26
#define FMI_MS_WRONG_PHYBLOCK	-27
#define FMI_MS_WRONG_TYPE		-28
#define FMI_MS_WRITE_DISABLE	-29
#define FMI_NO_MS_CARD			-30
#define FMI_SM_RB_ERR			-31
#define FMI_SM_STATUS_ERR		-32

#define FMI_SD_CMD8_ERROR		-33

//-- function return value
#define    Successful  0
#define    Failed      -1

/* IntraROM version define */
#define INTRAROM_CA        0x20070502

// extern global variables
extern UINT32 _fmi_uFMIReferenceClock;
extern UINT32 _fmi_uFirst_L2P;
extern BOOL volatile _fmi_bIsSDDataReady, _fmi_bIsSMDataReady;
extern BOOL volatile _fmi_bIsMSDataReady, _fmi_bIsMSTimeOut;

#define STOR_STRING_LEN 32

#if defined ( __CC_ARM   )
#pragma anon_unions
#endif

/* we allocate one of these for every device that we remember */
typedef struct disk_data_t {
    struct disk_data_t  *next;           /* next device */

    /* information about the device -- always good */
    unsigned int  totalSectorN;
    unsigned int  diskSize;         /* disk size in Kbytes */
    int           sectorSize;
    char          vendor[STOR_STRING_LEN];
    char          product[STOR_STRING_LEN];
    char          serial[STOR_STRING_LEN];
} DISK_DATA_T;


typedef struct dmac_desc_t {
    UINT32 uPhyAddress;
    UINT32 uSectorCount;
} DMAC_DESC_T;
extern DMAC_DESC_T DMAC_DESC[32];


typedef struct fmi_sd_info_t {
    UINT32  uCardType;      // sd2.0, sd1.1, or mmc
    UINT32  uRCA;           // relative card address
    BOOL    bIsCardInsert;
} FMI_SD_INFO_T;

extern FMI_SD_INFO_T *_pSD0, *_pSD1;

#define BCH_T15     0x00400000
#define BCH_T12     0x00200000
#define BCH_T8      0x00100000
#define BCH_T4      0x00100000//0x00080000
#define BCH_T24     0x00040000

typedef struct fmi_sm_info_t {
    UINT32  uBlockPerFlash;
    UINT32  uPagePerBlock;
    UINT32  uSectorPerBlock;
    UINT32  uPageSize;
    UINT32  uBadBlockCount;
    UINT32  uRegionProtect;     // the page number for Region Protect End Address
    UINT32  uNandECC;
    UINT32  uSpareSize;
    BOOL    bIsMulticycle;
    BOOL    bIsMLCNand;
    BOOL    bIsInResetState;
    BOOL    bIsRA224;
} FMI_SM_INFO_T;

extern FMI_SM_INFO_T *pSM;

/* F/W update information */
typedef struct fw_update_info_t {
    UINT16  imageNo;
    UINT16  imageFlag;
    UINT16  startBlock;
    UINT16  endBlock;
    UINT32  executeAddr;
    UINT32  blockCount;
    CHAR    imageName[16];
} FW_UPDATE_INFO_T;

typedef struct fw_nand_image_t {
    UINT32  actionFlag;
    UINT32  fileLength;
    UINT32  imageNo;
    CHAR    imageName[16];
    UINT32  imageType;
    UINT32  executeAddr;    // endblock
    UINT32  blockNo;        // startblock
    UINT32  dummy;
    UCHAR macaddr[8];
    UINT32 initSize;
} FW_NAND_IMAGE_T;

typedef struct fw_spinand_image_t {
    UINT32  actionFlag;
    UINT32  fileLength;
    UINT32  imageNo;
    CHAR    imageName[16];
    UINT32  imageType;
    UINT32  executeAddr;    // endblock
    UINT32  blockNo;        // startblock
    UINT32  dummy;
    UCHAR macaddr[8];
    UINT32 initSize;
} FW_SPINAND_IMAGE_T;

typedef struct fw_nor_image_t {
    UINT32  actionFlag;
    UINT32  fileLength;
    union {
        UINT32  imageNo;
        UINT32  num;
    };
    CHAR    imageName[16];
    UINT32  imageType;
    UINT32  executeAddr;
    UINT32  flashOffset;
    UINT32  endAddr;
    UCHAR   macaddr[8];
    UINT32  initSize;
} FW_NOR_IMAGE_T;

//MMC---------------------------------
typedef struct fw_mmc_image_t {
    UINT32  actionFlag;
    UINT32  fileLength;
    UINT32  imageNo;
    CHAR    imageName[16];
    UINT32  imageType;
    UINT32  executeAddr;
    UINT32  flashOffset;
    UINT32  endAddr;
    UINT32  ReserveSize;  //unit of sector
    UCHAR   macaddr[8];
    UINT32  initSize;
    UCHAR   FSType;
    UINT32  PartitionNum;
    UINT32  Partition1Size;  //unit of MB
    UINT32  Partition2Size;  //unit of MB
    UINT32  Partition3Size;  //unit of MB
    UINT32  Partition4Size;  //unit of MB	
    UINT32  PartitionS1Size; //Sector size unit 512Byte
    UINT32  PartitionS2Size; //Sector size unit 512Byte
    UINT32  PartitionS3Size; //Sector size unit 512Byte
    UINT32  PartitionS4Size; //Sector size unit 512Byte
} FW_MMC_IMAGE_T;


typedef struct _info {
    UINT32  Nand_uPagePerBlock;
    UINT32  Nand_uPageSize;
    UINT32  Nand_uSectorPerBlock;
    UINT32  Nand_uBlockPerFlash;
    UINT32  Nand_uBadBlockCount;
    UINT32  Nand_uSpareSize;
    UINT32  Nand_uIsUserConfig;

    UINT32  SPI_ID;
	  UINT32  SPI_uIsUserConfig;
    UINT8   SPI_QuadReadCmd;
    UINT8   SPI_ReadStatusCmd;
    UINT8   SPI_WriteStatusCmd;
    UINT8   SPI_StatusValue;
    UINT8   SPI_dummybyte;	

    UINT32  EMMC_uBlock;
    UINT32  EMMC_uReserved;

    UINT32  SPINand_uIsUserConfig;
    UINT32  SPINand_ID;
    UINT16  SPINand_PageSize;
    UINT16  SPINand_SpareArea;
    UINT8   SPINand_QuadReadCmd;
    UINT8   SPINand_ReadStatusCmd;
    UINT8   SPINand_WriteStatusCmd;
    UINT8   SPINand_StatusValue;
    UINT8   SPINand_dummybyte;
    UINT32  SPINand_BlockPerFlash;
    UINT32  SPINand_PagePerBlock;
} INFO_T;

//------------------------------------

//PACK--------------------------------
typedef struct _PACK_CHILD_HEAD {
    UINT32 filelen;
    UINT32 startaddr;
    UINT32 imagetype;
    UINT32 reserve[1];
} PACK_CHILD_HEAD,*PPACK_CHILD_HEAD;

typedef struct _PACK_HEAD {
    UINT32 actionFlag;
    UINT32 fileLength;
    UINT32 num;
    UINT32 reserve[1];
} PACK_HEAD,*PPACK_HEAD;
//------------------------------------

/****************************************************************************************************
 *                                                               
 * Power On Setting
 *
 ****************************************************************************************************/
/* PWRON[1:0] */
#define PWRON_BOOT_MSK		(0x00000003)
#define PWRON_BOOT_USB		(0x00000000)
#define PWRON_BOOT_SD		  (0x00000001)
#define PWRON_BOOT_NAND		(0x00000002)
#define PWRON_BOOT_SPI		(0x00000003)

/* PWRON[7:6] */
#define PWRON_NPAGE_MSK		(0x000000C0)
#define PWRON_NPAGE_2K		(0x00000000)
#define PWRON_NPAGE_4K		(0x00000040)
#define PWRON_NPAGE_8K		(0x00000080)

/* PWRON[9:8] */
#define PWRON_98_MSK		(0x00000300)
/* NAND boot for BCH */
#define PWRON_BCH_NONE		(0x00000000)
#define PWRON_BCH_T12		(0x00000100)
#define PWRON_BCH_T24		(0x00000200)
#define PWRON_BCH_T8		(0x00000300)
/* SD boot for MFP */
#define PWRON_SD_GPC		(0x00000300)
/* SPI boot for mode select */
#define PWRON_SPI_4_NAND	(0x00000000)
#define PWRON_SPI_1_NAND	(0x00000100)
#define PWRON_SPI_4_NOR		(0x00000200)
#define PWRON_SPI_1_NOR 	(0x00000300)
#define PWRON_SPI_NOR		(0x00000200)

/* PWRON[5] */
#define PWRON_DEBUG_MSK		(0x00000020)
/* PWRON[3] */
#define PWRON_WDT_MSK		(0x00000008)
/* PWRON[17] */
#define PWRON_TICMOD_MSK	(0x00020000)


#if defined ( __CC_ARM   )
#pragma no_anon_unions
#endif

// function declaration
#ifdef DMAC_SCATTER_GETTER
INT  dmacSetDescriptor(UINT32 uaddr, UINT32 ucount);
#endif

// SD functions
INT  fmiSDCommand(FMI_SD_INFO_T *pSD, UINT8 ucCmd, UINT32 uArg);
INT  fmiSDCmdAndRsp(FMI_SD_INFO_T *pSD, UINT8 ucCmd, UINT32 uArg, INT nCount);
INT  fmiSDCmdAndRsp2(FMI_SD_INFO_T *pSD, UINT8 ucCmd, UINT32 uArg, UINT32 *puR2ptr);
INT  fmiSD_Init(FMI_SD_INFO_T *pSD);
INT  fmiSelectCard(FMI_SD_INFO_T *pSD);
VOID fmiGet_SD_info(FMI_SD_INFO_T *pSD, DISK_DATA_T *_info);
INT  fmiSD_Read_in(FMI_SD_INFO_T *pSD, UINT32 uSector, UINT32 uBufcnt, UINT32 uDAddr);
INT  fmiSD_Write_in(FMI_SD_INFO_T *pSD, UINT32 uSector, UINT32 uBufcnt, UINT32 uSAddr);

// SM functions
INT fmiSMCheckRB(void);
INT fmiSM_Reset(void);
VOID fmiSM_Initial(FMI_SM_INFO_T *pSM);
INT fmiSM_ReadID(FMI_SM_INFO_T *pSM);
INT fmiSM2BufferM_large_page(UINT32 uPage, UINT32 ucColAddr);
INT fmiSM_Read_RA(UINT32 uPage, UINT32 ucColAddr);
INT fmiCheckInvalidBlock(FMI_SM_INFO_T *pSM, UINT32 BlockNo);
INT fmiSM_BlockErase(FMI_SM_INFO_T *pSM, UINT32 uBlock);
INT fmiSM_BlockEraseBad(FMI_SM_INFO_T *pSM, UINT32 uBlock);
INT fmiMarkBadBlock(FMI_SM_INFO_T *pSM, UINT32 BlockNo);
INT CheckBadBlockMark(FMI_SM_INFO_T *pSM, UINT32 block);
INT fmiSM_ChipErase(UINT32 uChipSel);
INT fmiSM_ChipEraseBad(UINT32 uChipSel);
INT fmiSM_Erase(UINT32 uChipSel, UINT32 start, UINT32 len);
INT fmiSM_EraseBad(UINT32 uChipSel, UINT32 start, UINT32 len);

INT fmiHWInit(void);
INT fmiNandInit(void);
INT fmiSM_Write_large_page(UINT32 uSector, UINT32 ucColAddr, UINT32 uSAddr);
INT fmiSM_Read_large_page(FMI_SM_INFO_T *pSM, UINT32 uPage, UINT32 uDAddr);

INT fmiSM_Write_large_page_oob(UINT32 uSector, UINT32 ucColAddr, UINT32 uSAddr,UINT32 oobsize);
INT fmiSM_Write_large_page_oob2(UINT32 uSector, UINT32 ucColAddr, UINT32 uSAddr);



// function prototype
VOID fmiInitDevice(void);
VOID fmiSetFMIReferenceClock(UINT32 uClock);

INT  fmiSD_Read(UINT32 uSector, UINT32 uBufcnt, UINT32 uDAddr);
INT  fmiSD_Write(UINT32 uSector, UINT32 uBufcnt, UINT32 uSAddr);
INT  fmiSM_Read(UINT32 uSector, UINT32 uBufcnt, UINT32 uDAddr);
INT  fmiSM_Write(UINT32 uSector, UINT32 uBufcnt, UINT32 uSAddr);
INT  fmiSM_ChipErase(UINT32 uChipSel);
INT  fmiMS_Read(UINT32 uChipSel, UINT32 uSector, UINT32 uBufcnt, UINT32 uDAddr);
INT  fmiMS_Write(UINT32 uChipSel, UINT32 uSector, UINT32 uBufcnt, UINT32 uSAddr);

// for file system
INT  fmiInitSDDevice(void);

// callback function
VOID fmiSetCallBack(UINT32 uCard, PVOID pvRemove, PVOID pvInsert);

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#endif /* END __FMI_H__ */
