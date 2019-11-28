#include "stdio.h"
#include "stdlib.h"
#include "nuc980.h"
#include "sys.h"
#include "etimer.h"
#include "config.h"
#include "usbd.h"
#include "fmi.h"
#include "sdglue.h"
#include "sd.h"
#include "qspi.h"
#include "filesystem.h"
#include "spinandflash.h"

#define BATCH_BRUN

#define TRANSFER_LEN 4096

#define SPI_BLOCK_SIZE (64*1024)
#define SPI_FLASH_SIZE (16*1024*1024)  //4Byte Address Mode

#define IBR_HEADER_LEN 32

UINT32 g_uIsUserConfig; //nand

UCHAR NandBlockSize=0;
unsigned int eMMCBlockSize=0;

unsigned char *pImageList;
unsigned char imageList[512];

FW_NOR_IMAGE_T spiImage;
FW_NOR_IMAGE_T *pSpiImage;

SPINAND_INFO_T SNInfo, *pSN;

//-------------------------
extern int usiInit(void);
extern int usiEraseAll(void);
extern int usiWrite(UINT32 addr, UINT32 len, UINT8 *buf);
extern int usiRead(UINT32 addr, UINT32 len, UINT8 *buf);
extern int usiEraseSector(UINT32 addr, UINT32 secCount);
extern int spiNorReset(void);

extern int ChangeSpiImageType(UINT32 imageNo, UINT32 imageType);
extern int DelSpiImage(UINT32 imageNo);
extern int DelSpiSector(UINT32 start, UINT32 len);

extern volatile unsigned char Enable4ByteFlag;
extern INFO_T info;

//-------------------------
#define PACK_Mode(val)      (((val)&0xF   )>> 0)
#define PACK_Option(val)    (((val)&0xF0  )>> 4)
#define PACK_Enable(val)    (((val)&0xF000)>>12)

#define WRITER_MODE   0
#define MODIFY_MODE   1
#define ERASE_MODE    2
#define VERIFY_MODE   3
#define READ_MODE     4
#define PACK_MODE     5
#define FORMAT_MODE   6
#define PACK_VERIFY_MODE   7

#define DATA           0
#define ENV            1
#define UBOOT          2
#define PARTITION      3
#define PACK           3
#define IMAGE          4
#define DATA_OOB       5

#define YAFFS2         41
#define UBIFS          42
#define PMTP           15

#define WDT_RSTCNT    outpw(REG_WDT_RSTCNT, 0x5aa5)

const char *au8ActionFlagName[8]= { "WRITE", "MODIFY", "ERASE", "VERIFY", "READ", "PACK", "FORMAT", "PACK_VERIFY"};
//const char *au8ImageTypeName[6]={ "DATA ", "ENV ", "UBOOT ", "PARTITION/PACK ", "IMAGE ", "DATA_OOB "};

void usleep(int count)
{
    int volatile i=0;

    for(i=0; i<count; i++);
}

UINT32 fmiGetSPIImageInfo(unsigned int *image)
{
    UINT32 volatile bmark, emark;
    int volatile i, imageCount=0;
    unsigned char volatile *pbuf;
    unsigned int volatile *ptr;
    UCHAR _fmi_ucBuffer[1024];
    pbuf = (UINT8 *)((UINT32)_fmi_ucBuffer | 0x80000000);
    ptr = (unsigned int *)((UINT32)_fmi_ucBuffer | 0x80000000);
    usiRead(((SPI_HEAD_ADDR-1)*64+63)*1024, 1024, (UINT8 *)pbuf);  /* offset, len, address */
    bmark = *(ptr+0);
    emark = *(ptr+3);

    if ((bmark == 0xAA554257) && (emark == 0x63594257))
    {
        imageCount = *(ptr+1);

        /* pointer to image information */
        ptr = ptr+4;
        for (i=0; i<imageCount; i++)
        {
            /* fill into the image list buffer */
            *image = 0;     // action flag, dummy
            *(image+1) = 0; // file len, dummy
            *(image+2) = *(ptr) & 0xffff;
            memcpy((char *)(image+3), (char *)(ptr+4), 16);
            *(image+7) = (*(ptr) >> 16) & 0xffff;
            *(image+8) = (*(ptr+1)) + (*(ptr+3));
            *(image+9) = *(ptr+1);
            *(image+10) = *(ptr+3);

            //MSG_DEBUG("\nNo[%d], Flag[%d], name[%s]\n\n",
            //  *(image+2), *(image+7), (char *)(image+3));
            /* pointer to next image */
            image += 11;
            ptr = ptr+8;
        }
    }
    else
        imageCount = 0;

    return imageCount;
}

void addMagicHeader(unsigned int address, unsigned int length)
{
    int volatile i;
    unsigned int *ptr;
    unsigned char *magic;
    unsigned char MagicWord[4] = {0x20, 'T', 'V', 'N'}; /* 1st word - Magic word */
    ptr =((unsigned int*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    magic =((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));

    for (i=0; i<sizeof(MagicWord); i++)
        *magic++ = MagicWord[i];

    *(ptr+1) = address;          /* 2nd word - Target Address */
    *(ptr+2) = length;           /* 3rd word - Total length   */
    *(ptr+3) = 0xFFFFFFFF;
}

void addNUC980MagicHeader(unsigned char u8IsSPINOR)
{
    int volatile i;
    unsigned int volatile *u32ptr;
    unsigned char volatile *u8header;

    u32ptr =((unsigned int*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    u8header =((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));

    if(u8IsSPINOR == 1)   // SPI NOR
    {
        // IBR_Header for SPI NOR Flash
        u8header[16] = 0x00;
        u8header[17] = 0x08;
        u8header[18] = 0x40;
        u8header[19] = 0x00;
        u8header[20] = info.SPI_QuadReadCmd;
        u8header[21] = info.SPI_ReadStatusCmd; // Read Status Register
        u8header[22] = info.SPI_WriteStatusCmd; // Write Status Register
        u8header[23] = info.SPI_StatusValue; // QE(Status Register[6]) Bit 6
        u8header[24] = info.SPI_dummybyte;
        u8header[25] = 0xff;
        u8header[26] = 0xff;
        u8header[27] = 0xff;
    }
    else
    {
        // IBR_Header for SPI NAND Flash
        u8header[16] = 0x00;//info.SPINand_PageSize;
        u8header[17] = 0x08;//info.SPINand_PageSize;
        u8header[18] = 0x40;//info.SPINand_SpareArea;
        u8header[19] = 0x00;//info.SPINand_SpareArea;
        u8header[20] = info.SPINand_QuadReadCmd;
        u8header[21] = info.SPINand_ReadStatusCmd;
        u8header[22] = info.SPINand_WriteStatusCmd;
        u8header[23] = info.SPINand_StatusValue;
        u8header[24] = info.SPINand_dummybyte;
        u8header[25] = 0xff;//0x2
        u8header[26]= 0xff;
        u8header[27]= 0xff;
    }
    *(u32ptr+7) = 0xFFFFFFFF;
}

int Burn_SPI(UINT32 len,UINT32 imageoffset)
{
    int volatile tmplen=0;
    int i, offset=0, blockCount;
    uint32_t spiSourceAddr;

    MSG_DEBUG("Burn_SPI: Enable4ByteFlag=%d len=%d(0x%x)     imageoffset= %d(0x%x)\n", Enable4ByteFlag, len, len, imageoffset, imageoffset);
    /* set up interface */
    if (usiInit() == Fail)
    {
        SendAck(0xFFFFFFFF);
        return Fail;
    }

    spiSourceAddr = (DOWNLOAD_BASE | NON_CACHE);
    blockCount = len / SPI_BLOCK_SIZE;
    offset = imageoffset;
    MSG_DEBUG("Burn_SPI: Enable4ByteFlag=%d offset=0x%x  len=%d   blockCount= %d  spiSourceAddr = 0x%x\n", Enable4ByteFlag, offset, len, blockCount, spiSourceAddr);

    for (i=0; i<blockCount; i++)
    {
        MSG_DEBUG("Burn_SPI  offset=0x%x(%d)\n", offset, offset);
        Enable4ByteFlag = 0;
        // 4Byte Address Mode (>16MByte)
        if((offset + SPI_BLOCK_SIZE) > SPI_FLASH_SIZE)
            Enable4ByteFlag = 1;
        if(len > SPI_FLASH_SIZE)
            Enable4ByteFlag = 1;

        if(Enable4ByteFlag)
            MSG_DEBUG("Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

        usiEraseSector(offset, 1);
        usiWrite(offset, SPI_BLOCK_SIZE, (UINT8 *)spiSourceAddr);

        spiSourceAddr += SPI_BLOCK_SIZE;
        offset += SPI_BLOCK_SIZE;

        //ack status
        tmplen += SPI_BLOCK_SIZE;
        SendAck((tmplen * 95) / len);
    }
    if ((len % (SPI_BLOCK_SIZE)) != 0)
    {
        MSG_DEBUG("remin Burn_SPI  offset=0x%x(%d)\n", offset, offset);
        // 4Byte Address Mode (>16MByte)
        Enable4ByteFlag = 0;
        if((offset + SPI_BLOCK_SIZE) > SPI_FLASH_SIZE)
            Enable4ByteFlag = 1;
        if(offset > SPI_FLASH_SIZE)
            Enable4ByteFlag = 1;

        if(Enable4ByteFlag)
            MSG_DEBUG("Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

        len = len - blockCount*SPI_BLOCK_SIZE;
        usiEraseSector(offset, 1);
        usiWrite(offset, len, (UINT8 *)spiSourceAddr);
    }
    //ack status
    SendAck(100);

    return 0;
}

int BatchBurn_SPI(UINT32 len,UINT32 imageoffset)
{
    int volatile tmplen=0;
    int i, offset=0, blockCount, total, remainlen;
    uint32_t spiSourceAddr, reclen;
    unsigned char buf[80];
    unsigned char *ptr;
    unsigned char *_ch;
    unsigned int *_ack;

    MSG_DEBUG("BatchBurn_SPI: Enable4ByteFlag=%d len=%d(0x%x)     imageoffset= %d(0x%x)\n", Enable4ByteFlag, len, len, imageoffset, imageoffset);
    /* set up interface */
    if (usiInit() == Fail)
    {
        SendAck(0xFFFFFFFF);
        return Fail;
    }

    _ack=((unsigned int*)(((unsigned int)buf)|NON_CACHE));
    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    ptr=_ch;
    spiSourceAddr = (DOWNLOAD_BASE | NON_CACHE);
    //len = (pSpiImage->fileLength+offset+((FW_NOR_IMAGE_T *)pSpiImage)->initSize);
    blockCount = len / SPI_BLOCK_SIZE;
    total = len;
    offset = imageoffset;
    MSG_DEBUG("fileLength=%d, len=%d, blockCount=%d\n", pSpiImage->fileLength, len, blockCount);
    for (i=0; i<blockCount; i++)
    {
        ptr=_ch;
        spiSourceAddr = (DOWNLOAD_BASE | NON_CACHE);
        remainlen=MIN(total, SPI_BLOCK_SIZE);
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                MSG_DEBUG("total=%08d,remainlen=%08d\n",total,remainlen);
                reclen=MIN(TRANSFER_LEN,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                total-=reclen;
                *_ack=reclen;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);

        MSG_DEBUG("Burn_SPI  offset=0x%x(%d)\n", offset, offset);
        Enable4ByteFlag = 0;
        // 4Byte Address Mode (>16MByte)
        if((offset + SPI_BLOCK_SIZE) > SPI_FLASH_SIZE)
            Enable4ByteFlag = 1;
        if(len > SPI_FLASH_SIZE)
            Enable4ByteFlag = 1;

        if(Enable4ByteFlag)
            MSG_DEBUG("Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

        usiEraseSector(offset, 1);
        usiWrite(offset, SPI_BLOCK_SIZE, (UINT8 *)spiSourceAddr);

        spiSourceAddr += SPI_BLOCK_SIZE;
        offset += SPI_BLOCK_SIZE;

        //ack status
        SendAck(i);//send ack(blockcount) to PC
    }

    tmplen = len % (SPI_BLOCK_SIZE);
    if (tmplen != 0)
    {
        MSG_DEBUG("tmplen=0x%x(%d)\n", tmplen, tmplen);
        ptr=_ch;
        spiSourceAddr = (DOWNLOAD_BASE | NON_CACHE);
        remainlen=MIN(tmplen, SPI_BLOCK_SIZE);
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                reclen=MIN(TRANSFER_LEN,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                total-=reclen;
                *_ack=reclen;
                usb_send((unsigned char*)_ack,4);//send ack to PC
                MSG_DEBUG("total=%08d,remainlen=%08d\n",total,remainlen);
            }
        }
        while(remainlen!=0);

        // 4Byte Address Mode (>16MByte)
        Enable4ByteFlag = 0;
        if((offset + SPI_BLOCK_SIZE) > SPI_FLASH_SIZE)
            Enable4ByteFlag = 1;
        if(offset > SPI_FLASH_SIZE)
            Enable4ByteFlag = 1;

        if(Enable4ByteFlag)
            printf("Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

        len = tmplen;
        usiEraseSector(offset, 1);
        usiWrite(offset, len, (UINT8 *)spiSourceAddr);

        //ack status
        SendAck(100); //done
    }


    return 0;
}

void UXmodem_SPI(void)
{
    int len,i,j;
    unsigned char *ptr;
    unsigned char buf[80];
    unsigned char *_ch;
    unsigned int *_ack;
    unsigned int offset=0, tempoffset = 0;
    unsigned int remain, ddrlen;
    PACK_CHILD_HEAD ppack;
    unsigned int ret;
    int volatile tmplen=0;
    int blockCount,remainlen, reclen, total;
    uint32_t spiSourceAddr;

    MSG_DEBUG("download image to SPI flash...\n");
    memset((char *)&spiImage, 0, sizeof(FW_NOR_IMAGE_T));
    pSpiImage = (FW_NOR_IMAGE_T *)&spiImage;
    usiInit();  //Init SPI
    MSG_DEBUG("UXmodem_SPI\n");
    _ch=((unsigned char*)(((unsigned int)buf)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)buf)|NON_CACHE));
    ptr=_ch;

    while(1)
    {
        if(Bulk_Out_Transfer_Size>0)
        {
            usb_recv(ptr,sizeof(FW_NOR_IMAGE_T));
            memcpy(pSpiImage,(unsigned char *)ptr,sizeof(FW_NOR_IMAGE_T));
            break;
        }
    }

    MSG_DEBUG("Action flag: %s, image %d, len=%d exec=0x%08x type=%d",
              au8ActionFlagName[pSpiImage->actionFlag], pSpiImage->imageNo,pSpiImage->fileLength,pSpiImage->executeAddr,pSpiImage->imageType);
    usiInit();
    spiNorReset();
    MSG_DEBUG("  Enable4ByteFlag=%d\n",Enable4ByteFlag);
    switch(pSpiImage->actionFlag)
    {
    case WRITER_MODE:   // normal write
    {
        MSG_DEBUG("SPI normal write !!!\n");

        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x85;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;

        //image size = DDR size - 1M - 64k
        if (pSpiImage->imageType == UBOOT) // uboot image
        {
            pSpiImage->imageNo = 0;
            pSpiImage->flashOffset = 0;
            addMagicHeader(pSpiImage->executeAddr, pSpiImage->fileLength);
            addNUC980MagicHeader(1);
            ptr += IBR_HEADER_LEN; // except the 32 bytes magic header
            offset = IBR_HEADER_LEN;

            do
            {
                if(Bulk_Out_Transfer_Size>0)
                {

                    len=Bulk_Out_Transfer_Size;
                    usb_recv(ptr,len);  //recv data from PC
                    ptr+=len;
                    *_ack=len;
                    usb_send((unsigned char*)_ack,4);//send ack to PC
                }
            }
            while((ptr-_ch)<(pSpiImage->fileLength+offset+((FW_NOR_IMAGE_T *)pSpiImage)->initSize));
            Burn_SPI(pSpiImage->fileLength+(((FW_NOR_IMAGE_T *)pSpiImage)->initSize)+offset,pSpiImage->flashOffset);
            MSG_DEBUG("uBOOT  len=%d, fileLength=%d, IBR_HEADER_LEN=%d, ddrlen=%d\n", pSpiImage->fileLength+(((FW_NOR_IMAGE_T *)pSpiImage)->initSize)+offset, pSpiImage->fileLength, offset, (((FW_NOR_IMAGE_T *)pSpiImage)->initSize));
        }
        else // BATCH_BRUN
        {
            offset=0;
            MSG_DEBUG("len=%d, fileLength=%d, IBR_HEADER_LEN=%d, ddrlen=%d\n", pSpiImage->fileLength+(((FW_NOR_IMAGE_T *)pSpiImage)->initSize)+offset, pSpiImage->fileLength, offset, (((FW_NOR_IMAGE_T *)pSpiImage)->initSize));
            BatchBurn_SPI(pSpiImage->fileLength+(((FW_NOR_IMAGE_T *)pSpiImage)->initSize)+offset,pSpiImage->flashOffset);
        }
    }
    break;

    case MODIFY_MODE:   // modify
    {
        MSG_DEBUG("SPI modify !!!\n");
        {
            int state;
            state = ChangeSpiImageType(pSpiImage->imageNo, pSpiImage->imageType);
            if (state < 0)
            {
                //MSG_DEBUG("error!!\n");
                SendAck(0xFFFFFFFF);
            }
            else
            {
                //MSG_DEBUG("OK!!\n");
                SendAck(100);
            }
        }
    }
    break;

    case ERASE_MODE:   // erase
    {
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x85;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        MSG_DEBUG("SPI erase !!! imageNo=%d\n",pSpiImage->imageNo);
        MSG_DEBUG("\nstart=%d\n",pSpiImage->flashOffset); //start block
        MSG_DEBUG("length=%d\n",pSpiImage->executeAddr);  //block length
        MSG_DEBUG("type=%d\n",pSpiImage->imageType);      //0: chip erase, 1: erase accord start and length blocks.
        MSG_DEBUG("type=0x%08x\n",pSpiImage->imageNo);

        if (pSpiImage->imageNo != 0xFFFFFFFF)
            DelSpiSector(pSpiImage->flashOffset,pSpiImage->executeAddr);
        else
        {
            // erase all
            ret = usiEraseAll();
            if(ret == 0)
            {
                MSG_DEBUG("erase all done \n");
                //SendAck(100);
            }
            else
            {
                MSG_DEBUG("Error erase all fail \n");
                //SendAck(0xffffff);
            }
        }
    }
    break;

    case VERIFY_MODE: // verify
    {
        MSG_DEBUG("SPI normal verify !!!\n");

        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x85;
            usb_send((unsigned char*)_ack,4); //send ack to PC
        }

        if (pSpiImage->imageType == UBOOT) // uboot image, image size = DDR size - 1M - 64k
        {
            _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
            ptr=_ch;
            MSG_DEBUG("UBOOT verify offset=%d,flashOffset=%d,fileLength=%d\n",offset,pSpiImage->flashOffset,pSpiImage->fileLength);
            memset(ptr, 0, pSpiImage->fileLength);
            offset=32;
            len =  pSpiImage->fileLength;
            do
            {
                tempoffset = 0;
                remainlen=MIN(len, TRANSFER_LEN);
                //4Byte Address Mode (>16MByte)
                Enable4ByteFlag = 0;
                if((pSpiImage->flashOffset + offset + remainlen) > SPI_FLASH_SIZE)
                    Enable4ByteFlag = 1;

                if(Enable4ByteFlag)
                    MSG_DEBUG("VERIFY_MODE  Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

                usiRead(pSpiImage->flashOffset+offset+((FW_NOR_IMAGE_T *)pSpiImage)->initSize, remainlen, ptr);//(UINT8 *)(DOWNLOAD_BASE));

                usb_send(ptr,remainlen); //send data to PC
                while(Bulk_Out_Transfer_Size==0) {}
                usb_recv((unsigned char*)_ack,4);   //recv data from PC

                if(*_ack==0)
                    break;
                else
                {
                    offset+=(*_ack);
                    len -= remainlen;
                }
                MSG_DEBUG("SPI VERIFY_MODE: pSpiImage->fileLength=%d  offset=0x%08x(%d)   len= 0x%08x(%d)\n", pSpiImage->fileLength, offset, offset, len, len);
            }while(len>0);
        }
        else // Batch verify
        {
            offset=0;
            _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
            blockCount = pSpiImage->fileLength / SPI_BLOCK_SIZE;
            for(i=0; i<blockCount; i++)
            {
                ptr=_ch;
                memset(ptr, 0, SPI_BLOCK_SIZE);
                tempoffset = SPI_BLOCK_SIZE;
                do
                {
                    //4Byte Address Mode (>16MByte)
                    Enable4ByteFlag = 0;
                    if((pSpiImage->flashOffset + offset + TRANSFER_LEN) > SPI_FLASH_SIZE)
                        Enable4ByteFlag = 1;

                    if(Enable4ByteFlag)
                        MSG_DEBUG("VERIFY_MODE  Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

                    if (pSpiImage->flashOffset == 0)
                        usiRead(pSpiImage->flashOffset+offset+((FW_NOR_IMAGE_T *)pSpiImage)->initSize, TRANSFER_LEN, ptr);
                    else
                        usiRead(pSpiImage->flashOffset+offset, TRANSFER_LEN, ptr);

                    usb_send(ptr,TRANSFER_LEN); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4);   //recv data from PC

                    if(*_ack==0)
                        break;
                    else
                    {
                        offset+=(*_ack);
                        tempoffset-=(*_ack);
                    }
                }while(tempoffset>0);
                MSG_DEBUG("SPI VERIFY_MODE: i=%d, pSpiImage->fileLength=%d  offset=0x%08x(%d)   tempoffset= 0x%08x(%d)\n", i, pSpiImage->fileLength, offset, offset, tempoffset, tempoffset);
            }

            tmplen = pSpiImage->fileLength % (SPI_BLOCK_SIZE);
            MSG_DEBUG("tmplen: pSpiImage->fileLength=%d  offset=0x%08x(%d)   tmplen= %d\n", pSpiImage->fileLength, offset, offset, tmplen);
            if (tmplen != 0)
            {
                ptr=_ch;
                memset(ptr, 0, SPI_BLOCK_SIZE);
                tempoffset = tmplen;
                do
                {
                    reclen=MIN(TRANSFER_LEN,tmplen);

                    //4Byte Address Mode (>16MByte)
                    Enable4ByteFlag = 0;
                    if((pSpiImage->flashOffset + offset + reclen) > SPI_FLASH_SIZE)
                        Enable4ByteFlag = 1;

                    if(Enable4ByteFlag)
                        MSG_DEBUG("VERIFY_MODE  Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

                    if (pSpiImage->flashOffset == 0)
                        usiRead(pSpiImage->flashOffset+offset+((FW_NOR_IMAGE_T *)pSpiImage)->initSize, reclen, ptr);
                    else
                        usiRead(pSpiImage->flashOffset+offset, reclen, ptr);

                    usb_send(ptr,reclen); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4);   //recv data from PC

                    if(*_ack==0)
                        break;
                    else
                    {
                        tmplen-=(*_ack);
                        offset+=(*_ack);
                        tempoffset -=(*_ack);
                    }

                }while(tempoffset>0);

            }

        }
    }
    break;
    case PACK_VERIFY_MODE: // Mass production verify
    {
        MSG_DEBUG("\n SPI PACK verify !!!\n");

        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x85;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;
        MSG_DEBUG("pSpiImage->imageNo=%d,flashOffset=%d,fileLength=%d\n",pSpiImage->imageNo,pSpiImage->flashOffset,pSpiImage->fileLength);

#if(0) //image size = DDR size - 1M - 64k
        memset(ptr, 0, pSpiImage->fileLength);
#else // PACK Batch Verify
        memset(ptr, 0, SPI_BLOCK_SIZE);
#endif

        offset=0;
        for(i=0; i<pSpiImage->imageNo; i++)
        {
            while(1)
            {
                if(Bulk_Out_Transfer_Size>=sizeof(PACK_CHILD_HEAD))
                {
                    usb_recv(ptr,sizeof(PACK_CHILD_HEAD));
                    memcpy(&ppack,(unsigned char *)ptr,sizeof(PACK_CHILD_HEAD));
                    usleep(1000);
                    *_ack=0x85;
                    usb_send((unsigned char*)_ack,4);//send ack to PC
                    break;
                }
            }

#if(0) //image size = DDR size - 1M - 64k
            pSpiImage->fileLength = ppack.filelen;
            pSpiImage->flashOffset = ppack.startaddr;
            offset = 0;
            MSG_DEBUG("imageNum = %d, offset=0x%x, flashOffset=0x%x, fileLength=0x%x\n",i, offset,pSpiImage->flashOffset,pSpiImage->fileLength);
            // Get DDR parameter length
            if (ppack.imagetype == UBOOT)  // system image
            {
                usb_recv(ptr,4);
                memcpy(&ddrlen,(unsigned char *)ptr,4);
                MSG_DEBUG("ddrlen = %d\n", ddrlen);
                //usleep(1000);
                *_ack=0x85;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }

            //MSG_DEBUG("=======> imageNo=%d, pSpiImage->flashOffset =0x%x [%d]\n",i, pSpiImage->flashOffset, pSpiImage->flashOffset);
            do
            {
                //4Byte Address Mode (>16MByte)
                Enable4ByteFlag = 0;
                if(pSpiImage->fileLength > SPI_FLASH_SIZE)
                    Enable4ByteFlag = 1;
                if((pSpiImage->flashOffset + offset + TRANSFER_LEN) > SPI_FLASH_SIZE)
                    Enable4ByteFlag = 1;

                if(Enable4ByteFlag)
                    MSG_DEBUG("PACK_VERIFY_MODE  Enable4ByteFlag %d:  pSpiImage->flashOffset=0x%08x(%d)  offset=0x%08x(%d)\n", Enable4ByteFlag, pSpiImage->flashOffset, pSpiImage->flashOffset, offset, offset);

                if (offset == 0)
                {
                    if (ppack.imagetype == UBOOT)     // system image
                    {
                        pSpiImage->fileLength = pSpiImage->fileLength - IBR_HEADER_LEN - ddrlen;
                        usiRead(pSpiImage->flashOffset+IBR_HEADER_LEN+ddrlen+offset+((FW_NOR_IMAGE_T *)pSpiImage)->initSize, TRANSFER_LEN, (UINT8 *)(DOWNLOAD_BASE));
                    }
                    else
                    {
                        usiRead(pSpiImage->flashOffset+offset, TRANSFER_LEN, (UINT8 *)(DOWNLOAD_BASE));
                    }
                }
                else
                {
                    if (ppack.imagetype == UBOOT)     // system image
                    {
                        usiRead(pSpiImage->flashOffset+IBR_HEADER_LEN+ddrlen+offset+((FW_NOR_IMAGE_T *)pSpiImage)->initSize, TRANSFER_LEN, (UINT8 *)(DOWNLOAD_BASE));
                    }
                    else
                    {
                        usiRead(pSpiImage->flashOffset+offset, TRANSFER_LEN, (UINT8 *)(DOWNLOAD_BASE));
                    }
                }

                usb_send(ptr,TRANSFER_LEN); //send data to PC
                while(Bulk_Out_Transfer_Size==0) {}
                usb_recv((unsigned char*)_ack,4);   //recv data from PC
                if(*_ack==0)
                    break;
                else
                    offset+=(*_ack);
            }while(offset<pSpiImage->fileLength);

#else // PACK Batch Verify

            pSpiImage->fileLength = ppack.filelen;
            pSpiImage->flashOffset = ppack.startaddr;
            _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
            blockCount = pSpiImage->fileLength / (SPI_BLOCK_SIZE);
            offset = 0;
            MSG_DEBUG("imageNum = %d, blockCount=0x%x, flashOffset=0x%x, fileLength=0x%x\n",i, blockCount,pSpiImage->flashOffset,pSpiImage->fileLength);
            // Get DDR parameter length
            if (ppack.imagetype == UBOOT)     // system image
            {
                usb_recv(ptr,4);
                memcpy(&ddrlen,(unsigned char *)ptr,4);
                MSG_DEBUG("ddrlen = %d\n", ddrlen);
                //usleep(1000);
                *_ack=0x85;
                usb_send((unsigned char*)_ack,4);//send ack to PC
                pSpiImage->fileLength = pSpiImage->fileLength - IBR_HEADER_LEN - ddrlen;
            }

            for(j=0; j<blockCount; j++)
            {
                ptr=_ch;
                memset(ptr, 0, SPI_BLOCK_SIZE);
                tempoffset = SPI_BLOCK_SIZE;
                do
                {
                    //4Byte Address Mode (>16MByte)
                    Enable4ByteFlag = 0;
                    if(pSpiImage->fileLength > SPI_FLASH_SIZE)
                        Enable4ByteFlag = 1;
                    if((pSpiImage->flashOffset + offset + TRANSFER_LEN) > SPI_FLASH_SIZE)
                        Enable4ByteFlag = 1;

                    if(Enable4ByteFlag)
                        MSG_DEBUG("PACK_VERIFY_MODE  Enable4ByteFlag %d:  pSpiImage->flashOffset=0x%08x(%d)  offset=0x%08x(%d)\n", Enable4ByteFlag, pSpiImage->flashOffset, pSpiImage->flashOffset, offset, offset);

                    if (offset == 0)
                    {
                        if (ppack.imagetype == UBOOT)     // system image
                        {
                            usiRead(pSpiImage->flashOffset+IBR_HEADER_LEN+ddrlen+offset+((FW_NOR_IMAGE_T *)pSpiImage)->initSize, TRANSFER_LEN, (UINT8 *)(DOWNLOAD_BASE));
                        }
                        else
                        {
                            usiRead(pSpiImage->flashOffset+offset, TRANSFER_LEN, ptr);
                        }
                    }
                    else
                    {
                        if (ppack.imagetype == UBOOT)     // system image
                        {
                            usiRead(pSpiImage->flashOffset+IBR_HEADER_LEN+ddrlen+offset+((FW_NOR_IMAGE_T *)pSpiImage)->initSize, TRANSFER_LEN, (UINT8 *)(DOWNLOAD_BASE));
                        }
                        else
                        {
                            usiRead(pSpiImage->flashOffset+offset, TRANSFER_LEN, ptr);
                        }
                    }
                    usb_send(ptr,TRANSFER_LEN); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4);   //recv data from PC

                    if(*_ack==0)
                        break;
                    else
                    {
                        offset+=(*_ack);
                        tempoffset-=(*_ack);
                    }
                }while(tempoffset>0);
                MSG_DEBUG("SPI PACK_VERIFY_MODE: i=%d, pSpiImage->fileLength=%d  offset=0x%08x(%d)   tempoffset= 0x%08x(%d)\n", i, pSpiImage->fileLength, offset, offset, tempoffset, tempoffset);
            }

            tmplen = pSpiImage->fileLength % (SPI_BLOCK_SIZE);
            MSG_DEBUG("tmplen: pSpiImage->fileLength=0x%08x(%d)  offset=0x%08x(%d)   tmplen= %d\n", pSpiImage->fileLength, pSpiImage->fileLength, offset, offset, tmplen);
            if (tmplen > 0)
            {
                ptr=_ch;
                tempoffset = tmplen;
                do
                {
                    reclen=MIN(TRANSFER_LEN,tmplen);

                    //4Byte Address Mode (>16MByte)
                    Enable4ByteFlag = 0;
                    if((pSpiImage->flashOffset + offset + reclen) > SPI_FLASH_SIZE)
                        Enable4ByteFlag = 1;

                    if(Enable4ByteFlag)
                        MSG_DEBUG("PACK_VERIFY_MODE  Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);

                    if (ppack.imagetype == UBOOT)     // system image
                    {
                        pSpiImage->fileLength = pSpiImage->fileLength - IBR_HEADER_LEN - ddrlen;
                        usiRead(pSpiImage->flashOffset+IBR_HEADER_LEN+ddrlen+offset+((FW_NOR_IMAGE_T *)pSpiImage)->initSize, reclen, ptr);
                    }
                    else
                    {
                        usiRead(pSpiImage->flashOffset+offset, reclen, ptr);
                    }
                    usb_send(ptr,reclen); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4);   //recv data from PC

                    if(*_ack==0)
                        break;
                    else
                    {
                        tmplen-=(*_ack);
                        offset+=(*_ack);
                        tempoffset -=(*_ack);
                    }
                }while(tempoffset>0);
            }
#endif
        }
    }
    break;
    case READ_MODE: // read
    {
        MSG_DEBUG("SPI normal read !!!\n");
        MSG_DEBUG("offset=%d,flashOffset=%d,fileLength=%d\n",offset,pSpiImage->flashOffset,pSpiImage->fileLength);
        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;

        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x85;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }


        remain=pSpiImage->fileLength;
        offset=pSpiImage->flashOffset;
        while(TRANSFER_LEN<remain)
        {

            Enable4ByteFlag = 0;
            if((offset + TRANSFER_LEN) > SPI_FLASH_SIZE)
                Enable4ByteFlag = 1;

            if(Enable4ByteFlag)
                MSG_DEBUG("Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);
            memset(ptr, 0, TRANSFER_LEN);
            usiRead(offset, TRANSFER_LEN , ptr);
            usb_send(ptr,TRANSFER_LEN); //send data to PC
            while(Bulk_Out_Transfer_Size==0) {}
            usb_recv((unsigned char*)_ack,4);   //recv data from PC
            remain-=TRANSFER_LEN;
            offset+=TRANSFER_LEN;
        }

        if(remain>0)
        {
            //4Byte Address Mode (>16MByte)
            Enable4ByteFlag = 0;
            if((offset + TRANSFER_LEN) > SPI_FLASH_SIZE)
                Enable4ByteFlag = 1;

            if(Enable4ByteFlag)
                MSG_DEBUG("Enable4ByteFlag %d:  offset=0x%08x(%d)\n", Enable4ByteFlag, offset, offset);
            memset(ptr, 0, TRANSFER_LEN);
            usiRead(offset, remain , ptr);
            usb_send(ptr,TRANSFER_LEN); //send data to PC
            while(Bulk_Out_Transfer_Size==0) {}
            usb_recv((unsigned char*)_ack,4);   //recv data from PC
        }
    }
    break;
    case PACK_MODE:
    {
        MSG_DEBUG("SPI pack mode !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x85;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;
        {
            PACK_HEAD pack;
            PACK_CHILD_HEAD ppack;
            while(1)
            {
                if(Bulk_Out_Transfer_Size>=sizeof(PACK_HEAD))
                {
                    usb_recv(ptr,sizeof(PACK_HEAD));
                    memcpy(&pack,(unsigned char *)ptr,sizeof(PACK_HEAD));
                    *_ack=sizeof(PACK_CHILD_HEAD);
                    usb_send((unsigned char*)_ack,4);//send ack to PC
                    break;
                }
            }
            MSG_DEBUG("pack.actionFlag=0x%x, pack.fileLength=0x%08x pack.num=%d!!!\n",pack.actionFlag,pack.fileLength,pack.num);
#if(0) //image size = DDR size - 1M - 64k
            for(i=0; i<pack.num; i++)
            {
                while(1)
                {
                    if(Bulk_Out_Transfer_Size>=sizeof(PACK_CHILD_HEAD))
                    {
                        usb_recv(ptr,sizeof(PACK_CHILD_HEAD));
                        memcpy(&ppack,(unsigned char *)ptr,sizeof(PACK_CHILD_HEAD));
                        break;
                    }
                }

                MSG_DEBUG("ppack.filelen=0x%x, ppack.startaddr=0x%08x!!!\n",ppack.filelen,ppack.startaddr);
                ptr=_ch;
                do
                {
                    if(Bulk_Out_Transfer_Size>0)
                    {
                        len=Bulk_Out_Transfer_Size;
                        usb_recv(ptr,len);  //recv data from PC
                        ptr+=len;
                        *_ack=len;
                        usb_send((unsigned char*)_ack,4);//send ack to PC
                    }
                }
                while((ptr-_ch)<ppack.filelen);
                if(ppack.imagetype!=PARTITION)
                {
                    Burn_SPI(ppack.filelen,((ppack.startaddr+SPI_BLOCK_SIZE-1)/SPI_BLOCK_SIZE)*SPI_BLOCK_SIZE);
                }
            }
#else // PACK BATCH_BRUN

            for(i=0; i<pack.num; i++)
            {
                MSG_DEBUG("ppack.filelen=0x%x, ppack.startaddr=0x%08x!!!\n",ppack.filelen,ppack.startaddr);
                ptr=_ch;
                while(1)
                {
                    if(Bulk_Out_Transfer_Size>=sizeof(PACK_CHILD_HEAD))
                    {
                        usb_recv(ptr,sizeof(PACK_CHILD_HEAD));
                        memcpy(&ppack,(unsigned char *)ptr,sizeof(PACK_CHILD_HEAD));
                        break;
                    }
                }

                spiSourceAddr = (DOWNLOAD_BASE | NON_CACHE);
                len = ppack.filelen;
                blockCount = len / SPI_BLOCK_SIZE;
                total = len;
                offset = ((ppack.startaddr+SPI_BLOCK_SIZE-1)/SPI_BLOCK_SIZE)*SPI_BLOCK_SIZE;
                MSG_DEBUG("fileLength=%d, len=%d, blockCount=%d  offset=0x%x\n", ppack.filelen, len, blockCount, offset);

                if(ppack.imagetype!=PARTITION)
                {
                    BatchBurn_SPI(len, offset);
                }
            }

        }
#endif
    }
    break;
    default:
        ;
    }
    MSG_DEBUG("finish SPI flash image download\n");
}

extern void fw_dtbfunc(int,int,int,int);
int Run_SDRAM(UINT32 address,UINT32 offset,UINT32 tmpAddr,UINT32 dtbEn,UINT32 dtbAddr)
{
    unsigned int i;
    int volatile status = 0;
    unsigned int volatile u32PowerOn;
    void    (*fw_func)(int, int, int);
    fw_func = (void(*)(int, int, int))address;

    /* Disable all interrupts */
    sysSetGlobalInterrupt(DISABLE_ALL_INTERRUPTS);
    sysSetLocalInterrupt(DISABLE_FIQ_IRQ);

    sysFlushCache(I_D_CACHE);
//  outpw(REG_USBD_PHY_CTL, inpw(REG_USBD_PHY_CTL) & ~0x200);    // offset 0x704

    outpw(REG_SYS_AHBIPRST,1<<19);  //USBD reset
    outpw(REG_SYS_AHBIPRST,0<<19);
    outpw(REG_USBD_PHYCTL, inpw(REG_USBD_PHYCTL) & ~0x100);
    outpw(REG_CLK_HCLKEN, inpw(REG_CLK_HCLKEN) & ~0x80000);

    MSG_DEBUG("run ... %x\n", address);

    SYS_UnlockReg();
    outpw(REG_CLK_HCLKEN, 0x00004527);
    outpw(REG_CLK_PCLKEN0, 0);
    outpw(REG_CLK_PCLKEN1, 0);
    outpw(REG_CLK_DIVCTL3, 0);//Reset SD0 Clock Source and Divider
    outpw(REG_CLK_DIVCTL9, 0);//Reset SD1 Clock Source and Divider
    outpw(REG_WDT_CTL, (inpw(REG_WDT_CTL) & ~(0xf << 8))|(0x6<<8));// Default WDT timeout 2^16 * (12M/512) = 2.79 sec
    MSG_DEBUG("SRAM WDT: 0x08%x/0x08%x/0x08%x\n",inpw(REG_WDT_CTL),inpw(REG_WDT_ALTCTL),inpw(REG_WDT_RSTCNT));
    u32PowerOn = inpw(REG_SYS_PWRON);
    if(((u32PowerOn&0x10) >> 4))   //PG[15:11] used as JTAG interface
    {
        for(i=(unsigned int)REG_SYS_GPA_MFPL; i<=(unsigned int)REG_SYS_GPG_MFPH; i+=4)
        {
            if(i == REG_SYS_GPG_MFPH)
            {
                outpw(i,inpw(REG_SYS_GPG_MFPH)& ~0x00000FFF);
                //printf("inpw(REG_SYS_GPG_MFPH) = 0x%x\n", inpw(REG_SYS_GPG_MFPH));
                continue;
            }
            else
                outpw(i,0x00);
        }
    }
    else     //PA[6:2] used as JTAG interface
    {
        for(i=(unsigned int)REG_SYS_GPA_MFPL; i<=(unsigned int)REG_SYS_GPG_MFPH; i+=4)
        {
            if(i == REG_SYS_GPA_MFPL)
            {
                outpw(i,inpw(REG_SYS_GPA_MFPL)& ~0xF00000FF);
                //printf("inpw(REG_SYS_GPA_MFPL) = 0x%x\n", inpw(REG_SYS_GPA_MFPL));
                continue;
            }
            else
                outpw(i,0x00);
        }
    }

    SYS_LockReg();

    if(address<offset)
        memcpy((unsigned char *)address,(unsigned char *)tmpAddr,offset);

    if(dtbEn==1)
        fw_dtbfunc(0,0,dtbAddr,address);
    else
        fw_func(0, ChipID, 0x100);
    return status;
}

void UXmodem_SDRAM(void)
{
    unsigned int fileAddr=0, dtbAddr=0, fileSize=0, exeFlag=0, dtbFlag=0;//actionFlag=0
    int len, debuglen = 0;
    unsigned char *ptr;
    unsigned char buf[80];
    unsigned char *_ch;
    unsigned int *_ack;

    int Otag;
    unsigned int offset=0x40;
    unsigned int tmpAddr=((EXEADDR-0x100000)-offset); // 62M

    MSG_DEBUG("download image to SDRAM...\n");

    _ch=((unsigned char*)(((unsigned int)buf)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)buf)|NON_CACHE));
    ptr=_ch;

    while(1)
    {
        if(Bulk_Out_Transfer_Size>0)
        {
            usb_recv(ptr,16);

            //actionFlag=*((unsigned int*)ptr);
            fileSize=*((unsigned int*)(ptr+4));
            fileAddr=*((unsigned int*)(ptr+8));
            dtbAddr=*((unsigned int*)(ptr+12));
            break;
        }
    }

    /* for debug or delay */
    {
        *_ack=0x80;
        usb_send((unsigned char*)_ack,4);//send ack to PC
    }

    MSG_DEBUG("action flag %d, address 0x%x, len %d, dtbAddr 0x%x\n", actionFlag, fileAddr, fileSize, dtbAddr);
    /* check if the image need execute */
    if (fileAddr & 0x80000000)
    {
        exeFlag = 1;
        fileAddr = fileAddr & 0x7FFFFFFF;
    }

    if (dtbAddr & 0x80000000)
    {
        dtbFlag = 1;
        dtbAddr = dtbAddr & 0x7FFFFFFF;
    }

    _ch=((unsigned char*)(((unsigned int)fileAddr | NON_CACHE)));
    if(fileAddr<offset && exeFlag == 1)
    {
        Otag=1;
        ptr=_ch+offset;
    }
    else
    {
        Otag=0;
        ptr=_ch;
    }
    MSG_DEBUG("Otag=%d,  0x%x\n", Otag, ptr);
    do
    {
        if(Bulk_Out_Transfer_Size>0)
        {
            len=Bulk_Out_Transfer_Size;
            usb_recv(ptr,len);  //recv data from PC
            if(Otag==1)
            {
                memcpy((unsigned char *)tmpAddr,ptr,offset);
                memcpy(ptr,ptr+offset,len-offset);
                ptr+=(len-offset);
                Otag=0;
            }
            else
            {
                ptr+=len;
            }
            *_ack=len;
            usb_send((unsigned char*)_ack,4);//send ack to PC
            debuglen+=len;
            //printf("ptr-_ch =0x%x, debugcnt = %d, debuglen =%d\n", (ptr-_ch), debugcnt++, debuglen);
        }

    }
    while((ptr-_ch)<fileSize);

    printf("\nfinish SDRAM download ...\n");

    if (exeFlag == 1)   /* execute image */
    {

#ifdef MSG_DEBUG_EN
        MSG_DEBUG("execute image ...\n");
#else
        ETIMER_Delay(0, 100); /* Waiting for application recevie ack */
#endif

        if(dtbFlag == 1)
        {
            dtbAddr = dtbAddr & 0x7FFFFFFF;
            Run_SDRAM(fileAddr,offset,tmpAddr,1,dtbAddr);
        }
        Run_SDRAM(fileAddr,offset,tmpAddr,0,0);
    }
    else
        MSG_DEBUG("only download ...\n");
}

/******************************************************************************
 *
 *  eMMC Functions
 *
 ******************************************************************************/
extern FW_MMC_IMAGE_T mmcImage;
extern FW_MMC_IMAGE_T *pmmcImage;
extern void GetMMCImage(void);

void Burn_MMC_RAW(UINT32 len, UINT32 offset,UINT8 *ptr)
{
    MSG_DEBUG("offset=%d,len=%d\n",offset>>9,len>>9);
    fmiSD_Write(offset>>9, len>>9, (UINT32)ptr);
}

void Read_MMC_RAW(UINT32 len, UINT32 offset,UINT8 *ptr)
{
    MSG_DEBUG("offset=%d,len=%d\n",offset>>9,len>>9);
    fmiSD_Read(offset>>9, len>>9, (UINT32)ptr);
}

int BatchBurn_MMC_BOOT(UINT32 len,UINT32 offset)
{
    int volatile tmplen=0;
    UINT32 TotalBlkCount,blockCount, mmcSourceAddr;
    UINT8 infoBuf[512],*ptr;

    unsigned char *_ch;
    unsigned int *_ack,ack=0;
    unsigned int reclen,remainlen;

    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)ack)|NON_CACHE));

    /* set up interface */
    if (fmiInitSDDevice() < 0)
    {
        SendAck(0xFFFFFFFF);
        return Fail;
    }

    /* check image 0 / offset 0 and back up */
    //if (pmmcImage->imageType == UBOOT)  // system image, burn nomal image
    //    fmiSD_Read(MMC_INFO_SECTOR,1,(UINT32)pInfo);

    mmcSourceAddr = (DOWNLOAD_BASE | NON_CACHE);
    ptr = (UINT8 *)mmcSourceAddr;
    TotalBlkCount = blockCount = (len+((SD_SECTOR)-1))/(SD_SECTOR);

    while(blockCount>=SD_MUL)
    {
        ptr=_ch;
        remainlen=(SD_SECTOR*SD_MUL);
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                reclen=MIN(TRANSFER_LEN,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                *_ack=reclen;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while((ptr-_ch)<(remainlen!=0));

        MSG_DEBUG("offset=0x%08x,ptr_addr=0x%08x,ptr=%d\n",offset,(UINT32)ptr,*(ptr));
        fmiSD_Write(offset,SD_MUL,(UINT32)ptr);
        blockCount-=SD_MUL;
        offset+=SD_MUL;
        /* ack status */
        tmplen += (SD_SECTOR*SD_MUL);
    }
    if(blockCount!=0)
    {
        ptr=_ch;
        remainlen=len-((SD_SECTOR)*(TotalBlkCount-blockCount));
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                reclen=MIN(TRANSFER_LEN,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                *_ack=reclen;//|((pagetmp * 95) / total)<<16;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while((ptr-_ch)<(remainlen!=0));
        fmiSD_Write(offset,blockCount,(UINT32)ptr);
    }
    /* restore image 0 and offset 0 */
    if (pmmcImage->imageType == 3)   // system image, burn nomal image
    {
        if((pmmcImage)->macaddr[7]==1)
            memcpy(infoBuf+0x190,(UCHAR *)((pmmcImage)->macaddr),6);  // MAC Address
    }

    if ((pmmcImage->flashOffset != 2) || (pmmcImage->imageType == 3))
    {
        /* set MMC information */
        MSG_DEBUG("SetMMCImageInfo\n");
        SetMMCImageInfo(pmmcImage);
    }
    //ack status
    SendAck(100);

    return 0;
}

int BatchBurn_MMC(UINT32 len,UINT32 offset,UINT32 HeaderFlag)
{
    int volatile tmplen=0, i, blockCount=0;
    UINT32 volatile mmcSourceAddr;
    UINT8 volatile infoBuf[512],*ptr;
    unsigned char *_ch;
    unsigned int *_ack,ack=0;
    unsigned int volatile reclen,remainlen;
    unsigned int volatile headlen;

    if(HeaderFlag==1)
        headlen=IBR_HEADER_LEN;
    else
        headlen=0;

    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)ack)|NON_CACHE));

    /* set up interface */
    if (fmiInitSDDevice() < 0)
    {
        SendAck(0xFFFFFFFF);
        return Fail;
    }

    /* check image 0 / offset 0 and back up */
    //if (pmmcImage->imageType == UBOOT)  // system image, burn normal image
    //    fmiSD_Read(MMC_INFO_SECTOR,1,(UINT32)pInfo);

    mmcSourceAddr = (DOWNLOAD_BASE | NON_CACHE);
    ptr = (UINT8 *)mmcSourceAddr;
    tmplen=len;
    MSG_DEBUG("debug eMMC  blockCount=%d   tmplen=%d    _ack = 0x%x\n",blockCount, tmplen, *_ack);
    while(tmplen>TRANSFER_LEN)
    {
        ptr=_ch+headlen;
        remainlen=(SD_SECTOR*SD_MUL);//4096
        //usleep(1000);
        do
        {
            MSG_DEBUG("Bulk_Out_Transfer_Size=%d   remainlen =%d\n",Bulk_Out_Transfer_Size, remainlen);
            if(Bulk_Out_Transfer_Size>0)
            {
                reclen=MIN(TRANSFER_LEN,remainlen);
                //MSG_DEBUG("reclen=0x%08x\n",reclen);
                usb_recv((UINT8 *)ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                *_ack=reclen;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);
        ptr=_ch;
        MSG_DEBUG("tmplen=0x%08x,ptr_addr=0x%08x,ptr=%d\n",tmplen,(UINT32)ptr,*(ptr));
        fmiSD_Write(offset,SD_MUL,(UINT32)ptr);
        //MSG_DEBUG("eMMC ret=0x%x\n",ret);
        blockCount-=SD_MUL;
        offset+=SD_MUL;
        tmplen -= (SD_SECTOR*SD_MUL);
        memcpy(_ch,_ch+(SD_SECTOR*SD_MUL),headlen);
    }

    if(tmplen!=0)
    {
        ptr=_ch+headlen;
        remainlen=tmplen;
        do
        {
            MSG_DEBUG("Last Bulk_Out_Transfer_Size=%d   remainlen =%d\n",Bulk_Out_Transfer_Size, remainlen);
            if(Bulk_Out_Transfer_Size>0)
            {
                reclen=MIN(TRANSFER_LEN,remainlen);
                usb_recv((UINT8 *)ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                *_ack=reclen;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);
        ptr=_ch;
        MSG_DEBUG("tmplen=0x%08x,ptr_addr=0x%08x,ptr=%d\n",tmplen,(UINT32)ptr,*(ptr));
        fmiSD_Write(offset,(tmplen+SD_SECTOR-1)/SD_SECTOR,(UINT32)ptr);
    }
    /* restore image 0 and offset 0 */
    if (pmmcImage->imageType == PACK)   // system image, burn normal image
    {
        if((pmmcImage)->macaddr[7]==1)
            memcpy((UINT8 *)infoBuf+0x190,(UCHAR *)((pmmcImage)->macaddr),6);  // MAC Address
    }

    if ((pmmcImage->flashOffset != 2) || (pmmcImage->imageType == PACK))
    {
        /* set MMC information */
        MSG_DEBUG("SetMMCImageInfo\n");
        SetMMCImageInfo(pmmcImage);
    }
    //ack status
    SendAck(100);

    return 0;
}

void UXmodem_MMC()
{
    int volatile len,i, j, ret;
    PMBR pmbr;
    unsigned char *ptr;
    unsigned char buf[80];
    unsigned char *_ch;
    unsigned int *_ack;
    unsigned int volatile blockCount,offset=0;
    PACK_CHILD_HEAD ppack;
    unsigned int ddrlen;

    MSG_DEBUG("Start  download image to eMMC flash...\n");
    /* initial eMMC */
    fmiInitSDDevice();

    memset((char *)&mmcImage, 0, sizeof(FW_MMC_IMAGE_T));
    pmmcImage = (FW_MMC_IMAGE_T *)&mmcImage;
    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)buf)|NON_CACHE));
    ptr=_ch;
    while(1)
    {
        if(Bulk_Out_Transfer_Size>0)
        {
            usb_recv(ptr,sizeof(FW_MMC_IMAGE_T));
            memcpy(pmmcImage,(unsigned char *)ptr,sizeof(FW_MMC_IMAGE_T));
            break;
        }
    }
    MSG_DEBUG("Action flag: %s, image %d, len=0x%x(%d) exec=0x%08x\n",
              au8ActionFlagName[pmmcImage->actionFlag], pmmcImage->imageNo,pmmcImage->fileLength, pmmcImage->fileLength, pmmcImage->executeAddr);

    switch(pmmcImage->actionFlag)
    {
    case WRITER_MODE: /* normal write */
        MSG_DEBUG("eMMC normal write !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=(USBD_BURN_TYPE | USBD_FLASH_MMC);
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }
        MSG_DEBUG("Wait for USBD !!!\n");
        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;

        if (pmmcImage->imageType == UBOOT)      // system image
        {

            pmmcImage->imageNo = 0;
            pmmcImage->flashOffset = 0x400;
            addMagicHeader(pmmcImage->executeAddr, pmmcImage->fileLength);
            addNUC980MagicHeader(1);
            ptr += IBR_HEADER_LEN;// except the 32 bytes magic header
#if 0
            pmmcImage->executeAddr = 0x1200000;  /* 18MB */
#endif
        }
        MSG_DEBUG("Bulk_Out_Transfer_Size pmmcImage->fileLength=0x%08x!!!\n",pmmcImage->fileLength);

        MSG_DEBUG("Burn_MMC!!!\n");
        if (pmmcImage->imageType == UBOOT)
        {
            MSG_DEBUG("BatchBurn_MMC() [0x%x(%d)/0x%x(%d)/1]\n", pmmcImage->fileLength+pmmcImage->initSize+IBR_HEADER_LEN, pmmcImage->fileLength+pmmcImage->initSize+IBR_HEADER_LEN, (pmmcImage->flashOffset/SD_SECTOR), (pmmcImage->flashOffset/SD_SECTOR));
            ret = BatchBurn_MMC(pmmcImage->fileLength+pmmcImage->initSize+IBR_HEADER_LEN,(pmmcImage->flashOffset/SD_SECTOR),1);
            if(ret == 1)
            {
                printf("Error BatchBurn_MMC Device UBOOT image error !!! \n");
                return;
            }
        }
        else
        {
            MSG_DEBUG("BatchBurn_MMC() [0x%x(%d)/0x%x(%d)/0]\n", pmmcImage->fileLength, pmmcImage->fileLength, pmmcImage->flashOffset/SD_SECTOR, pmmcImage->flashOffset/SD_SECTOR);
            ret = BatchBurn_MMC(pmmcImage->fileLength,(pmmcImage->flashOffset/SD_SECTOR),0);
            if(ret == 1)
            {
                printf("Error BatchBurn_MMC Device others image error !!! \n");
                return;
            }
        }

        break;
    case MODIFY_MODE: /* modify */
        MSG_DEBUG("eMMC modify !!!\n");
        {
            int state;
            state = ChangeMMCImageType(pmmcImage->imageNo, pmmcImage->imageType);
            if (state < 0)
            {
                //MSG_DEBUG("error!!\n");
                SendAck(0xFFFFFFFF);
            }
            else
            {
                //MSG_DEBUG("OK!!\n");
                SendAck(100);
            }
        }
        break;
    case ERASE_MODE: /* erase */
        MSG_DEBUG("eMMC erase !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=(USBD_BURN_TYPE | USBD_FLASH_MMC);
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }
        //if pmmcImage->imageNo = 0xFFFFFFFF then erase all
        DelMMCImage(pmmcImage->imageNo);
        break;
    case VERIFY_MODE:   // verify
    {
        MSG_DEBUG("MMC normal verify !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=(USBD_BURN_TYPE | USBD_FLASH_MMC);
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;
        if(pmmcImage->imageType == UBOOT)
            blockCount = (pmmcImage->fileLength+IBR_HEADER_LEN+pmmcImage->initSize+((SD_SECTOR)-1))/(SD_SECTOR);
        else
            blockCount = (pmmcImage->fileLength+((SD_SECTOR)-1))/(SD_SECTOR);

        len=pmmcImage->fileLength;
        blockCount = (blockCount+SD_MUL-1)/SD_MUL;
        offset = pmmcImage->flashOffset/SD_SECTOR;

        if(pmmcImage->imageType==UBOOT)
        {
            fmiSD_Read(offset,SD_MUL,(UINT32)ptr);
            offset+=SD_MUL;
            if(blockCount==1)
            {
                ptr=_ch+(IBR_HEADER_LEN+pmmcImage->initSize);
                usb_send(ptr,TRANSFER_LEN); //send data to PC
                while(Bulk_Out_Transfer_Size==0) {}
                usb_recv((unsigned char*)_ack,4);   //recv data from PC
            }
            else
            {
                INT32 volatile mvlen,tranferlen;
                tranferlen=len;
                mvlen=(SD_MUL*SD_SECTOR)-(IBR_HEADER_LEN+pmmcImage->initSize);
                memmove(_ch,_ch+(IBR_HEADER_LEN+pmmcImage->initSize),mvlen);
                //printf("len %d, blockCount=%d\n", len, blockCount);
                for(i=1; i<blockCount; i++)
                {
                    ptr=_ch+mvlen;
                    fmiSD_Read(offset,SD_MUL,(UINT32)ptr);
                    ptr=_ch;
                    usb_send(ptr,TRANSFER_LEN); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4);   //recv data from PC
                    tranferlen-=TRANSFER_LEN;
                    memmove(_ch,_ch+(8*SD_SECTOR),mvlen);
                    offset+=SD_MUL;
                    //printf("%d   tranferlen = %d\n", i, tranferlen);
                }
                if(tranferlen>0)
                {
                    //printf("rnt tranferlen = %d\n", tranferlen);
                    usb_send(ptr,TRANSFER_LEN); //send data to PC
                    //usb_send(ptr,tranferlen); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4);   //recv data from PC
                }
            }
        }
        else
        {
            for(i=0; i<blockCount; i++)
            {
                fmiSD_Read(offset,SD_MUL,(UINT32)ptr);
                usb_send(ptr,TRANSFER_LEN); //send data to PC
                usb_recv((unsigned char*)_ack,4);   //recv data from PC
                offset+=SD_MUL;
            }
        }
    }
    break;
    case PACK_VERIFY_MODE:   // verify
    {
        MSG_DEBUG("eMMC PACK verify !!!\n");

        /* for debug or delay */
        {
            usleep(1000);
            *_ack=(USBD_BURN_TYPE | USBD_FLASH_MMC);
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;

        for(j=0; j<pmmcImage->imageNo; j++)
        {
            MSG_DEBUG("loop %d:  pmmcImage->imageNo = %d,  initSize=0x%08x\n", j, pmmcImage->imageNo, pmmcImage->initSize);
            while(1)
            {
                if(Bulk_Out_Transfer_Size>=sizeof(PACK_CHILD_HEAD))
                {
                    usb_recv(ptr,sizeof(PACK_CHILD_HEAD));
                    memcpy(&ppack,(unsigned char *)ptr,sizeof(PACK_CHILD_HEAD));
                    usleep(1000);
                    *_ack=(USBD_BURN_TYPE | USBD_FLASH_MMC);
                    usb_send((unsigned char*)_ack,4);//send ack to PC
                    break;
                }
            }

            pmmcImage->imageType = ppack.imagetype;
            pmmcImage->fileLength = ppack.filelen;
            pmmcImage->flashOffset = ppack.startaddr;
            MSG_DEBUG("pmmcImage->fileLength = %d(0x%x)\n", pmmcImage->fileLength, pmmcImage->fileLength);
            MSG_DEBUG("pmmcImage->flashOffset = %d(0x%x)\n", pmmcImage->flashOffset, pmmcImage->flashOffset);
            MSG_DEBUG("pmmcImage->imageType = %d(0x%x)\n", pmmcImage->imageType, pmmcImage->imageType);

            if(pmmcImage->imageType == UBOOT)
            {
                blockCount = (pmmcImage->fileLength+IBR_HEADER_LEN+pmmcImage->initSize+((SD_SECTOR)-1))/(SD_SECTOR);
                pmmcImage->flashOffset = 0x400;
                MSG_DEBUG("blockCount = %d\n", blockCount);
                MSG_DEBUG("pmmcImage->flashOffset = %d(0x%x)\n", pmmcImage->flashOffset, pmmcImage->flashOffset);

                // Get DDR parameter length
                usb_recv(ptr,4);
                memcpy(&ddrlen,(unsigned char *)ptr,4);
                MSG_DEBUG("ddrlen = %d\n", ddrlen);
                usleep(1000);
                *_ack=(USBD_BURN_TYPE | USBD_FLASH_MMC);
                usb_send((unsigned char*)_ack,4);//send ack to PC

            }
            else
            {
                blockCount = (pmmcImage->fileLength+((SD_SECTOR)-1))/(SD_SECTOR);
                MSG_DEBUG("blockCount = %d\n", blockCount);
            }

            ptr=_ch;
            len=pmmcImage->fileLength;
            blockCount = (blockCount+SD_MUL-1)/SD_MUL;
            offset = pmmcImage->flashOffset/SD_SECTOR;
            if(pmmcImage->imageType==UBOOT)
            {
                fmiSD_Read(offset,SD_MUL,(UINT32)ptr);
                offset+=8;
                if(blockCount==1)
                {
                    ptr=_ch+(IBR_HEADER_LEN+pmmcImage->initSize);
                    usb_send(ptr,TRANSFER_LEN); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4);   //recv data from PC
                }
                else
                {
                    INT32 mvlen,tranferlen;
                    tranferlen=len-IBR_HEADER_LEN-ddrlen;
                    mvlen=(SD_MUL*SD_SECTOR)-(IBR_HEADER_LEN+pmmcImage->initSize)-ddrlen;
                    memmove(_ch,_ch+(IBR_HEADER_LEN+pmmcImage->initSize)+ddrlen,mvlen);
                    for(i=1; i<blockCount; i++)
                    {
                        ptr=_ch+mvlen;
                        fmiSD_Read(offset,8,(UINT32)ptr);
                        ptr=_ch;
                        usb_send(ptr,TRANSFER_LEN); //send data to PC
                        while(Bulk_Out_Transfer_Size==0) {}
                        usb_recv((unsigned char*)_ack,4);   //recv data from PC
                        tranferlen-=TRANSFER_LEN;
                        memmove(_ch,_ch+(SD_MUL*SD_SECTOR),mvlen);
                        offset+=SD_MUL;
                    }
                    if(tranferlen>0)
                    {
                        MSG_DEBUG("Last transfer %d    0x%x\n", tranferlen, mvlen);
                        usb_send(ptr,TRANSFER_LEN); //send data to PC
                        //usb_send(ptr,tranferlen); //send data to PC
                        while(Bulk_Out_Transfer_Size==0) {}
                        usb_recv((unsigned char*)_ack,4); //recv data from PC
                    }
                }
            }
            else
            {
                for(i=0; i<blockCount; i++)
                {
                    fmiSD_Read(offset,SD_MUL,(UINT32)ptr);
                    usb_send(ptr,TRANSFER_LEN);//send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4);//recv data from PC
                    offset+=8;
                }
            }
        }
    }
    break;
    case FORMAT_MODE:   /* Format */
    {
        unsigned int *ptr;
        UCHAR _fmi_ucBuffer[512];
        ptr = (unsigned int *)((UINT32)_fmi_ucBuffer | 0x80000000);
        MSG_DEBUG("eMMC format !!!\n");

        /* for debug or delay */
        {
            usleep(1000);
            *_ack=(USBD_BURN_TYPE | USBD_FLASH_MMC);
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        printf("pmmcImage->ReserveSize = %d ! eMMCBlockSize = %d\n",pmmcImage->ReserveSize, eMMCBlockSize);
        MSG_DEBUG("PartitionNum =%d, P1=%d(0x%x)MB P2=%d(0x%x)MB P3=%d(0x%x)MB P4=%d(0x%x)MB\n", pmmcImage->PartitionNum, pmmcImage->PartitionS1Size, pmmcImage->PartitionS1Size, pmmcImage->PartitionS2Size, pmmcImage->PartitionS2Size,
                  pmmcImage->PartitionS3Size, pmmcImage->PartitionS3Size, pmmcImage->PartitionS4Size, pmmcImage->PartitionS4Size);
        if(eMMCBlockSize>0)
        {
            //pmbr=create_mbr(eMMCBlockSize,pmmcImage->ReserveSize);
            //FormatFat32(pmbr,0);
            pmbr=create_mbr(eMMCBlockSize, pmmcImage);
            switch(pmmcImage->PartitionNum)
            {
            case 1:
                FormatFat32(pmbr,0);
                break;
            case 2:
            {
                FormatFat32(pmbr,0);
                FormatFat32(pmbr,1);
            }
            break;
            case 3:
            {
                FormatFat32(pmbr,0);
                FormatFat32(pmbr,1);
                FormatFat32(pmbr,2);
            }
            break;
            case 4:
            {
                FormatFat32(pmbr,0);
                FormatFat32(pmbr,1);
                FormatFat32(pmbr,2);
                FormatFat32(pmbr,3);
            }
            break;
            }

            fmiSD_Read(MMC_INFO_SECTOR,1,(UINT32)ptr);
            *(ptr+125)=0x11223344;
            *(ptr+126)=pmmcImage->ReserveSize;
            *(ptr+127)=0x44332211;
            fmiSD_Write(MMC_INFO_SECTOR,1,(UINT32)ptr);
            SendAck(100);
        }
        else
            SendAck(0xFFFFFFFF);
    }
    break;
    case READ_MODE:
    {
        MSG_DEBUG("MMC normal read !!!\n");

        /* for debug or delay */
        {
            usleep(1000);
            *_ack=(USBD_BURN_TYPE | USBD_FLASH_MMC);
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;
        offset = pmmcImage->flashOffset;
        blockCount = (pmmcImage->fileLength+((SD_SECTOR)-1))/(SD_SECTOR);
        blockCount = (blockCount+8-1)/8;
        while(blockCount)
        {

            fmiSD_Read(offset,8,(UINT32)ptr);
            MSG_DEBUG("offset=0x%08x,ptr_addr=0x%08x,ptr=%d\n",offset,(UINT32)ptr,*(ptr));
            offset+=8;
            blockCount--;
            usb_send(ptr,4096); //send data to PC
            while(Bulk_Out_Transfer_Size==0) {}
            usb_recv((unsigned char*)_ack,4);   //recv data from PC
        }
    }
    break;
    case PACK_MODE:
    {
        MSG_DEBUG("MMC normal pack !!!\n");

        /* for debug or delay */
        {
            usleep(1000);
            *_ack=(USBD_BURN_TYPE | USBD_FLASH_MMC);
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;
        {
            PACK_HEAD pack;
            PACK_CHILD_HEAD ppack;
            while(1)
            {
                if(Bulk_Out_Transfer_Size>=sizeof(PACK_HEAD))
                {
                    usb_recv(ptr,sizeof(PACK_HEAD));
                    memcpy(&pack,(unsigned char *)ptr,sizeof(PACK_HEAD));
                    *_ack=sizeof(PACK_HEAD);
                    usb_send((unsigned char*)_ack,4);//send ack to PC
                    break;
                }
            }
            MSG_DEBUG("pack.actionFlag=0x%x, pack.fileLength=0x%08x pack.num=%d!!!\n",pack.actionFlag,pack.fileLength,pack.num);
            for(i=0; i<pack.num; i++)
            {
                while(1)
                {
                    if(Bulk_Out_Transfer_Size>=sizeof(PACK_CHILD_HEAD))
                    {
                        usb_recv(ptr,sizeof(PACK_CHILD_HEAD));
                        memcpy(&ppack,(unsigned char *)ptr,sizeof(PACK_CHILD_HEAD));
                        *_ack=sizeof(PACK_CHILD_HEAD);
                        usb_send((unsigned char*)_ack,4);//send ack to PC
                        break;
                    }
                }
                MSG_DEBUG("ppack.filelen=0x%x, ppack.imagetype %d  ppack.startaddr=0x%08x!!!\n",ppack.filelen,ppack.imagetype,ppack.startaddr);
                if(ppack.imagetype!=PARTITION)
                {
                    if(ppack.imagetype==UBOOT)
                        ppack.startaddr=0x400;
                    BatchBurn_MMC(ppack.filelen,ppack.startaddr/SD_SECTOR,0);
                }
            }
        }
    }
    break;
    }
}

/******************************************************************************
 *
 *  NAND Functions
 *
 ******************************************************************************/
FW_NAND_IMAGE_T nandImage;
FW_NAND_IMAGE_T *pNandImage;

void Burn_NAND_RAW(UINT32 len, UINT8 *ptr)
{
    MSG_DEBUG("len=%d\n",len);
}

void Read_NAND_RAW(UINT32 len, UINT8 *ptr)
{
    MSG_DEBUG("len=%d\n", len);
}

#define DISABLE_YAFFS2
#if defined(BATCH_BRUN)
int BatchBurn_NAND(UINT32 len,UINT32 blockNo,UINT32 type)
{
    int volatile status = 0;
    int volatile page_count, block_count, page, addr, blockNum, total;
    int volatile i, j, pagetmp=0;
    unsigned int address = DOWNLOAD_BASE;

    unsigned int sparesize;  //Put yaffs2 tag on the oob(sparesize).

    unsigned char *_ch,*ptr;
    unsigned int *_ack,ack=0;
    unsigned int reclen,remainlen;
    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)ack)|NON_CACHE));
    ptr=_ch;

#ifndef DISABLE_YAFFS2
    if(type==YAFFS2 && (len%512)!=0)
        sparesize=pSM->uSpareSize;
    else
#endif
        sparesize=0;

    page_count = len / (pSM->uPageSize+sparesize);
    if(len%(pSM->uPageSize+sparesize)!=0) page_count++;
    // erase needed blocks
    block_count = page_count / (pSM->uPagePerBlock);


    // write into flash
    blockNum = blockNo;
    total = len;
    MSG_DEBUG("blockNum=%d,total=%d\n",blockNum,total);
    for (j=0; j<block_count; j++)
    {
        MSG_DEBUG("%d,%d\n",j,block_count);
        ptr=_ch;
        remainlen=MIN(total,pSM->uPagePerBlock*(pSM->uPageSize+sparesize));
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                MSG_DEBUG("total=%08d,remainlen=%08d\n",total,remainlen);
                reclen=MIN(TRANSFER_LEN,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                total-=reclen;
                *_ack=reclen;//|((pagetmp * 95) / total)<<16;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);

        MSG_DEBUG("remainlen OK\n");
_retry_1:
        addr = address | NON_CACHE;
        page = pSM->uPagePerBlock * (blockNum);
        MSG_DEBUG("fmiSM_BlockErase page = %d   blockNum = %d   addr =0x%x\n", page, blockNum, addr);
        status = fmiSM_BlockErase(pSM, blockNum); // block erase
        //if (status != 0) {
        if (status == 1)
        {
            fmiMarkBadBlock(pSM, blockNum);
            blockNum++;
            goto _retry_1;
        }
        else if (status == -1)
        {
            MSG_DEBUG("Error device error !! \n");

            usb_recv(ptr,Bulk_Out_Transfer_Size);  //recv data from PC
            //SendAck(0xffff);
            *_ack=0xffff;
            usb_send((unsigned char*)_ack,4);//send ack to PC
            return status;
        }
        MSG_DEBUG("pSM->uPagePerBlock=%08d\n",pSM->uPagePerBlock);
        // write block
        for (i=0; i<pSM->uPagePerBlock; i++)
        {
#ifndef DISABLE_YAFFS2
            if(type==YAFFS2 && (len%512)!=0)
            {
                status = fmiSM_Write_large_page_oob(page+i, 0, addr,pSM->uSpareSize);
            }
            else
#endif
                status = fmiSM_Write_large_page(page+i, 0, addr);

            if (status != 0)
            {
                fmiMarkBadBlock(pSM, blockNum);
                blockNum++;
                goto _retry_1;
            }
            addr += pSM->uPageSize;
#ifndef DISABLE_YAFFS2
            if(type==YAFFS2 && (len%512)!=0)
                addr += (pSM->uPageSize>>5);
#endif
            pagetmp++;
        }
        blockNum++;
    }
    MSG_DEBUG("page_count=%d,pSM->uPagePerBlock=%d\n",page_count,pSM->uPagePerBlock);
    if ((page_count % pSM->uPagePerBlock) != 0)
    {
        MSG_DEBUG("Cnt=%d\n",page_count % pSM->uPagePerBlock);
        memset(_ch,0xff,pSM->uPagePerBlock*(pSM->uPageSize+sparesize));
        ptr=_ch;
        remainlen=total;//(len-block_count*(pSM->uPagePerBlock*(pSM->uPageSize+sparesize)));
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                MSG_DEBUG("remainlen=%d\n",remainlen);
                reclen=MIN(TRANSFER_LEN,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                total-=reclen;
                *_ack=reclen;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);
        page_count = page_count - block_count * pSM->uPagePerBlock;
        MSG_DEBUG("page_count=%d\n",page_count);
_retry_2:
        addr = address | NON_CACHE;
        page = pSM->uPagePerBlock * (blockNum);
        status = fmiSM_BlockErase(pSM, blockNum);      // erase block
        //if (status != 0) {
        if (status == 1)
        {
            fmiMarkBadBlock(pSM, blockNum);
            blockNum++;
            goto _retry_2;
        }
        else if (status == -1)
        {
            MSG_DEBUG("device error !! \n");
            SendAck(0xffff);
            return status;
        }
        // write block
        MSG_DEBUG("page_count=%d,type=%d\n",page_count,type);
        for (i=0; i<page_count; i++)
        {
            MSG_DEBUG("i=%d\n",i);
#ifndef DISABLE_YAFFS2
            if(type==YAFFS2 && (len%512)!=0)
                status = fmiSM_Write_large_page_oob(page+i, 0, addr,pSM->uPageSize>>5);
            else
#endif
                status = fmiSM_Write_large_page(page+i, 0, addr);

            if (status != 0)
            {
                fmiMarkBadBlock(pSM, blockNum);
                blockNum++;
                goto _retry_2;
            }
            addr += pSM->uPageSize;
#ifndef DISABLE_YAFFS2
            if(type==YAFFS2 && (len%512)!=0)
                addr += (pSM->uPageSize>>5);
#endif
            pagetmp++;
        }
        blockNum++;
    }

    SendAck(blockNum-blockNo);
    return status;
}
#endif


int BatchBurn_NAND_Data_OOB(UINT32 len,UINT32 blockNo,UINT32 type)
{
    int volatile status = 0;
    int volatile page_count, block_count, page, addr, blockNum, total;
    int volatile i, j, pagetmp=0;
    unsigned int address = DOWNLOAD_BASE;
    unsigned int BurnLen;
    unsigned int sparesize;  //Put yaffs2 tag on the oob(sparesize).

    unsigned char *_ch,*ptr;
    unsigned int *_ack,ack=0;
    unsigned int reclen,remainlen;
    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)ack)|NON_CACHE));
    ptr=_ch;

    sparesize=pSM->uSpareSize;
    page_count = len / (pSM->uPageSize+sparesize);
    if(len%(pSM->uPageSize+sparesize)!=0) page_count++;
    // erase needed blocks
    block_count = page_count / (pSM->uPagePerBlock);


    // write into flash
    blockNum = blockNo;
    total = len;
    BurnLen=(pSM->uPageSize+sparesize);
    MSG_DEBUG("blockNum=%d,total=%d,block_count=%d\n",blockNum,total,block_count);
    for (j=0; j<block_count; j++)
    {
        MSG_DEBUG("%d,%d\n",j,block_count);
        ptr=_ch;
        remainlen=MIN(total,pSM->uPagePerBlock*(pSM->uPageSize+sparesize));
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                MSG_DEBUG("total=%08d,remainlen=%08d\n",total,remainlen);
                reclen=MIN(BurnLen,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                total-=reclen;
                *_ack=reclen;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);

        MSG_DEBUG("remainlen OK\n");
//_retry_1:
        addr = address | NON_CACHE;
        page = pSM->uPagePerBlock * (blockNum);
        status = fmiSM_BlockEraseBad(pSM, blockNum); // block erase
        // write block
        for (i=0; i<pSM->uPagePerBlock; i++)
        {
            status = fmiSM_Write_large_page_oob2(page+i, 0, addr);
//             if (status != 0)
//             {
//                 fmiMarkBadBlock(pSM, blockNum);
//                 blockNum++;
//                 goto _retry_1;
//             }
            addr += pSM->uPageSize+sparesize;
            pagetmp++;
            //SendAck((pagetmp * 95) / total);
        }
        blockNum++;
    }
    MSG_DEBUG("page_count=%d,pSM->uPagePerBlock=%d\n",page_count,pSM->uPagePerBlock);

    //SendAck(100);
    SendAck(blockNum-blockNo);
    return status;
}

int Read_Nand(UINT32 dst_adr,UINT32 blockNo, UINT32 len)
{
    volatile char *ptr;
    int spareSize;
    int volatile status = 0;
    int volatile page_count, block_count, blockNum;
    int volatile page_no, count, i=0, j=0, k;

    page_count = len / pSM->uPageSize;
    if ((len % pSM->uPageSize) != 0) page_count++;
    blockNum = blockNo;
    count = len;
    while(1)
    {
        MSG_DEBUG("blockNum=%d to start reading...\n",blockNum);
        if (fmiCheckInvalidBlock(pSM, blockNum) != 1)
        {
            for (i=0; i<pSM->uPagePerBlock; i++)
            {
                page_no = blockNum * pSM->uPagePerBlock + i;

                //--- read redunancy area to register SMRAx
                spareSize = inpw(REG_NANDRACTL) & 0x1ff;
                ptr = (volatile char *)REG_NANDRA0;
                fmiSM_Read_RA(page_no, pSM->uPageSize);
                for (k=0; k<spareSize; k++)
                    *ptr++ = inpw(REG_NANDDATA) & 0xff;                   // copy RA data from NAND to SMRA by SW
                fmiSM_Read_large_page(pSM, page_no, (UINT32)dst_adr);
                dst_adr += pSM->uPageSize;
                count -= pSM->uPageSize;
                MSG_DEBUG("pSM->uPagePerBlock=%d   --> %d...0x%x \n",pSM->uPagePerBlock, i, dst_adr);
            }
            j++;

        }
        else
        {
            printf("block%d is bad block...\n",blockNum);
        }
        blockNum++;
        if ((j >= block_count) || (count <= 0))
            break;
    }

    return 0;
}

int Read_Nand_Redunancy(UINT32 dst_adr,UINT32 blockNo, UINT32 len)
{
    volatile char *ptr;
    volatile char *dst_redunancy_adr;
    int spareSize;
    int volatile status = 0;
    int volatile page_count, block_count, blockNum;
    int volatile page_no, count, i=0, j=0, k;

    page_count = len / pSM->uPageSize;
    if ((len % pSM->uPageSize) != 0) page_count++;
    blockNum = blockNo;
    block_count = page_count / pSM->uPagePerBlock;
    if ((page_count % pSM->uPagePerBlock) != 0)
        block_count++;
    count = len;
    while(1)
    {
        MSG_DEBUG("blockNum=%d to start reading...\n",blockNum);
        for (i=0; i<pSM->uPagePerBlock; i++)
        {
            dst_redunancy_adr=(volatile char *)(dst_adr + pSM->uPageSize);
            page_no = blockNum * pSM->uPagePerBlock + i;
            //--- read redunancy area to register SMRAx
            spareSize = inpw(REG_NANDRACTL) & 0x1ff;
            ptr = (volatile char *)REG_NANDRA0;
            fmiSM_Read_RA(page_no, pSM->uPageSize);
            for (k=0; k<spareSize; k++)
            {
                *dst_redunancy_adr = inpw(REG_NANDDATA) & 0xff;// copy RA data from NAND to SMRA by SW
                *ptr++ = *dst_redunancy_adr;
                dst_redunancy_adr++;
            }
            fmiSM_Read_large_page(pSM, page_no, (UINT32)dst_adr);
            dst_adr += (pSM->uPageSize+pSM->uSpareSize);
            count -= (pSM->uPageSize+pSM->uSpareSize);
        }
        j++;
        blockNum++;
        if ((j >= block_count) || (count <= 0))
            break;
    }
    return 0;
}

#if defined(BATCH_BRUN)
int BatchBurn_NAND_BOOT(UINT32 len,UINT32 blockNo,UINT32 blockLen,UINT32 HeaderFlag)
{
    int volatile status = 0;
    int volatile page_count, block_count, page, addr, blockNum, total;
    int volatile blkindx,i, j, pagetmp=0;
    unsigned int address = DOWNLOAD_BASE;

    unsigned char *_ch,*ptr;
    unsigned int *_ack,ack=0;
    unsigned int reclen,remainlen;
    unsigned int headlen;

    if(HeaderFlag==1)
        headlen=IBR_HEADER_LEN;//16;
    else
        headlen=0;

    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)ack)|NON_CACHE));
    ptr=_ch;
    page_count = len / pSM->uPageSize;
    if ((len % pSM->uPageSize) != 0)
        page_count++;

    // write into flash
    blockNum = blockNo;
    total = page_count*blockLen;

    // define for USB ack
    addr = address | NON_CACHE;

    // erase needed blocks
    block_count = page_count / pSM->uPagePerBlock;
    if (page_count <= pSM->uPagePerBlock)
    {
        //page_count = page_count - block_count * pSM->uPagePerBlock;
        memset(_ch+headlen,0xff,pSM->uPagePerBlock*(pSM->uPageSize));
        ptr=_ch+headlen;
        remainlen=len-(block_count*pSM->uPagePerBlock*pSM->uPageSize)-headlen;
        MSG_DEBUG("remainlen=%d,block_count=%d\n",remainlen,block_count);
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                reclen=MIN(Bulk_Out_Transfer_Size,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                *_ack=reclen|((pagetmp * 95) / total)<<16;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);
        // erase block
//_retry_2:
        for(blkindx=blockNum; blkindx<blockLen; blkindx++)
        {
            addr = address | NON_CACHE;
            page = pSM->uPagePerBlock * (blkindx);
            status = fmiSM_BlockErase(pSM, blkindx);
            if (status != 0)
            {
                fmiMarkBadBlock(pSM, blkindx);
                //blockNum++;
                //goto _retry_2;
                continue;
            }

            // write block
            for (i=0; i<page_count; i++)
            {
                status = fmiSM_Write_large_page(page+i, 0, addr);
                if (status != 0)
                {
                    fmiMarkBadBlock(pSM, blkindx);
                    //blockNum++;
                    addr = (address + block_count * pSM->uPagePerBlock * pSM->uPageSize) | NON_CACHE;
                    //goto _retry_2;
                    continue;
                }
                addr += pSM->uPageSize;
                pagetmp++;
            }
        }
        blockNum++;
    }
    else
    {
        printf("Error Device image Error !!!\n");
        usb_recv(ptr, Bulk_Out_Transfer_Size);  //recv data from PC
        *_ack = 0xFFFF;
        usb_send((unsigned char*)_ack,4);//send ack to PC
        return -1;
    }
    SendAck(block_count+blockLen);
    MSG_DEBUG("SendAck 0x%x\n", block_count+blockLen);
    return status;
}
#endif

int Burn_NAND_BACKUP(UINT32 len,UINT32 blockNo)
{
    int volatile status = 0;
    int volatile page_count, block_count, page, addr, blockNum, total;
    int volatile i, j, pagetmp=0;
    unsigned int address = DOWNLOAD_BASE;
    page_count = len / pSM->uPageSize;
    if ((len % pSM->uPageSize) != 0)
        page_count++;

    // write into flash
    blockNum = blockNo;
    total = page_count;

    // define for USB ack
    addr = address | NON_CACHE;

    // erase needed blocks
    block_count = page_count / pSM->uPagePerBlock;

    for (j=0; j<block_count; j++)
    {
        // block erase
//_retry_1:
        page = pSM->uPagePerBlock * (blockNum);
        status = fmiSM_BlockErase(pSM, blockNum);
        if (status != 0)
        {
            fmiMarkBadBlock(pSM, blockNum);
            MSG_DEBUG("bad block = %d\n",blockNum);
            blockNum++;
            continue;
        }

        // write block
        for (i=0; i<pSM->uPagePerBlock; i++)
        {
            status = fmiSM_Write_large_page(page+i, 0, addr);
            if (status != 0)
            {
                fmiMarkBadBlock(pSM, blockNum);
                MSG_DEBUG("bad block = %d\n",blockNum);
                blockNum++;
                addr = (address + j * pSM->uPagePerBlock * pSM->uPageSize) | NON_CACHE;
                continue;
            }
            addr += pSM->uPageSize;
            pagetmp++;
        }
        blockNum++;
    }

    if ((page_count % pSM->uPagePerBlock) != 0)
    {
        page_count = page_count - block_count * pSM->uPagePerBlock;
        block_count++;
        // erase block
//_retry_2:
        page = pSM->uPagePerBlock * (blockNum);
        status = fmiSM_BlockErase(pSM, blockNum);
        if (status != 0)
        {
            fmiMarkBadBlock(pSM, blockNum);
            blockNum++;
            //goto _retry_2;
            return status;
        }

        // write block
        for (i=0; i<page_count; i++)
        {
            status = fmiSM_Write_large_page(page+i, 0, addr);
            if (status != 0)
            {
                fmiMarkBadBlock(pSM, blockNum);
                blockNum++;
                addr = (address + block_count * pSM->uPagePerBlock * pSM->uPageSize) | NON_CACHE;
                //goto _retry_2;
                return status;
            }
            addr += pSM->uPageSize;
            pagetmp++;
        }
        blockNum++;
    }
    return status;
}

void UXmodem_NAND()
{
    int i,offset=0, ret;
    unsigned char *ptr;
    unsigned char buf[80];
    unsigned char *_ch;
    unsigned int *_ack;
    unsigned int len;
    PACK_CHILD_HEAD ppack;
    unsigned int ddrlen;
    unsigned int total,offblk=0;

    MSG_DEBUG("download image to NAND flash...\n");
    /* TODO: initial NAND */
    fmiNandInit();
    MSG_DEBUG("g_uIsUserConfig=%d\n",g_uIsUserConfig);
    MSG_DEBUG("UXmodem_NAND BlockPerFlash=%d\n",pSM->uBlockPerFlash);
    MSG_DEBUG("UXmodem_NAND PagePerBlock=%d\n",pSM->uPagePerBlock);
    MSG_DEBUG("UXmodem_NAND PageSize=%d\n",pSM->uPageSize);
    memset((char *)&nandImage, 0, sizeof(FW_NAND_IMAGE_T));
    pNandImage = (FW_NAND_IMAGE_T *)&nandImage;

    _ch=((unsigned char*)(((unsigned int)buf)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)buf)|NON_CACHE));
    ptr=_ch;
    while(1)
    {
        if(Bulk_Out_Transfer_Size>0)
        {
            usb_recv(ptr,sizeof(FW_NAND_IMAGE_T));
            memcpy(pNandImage, (unsigned char *)ptr, sizeof(FW_NAND_IMAGE_T));
            break;
        }
    }
    MSG_DEBUG("Action flag: %s, blockNo 0x%x(%d) len 0x%x(%d)\n", au8ActionFlagName[pNandImage->actionFlag],pNandImage->blockNo, ,pNandImage->blockNo, pNandImage->fileLength, pNandImage->fileLength);

    switch (pNandImage->actionFlag)
    {
    case WRITER_MODE:   // normal write
    {
        MSG_DEBUG("NAND normal write !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x83;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }
        MSG_DEBUG("NAND normal write2 !!!\n");
        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;

        if (pNandImage->imageType == UBOOT)   // system image
        {
            pNandImage->imageNo = 0;
            pNandImage->blockNo = 0;
            addMagicHeader(pNandImage->executeAddr, pNandImage->fileLength);
            addNUC980MagicHeader(1);
            ptr += IBR_HEADER_LEN;// except the 32 bytes magic header
            offset = IBR_HEADER_LEN;
        }
        else
            offset = 0;

        len=pSM->uPagePerBlock*pSM->uPageSize;
        MSG_DEBUG("NAND normal Burn_NAND !!!\n");
        if (pNandImage->imageType == UBOOT)     // system image
        {
            BatchBurn_NAND_BOOT(pNandImage->fileLength + offset +((FW_NAND_IMAGE_T *)pNandImage)->initSize,0,4,1);
            MSG_DEBUG("pNandImage->fileLength = 0x%x\n", pNandImage->fileLength);
        }
        else
        {
            if(pNandImage->imageType!=IMAGE)
            {
                if(pNandImage->imageType == DATA_OOB)
                {
                    MSG_DEBUG("DATA_OOB type\n");
                    BatchBurn_NAND_Data_OOB(pNandImage->fileLength + offset +((FW_NAND_IMAGE_T *)pNandImage)->initSize,pNandImage->blockNo/(pSM->uPagePerBlock*pSM->uPageSize),pNandImage->imageType);
                }
                else
                {
                    BatchBurn_NAND(pNandImage->fileLength + offset +((FW_NAND_IMAGE_T *)pNandImage)->initSize,pNandImage->blockNo/(pSM->uPagePerBlock*pSM->uPageSize),pNandImage->imageType);
                }
            }
            else
            {
                MSG_DEBUG("_ch[0]=%c,_ch[1]=%c,_ch[2]=%c,_ch[3]=%c\n",(char)_ch[0],(char)_ch[1],(char)_ch[2],(char)_ch[3]);
                if(((char)_ch[0])=='U' && ((char)_ch[1])=='B' && ((char)_ch[2])=='I')
                {
                    BatchBurn_NAND(pNandImage->fileLength + offset +((FW_NAND_IMAGE_T *)pNandImage)->initSize,pNandImage->blockNo/(pSM->uPagePerBlock*pSM->uPageSize),UBIFS);
                }
                else
                {
                    BatchBurn_NAND(pNandImage->fileLength + offset +((FW_NAND_IMAGE_T *)pNandImage)->initSize,pNandImage->blockNo/(pSM->uPagePerBlock*pSM->uPageSize),YAFFS2);
                }
            }
        }
    }
    break;

    case MODIFY_MODE:   // modify
    {
        MSG_DEBUG("NAND modify !!!\n");
    }
    break;

    case ERASE_MODE:    // erase
    {
        MSG_DEBUG("NAND erase !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x83;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        MSG_DEBUG("\nstart=%d\n",pNandImage->blockNo); //start block
        MSG_DEBUG("length=%d\n",pNandImage->executeAddr); //block length
        MSG_DEBUG("type=%d\n",pNandImage->imageType); //0: chip erase, 1: erase accord start and length blocks.
        MSG_DEBUG("imageNo=0x%08x\n",pNandImage->imageNo);

        if(pNandImage->imageType==0)   //chip erase
        {
            if (pNandImage->imageNo == 0xFFFFFFFF)
            {
                int bb;
                bb = fmiSM_ChipErase(0);
                if (bb < 0)
                {
                    MSG_DEBUG("ERROR: %d bad block\n", bb); // storage error
                    *_ack=0xffff;
                    usb_send((unsigned char*)_ack,4);//send ack to PC
                }
                else
                    MSG_DEBUG("total %d bad block\n", bb);
            }
            else
            {
                fmiSM_ChipEraseBad(0);
            }
        }
        else
        {
            if (pNandImage->imageNo == 0xFFFFFFFF)
            {
                int bb;
                bb = fmiSM_Erase(0,pNandImage->blockNo,pNandImage->executeAddr);
                printf("total %d bad block\n", bb);
            }
            else
            {
                MSG_DEBUG("fmiSM_EraseBad\n");
                fmiSM_EraseBad(0,pNandImage->blockNo,pNandImage->executeAddr);
            }
        }
    }
    break;

    case VERIFY_MODE:   // verify
    {
        MSG_DEBUG("NAND normal verify !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x83;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }
        {
            if (pNandImage->imageType == UBOOT)     // system image
            {
                int offblk=0;
                _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
                ptr=_ch;
                while(fmiCheckInvalidBlock(pSM, pNandImage->blockNo/(pSM->uPagePerBlock*pSM->uPageSize)+offblk) == 1)
                {
                    offblk++;
                }
                Read_Nand(DOWNLOAD_BASE,pNandImage->blockNo/(pSM->uPagePerBlock*pSM->uPageSize)+offblk,pNandImage->fileLength+IBR_HEADER_LEN+pNandImage->initSize);
                memmove(_ch,_ch+IBR_HEADER_LEN+pNandImage->initSize,pNandImage->fileLength);
                do
                {
                    usb_send(ptr,TRANSFER_LEN); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4); //recv data from PC
                    ptr += (*_ack);
                }
                while((ptr-_ch)<(pNandImage->fileLength));
            }
            else
            {
                int total,offblk=0;
                total=pNandImage->fileLength;
                do
                {
                    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
                    ptr=_ch;
                    len=MIN(total,pSM->uPagePerBlock*pSM->uPageSize);
                    memset(ptr, 0xff, pSM->uPagePerBlock*pSM->uPageSize);
                    while(fmiCheckInvalidBlock(pSM, pNandImage->blockNo/(pSM->uPagePerBlock*pSM->uPageSize)+offblk) == 1)
                    {
                        offblk++;
                    }
                    Read_Nand(DOWNLOAD_BASE,pNandImage->blockNo/(pSM->uPagePerBlock*pSM->uPageSize)+offblk,len);
                    MSG_DEBUG("Read_NAND offblk=%d,len=%d\n",offblk,len);
                    do
                    {
                        usb_send(ptr,TRANSFER_LEN); //send data to PC
                        while(Bulk_Out_Transfer_Size==0) {}
                        usb_recv((unsigned char*)_ack,4); //recv data from PC
                        ptr += (*_ack);
                        MSG_DEBUG("read size=0x%08x\n",(unsigned int)(ptr-_ch));
                    }
                    while((int)(ptr-_ch)<len);
                    total-=len;
                    offblk+=1;
                    MSG_DEBUG("total=%d len=%d\n",total,len);
                }
                while(total!=0);
            }
        }
    }
    break;
    case PACK_VERIFY_MODE: // verify
    {
        MSG_DEBUG("\n NAND PACK verify !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x83;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }
        //printf("PACK_VERIFY_MODE pNandImage->imageNo= %d\n", pNandImage->imageNo);
        for(i=0; i<pNandImage->imageNo; i++)
        {
            while(1)
            {
                if(Bulk_Out_Transfer_Size>=sizeof(PACK_CHILD_HEAD))
                {
                    usb_recv(ptr,sizeof(PACK_CHILD_HEAD));
                    memcpy(&ppack,(unsigned char *)ptr,sizeof(PACK_CHILD_HEAD));
                    usleep(1000);
                    *_ack=0x83;
                    usb_send((unsigned char*)_ack,4);//send ack to PC
                    break;
                }
            }

            if (ppack.imagetype == UBOOT)     // system image
            {
                int offblk=0;
                int rawfilelen = 0;
                _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
                ptr=_ch;
                while(fmiCheckInvalidBlock(pSM, pNandImage->blockNo/(pSM->uPagePerBlock*pSM->uPageSize)+offblk) == 1)
                {
                    offblk++;
                }
                // Get DDR parameter length
                usb_recv(ptr,4);
                memcpy(&ddrlen,(unsigned char *)ptr,4);
                MSG_DEBUG("ddrlen = 0x%x(%d)\n", ddrlen, ddrlen);
                //usleep(1000);
                *_ack=0x83;
                usb_send((unsigned char*)_ack,4);//send ack to PC
                Read_Nand(DOWNLOAD_BASE,offblk,ppack.filelen+IBR_HEADER_LEN+ppack.startaddr);
                memmove(_ch,_ch+IBR_HEADER_LEN+ddrlen+ppack.startaddr,ppack.filelen);
                rawfilelen = ppack.filelen - (ddrlen+IBR_HEADER_LEN+ppack.startaddr);
                do
                {
                    usb_send(ptr,TRANSFER_LEN); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4); //recv data from PC
                    ptr += (*_ack);
                }
                while((ptr-_ch)<(rawfilelen));

            }
            else
            {
                int total,offblk=0;
                total=ppack.filelen;
                pNandImage->blockNo = ppack.startaddr;
                do
                {
                    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
                    ptr=_ch;
                    len=MIN(total,pSM->uPagePerBlock*pSM->uPageSize);
                    memset(ptr, 0xff, pSM->uPagePerBlock*pSM->uPageSize);
                    while(fmiCheckInvalidBlock(pSM, pNandImage->blockNo/(pSM->uPagePerBlock*pSM->uPageSize)+offblk) == 1)
                    {
                        offblk++;
                    }
                    Read_Nand(DOWNLOAD_BASE,pNandImage->blockNo/(pSM->uPagePerBlock*pSM->uPageSize)+offblk,len);
                    MSG_DEBUG("Read_NAND offblk=%d,len=%d\n",offblk,len);
                    do
                    {
                        usb_send(ptr,TRANSFER_LEN); //send data to PC
                        while(Bulk_Out_Transfer_Size==0) {}
                        usb_recv((unsigned char*)_ack,4); //recv data from PC
                        ptr += (*_ack);
                        MSG_DEBUG("read size=0x%08x\n",(unsigned int)(ptr-_ch));
                    }
                    while((int)(ptr-_ch)<len);
                    total-=len;
                    offblk+=1;
                    MSG_DEBUG("total=%d len=%d\n",total,len);
                }
                while(total!=0);
            }
        }
    }
    break;
    case READ_MODE:   // read
    {
        MSG_DEBUG("NAND normal read !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x83;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        MSG_DEBUG("offset=%d,blockNo=%d,fileLength=%d\n",offset,pNandImage->blockNo,pNandImage->fileLength);

        total=pNandImage->fileLength;
        do
        {
            _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
            ptr=_ch;
            if(pNandImage->initSize==0)   //0: read good block , others : read redundancy area,good block and bad block
            {
                len=MIN(total,pSM->uPagePerBlock*pSM->uPageSize);
                memset(ptr, 0, len);
                while(fmiCheckInvalidBlock(pSM, pNandImage->blockNo+offblk) == 1)
                {
                    offblk++;
                }
                Read_Nand(DOWNLOAD_BASE,pNandImage->blockNo+offblk,len);
                do
                {
                    usb_send(ptr,4096); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4); //recv data from PC
                    ptr += (*_ack);
                }
                while((ptr-_ch)<len);
                total-=len;
                offblk+=1;
            }
            else
            {
                len=MIN(total,pSM->uPagePerBlock*(pSM->uPageSize+pSM->uSpareSize));
                memset(ptr, 0, len);
                Read_Nand_Redunancy(DOWNLOAD_BASE,pNandImage->blockNo+offblk,len);
                do
                {
                    usb_send(ptr,pSM->uPageSize); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4); //recv data from PC
                    ptr += (*_ack);
                    usb_send(ptr,pSM->uSpareSize); //send redundancy data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4); //recv redundancy data from PC
                    ptr += (*_ack);
                }
                while((ptr-_ch)<len);
                total-=(len);
                offblk+=1;
            }
        }
        while(total!=0);
    }
    break;
    case PACK_MODE:
    {
        MSG_DEBUG("NAND pack mode !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x83;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;
        {
            PACK_HEAD pack;
            PACK_CHILD_HEAD ppack;
            while(1)
            {
                if(Bulk_Out_Transfer_Size>=sizeof(PACK_HEAD))
                {
                    usb_recv(ptr,sizeof(PACK_HEAD));
                    memcpy(&pack,(unsigned char *)ptr,sizeof(PACK_HEAD));
                    *_ack=sizeof(PACK_HEAD);
                    usb_send((unsigned char*)_ack,4);//send ack to PC
                    break;
                }
            }
            MSG_DEBUG("pack.actionFlag=0x%x, pack.fileLength=0x%08x pack.num=%d!!!\n",pack.actionFlag,pack.fileLength,pack.num);
            for(i=0; i<pack.num; i++)
            {
                while(1)
                {
                    if(Bulk_Out_Transfer_Size>=sizeof(PACK_CHILD_HEAD))
                    {
                        usb_recv(ptr,sizeof(PACK_CHILD_HEAD));
                        memcpy(&ppack,(unsigned char *)ptr,sizeof(PACK_CHILD_HEAD));
                        *_ack=sizeof(PACK_CHILD_HEAD);
                        usb_send((unsigned char*)_ack,4);//send ack to PC
                        break;
                    }
                }
                MSG_DEBUG("%d ppack.filelen=0x%x, ppack.startaddr=0x%08x!!!\n",i,ppack.filelen,ppack.startaddr);

                if(ppack.imagetype==UBOOT)
                {
                    ret = BatchBurn_NAND_BOOT(ppack.filelen,0,4,0);
                    if(ret == -1)
                    {
                        printf("Nand Device image error !!! \n");
                        return;
                    }
                }
                else
                {
                    if(ppack.imagetype!=IMAGE)
                    {
                        BatchBurn_NAND(ppack.filelen,ppack.startaddr/(pSM->uPageSize*pSM->uPagePerBlock),pNandImage->imageType);
                    }
                    else
                    {
                        MSG_DEBUG("_ch[0]=%c,_ch[1]=%c,_ch[2]=%c,_ch[3]=%c\n",(char)_ch[0],(char)_ch[1],(char)_ch[2],(char)_ch[3]);
                        if(((char)_ch[0])=='U' && ((char)_ch[1])=='B' && ((char)_ch[2])=='I')
                        {
                            ret = BatchBurn_NAND(ppack.filelen,ppack.startaddr/(pSM->uPageSize*pSM->uPagePerBlock),UBIFS);
                            if(ret == -1)
                            {
                                MSG_DEBUG("BatchBurn_NAND Device image error !!! \n");
                                return;
                            }
                        }
                        else
                        {
                            BatchBurn_NAND(ppack.filelen,ppack.startaddr/(pSM->uPageSize*pSM->uPagePerBlock),YAFFS2);
                        }
                    }
                }
            }
        }
    }
    break;
    default:
        ;
        break;
    }
    MSG_DEBUG("finish NAND image download\n");
}

/******************************************************************************
 *
 *  SPI NAND Functions
 *
 ******************************************************************************/
FW_SPINAND_IMAGE_T spinandImage;
FW_SPINAND_IMAGE_T *pspiNandImage;
//unsigned char *program_buffer;
unsigned char *read_buffer;

int BatchBurn_SPINAND(UINT32 len,UINT32 blockStartIdx,UINT32 type)
{
    unsigned int volatile Blk_Idx, Blk_Num;  //Blk_Idx: writing index, Blk_Num : how many need to write
    int volatile status = 0;
    int volatile page_count, block_count, page, addr;
    int volatile i, j;
    unsigned int address = DOWNLOAD_BASE;
    unsigned int sparesize;  //Put yaffs2 tag on the oob(sparesize).
    unsigned char *_ch,*ptr;
    unsigned int *_ack,ack=0;
    unsigned int reclen,remainlen, block_idx, blockNum, total;
    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)ack)|NON_CACHE));
    ptr=_ch;

    sparesize=0;

    page_count = len / (pSN->SPINand_PageSize+sparesize);
    if(len%(pSN->SPINand_PageSize+sparesize)!=0) page_count++;
    // erase needed blocks
    block_count = page_count / (pSN->SPINand_PagePerBlock);

    Blk_Num = page_count / (pSN->SPINand_PagePerBlock);
    Blk_Idx = blockStartIdx;

    // write into flash
    total = len;
    MSG_DEBUG(">>>>>> Blk_Idx=%d total=%d Blk_Num=%d page_count =%d\n",Blk_Idx,total, Blk_Num, page_count);
    for (j=0; j<Blk_Num; j++)
    {
        MSG_DEBUG("======= j=%d, Blk_Num=%d\n", j, Blk_Num);
        ptr=_ch;
        remainlen=MIN(total,pSN->SPINand_PagePerBlock*(pSN->SPINand_PageSize+sparesize));
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                MSG_DEBUG("total=%d,remainlen=%d\n",total,remainlen);
                reclen=MIN(TRANSFER_LEN,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                total-=reclen;
                *_ack=reclen;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);

        MSG_DEBUG(">>>>>> remainlen OK\n");

_retry_1:
        // Multi die , 2 X 1G Bit
        if(pSN->SPINand_IsDieSelect == 1)
        {
            spiNAND_Die_Select(Blk_Idx/pSN->SPINand_BlockPerFlash);
            MSG_DEBUG("ID%d  Blk_Idx=%d\n", Blk_Idx/pSN->SPINand_BlockPerFlash, Blk_Idx%pSN->SPINand_BlockPerFlash);
        }

        block_idx = (Blk_Idx % pSN->SPINand_BlockPerFlash)*pSN->SPINand_PagePerBlock;
        addr = address | NON_CACHE;
        page = pSN->SPINand_PagePerBlock * (Blk_Idx % pSN->SPINand_BlockPerFlash);
        MSG_DEBUG("fmiSM_BlockErase page = %d   blockNum = %d  j=%d  shiftBlockNumStart =0x%x\n", page, blockNum, j, shiftBlockNumStart);

        if(spiNAND_bad_block_check(page) == 1)
        {
            printf("bad block = %d\n", (Blk_Idx % pSN->SPINand_BlockPerFlash));
            Blk_Idx++;
            goto _retry_1;
        }
        else
        {
            spiNAND_BlockErase(((block_idx>>8)&0xFF), (block_idx&0xFF)); // block erase
            status = spiNAND_Check_Program_Erase_Fail_Flag();
            if (status == 1)
            {
                printf("Error erase status! spiNANDMarkBadBlock blockNum = %d\n",  (Blk_Idx % pSN->SPINand_BlockPerFlash));
                spiNANDMarkBadBlock( (Blk_Idx % pSN->SPINand_BlockPerFlash)*pSN->SPINand_PagePerBlock);                
                Blk_Idx++;
                goto _retry_1;
            }
        }

        MSG_DEBUG("#2294 pSN->SPINand_PagePerBlock=%08d\n",pSN->SPINand_PagePerBlock);
        // write block
        for (i=0; i<pSN->SPINand_PagePerBlock; i++)
        {
            MSG_DEBUG("#2297 Blk_Idx=%d,  page+i=%d\n", (Blk_Idx % pSN->SPINand_BlockPerFlash), page+i);
            spiNAND_Pageprogram_Pattern(0, 0, (uint8_t*)addr, pSN->SPINand_PageSize);
            spiNAND_Program_Excute((((page+i)>>8)&0xFF), (page+i)&0xFF);
            status = (spiNAND_StatusRegister(3) & 0x0C)>>2;
            if (status == 1)
            {
                spiNANDMarkBadBlock( (Blk_Idx % pSN->SPINand_BlockPerFlash)*pSN->SPINand_PagePerBlock);
                printf("Error write status! spiNANDMarkBadBlock blockNum = %d\n",  (Blk_Idx % pSN->SPINand_BlockPerFlash));
                Blk_Idx++;
                addr+=(pSN->SPINand_PageSize* pSN->SPINand_PagePerBlock);
                goto _retry_1;
            }
            addr += pSN->SPINand_PageSize;
        }
        Blk_Idx++;
    }

    MSG_DEBUG("page_count=%d,pSM->uPagePerBlock=%d, blockNum=%d\n",page_count,pSN->SPINand_PagePerBlock, blockNum);
    if ((page_count % pSN->SPINand_PagePerBlock) != 0)
    {
        MSG_DEBUG("Cnt=%d\n",page_count % pSN->SPINand_PagePerBlock);
        memset(_ch,0xff,pSN->SPINand_PagePerBlock*(pSN->SPINand_PageSize+sparesize));
        ptr=_ch;
        remainlen=total;
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                MSG_DEBUG("remainlen=%d\n",remainlen);
                reclen=MIN(TRANSFER_LEN,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                total-=reclen;
                *_ack=reclen;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);
        page_count = page_count - (Blk_Num *  pSN->SPINand_PagePerBlock);
        MSG_DEBUG("page_count=%d\n",page_count);
_retry_2:
        // Multi die , 2 X 1G Bit
        if(pSN->SPINand_IsDieSelect == 1)
        {
            spiNAND_Die_Select(Blk_Idx/pSN->SPINand_BlockPerFlash);
            MSG_DEBUG("ID%d  Blk_Idx=%d\n", Blk_Idx/pSN->SPINand_BlockPerFlash, Blk_Idx%pSN->SPINand_BlockPerFlash);
        }

        addr = address | NON_CACHE;
        page =  pSN->SPINand_PagePerBlock * (Blk_Idx % pSN->SPINand_BlockPerFlash);
        if(spiNAND_bad_block_check(page) == 1)
        {
            printf("bad_block:%d\n", (Blk_Idx % pSN->SPINand_BlockPerFlash));
            Blk_Idx++;
            goto _retry_2;
        }
        else
        {
            spiNAND_BlockErase( ((page>>8)&0xFF), (page&0xFF));
            status = spiNAND_Check_Program_Erase_Fail_Flag();
            if (status == 1)
            {
                spiNANDMarkBadBlock((Blk_Idx % pSN->SPINand_BlockPerFlash)*pSN->SPINand_PagePerBlock);
                printf("Error erase status! bad_block:%d\n", (Blk_Idx % pSN->SPINand_BlockPerFlash));
                Blk_Idx++;
                goto _retry_2;
            }

            // write block
            MSG_DEBUG("page_count=%d,type=%d\n",page_count,type);
            for (i=0; i<page_count; i++)
            {
                MSG_DEBUG("Blk_Idx=%d,  page+i=%d\n",(Blk_Idx % pSN->SPINand_BlockPerFlash), page+i);
                spiNAND_Pageprogram_Pattern(0, 0, (uint8_t*)addr, pSN->SPINand_PageSize);
                spiNAND_Program_Excute((((page+i)>>8)&0xFF), ((page+i)&0xFF));
                status = (spiNAND_StatusRegister(3) & 0x0C)>>2;
                if (status != 0)
                {
                    spiNANDMarkBadBlock((Blk_Idx % pSN->SPINand_BlockPerFlash)*pSN->SPINand_PagePerBlock);
                    printf("Error write status! spiNANDMarkBadBlock (Blk_Idx mod pSN->SPINand_BlockPerFlash) = %d\n", (Blk_Idx % pSN->SPINand_BlockPerFlash));
                    Blk_Idx++;
                    addr+=(pSN->SPINand_PageSize* pSN->SPINand_PagePerBlock);
                    goto _retry_2;
                }
                addr += pSN->SPINand_PageSize;
            }
            Blk_Idx++;
        }
    }

    MSG_DEBUG("BatchBurn_SPINAND() End Blk_Idx-blockStartIdx=%d\n", (Blk_Idx % pSN->SPINand_BlockPerFlash)-blockStartIdx);
    SendAck((Blk_Idx % pSN->SPINand_BlockPerFlash)-blockStartIdx);
    return status;
}

//To Do: Multi-Die
int BatchBurn_SPINAND_Data_OOB(UINT32 len,UINT32 blockStartIdx,UINT32 type)
{
    int volatile status = 0;
    int volatile page_count, block_count, page, addr;
    int volatile i, j;
    unsigned int address = DOWNLOAD_BASE;
    unsigned char *_ch,*ptr;
    unsigned int *_ack,ack=0;
    unsigned int reclen,remainlen, block_idx, blockNum, total;
    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)ack)|NON_CACHE));
    ptr=_ch;

    page_count = len / (pSN->SPINand_PageSize+pSN->SPINand_SpareArea);
    if(len%(pSN->SPINand_PageSize+pSN->SPINand_SpareArea)!=0) page_count++;
    // erase needed blocks
    block_count = page_count / (pSN->SPINand_PagePerBlock);

    // write into flash
    blockNum = blockStartIdx;
    total = len;
    MSG_DEBUG(">>>>>> blockNum=%d    total=%d    block_count=%d   sparesize =%d\n",blockNum,total, block_count, pSN->SPINand_SpareArea);
    for (j=0; j<block_count; j++)
    {
        ptr=_ch;
        remainlen=MIN(total,pSN->SPINand_PagePerBlock*(pSN->SPINand_PageSize+pSN->SPINand_SpareArea));
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                MSG_DEBUG("total=%d,remainlen=%d\n",total,remainlen);
                reclen=MIN(TRANSFER_LEN,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                total-=reclen;
                *_ack=reclen;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);

        MSG_DEBUG(">>>>>> remainlen OK\n");

_retry_1:
        block_idx = (blockNum+j)*pSN->SPINand_PagePerBlock;
        addr = address | NON_CACHE;
        page = pSN->SPINand_PagePerBlock * blockNum;
        MSG_DEBUG("fmiSM_BlockErase page = %d   blockNum = %d   addr =0x%x\n", page, blockNum, addr);

        if(spiNAND_bad_block_check(page) == 1)
        {
            MSG_DEBUG("bad block = %d\n", blockNum);
            blockNum++;
            goto _retry_1;
        }
        else
        {
            spiNAND_BlockErase(((block_idx>>8)&0xFF), (block_idx&0xFF)); // block erase
            status = spiNAND_Check_Program_Erase_Fail_Flag();
            if (status == 1)
            {
                MSG_DEBUG("Error erase status! blockNum = %d\n", blockNum);
                spiNANDMarkBadBlock(blockNum*pSN->SPINand_PagePerBlock);
                blockNum++;
                goto _retry_1;
            }
        }

        MSG_DEBUG("pSN->SPINand_PagePerBlock=%08d\n",pSN->SPINand_PagePerBlock);
        // write block
        for (i=0; i<pSN->SPINand_PagePerBlock; i++)
        {
            MSG_DEBUG("blockNum+j=%d,  page+i=%d\n",blockNum, page+i);
            spiNAND_Pageprogram_Pattern(0, 0, (uint8_t*)addr, (pSN->SPINand_PageSize+pSN->SPINand_SpareArea));
            spiNAND_Program_Excute((((page+i)>>8)&0xFF), (page+i)&0xFF);
            status = (spiNAND_StatusRegister(3) & 0x0C)>>2;
            if (status == 1)
            {
                spiNANDMarkBadBlock(blockNum*pSN->SPINand_PagePerBlock);
                MSG_DEBUG("Error write status! bad_block:%d\n", blockNum);
                blockNum++;
                addr+=((pSN->SPINand_PageSize+pSN->SPINand_SpareArea) * pSN->SPINand_PagePerBlock);
                goto _retry_1;
            }
            addr += (pSN->SPINand_PageSize+pSN->SPINand_SpareArea);
        }
        blockNum++;
    }

    MSG_DEBUG("page_count=%d,pSM->uPagePerBlock=%d, blockNum=%d\n",page_count,pSN->SPINand_PagePerBlock, blockNum);
    if ((page_count % pSN->SPINand_PagePerBlock) != 0)
    {
        MSG_DEBUG("Cnt=%d\n",page_count % pSN->SPINand_PagePerBlock);
        memset(_ch,0xff,pSN->SPINand_PagePerBlock*(pSN->SPINand_PageSize+pSN->SPINand_SpareArea));
        ptr=_ch;
        remainlen=total;
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                MSG_DEBUG("remainlen=%d\n",remainlen);
                reclen=MIN(TRANSFER_LEN,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                total-=reclen;
                *_ack=reclen;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);
        page_count = page_count - (block_count *  pSN->SPINand_PagePerBlock);
        MSG_DEBUG("page_count=%d\n",page_count);
_retry_2:
        addr = address | NON_CACHE;
        page =  pSN->SPINand_PagePerBlock * (blockNum);

        if(spiNAND_bad_block_check(page) == 1)
        {
            MSG_DEBUG("bad_block:%d\n", blockNum);
            blockNum++;
            goto _retry_2;
        }
        else
        {
            spiNAND_BlockErase( ((page>>8)&0xFF), (page&0xFF));
            status = spiNAND_Check_Program_Erase_Fail_Flag();
            if (status == 1)
            {
                spiNANDMarkBadBlock(blockNum*pSN->SPINand_PagePerBlock);
                MSG_DEBUG("Error erase status! bad_block:%d\n", blockNum);
                blockNum++;
                goto _retry_2;
            }

            // write block
            MSG_DEBUG("page_count=%d,type=%d\n",page_count,type);
            for (i=0; i<page_count; i++)
            {
                MSG_DEBUG("page+i=%d\n",page+i);
                spiNAND_Pageprogram_Pattern(0, 0, (uint8_t*)addr, (pSN->SPINand_PageSize+pSN->SPINand_SpareArea));
                spiNAND_Program_Excute((((page+i)>>8)&0xFF), ((page+i)&0xFF));
                status = (spiNAND_StatusRegister(3) & 0x0C)>>2;
                if (status != 0)
                {
                    spiNANDMarkBadBlock(blockNum*pSN->SPINand_PagePerBlock);
                    MSG_DEBUG("Error write status! bad_block:%d\n", blockNum);
                    blockNum++;
                    addr+=((pSN->SPINand_PageSize+pSN->SPINand_SpareArea)* pSN->SPINand_PagePerBlock);
                    goto _retry_2;
                }
                addr += (pSN->SPINand_PageSize+pSN->SPINand_SpareArea);
            }
            blockNum++;
        }
    }

    MSG_DEBUG("BatchBurn_SPINAND_Data_OOB() End blockNum-blockNo=%d\n", blockNum-blockStartIdx);
    SendAck(blockNum-blockStartIdx);
    return status;
}

int BatchBurn_SPINAND_BOOT(UINT32 len,UINT32 blockStartIdx,UINT32 blockLen,UINT32 HeaderFlag)
{
    int volatile status = 0;
    int volatile page_count, block_count, page, addr, blockNum, total;
    int volatile blkindx,i, j, pagetmp=0;
    unsigned int address = DOWNLOAD_BASE;
    unsigned char *_ch,*ptr;
    unsigned int *_ack,ack=0;
    unsigned int reclen,remainlen;
    unsigned int headlen;

    if(HeaderFlag==1)
        headlen=IBR_HEADER_LEN;
    else
        headlen=0;

    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)ack)|NON_CACHE));
    ptr=_ch;
    page_count = len / pSN->SPINand_PageSize;
    if ((len % pSN->SPINand_PageSize) != 0)
        page_count++;

    // write into flash
    blockNum = blockStartIdx;
    total = page_count*blockLen;

    // erase needed blocks
    block_count = page_count / (pSN->SPINand_PagePerBlock);
    MSG_DEBUG(">>>>>> len=%d  blockLen=%d  blockNum=%d    total=%d    block_count=%d  page_count =%d\n",len, blockLen, blockNum, total, block_count, page_count);

    if (page_count <= pSN->SPINand_PagePerBlock)
    {
        memset(_ch+headlen,0xff,(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize));
        ptr=_ch+headlen;
        remainlen=len-(block_count*pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize)-headlen;
        MSG_DEBUG("remainlen=%d,block_count=%d\n",remainlen,block_count);
        do
        {
            if(Bulk_Out_Transfer_Size>0)
            {
                reclen=MIN(Bulk_Out_Transfer_Size,remainlen);
                usb_recv(ptr,reclen);  //recv data from PC
                ptr+=reclen;
                remainlen-=reclen;
                *_ack=reclen|((pagetmp * 95) / total)<<16;
                usb_send((unsigned char*)_ack,4);//send ack to PC
            }
        }
        while(remainlen!=0);
//_retry_2:
        // erase block
        for(blkindx=blockNum; blkindx<blockLen; blkindx++)
        {
            addr = address | NON_CACHE;
            page = pSN->SPINand_PagePerBlock * (blkindx);

            if(spiNAND_bad_block_check(page) == 1)
            {
                printf("bad_block:%d\n", blkindx);
                //blockNum++;
                //goto _retry_2;
                continue;
            }
            else
            {
                MSG_DEBUG("blkindx = %d   page = %d   page_count=%d\n", blkindx, page, page_count);
                spiNAND_BlockErase(((page>>8)&0xFF), (page&0xFF)); // block erase
                status = spiNAND_Check_Program_Erase_Fail_Flag();
                if (status != 0)
                {
                    printf("Error erase status! bad_block:%d\n", blkindx);
                    spiNANDMarkBadBlock(page);
                    //blockNum++;
                    //goto _retry_2;
                    continue;
                }

                // write block
                for (i=0; i<page_count; i++)
                {
                    MSG_DEBUG("page+i=%d\n",page+i);
                    spiNAND_Pageprogram_Pattern(0, 0, (uint8_t*)addr, pSN->SPINand_PageSize);
                    spiNAND_Program_Excute((((page+i)>>8)&0xFF), (page+i)&0xFF);
                    status = (spiNAND_StatusRegister(3) & 0x0C)>>2;
                    if (status == 1)
                    {
                        printf("Error write status! bad_block:%d\n", blockNum);
                        spiNANDMarkBadBlock(page);
                        //blockNum++;
                        //addr = (address + block_count * pSN->SPINand_PagePerBlock * pSN->SPINand_PageSize) | NON_CACHE;
                        //addr+=(pSN->SPINand_PageSize* pSN->SPINand_PagePerBlock);
                        //goto _retry_2;
                        continue;
                    }
                    addr += pSN->SPINand_PageSize;
                    pagetmp++;
                }
            }
            //blockNum++;
        }
    }
    else
    {
        printf("Error Device image Error !!!\n");
        usb_recv(ptr, Bulk_Out_Transfer_Size);  //recv data from PC
        *_ack = 0xFFFF;
        usb_send((unsigned char*)_ack,4);//send ack to PC
        return -1;
    }
    SendAck(block_count+blockLen);
    MSG_DEBUG("SendAck 0x%x\n", block_count+blockLen);
    return status;
}

int Read_SPINand(UINT32 dst_adr,UINT32 blockStartIdx, UINT32 len)
{
    int volatile status = 0;
    int volatile page_count, blockNum;
    int volatile page_no, total, i=0, j=0, k;
    unsigned int block_count;

    MSG_DEBUG("Read dst_adr =0x%x  len =%d, blockStartIdx=%d\n", dst_adr, len, blockStartIdx);
    block_count = len / (pSN->SPINand_PageSize*pSN->SPINand_PagePerBlock);
    page_count = len / pSN->SPINand_PageSize;
    if ((len % (pSN->SPINand_PageSize*pSN->SPINand_PagePerBlock)) != 0) block_count++;
    if ((len % pSN->SPINand_PageSize) != 0) page_count++;

    blockNum = blockStartIdx;
    total = len;
    while(1)
    {
_retry_:
        for (i=0; i<pSN->SPINand_PagePerBlock; i++)
        {
            page_no = (blockNum * pSN->SPINand_PagePerBlock) + i;
            MSG_DEBUG("blockNum =%d  page_no = %d   total=%d  pageSize=%d\n", blockNum, page_no, total, pSN->SPINand_PageSize);
            spiNAND_PageDataRead((page_no>>8)&0xFF, (page_no&0xFF));// Read verify
            //spiNAND_QuadIO_Read(0, 0, (uint8_t*)dst_adr, pSN->SPINand_PageSize);
            spiNAND_Normal_Read(0, 0, (uint8_t*)dst_adr, pSN->SPINand_PageSize);
            status = spiNAND_Check_Embedded_ECC_Flag();
            if(status != 0x00 && status != 0x01)
            {
                //spiNANDMarkBadBlock(page_no);
                printf("Error ECC status error[0x%x].\n", status);// Check ECC status and return fail if (ECC-1, ECC0) != (0,0) or != (0,1)
                blockNum++;
                total = len;
                goto _retry_;
            }
            dst_adr += pSN->SPINand_PageSize;
            total -= pSN->SPINand_PageSize;
        }
        j++;
        blockNum++;
        if ((j >= block_count) || (total <= 0))
            break;
    }

    MSG_DEBUG("End Read_SPINand  total = %d\n", total);
    return 0;
}

//To Do: Multi-Die
int Read_SPINand_Redunancy(UINT32 dst_adr,UINT32 blockStartIdx, UINT32 len)
{
    int volatile status = 0;
    int volatile page_count, blockNum;
    int volatile page_no, total, i=0, j=0, k;
    unsigned int block_count;
    int spareSize;

    MSG_DEBUG("Read_SPINand_Redunancy %d len =%d, blockNo=%d    %d\n", pSN->SPINand_PageSize+spareSize, len, blockNo, ((pSN->SPINand_PageSize+spareSize)*pSN->SPINand_PagePerBlock));
    spareSize = pSN->SPINand_SpareArea;
    block_count = len / ((pSN->SPINand_PageSize+spareSize)*pSN->SPINand_PagePerBlock);
    page_count = len / (pSN->SPINand_PageSize+spareSize);
    if ((len % ((pSN->SPINand_PageSize+spareSize)*pSN->SPINand_PagePerBlock)) != 0) block_count++;
    blockNum = blockStartIdx;
    total = len;

    while(1)
    {
_retry_:
        for (i=0; i<pSN->SPINand_PagePerBlock; i++)
        {
            page_no = (blockNum * pSN->SPINand_PagePerBlock) + i;
            MSG_DEBUG("blockNum =%d  page_no = %d   total=%d  pageSize=%d\n", blockNum, page_no, total, pSN->SPINand_PageSize+64);
            spiNAND_PageDataRead((page_no>>8)&0xFF, (page_no&0xFF));// Read verify
            spiNAND_Normal_Read(0, 0, (uint8_t*)dst_adr, pSN->SPINand_PageSize+spareSize);
            status = spiNAND_Check_Embedded_ECC_Flag();
            if(status != 0x00 && status != 0x01)
            {
                MSG_DEBUG("Error ECC status error[0x%x].\n", status);// Check ECC status and return fail if (ECC-1, ECC0) != (0,0) or != (0,1)
                blockNum++;
                total = len;
                goto _retry_;
            }
            dst_adr += (pSN->SPINand_PageSize+spareSize);
            total -= (pSN->SPINand_PageSize-spareSize);
        }
        j++;
        blockNum++;
        if ((j >= block_count) || (total <= 0))
            break;
    }

    MSG_DEBUG("End Read_SPINand_Redunancy  total = %d\n", total);
    return 0;
}

void UXmodem_SPINAND()
{
    int i,offset=0, ret;
    unsigned char *ptr;
    unsigned char buf[80], u8selectid;
    unsigned char *_ch;
    unsigned int *_ack, tmpBlockPerFlash;
    unsigned int len;
    PACK_CHILD_HEAD ppack;
    unsigned int volatile blockNum, total, block_cnt, totalblock;
    unsigned int volatile PA_Num, badBlock = 0;
    unsigned int ddrlen;
    unsigned int volatile offblk=0;
    unsigned int volatile page_count, Blk_Idx = 0;
    uint32_t shiftBlockNumStart = 0, shiftBlockNumEnd, tmpBlockNum= 0, totalBlockNum;

    MSG_DEBUG("\n SPI NAND flash...\n");
    /* Initial SPI NAND */
    spiNANDInit();

    MSG_DEBUG("pSN->SPINand_PageSize=%d\n",pSN->SPINand_PageSize);
    MSG_DEBUG("pSN->SPINand_PagePerBlock=%d\n",pSN->SPINand_PagePerBlock);

    memset((char *)&spinandImage, 0, sizeof(FW_SPINAND_IMAGE_T));
    pspiNandImage = (FW_SPINAND_IMAGE_T *)&spinandImage;

    _ch=((unsigned char*)(((unsigned int)buf)|NON_CACHE));
    _ack=((unsigned int*)(((unsigned int)buf)|NON_CACHE));
    ptr=_ch;
    while(1)
    {
        if(Bulk_Out_Transfer_Size>0)
        {
            usb_recv(ptr,sizeof(FW_SPINAND_IMAGE_T));
            memcpy(pspiNandImage, (unsigned char *)ptr, sizeof(FW_SPINAND_IMAGE_T));
            break;
        }
    }
    MSG_DEBUG("Action flag: %s, blockNo 0x%x(%d) len 0x%x(%d)\n", au8ActionFlagName[pspiNandImage->actionFlag],pspiNandImage->blockNo, pspiNandImage->blockNo, pspiNandImage->fileLength, pspiNandImage->fileLength);
    switch (pspiNandImage->actionFlag)
    {
    case WRITER_MODE:   // normal write
    {
        MSG_DEBUG("SPI NAND normal write !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x89;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }
        MSG_DEBUG("SPI NAND normal write2 !!!\n");
        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;

        if (pspiNandImage->imageType == UBOOT)   // system image
        {
            pspiNandImage->imageNo = 0;
            pspiNandImage->blockNo = 0;
            addMagicHeader(pspiNandImage->executeAddr, pspiNandImage->fileLength);
            addNUC980MagicHeader(0);
            ptr += IBR_HEADER_LEN;  // except the 32 bytes magic header
            offset = IBR_HEADER_LEN;
        }
        else
            offset = 0;

        len = pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize;
        MSG_DEBUG("SPI NAND normal BatchBurn_SPINAND !!! page_count=0x%x(%d)\n", len, len);
        MSG_DEBUG("\nimageNo=%d, blockNo=%d executeAddr=0x%x  fileLength=%d    len = %d\n", pspiNandImage->imageNo,pspiNandImage->blockNo, pspiNandImage->executeAddr, pspiNandImage->fileLength, len);
        if (pspiNandImage->imageType == UBOOT)     // system image
        {
            if(pSN->SPINand_IsDieSelect == 1)
            {
                spiNAND_Die_Select(SPINAND_DIE_ID0);
            }
            BatchBurn_SPINAND_BOOT(pspiNandImage->fileLength + offset +((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize,0,4,1);
            MSG_DEBUG("BatchBurn_SPINAND_BOOT(%d   ,0,4,1)\n", pspiNandImage->fileLength + offset +((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize);
        }
        else
        {
            if(pspiNandImage->imageType!=IMAGE)
            {
                if(pspiNandImage->imageType == DATA_OOB)
                {
                    MSG_DEBUG("DATA_OOB type\n");
                    // ChipWriteWithOOB
                    //BatchBurn_SPINAND_Data_OOB(pspiNandImage->fileLength + offset +((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize,pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSM->uPageSize),pspiNandImage->imageType);
                }
                else
                {
                    MSG_DEBUG("WRITER_MODE: BatchBurn_SPINAND(0x%x, 0x%x, 0x%x)\n", pspiNandImage->fileLength + offset +((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize, pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize), pspiNandImage->imageType);
                    BatchBurn_SPINAND(pspiNandImage->fileLength + offset +((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize,pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSM->uPageSize),pspiNandImage->imageType);
                    MSG_DEBUG("BatchBurn_SPINAND(%d   %d   %d  )\n", pspiNandImage->fileLength + offset +((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize, pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize), pspiNandImage->imageType);
                    MSG_DEBUG("BatchBurn_SPINAND offset = %d    ((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize  %d \n", pspiNandImage->fileLength + offset +((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize,pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSM->uPageSize), ((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize);
                }
            }
            else
            {
                MSG_DEBUG("_ch[0]=%c,_ch[1]=%c,_ch[2]=%c,_ch[3]=%c\n",(char)_ch[0],(char)_ch[1],(char)_ch[2],(char)_ch[3]);
                if(((char)_ch[0])=='U' && ((char)_ch[1])=='B' && ((char)_ch[2])=='I')
                {
                    BatchBurn_SPINAND(pspiNandImage->fileLength + offset +((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize,pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize),UBIFS);
                }
                else
                {
                    MSG_DEBUG("WRITER_MODE: BatchBurn_SPINAND(0x%x,  0x%x,  0x%x)\n", pspiNandImage->fileLength + offset +((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize, pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize), YAFFS2);
                    BatchBurn_SPINAND(pspiNandImage->fileLength + offset +((FW_SPINAND_IMAGE_T *)pspiNandImage)->initSize,pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize),YAFFS2);
                }
            }
        }
    }
    break;
    case MODIFY_MODE:   // modify
    {
        MSG_DEBUG("SPI NAND modify !!!\n");
    }
    break;

    case ERASE_MODE:    // erase
    {
        MSG_DEBUG("SPI NAND erase !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x89;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        MSG_DEBUG("\nstart=%d\n",pspiNandImage->blockNo); //start block
        MSG_DEBUG("length=%d\n",pspiNandImage->executeAddr); //block length
        MSG_DEBUG("type=%d, pspiNandImage->imageNo=0x%x\n",pspiNandImage->imageType, pspiNandImage->imageNo); //0: chip erase, 1: erase accord start and length blocks.
        MSG_DEBUG("imageNo=0x%08x\n",pspiNandImage->imageNo);
        MSG_DEBUG("SPINand_BlockPerFlash=0x%08x(%d)\n",pSN->SPINand_BlockPerFlash, pSN->SPINand_BlockPerFlash);
        MSG_DEBUG("SPINand_PagePerBlock=0x%08x(%d)\n",pSN->SPINand_PagePerBlock, pSN->SPINand_PagePerBlock);
        MSG_DEBUG("SPINand_PageSize=0x%08x(%d)\n",pSN->SPINand_PageSize, pSN->SPINand_PageSize);

        // Multi die , 2 X 1G Bit
        if(pSN->SPINand_IsDieSelect == 1)
        {
            u8selectid = 2;
        }
        else
            u8selectid = 1;

        tmpBlockPerFlash = pSN->SPINand_BlockPerFlash*u8selectid;

        if(pspiNandImage->imageType==0)   //all chip erase
        {
            uint8_t volatile SR;
            usb_send((unsigned char*)&tmpBlockPerFlash,4);//send Erase block size to PC

            if (pspiNandImage->imageNo == 0xFFFFFFFF)
            {
                for(i = 0; i < u8selectid; i++)
                {
                    // Multi die , 2 X 1G Bit
                    if(pSN->SPINand_IsDieSelect == 1)
                    {
                        spiNAND_Die_Select(i);
                    }
                    for(blockNum=0; blockNum < pSN->SPINand_BlockPerFlash; blockNum++)
                    {
                        PA_Num = blockNum*pSN->SPINand_PagePerBlock;
                        if(spiNAND_bad_block_check(PA_Num) == 1)
                        {
                            badBlock++;
                            *_ack=(pSN->SPINand_BlockPerFlash*i)+blockNum;
                            usb_send((unsigned char*)_ack,4);//send ack to PC
                            printf("bad_block:%d\n", (pSN->SPINand_BlockPerFlash*i)+blockNum);
                        }
                        else
                        {
                            spiNAND_BlockErase( (PA_Num>>8)&0xFF, PA_Num&0xFF);
                            SR = spiNAND_Check_Program_Erase_Fail_Flag();
                            if (SR != 0)
                            {
                                spiNANDMarkBadBlock(PA_Num);
                                badBlock++;
                                *_ack=(pSN->SPINand_BlockPerFlash*i)+blockNum;
                                usb_send((unsigned char*)_ack,4);//send ack to PC
                                printf("Error erase status! bad_block:%d\n", (pSN->SPINand_BlockPerFlash*i)+blockNum);
                            }
                            else
                            {
                                *_ack=(pSN->SPINand_BlockPerFlash*i)+blockNum;
                                usb_send((unsigned char*)_ack,4);//send ack to PC
                                MSG_DEBUG("BlockErase %d Done\n", (pSN->SPINand_BlockPerFlash*i)+blockNum);
                            }
                        }
                        MSG_DEBUG("id=%d, blockNum %d, index:%d\n", i, blockNum, (pSN->SPINand_BlockPerFlash*i)+blockNum);
                    }
                }
            }
#if(0)
            // ChipEraseWithBad - OOB
            else
            {
                // ChipEraseWithBad
                for(blockNum=0; blockNum < pSN->SPINand_BlockPerFlash; blockNum++)
                {
                    PA_Num = blockNum*pSN->SPINand_PagePerBlock;
                    spiNAND_BlockErase( (PA_Num>>8)&0xFF, PA_Num&0xFF);
                    SR = spiNAND_Check_Program_Erase_Fail_Flag();
                    if (SR != 0)
                    {
                        spiNANDMarkBadBlock(PA_Num);
                        //*_ack=0xffff;
                        *_ack=blockNum;
                        usb_send((unsigned char*)_ack,4);//send ack to PC
                        printf("Error erase status! bad_block:%d\n", blockNum);
                    }
                    else
                    {
                        *_ack=blockNum;
                        usb_send((unsigned char*)_ack,4);//send ack to PC
                        MSG_DEBUG("BlockErase %d Done\n", blockNum);
                    }
                }
            }
#endif
        }
        else     // Erase accord start and length blocks
        {
            uint8_t volatile SR;
            uint32_t volatile cnt = 0;
            usb_send((unsigned char*)&pspiNandImage->executeAddr,4);// send Erase block size to PC
            MSG_DEBUG("pspiNandImage->executeAddr = 0x%x   pspiNandImage->blockNo= %d\n",  pspiNandImage->executeAddr, pspiNandImage->blockNo);
            Blk_Idx = pspiNandImage->blockNo;
			shiftBlockNumEnd = pspiNandImage->blockNo + pspiNandImage->executeAddr;
            if (pspiNandImage->imageNo == 0xFFFFFFFF)
            {
                for(blockNum=pspiNandImage->blockNo; blockNum < (pspiNandImage->blockNo+pspiNandImage->executeAddr); blockNum++)
                {
                    cnt++;

                    if(pSN->SPINand_IsDieSelect == 1)
                    {
						spiNAND_Die_Select(Blk_Idx/pSN->SPINand_BlockPerFlash);
                    }
                    PA_Num = (Blk_Idx%pSN->SPINand_BlockPerFlash)*pSN->SPINand_PagePerBlock;
					MSG_DEBUG("SPINAND_DIE_ID%d erase, %d, %d, shiftBlockNumEnd=%d\n", Blk_Idx/pSN->SPINand_BlockPerFlash, (Blk_Idx%pSN->SPINand_BlockPerFlash), pspiNandImage->blockNo, shiftBlockNumEnd);
                    // if(blockNum == 0)
                    // spiNANDClearMarkBadBlock(PA_Num);
                    if(spiNAND_bad_block_check(PA_Num) == 1)
                    {
                        printf("bad_block:%d\n", Blk_Idx);
                        badBlock++;
                        //*_ack = blockNum /pspiNandImage->executeAddr;
                        *_ack=((Blk_Idx+1)*100) /shiftBlockNumEnd;
                        usb_send((unsigned char*)_ack,4);//send ack to PC
                        continue;
                    }
                    spiNAND_BlockErase( (PA_Num>>8)&0xFF, PA_Num&0xFF);
                    SR = spiNAND_Check_Program_Erase_Fail_Flag();
                    if (SR != 0)
                    {
                        printf("Error erase status! bad_block:%d\n", Blk_Idx);
                        spiNANDMarkBadBlock(PA_Num);
                        badBlock++;
                        *_ack=((Blk_Idx+1)*100) /((pspiNandImage->blockNo+pspiNandImage->executeAddr));
                        usb_send((unsigned char*)_ack,4);//send ack to PC
                    }
                    else
                    {
                        /* send status */
                        //*_ack=((cnt+1)*100) /pspiNandImage->executeAddr;
                        *_ack=((Blk_Idx+1)*100) /((pspiNandImage->blockNo+pspiNandImage->executeAddr));
                        usb_send((unsigned char*)_ack,4);//send ack to PC
                        MSG_DEBUG("blockNum = %d  cnt=%d\n", *_ack, cnt);
                    }
					Blk_Idx++;
                }
            }
#if(0)
            // ChipEraseWithBad - OOB
            else
            {
                MSG_DEBUG("ChipEraseWithBad\n");
                for(blockNum=pspiNandImage->blockNo; blockNum < (pspiNandImage->blockNo+pspiNandImage->executeAddr); blockNum++)
                {
                    cnt++;
                    PA_Num = blockNum*pSN->SPINand_PagePerBlock;
                    spiNAND_BlockErase( (PA_Num>>8)&0xFF, PA_Num&0xFF);
                    SR = spiNAND_Check_Program_Erase_Fail_Flag();
                    if (SR != 0)
                    {
                        printf("Error erase status! bad_block:%d\n", blockNum);
                        //spiNANDMarkBadBlock(PA_Num);
                        badBlock++;
                        //*_ack=0xffff;
                        //*_ack=((cnt+1)*100) /pspiNandImage->executeAddr;
                        *_ack=((blockNum+1)*100) /((pspiNandImage->blockNo+pspiNandImage->executeAddr));
                        usb_send((unsigned char*)_ack,4);//send ack to PC
                        //continue;
                    }
                    else
                    {
                        /* send status */
                        //*_ack=((cnt+1)*100) /pspiNandImage->executeAddr;
                        *_ack=((blockNum+1)*100) /((pspiNandImage->blockNo+pspiNandImage->executeAddr));
                        usb_send((unsigned char*)_ack,4);//send ack to PC
                        MSG_DEBUG("blockNum = %d  cnt=%d\n", *_ack, cnt);
                    }
                }
            }
#endif
        }
    }
    break;

    case VERIFY_MODE:   // verify
    {
        MSG_DEBUG("SPI NAND normal verify !!!\n");

        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x89;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }
        {
            if (pspiNandImage->imageType == UBOOT)     // system image
            {
                offblk=0;
                _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
                ptr=_ch;
                PA_Num = pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);
_retry3_:
                if(pSN->SPINand_IsDieSelect == 1)
                {
                    spiNAND_Die_Select(SPINAND_DIE_ID0);
                }
                page_count = (PA_Num+offblk)*pSN->SPINand_PagePerBlock;
                while(spiNAND_bad_block_check(page_count) == 1)
                {
                    offblk++;
                    goto _retry3_;
                }
                MSG_DEBUG("offblk %d\n", offblk);
                Read_SPINand(DOWNLOAD_BASE,PA_Num+offblk,pspiNandImage->fileLength+IBR_HEADER_LEN+pspiNandImage->initSize);
                MSG_DEBUG("VERIFY_MODE: Read_SPINand(0x%x, 0x%x, 0x%x)\n", DOWNLOAD_BASE, pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize)+offblk, pspiNandImage->fileLength+IBR_HEADER_LEN+pspiNandImage->initSize);

                memmove(_ch,_ch+IBR_HEADER_LEN+pspiNandImage->initSize,pspiNandImage->fileLength);
                MSG_DEBUG("memmove  0x%x, 0x%x, 0x%x\n", _ch, _ch+IBR_HEADER_LEN+pspiNandImage->initSize, pspiNandImage->fileLength);
                do
                {
                    usb_send(ptr,TRANSFER_LEN); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4); //recv data from PC
                    ptr += (*_ack);
                }
                while((ptr-_ch)<(pspiNandImage->fileLength));
            }
            else
            {
                offblk=0;
                total=pspiNandImage->fileLength;
                Blk_Idx = pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);
                do
                {
                    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
                    ptr=_ch;
                    len=MIN(total,pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);
                    memset(ptr, 0xff, pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);
_retry4_:
                    // Multi die , 2 X 1G Bit
                    if(pSN->SPINand_IsDieSelect == 1)
                    {
                        spiNAND_Die_Select(Blk_Idx/pSN->SPINand_BlockPerFlash);
                        MSG_DEBUG("ID%d  Blk_Idx=%d\n", Blk_Idx/pSN->SPINand_BlockPerFlash, Blk_Idx%pSN->SPINand_BlockPerFlash);
                    }

                    page_count = (Blk_Idx%pSN->SPINand_BlockPerFlash)*pSN->SPINand_PagePerBlock;
                    MSG_DEBUG("VERIFY_MODE: pSN->SPINand_IsDieSelect %d [shiftBlockNumStart/PA_Num/offblk] [%d,%d,%d]\n", pSN->SPINand_IsDieSelect, (Blk_Idx%pSN->SPINand_BlockPerFlash), PA_Num, offblk);
                    while(spiNAND_bad_block_check(page_count) == 1)
                    {
                        printf("read bad_block:%d   %d  %d   %d \n", (Blk_Idx%pSN->SPINand_BlockPerFlash), page_count, PA_Num, offblk);
                        Blk_Idx++;
                        goto _retry4_;
                    }
                    Read_SPINand(DOWNLOAD_BASE,(Blk_Idx%pSN->SPINand_BlockPerFlash),len);

                    MSG_DEBUG("VERIFY_MODE: Read_SPINand(DOWNLOAD_BASE, %d, %d)\n\n", pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize)+offblk,len);
                    do
                    {
                        usb_send(ptr,TRANSFER_LEN); //send data to PC
                        while(Bulk_Out_Transfer_Size==0) {}
                        usb_recv((unsigned char*)_ack,4); //recv data from PC
                        ptr += (*_ack);
                        MSG_DEBUG("read size=0x%08x\n",(unsigned int)(ptr-_ch));
                    }
                    while((int)(ptr-_ch)<len);
                    total-=len;
                    Blk_Idx+=1;
                    MSG_DEBUG("total=%d len=%d\n",total,len);
                }
                while(total!=0);
            }
        }
    }
    break;

    case PACK_VERIFY_MODE:   // verify
    {
        MSG_DEBUG("\n SPI NAND PACK verify !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x89;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        for(i=0; i<pspiNandImage->imageNo; i++)
        {
            while(1)
            {
                if(Bulk_Out_Transfer_Size>=sizeof(PACK_CHILD_HEAD))
                {
                    usb_recv(ptr,sizeof(PACK_CHILD_HEAD));
                    memcpy(&ppack,(unsigned char *)ptr,sizeof(PACK_CHILD_HEAD));
                    usleep(1000);
                    *_ack=0x89;
                    usb_send((unsigned char*)_ack,4);//send ack to PC
                    break;
                }
            }

            if (ppack.imagetype == UBOOT)     // system image
            {
                int rawfilelen = 0;
                offblk=0;
                _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
                ptr=_ch;
                len=MIN(total,pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);
                memset(ptr, 0xff, pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);
                PA_Num = pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);
                MSG_DEBUG("PACK_VERIFY_MODE: %d   %d   %d \n", PA_Num+offblk, PA_Num, offblk);
_retry5_:
                if(pSN->SPINand_IsDieSelect == 1)
                {
                    spiNAND_Die_Select(SPINAND_DIE_ID0);
                }
                page_count = (PA_Num+offblk)*pSN->SPINand_PagePerBlock;
                while(spiNAND_bad_block_check(page_count) == 1)
                {
                    printf("pack verify read bad_block:%d\n", PA_Num+offblk);
                    offblk++;
                    goto _retry5_;
                }
                // Get DDR parameter length
                usb_recv(ptr,4);
                memcpy(&ddrlen,(unsigned char *)ptr,4);
                MSG_DEBUG("ddrlen = 0x%x(%d)\n", ddrlen, ddrlen);
                //usleep(1000);
                *_ack=0x89;
                usb_send((unsigned char*)_ack,4);//send ack to PC

                ret = Read_SPINand(DOWNLOAD_BASE,offblk,ppack.filelen+IBR_HEADER_LEN+ppack.startaddr);
                MSG_DEBUG("offblk = %d, ptr[0~3] = 0x%x  0x%x  0x%x  0x%x, 0x%x, ret=%d\n", offblk, ptr[0], ptr[1], ptr[2], ptr[3], ppack.filelen+IBR_HEADER_LEN+ppack.startaddr, ret);
                memmove(_ch,_ch+IBR_HEADER_LEN+ddrlen+ppack.startaddr,ppack.filelen);
                rawfilelen = ppack.filelen - (ddrlen+IBR_HEADER_LEN+ppack.startaddr);
                MSG_DEBUG("ppack.filelen = (%d) 0x%x,  rawfilelen = (%d)0x%x\n", ppack.filelen, ppack.filelen, rawfilelen, rawfilelen);
                do
                {
                    usb_send(ptr,TRANSFER_LEN); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4); //recv data from PC
                    ptr += (*_ack);
                }
                while((ptr-_ch)<(rawfilelen));

            }
            else
            {
                //int total,offblk=0;
                offblk = 0;
                total=ppack.filelen;
                pspiNandImage->blockNo = ppack.startaddr;
                Blk_Idx = pspiNandImage->blockNo/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);
                do
                {
                    _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
                    ptr=_ch;
                    len=MIN(total,pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);
                    memset(ptr, 0xff, pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);

_retry6_:
                    if(pSN->SPINand_IsDieSelect == 1)
                    {
                        spiNAND_Die_Select(Blk_Idx/pSN->SPINand_BlockPerFlash);
                        MSG_DEBUG("PACK_VERIFY_MODE: ID%d  Blk_Idx=%d\n", Blk_Idx/pSN->SPINand_BlockPerFlash, Blk_Idx%pSN->SPINand_BlockPerFlash);
                    }

                    page_count = (Blk_Idx%pSN->SPINand_BlockPerFlash)*pSN->SPINand_PagePerBlock;
                    //printf("pack verify :%d   %d   %d   page_count=%d\n", PA_Num+offblk, PA_Num, offblk, page_count);
                    while(spiNAND_bad_block_check(page_count) == 1)  // || (page_count == 24*pSN->SPINand_PagePerBlock)) {
                    {
                        printf("pack verify read bad_block:%d\n", Blk_Idx);
                        Blk_Idx++;
                        goto _retry6_;
                    }
                    //printf("pack verify :%d   %d   %d   page_count=%d\n", PA_Num+offblk, PA_Num, offblk, page_count);
                    Read_SPINand(DOWNLOAD_BASE, Blk_Idx%pSN->SPINand_BlockPerFlash, len);
                    MSG_DEBUG("PACK_VERIFY_MODE: Blk_Idx=%d,len=%d\n",Blk_Idx%pSN->SPINand_BlockPerFlash,len);
                    do
                    {
                        usb_send(ptr,TRANSFER_LEN); //send data to PC
                        while(Bulk_Out_Transfer_Size==0) {}
                        usb_recv((unsigned char*)_ack,4); //recv data from PC
                        ptr += (*_ack);
                        MSG_DEBUG("read size=0x%08x\n",(unsigned int)(ptr-_ch));
                    }while((int)(ptr-_ch)<len);
                    total-=len;
                    Blk_Idx+=1;
                    MSG_DEBUG("total=%d len=%d\n",total,len);
                }while(total!=0);
            }
        }
    }
    break;

    case READ_MODE:   // read
    {
        MSG_DEBUG("SPI NAND normal read !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x89;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        MSG_DEBUG("READ_MODE offset=%d,blockNo=%d,fileLength=%d\n",offset,pspiNandImage->blockNo,pspiNandImage->fileLength);
        offblk=0;
        block_cnt = 0;
        total=pspiNandImage->fileLength;
        totalblock = total/(pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);
        Blk_Idx = pspiNandImage->blockNo;// Start block index
        MSG_DEBUG("totalblock=%d  total=%d  Blk_Idx =%d\n",totalblock, total, Blk_Idx);
        do
        {
            _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
            ptr=_ch;
            if(pspiNandImage->initSize==0)   //0: read good block , others : read redundancy area,good block and bad block
            {
                len=MIN(total,pSN->SPINand_PagePerBlock*pSN->SPINand_PageSize);
                memset(ptr, 0, len);
_retry7_:
                if(pSN->SPINand_IsDieSelect == 1)
                {
                    spiNAND_Die_Select(Blk_Idx/pSN->SPINand_BlockPerFlash);
                    MSG_DEBUG("READ_MODE: ID%d  Blk_Idx=%d\n", Blk_Idx/pSN->SPINand_BlockPerFlash, Blk_Idx%pSN->SPINand_BlockPerFlash);
                }

                page_count = (Blk_Idx%pSN->SPINand_BlockPerFlash)*pSN->SPINand_PagePerBlock;
                MSG_DEBUG("page_count=0x%x(%d)  offblk=%d  Blk_Idx=%d\n",page_count, page_count, offblk, (Blk_Idx%pSN->SPINand_BlockPerFlash));
                while(spiNAND_bad_block_check(page_count) == 1)
                {
                    Blk_Idx++;
                    goto _retry7_;
                }
                MSG_DEBUG("Read_SPINand(0x%x, %d, %d)\n", DOWNLOAD_BASE, (Blk_Idx%pSN->SPINand_BlockPerFlash), len);
                Read_SPINand(DOWNLOAD_BASE, (Blk_Idx%pSN->SPINand_BlockPerFlash), len);
                do
                {
                    usb_send(ptr,4096); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4); //recv data from PC
                    ptr += (*_ack);
                }
                while((ptr-_ch)<len);
                total-=len;
                Blk_Idx+=1;
            }
            else
            {
                MSG_DEBUG("ChipReadWithBad\n");
                len=MIN(total,pSN->SPINand_PagePerBlock*(pSN->SPINand_PageSize+pSN->SPINand_SpareArea));
                memset(ptr, 0, len);
                if(pSN->SPINand_IsDieSelect == 1)
                {
                    spiNAND_Die_Select(Blk_Idx/pSN->SPINand_BlockPerFlash);
                    MSG_DEBUG("READ_MODE: ID%d  Blk_Idx=%d\n", Blk_Idx/pSN->SPINand_BlockPerFlash, Blk_Idx%pSN->SPINand_BlockPerFlash);
                }
                Read_SPINand_Redunancy(DOWNLOAD_BASE,(Blk_Idx%pSN->SPINand_BlockPerFlash),len);
                do
                {
                    usb_send(ptr,4096); //send data to PC
                    while(Bulk_Out_Transfer_Size==0) {}
                    usb_recv((unsigned char*)_ack,4); //recv data from PC
                    ptr += (*_ack);
                }
                while((ptr-_ch)<len);
                total-=(len);
                Blk_Idx+=1;
            }
        }
        while(total!=0);
    }
    break;

    case PACK_MODE:
    {
        MSG_DEBUG("SPI NAND pack mode !!!\n");
        /* for debug or delay */
        {
            usleep(1000);
            *_ack=0x89;
            usb_send((unsigned char*)_ack,4);//send ack to PC
        }

        _ch=((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
        ptr=_ch;
        {
            PACK_HEAD pack;
            PACK_CHILD_HEAD ppack;
            while(1)
            {
                if(Bulk_Out_Transfer_Size>=sizeof(PACK_HEAD))
                {
                    usb_recv(ptr,sizeof(PACK_HEAD));
                    memcpy(&pack,(unsigned char *)ptr,sizeof(PACK_HEAD));
                    *_ack=sizeof(PACK_HEAD);
                    usb_send((unsigned char*)_ack,4);//send ack to PC
                    break;
                }
            }
            MSG_DEBUG("pack.actionFlag=0x%x, pack.fileLength=0x%08x pack.num=%d!!!\n",pack.actionFlag,pack.fileLength,pack.num);
            for(i=0; i<pack.num; i++)
            {
                while(1)
                {
                    if(Bulk_Out_Transfer_Size>=sizeof(PACK_CHILD_HEAD))
                    {
                        usb_recv(ptr,sizeof(PACK_CHILD_HEAD));
                        memcpy(&ppack,(unsigned char *)ptr,sizeof(PACK_CHILD_HEAD));
                        *_ack=sizeof(PACK_CHILD_HEAD);
                        usb_send((unsigned char*)_ack,4);//send ack to PC
                        break;
                    }
                }
                MSG_DEBUG("%d ppack.filelen=0x%x, ppack.startaddr=0x%08x!!!\n",i,ppack.filelen,ppack.startaddr);

                if(ppack.imagetype==UBOOT)
                {
                    if(pSN->SPINand_IsDieSelect == 1)
                    {
                        spiNAND_Die_Select(SPINAND_DIE_ID0);
                    }
                    ret = BatchBurn_SPINAND_BOOT(ppack.filelen,0,4,0);
                    if(ret == -1)
                    {
                        printf("Error SPI Nand Device image error !!! \n");
                        return;
                    }
                }
                else
                {
                    if(ppack.imagetype!=IMAGE)
                    {
                        Blk_Idx = ppack.startaddr/(pSN->SPINand_PageSize*pSN->SPINand_PagePerBlock);

                        MSG_DEBUG("PACK_MODE: BatchBurn_SPINAND(%d, %d, %d)\n", ppack.filelen, Blk_Idx, pspiNandImage->imageType);
                        BatchBurn_SPINAND(ppack.filelen, Blk_Idx, pspiNandImage->imageType);
                    }
                    else
                    {
                        MSG_DEBUG("_ch[0]=%c,_ch[1]=%c,_ch[2]=%c,_ch[3]=%c\n",(char)_ch[0],(char)_ch[1],(char)_ch[2],(char)_ch[3]);
                        BatchBurn_SPINAND(ppack.filelen,ppack.startaddr/(pSN->SPINand_PageSize*pSN->SPINand_PagePerBlock),YAFFS2);
                    }
                }
            }
        }
    }
    break;
    default:
        ;
        break;
    }
}

extern UINT16 usiReadID(void);
extern UINT32 Custom_uBlockPerFlash;
extern UINT32 Custom_uPagePerBlock;
INFO_T info;

void UXmodem_INFO()
{
    UINT8 *ptr=(UINT8 *)&info;
    memset((char *)&info,0x0,sizeof(INFO_T));

    MSG_DEBUG("Receive INFO(%d) flash Image ...\n", sizeof(INFO_T));
    while(1)
    {
        if(Bulk_Out_Transfer_Size>=sizeof(INFO_T))
        {
            usb_recv(ptr,sizeof(INFO_T));
            break;
        }
    }

    if(info.Nand_uIsUserConfig == 1)
        g_uIsUserConfig = 1;
    else
        g_uIsUserConfig = 0;

    Custom_uBlockPerFlash = info.Nand_uBlockPerFlash;
    Custom_uPagePerBlock = info.Nand_uPagePerBlock;
    MSG_DEBUG("g_uIsUserConfig %d -> Nand_uBlockPerFlash=%d, Nand_uPagePerBlock=%d\n", g_uIsUserConfig, Custom_uBlockPerFlash, Custom_uPagePerBlock);
    MSG_DEBUG("Get INFO flash Image ...\n");

#if(1) /* QSPI0 Init */
    if (usiInit() == Fail)
    {
        info.SPI_ID=usiReadID();

        if(info.SPI_ID == 0xffff)
            printf("Error! Read SPI ID(0x%x)\n", info.SPI_ID);
    }
#endif

#if(1) //SPI NAND
    /* Reset QSPI0 */
    outpw(REG_SYS_APBIPRST1, inpw(REG_SYS_APBIPRST1) | 0x00000010);
    outpw(REG_SYS_APBIPRST1, inpw(REG_SYS_APBIPRST1) & ~(0x00000010));
    printf("\nSPI NAND: ");
    if (spiNANDInit() == 0)
    {
        info.SPINand_ID = pSN->SPINand_ID;
        if(info.SPINand_ID)   // Detect ID
        {
            printf("%s\n", (info.SPINand_uIsUserConfig==1)?"User Configure":"Auto Detect");
            printf("ID=[0x%x]\n", info.SPINand_ID);
            printf("BlockPerFlash = %d, PagePerBlock = %d\n", pSN->SPINand_BlockPerFlash, pSN->SPINand_PagePerBlock);
            printf("PageSize = %d, SpareArea = %d\n", pSN->SPINand_PageSize, pSN->SPINand_SpareArea);
            printf("QuadReadCmd    = 0x%x\n", pSN->SPINand_QuadReadCmd);
            printf("ReadStatusCmd  = 0x%x\n", pSN->SPINand_ReadStatusCmd);
            printf("WriteStatusCmd = 0x%x\n", pSN->SPINand_WriteStatusCmd);
            printf("StatusValue    = 0x%x\n", pSN->SPINand_StatusValue);
            printf("Dummybyte      = 0x%x\n", pSN->SPINand_dummybyte);
            printf("Multi-Die      = %d\n", pSN->SPINand_IsDieSelect);
        } // Detect ID
    }
#endif

    // NAND Init
    if(!fmiNandInit())
    {
        info.Nand_uBlockPerFlash=pSM->uBlockPerFlash;
        info.Nand_uPagePerBlock=pSM->uPagePerBlock;
        info.Nand_uPageSize=pSM->uPageSize;
        info.Nand_uBadBlockCount=pSM->uBadBlockCount;
        info.Nand_uSpareSize=pSM->uSpareSize;

        printf("BlockPerFlash=%d, PagePerBlock=%d, PageSize=%d\n",info.Nand_uBlockPerFlash, info.Nand_uPagePerBlock, info.Nand_uPageSize);
    }


    /* eMMC/SD Init */
    _sd_ReferenceClock = 12000;    // kHz

    /* select eMMC/SD function pins */
    if (((inpw(REG_SYS_PWRON) & 0x00000300) == 0x300))
    {
        printf("\neMMC0/SD0 ");
    }
    else
    {
        /* Set GPF for eMMC1/SD1 */
        printf("\neMMC1/SD1 ");
    }

    eMMCBlockSize=fmiInitSDDevice();
    if((eMMCBlockSize&SD_ERR_ID)!= SD_ERR_ID)
    {
        info.EMMC_uReserved=GetMMCReserveSpace();
        MSG_DEBUG("eMMC_uReserved =%d ...\n",info.EMMC_uReserved);
        info.EMMC_uBlock=eMMCBlockSize;
    }
    else
    {
        printf("Init Fail\n");
        goto _init_done;
    }
    printf("BlockSize=0x%08x(%d), %d KB\n", eMMCBlockSize, eMMCBlockSize, info.EMMC_uBlock/2);

_init_done:
    usb_send((UINT8 *)&info, sizeof(INFO_T));
    printf("\nFinish get INFO!!\n\n");
    SendAck((UINT32)0x90); // get INFO done
}

INT ParseFlashType()
{
    switch (_usbd_flash_type)
    {
    case USBD_FLASH_SDRAM:
    {
        UXmodem_SDRAM();
        _usbd_flash_type = -1;
    }
    break;
    case USBD_FLASH_MMC:
    {
        UXmodem_MMC();
        _usbd_flash_type = -1;
    }
    break;
    case USBD_FLASH_NAND:
    {
        UXmodem_NAND();
        _usbd_flash_type = -1;
    }
    break;
    case USBD_FLASH_SPI:
    {
        UXmodem_SPI();
        _usbd_flash_type = -1;
    }
    break;
    case USBD_FLASH_SPINAND:
    {
        UXmodem_SPINAND();
        _usbd_flash_type = -1;
    }
    break;
    case USBD_INFO:
    {
        UXmodem_INFO();
        _usbd_flash_type = -1;
    }

    case -1 :   // load xusb.bin again
    {
        if(Bulk_Out_Transfer_Size>0)
        {
            unsigned char *ptr;
            unsigned int *_ack=NULL,len;
            ptr =((unsigned char*)(((unsigned int)DOWNLOAD_BASE)|NON_CACHE));
            len=Bulk_Out_Transfer_Size;
            usb_recv(ptr,len);
            do
            {
                len=Bulk_Out_Transfer_Size;
                if(len>0)
                {
                    len=Bulk_Out_Transfer_Size;
                    usb_recv(ptr,len);
                    *_ack=4097;
                    usb_send((unsigned char*)_ack,4);//send ack to PC
                    break;
                }
            }
            while(1);
        }
    }
    break;

    default:
        break;
    }

    return 0;
}
