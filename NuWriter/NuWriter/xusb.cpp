#include "stdafx.h"
#include "NuWriter.h"
#include "NuWriterDlg.h"
#include "NucWinUsb.h"
#define BATCH_BURN

#define EMMC_RW_PIPE_TIMEOUT    (1)    ///< timeout for 1 secs
#define MIN(a,b) ((a) < (b) ? (a) : (b))

unsigned char * DDR2Buf(char *buf,int buflen,int *ddrlen);

//const UCHAR usb_count=sizeof(usbdesc)/sizeof(USB_DESC);

int GetFileLength(CString& filename)
{
    FILE* rfp;
    int fileSize;
    if( (rfp=_wfopen(filename, _T("rb")))==NULL )
    {
        AfxMessageBox(_T("Error! Can't open file !"));
        fclose(rfp);
        return -1;
    }

    fseek(rfp, 0,SEEK_END);
    fileSize=ftell(rfp);
    fclose(rfp);

    return fileSize;

}

DWORD atox(CString& str)
{
    DWORD value;
    swscanf_s(str,_T("%x"),&value);
    return value;
}


MaxOffset GetMaxOffset(CIniFile& inifile,CString keyname)
{
    MaxOffset maxdata;
    CString nostr;
    UINT32 offset;
    int image_index=inifile.FindKeyX(_T("Image "));

    maxdata.maxoffset=0;

    for(int k=7; k>=0; k--)
    {
        if((1<<k)&image_index)
        {
            nostr.Format(_T("Image %d"),k);
            offset=atox(inifile.GetValue(nostr,keyname));
            if(offset>=maxdata.maxoffset)
            {
                maxdata.maxoffset=offset;
                maxdata.imageno=k;
            }
        }

    }

    return maxdata;
}

BOOL OffsetLess (MaxOffset e1, MaxOffset e2 )
{
    return e1.maxoffset < e2.maxoffset;
}


VOID SortbyOffset(CIniFile& inifile,CString keyname,vector<MaxOffset>& vect)
{
    MaxOffset maxdata;
    CString nostr;
    UINT32 offset;
    int image_index=inifile.FindKeyX(_T("Image "));

    maxdata.maxoffset=0;

    for(int k=7; k>=0; k--)
    {
        if((1<<k)&image_index)
        {
            nostr.Format(_T("Image %d"),k);
            offset=atox(inifile.GetValue(nostr,keyname));

            maxdata.maxoffset=offset;
            maxdata.imageno=k;

            vect.push_back(maxdata);

        }

    }
    sort(vect.begin(), vect.end(), OffsetLess);

    return ;
}

BOOL Auto_Detect(CString& portName,CString& tempName)
{
    BOOL bResult = TRUE;
    BOOL bDev = FALSE;
    bResult=NucUsb.EnableWinUsbDevice();

    if(bResult==TRUE)
    {
        tempName.Format(_T(" Connected"));
        portName.Format(_T("Nuvoton VCOM"));
    }
    NucUsb.NUC_CloseHandle();
    return (bResult);
}

BOOL Device_Detect(CString& portName,CString& tempName)
{
    int iDeviceNum= 0;
    BOOL bResult = FALSE;
    iDeviceNum = NucUsb.UsbDevice_Detect();
    NucUsb.WinUsbNumber = iDeviceNum;

    if(iDeviceNum)
    {
        tempName.Format(_T("Device Connected"));
        portName.Format(_T("Nuvoton VCOM"));
        bResult = TRUE;
    }
    //TRACE("Device_Detect: NucUsb.WinUsbNumber=%d\n",NucUsb.WinUsbNumber);
    return (bResult);
}

BOOLEAN DataCompare(char* base,char* src,int len)
{
    int i=0;
    for(i=0; i<len; i++)
    {
        if(base[i]!=src[i])
        {
            TRACE(_T("DataCompare error Idx = %d   0x%x  [0x%x 0x%x 0x%x 0x%x]\n"),i, base[i], src[i], src[i+1], src[i+2], src[i+3]);
            return FALSE;
        }
    }

    return TRUE;
}


unsigned char * DDR2Buf(char *buf,int buflen,int *ddrlen)
{
#if(0)  // memory leak
    unsigned char *ddrbuf;
    *ddrlen=((buflen+8+15)/16)*16;
    ddrbuf=(unsigned char *)malloc(sizeof(unsigned char)*(*ddrlen));
    memset(ddrbuf,0x0,*ddrlen);
    *(ddrbuf+0)=0x55;
    *(ddrbuf+1)=0xAA;
    *(ddrbuf+2)=0x55;
    *(ddrbuf+3)=0xAA;
    *((unsigned int *)(ddrbuf+4))=(buflen/8);        /* len */
    memcpy((ddrbuf+8),buf,buflen);
    //TRACE(_T("DDR2Buf --> ddrlen = %d\n"), *ddrlen);
    return ddrbuf;
#else

    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    *ddrlen=((buflen+8+15)/16)*16;
    memset(mainWnd->DDR2Buf, 0, 512);

    mainWnd->DDR2Buf[0]=0x55;
    mainWnd->DDR2Buf[1]=0xAA;
    mainWnd->DDR2Buf[2]=0x55;
    mainWnd->DDR2Buf[3]=0xAA;
    *((unsigned int *)(mainWnd->DDR2Buf+4))=(buflen/8);/* len */
    memcpy(&mainWnd->DDR2Buf[8],buf,buflen);
    TRACE(_T("DDR2Buf --> ddrlen = %d\n"), *ddrlen);
    return (unsigned char *)mainWnd->DDR2Buf;
#endif
}

BOOL CheckDDRiniData(char *buf, int filelen)
{
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    INT i, j, ini_idx, ddr_cnt, start_idx = 0, ddr_len = 0;

    // Check DDR *ini
    i = PACK_FORMAT_HEADER; // PACK Header 32
    ini_idx = 0;
    ddr_cnt = 0;
    // Find DDR Initial Marker
    while(i < filelen)
    {
        if((buf[i] == 0x20) && (buf[i+1] == 'T') && (buf[i+2] == 'V') && (buf[i+3] == 'N'))
        {
            if((buf[i+BOOT_HEADER] == 0x55) && (buf[i+BOOT_HEADER+1] == 0xffffffaa) && (buf[i+BOOT_HEADER+2] == 0x55) && (buf[i+BOOT_HEADER+3] == 0xffffffaa))
            {
                ini_idx = (i+BOOT_HEADER); // Found DDR
                ddr_cnt = ((buf[ini_idx+7]&0xff) << 24 | (buf[ini_idx+6]&0xff) << 16 | (buf[ini_idx+5]&0xff) << 8 | (buf[ini_idx+4]&0xff));
                TRACE(_T("ini_idx:0x%x(%d)  ddr_cnt =0x%x(%d)\n"), ini_idx, ini_idx, ddr_cnt, ddr_cnt);
                break;
            }
        }
        i++;
    }

    j = 0;
    // Compare DDR *ini content
    start_idx = ini_idx+DDR_INITIAL_MARKER+DDR_COUNTER;//ini_idx+8
    ddr_len = ddr_cnt*8;
    for(i = start_idx; i < (start_idx + ddr_len); i++)
    {
        if(buf[i] != mainWnd->ShareDDRBuf[j++])
        {
            TRACE(_T("DDR parameter error! buf[%d]= 0x%x, mainWnd->ShareDDRBuf[%d]=0x%x\n"), i, buf[i], j, mainWnd->ShareDDRBuf[j]);
            return FALSE;
        }
    }

    return TRUE;
}

/************** SDRAM Begin ************/
BOOL CRAMDlg::XUSB(CString& portName,CString& m_pathName,CString address,int autorun)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    SDRAM_RAW_TYPEHEAD fhead;
    unsigned int total,file_len,scnt,rcnt,ack;
    char* lpBuffer;

    bResult=NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(0,SDRAM,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Typeack failed !!!\n"));
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];

    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if(!file_len)
    {
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    fhead.flag=WRITE_ACTION;
    fhead.filelen=file_len;
    fhead.dtbaddress = 0;
    swscanf_s(address,_T("%x"),&fhead.address);

    if(autorun)
    {
        fhead.address|=NEED_AUTORUN;
    }

    if(autorun==2)
    {
        swscanf_s(m_dtbaddress,_T("%x"),&fhead.dtbaddress);
        fhead.dtbaddress |= NEED_AUTORUN;
    }

    memcpy(lpBuffer,(unsigned char*)&fhead,sizeof(SDRAM_RAW_TYPEHEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(SDRAM_RAW_TYPEHEAD));
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write SDRAM head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! SDRAM head ack error\n"));
        return FALSE;
    }

    scnt=file_len/BUF_SIZE;
    rcnt=file_len%BUF_SIZE;

    total=0;
    while(scnt>0)
    {

        fread(lpBuffer,BUF_SIZE,1,fp);
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer,BUF_SIZE);

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            fclose(fp);
            return FALSE;
        }

        if(bResult==TRUE)
        {
            total+=BUF_SIZE;

            pos=(int)(((float)(((float)total/(float)file_len))*100));
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            if(bResult==FALSE || ack!=BUF_SIZE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                return FALSE;
            }

        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            fclose(fp);
            return FALSE;
        }

        scnt--;

        if(pos%5==0)
        {
            PostMessage(WM_SDRAM_PROGRESS,(LPARAM)pos,0);
        }
    }

    if(rcnt>0)
    {
        fread(lpBuffer,rcnt,1,fp);
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer,rcnt);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            fclose(fp);
            return FALSE;
        }

        if(bResult==TRUE)
        {
            total+=rcnt;
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                return FALSE;
            }

        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            fclose(fp);
            return FALSE;
        }

        pos=(int)(((float)(((float)total/(float)file_len))*100));

        if(pos>=100)
        {
            pos=100;
        }

        if(pos%5==0)
        {
            PostMessage(WM_SDRAM_PROGRESS,(LPARAM)pos,0);
        }
    }

    delete []lpBuffer;
    fclose(fp);
    NucUsb.NUC_CloseHandle();
    return TRUE;

}
/************** SDRAM End ************/

/************** SPI Begin ************/
BOOL CSPIDlg::XUSB_Pack(CString& portName,CString& m_pathName,int *len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len;//,scnt,rcnt;
    ULONG ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(0,SPI,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }


    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if(!file_len)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);

    memset((unsigned char*)m_fhead,0,sizeof(NORBOOT_NAND_HEAD));
    total=0;
    m_fhead->flag=PACK_ACTION;
    m_fhead->type=m_type;
    m_fhead->filelen=file_len;
    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write SPI head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    fread(lpBuffer,m_fhead->filelen,1,fp);
    if(lpBuffer[0]!=0x5)
    {
        AfxMessageBox(_T("Error! This file is not pack image"));
        delete []lpBuffer;
        fclose(fp);
        return FALSE;
    }
    fclose(fp);

    // Check DDR *ini
    bResult=CheckDDRiniData(lpBuffer, m_fhead->filelen);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! DDR Init select error\n"));
        delete []lpBuffer;
        return FALSE;
    }

    char *pbuf = lpBuffer;
    PACK_HEAD *ppackhead=(PACK_HEAD *)lpBuffer;
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, sizeof(PACK_HEAD));
    Sleep(5);
    if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT) return FALSE;
    if(bResult!=TRUE) return FALSE;
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }
    total+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);

#if(0) //image size = DDR size – 1M - 64k
    PACK_CHILD_HEAD child;
    m_progress.SetRange(0,short(ppackhead->num*200));
    int posnum=0;
    for(int i=0; i<(int)(ppackhead->num); i++)
    {
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            return FALSE;
        }
        if(bResult!=TRUE)
        {
            delete []lpBuffer;
            return FALSE;
        }
        total+= sizeof(PACK_CHILD_HEAD);
        pbuf+= sizeof(PACK_CHILD_HEAD);

        scnt=child.filelen/BUF_SIZE;
        rcnt=child.filelen%BUF_SIZE;

        while(scnt>0)
        {
            //Sleep(20);
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=BUF_SIZE;
                total+=BUF_SIZE;

                pos=(int)(((float)(((float)total/(float)file_len))*100));

                //DbgOut("SPI wait ack");
                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                //DbgOut("SPI wait ack end");

                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                    return FALSE;
                }

            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                return FALSE;
            }

            scnt--;

            if(pos%5==0)
            {
                PostMessage(WM_SPI_PROGRESS,(LPARAM)(posnum+pos),0);
            }

        }

        if(rcnt>0)
        {
            //Sleep(20);
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf,rcnt);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }

            //TRACE(_T("upload %%.2f\r"),((float)total/file_len)*100);
            if(bResult==TRUE)
            {
                pbuf+=rcnt;
                total+=rcnt;
                //DbgOut("SPI wait ack");
                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                //DbgOut("SPI wait ack end");

                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                    return FALSE;

                }

            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                return FALSE;
            }

            pos=(int)(((float)(((float)total/(float)file_len))*100));

            if(pos>=100)
            {
                pos=100;
            }

            if(pos%5==0)
            {

                PostMessage(WM_SPI_PROGRESS,(LPARAM)(posnum+pos),0);
            }
        }
        posnum+=100;

        //burn progress...
        burn_pos=0;
        //PostMessage(WM_SPI_PROGRESS,(LPARAM)0,0);
        m_progress.SetBkColor(COLOR_BURN);

        while(burn_pos!=100)
        {
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                return FALSE;
            }

            //DbgOut("SPI wait burn ack");
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                AfxMessageBox(_T("ACK Error!  Please reset device and Re-connect now !!!"));
                return FALSE;
            }
            //DbgOut("SPI wait burn ack end");
            if(!((ack>>16)&0xffff))
            {
                burn_pos=(UCHAR)(ack&0xffff);
                PostMessage(WM_SPI_PROGRESS,(LPARAM)(posnum+burn_pos),0);
            }
            else
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                AfxMessageBox(_T("Error! Burn error"));
                return FALSE;
            }
        }
        posnum+=100;
    }

    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    return TRUE;

#else//BATCH_BRUN

    PACK_CHILD_HEAD child;
    m_progress.SetRange(0,100);
    int i, j, translen, reclen, blockcnt;
    int posnum=0;

    for(i=0; i<(int)(ppackhead->num); i++)
    {
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            return FALSE;
        }
        if(bResult!=TRUE)
        {
            delete []lpBuffer;
            return FALSE;
        }
        total+= sizeof(PACK_CHILD_HEAD);
        pbuf+= sizeof(PACK_CHILD_HEAD);
        blockcnt = child.filelen/(SPI_BLOCK_SIZE);
        for(j=0; j<blockcnt; j++)
        {
            //TRACE(_T("blockcnt=%d, total = %d, child.filelen=%d  file_len=%d j=%d\n"), blockcnt, total, child.filelen, file_len, j);
            translen = SPI_BLOCK_SIZE;//64*1024;
            while(translen>0)
            {
                bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, BUF_SIZE);
                pbuf+=BUF_SIZE;
                translen -= BUF_SIZE;
                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    total+=BUF_SIZE;
                    pos=(int)(((float)(((float)total/(float)file_len))*100));

                    //DbgOut("SPI wait ack");
                    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                    //DbgOut("SPI wait ack end");
                    if(bResult==FALSE)
                    {
                        delete []lpBuffer;
                        NucUsb.NUC_CloseHandle();
                        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                        return FALSE;
                    }
                }

                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    fclose(fp);
                    return FALSE;
                }
            }

            TRACE(_T("blockcnt=%d,  j=%d, total = %d  pos=%d\n"), blockcnt, j, total, pos);

            PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);

            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            if(bResult==TRUE)
            {
                while(ack!=j);
            }
            else
                TRACE(_T("file_len = %d total = %d  remain = %d\n"), file_len, total, file_len-total);

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                return FALSE;
            }
        }
        TRACE(_T("child.filelen = %d     %d\n"), child.filelen, child.filelen % (SPI_BLOCK_SIZE));
        if ((child.filelen % (SPI_BLOCK_SIZE)) != 0)
        {
            translen = child.filelen - (blockcnt*SPI_BLOCK_SIZE);
            //TRACE(_T("translen = %d\n"), translen);
            while(translen>0)
            {
                reclen = MIN(BUF_SIZE, translen);
                bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, reclen);
                pbuf+=reclen;
                translen -= reclen;
                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    total+=reclen;
                    pos=(int)(((float)(((float)total/(float)file_len))*100));
                    //TRACE(_T("file_len=%d  total = %d   remin=%d\n"), file_len, total, file_len-total);
                    //DbgOut("SPI wait ack");
                    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                    //DbgOut("SPI wait ack end");
                    if(bResult==FALSE)
                    {
                        delete []lpBuffer;
                        NucUsb.NUC_CloseHandle();
                        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                        return FALSE;
                    }
                }

                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    return FALSE;
                }

            }

            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            if(bResult==TRUE)
            {
                while(ack!=100);
                TRACE(_T("ack %d\n"), ack);
            }
            else
            {
                TRACE(_T("file_len = %d  total = %d  remain = %d\n"), file_len, total, file_len-total);
            }
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                return FALSE;
            }

            PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);
        }
    }

    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    return TRUE;
#endif
}

BOOL CSPIDlg::XUSB_Burn(CString& portName,CString& m_pathName,int *len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len,totalsize,scnt,rcnt;
    ULONG ack;
    char* lpBuffer;
    unsigned char *ddrbuf;
    int ddrlen, blockcnt = 0;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    m_progress.SetRange(0,100);
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(0,SPI,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if(!file_len)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    memset((unsigned char*)m_fhead,0,sizeof(NORBOOT_NAND_HEAD));
    m_fhead->flag=WRITE_ACTION;
    ((NORBOOT_NAND_HEAD *)m_fhead)->initSize=0;
    m_fhead->filelen=file_len;

    switch(m_type)
    {
    case DATA:
    case PACK:
        swscanf_s(_T("0"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        *len=file_len;
        lpBuffer = new char[file_len]; //read file to buffer
        memset(lpBuffer,0xff,file_len);
        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;
        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);
        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    case ENV:
        swscanf_s(_T("0"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        if(file_len>(0x10000-4))
        {
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            AfxMessageBox(_T("Error! The environment file size is less then 64KB\n"));
            return FALSE;
        }
        lpBuffer = new char[0x10000]; //read file to buffer
        memset(lpBuffer,0x00,0x10000);

        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;

        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);
        m_fhead->filelen=0x10000;
        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    case UBOOT:
        swscanf_s(_T("1"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        //-------------------DDR---------------------
        ddrbuf=DDR2Buf(mainWnd->ShareDDRBuf,mainWnd->DDRLen,&ddrlen);
        file_len=file_len+ddrlen;
        ((NORBOOT_NAND_HEAD *)m_fhead)->initSize=ddrlen;
        //-------------------------------------------
        *len=file_len;
        lpBuffer = new char[file_len]; //read file to buffer
        memset(lpBuffer,0xff,file_len);
        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;

        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);

        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    }

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write SPI head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    switch(m_type)
    {
    case DATA:
    case PACK:
        fread(lpBuffer,m_fhead->filelen,1,fp);
        break;
    case ENV:
#if 0
        fread(lpBuffer+4,file_len,1,fp);
#else
        {
            char line[256];
            char* ptr=(char *)(lpBuffer+4);
            while (1)
            {
                if (fgets(line,256, fp) == NULL) break;
                if(line[strlen(line)-2]==0x0D || line[strlen(line)-1]==0x0A)
                {
                    strncpy(ptr,line,strlen(line)-1);
                    ptr[strlen(line)-2]=0x0;
                    ptr+=(strlen(line)-1);
                }
                else
                {
                    strncpy(ptr,line,strlen(line));
                    ptr+=(strlen(line));
                }
            }

        }
#endif

        *(unsigned int *)lpBuffer=mainWnd->CalculateCRC32((unsigned char *)(lpBuffer+4),0x10000-4);
        *len=file_len=0x10000;
        if(mainWnd->envbuf!=NULL) free(mainWnd->envbuf);
        mainWnd->envbuf=(unsigned char *)malloc(0x10000);
        memcpy(mainWnd->envbuf,lpBuffer,0x10000);
        break;
    case UBOOT:
        memcpy(lpBuffer,ddrbuf,ddrlen);
        fread(lpBuffer+ddrlen,m_fhead->filelen,1,fp);
        break;
    }

    m_progress.SetRange(0,100);
    blockcnt = file_len/(SPI_BLOCK_SIZE);
    total=0;
    char *pbuf = lpBuffer;
    int prepos=0, debugCnt = 0;
    totalsize = 0;

    if(m_type == UBOOT)  // uboot image, image size = DDR size – 1M - 64k
    {
        scnt=file_len/BUF_SIZE;
        rcnt=file_len%BUF_SIZE;
        TRACE(_T("scnt = %d  rcnt = %d\n"), scnt, rcnt);

        while(scnt>0)
        {
            debugCnt++;
            if(debugCnt == 33)
                TRACE(_T("scnt = %d  debugCnt=%d\n"), scnt, debugCnt);
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, BUF_SIZE);
            pbuf+=BUF_SIZE;
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                fclose(fp);
                return FALSE;
            }
        
            if(bResult==TRUE)
            {
                total+=BUF_SIZE;
                pos=(int)(((float)(((float)total/(float)file_len))*100));
        
                //DbgOut("SPI wait ack");
                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                //DbgOut("SPI wait ack end");
                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    fclose(fp);
                    AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                    return FALSE;
                }
            }
        
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                fclose(fp);
                return FALSE;
            }
        
            scnt--;
        
            if(pos%5==0)
            {
                PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);
            }
        
        }

        if(rcnt>0)
        {
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf,rcnt);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                fclose(fp);
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }

            //TRACE(_T("upload %%.2f\r"),((float)total/file_len)*100);
            if(bResult==TRUE)
            {
                total+=rcnt;
                //DbgOut("SPI wait ack");
                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                //DbgOut("SPI wait ack end");

                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    fclose(fp);
                    AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                    return FALSE;

                }
            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                fclose(fp);
                return FALSE;
            }

            pos=(int)(((float)(((float)total/(float)file_len))*100));

            if(pos>=100)
            {
                pos=100;
            }

            if(pos%5==0)
            {

                PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);
            }

        }


//burn progress...
        burn_pos=0;
        PostMessage(WM_SPI_PROGRESS,(LPARAM)0,0);
        m_progress.SetBkColor(COLOR_BURN);

        while(burn_pos!=100)
        {
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                return FALSE;
            }

            //DbgOut("SPI wait burn ack");
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                NucUsb.NUC_CloseHandle();
                AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                return FALSE;
            }
            //DbgOut("SPI wait burn ack end");
            if(!((ack>>16)&0xffff))
            {
                burn_pos=(UCHAR)(ack&0xffff);
                PostMessage(WM_SPI_PROGRESS,(LPARAM)burn_pos,0);
            }
            else
            {
                NucUsb.NUC_CloseHandle();
                AfxMessageBox(_T("Error! Burn error"));
                return FALSE;
            }

        }
    }
    else // Batch Burn
    {
        int i, translen, reclen;
        for(i=0; i<blockcnt; i++)
        {
            //TRACE(_T("file_len = %d  blockcnt=%d, total = %d, i=%d\n"), file_len, blockcnt, total, i);
            translen = SPI_BLOCK_SIZE;
            while(translen>0)
            {
                bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, BUF_SIZE);
                pbuf+=BUF_SIZE;
                translen -= BUF_SIZE;
                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    fclose(fp);
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    total+=BUF_SIZE;
                    pos=(int)(((float)(((float)total/(float)file_len))*100));

                    //DbgOut("SPI wait ack");
                    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                    //DbgOut("SPI wait ack end");
                    if(bResult==FALSE)
                    {
                        delete []lpBuffer;
                        NucUsb.NUC_CloseHandle();
                        fclose(fp);
                        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                        return FALSE;
                    }
                }

                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    fclose(fp);
                    return FALSE;
                }
            }

            if(pos%5==0)
            {
                PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);
            }

            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            if(bResult==TRUE)
            {
                while(ack!=i);
            }
            else
                TRACE(_T("file_len = %d  total = %d  remain = %d\n"), file_len, total, file_len-total);

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                fclose(fp);
                return FALSE;
            }
        }

        //TRACE(_T("file_len = %d     %d\n"), file_len, file_len % (SPI_BLOCK_SIZE));
        if ((file_len % (SPI_BLOCK_SIZE)) != 0)
        {
            translen = file_len - blockcnt*SPI_BLOCK_SIZE;
            //TRACE(_T("translen = %d\n"), translen);
            while(translen>0)
            {
                reclen = MIN(BUF_SIZE, translen);
                bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, reclen);
                pbuf+=reclen;
                translen -= reclen;
                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    fclose(fp);
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    total+=reclen;
                    pos=(int)(((float)(((float)total/(float)file_len))*100));
                    TRACE(_T("file_len=%d  total = %d   remin=%d\n"), file_len, total, file_len-total);
                    //DbgOut("SPI wait ack");
                    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                    //DbgOut("SPI wait ack end");
                    if(bResult==FALSE)
                    {
                        delete []lpBuffer;
                        NucUsb.NUC_CloseHandle();
                        fclose(fp);
                        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                        return FALSE;
                    }
                }

                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    fclose(fp);
                    return FALSE;
                }

            }

            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            if(bResult==TRUE)
            {
                while(ack!=100);
            }
            else
            {
                TRACE(_T("Error! file_len = %d  total = %d  remain = %d\n"), file_len, total, file_len-total);
            }
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                fclose(fp);
                return FALSE;
            }

            PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);
        }

    }



    delete []lpBuffer;
    fclose(fp);

    NucUsb.NUC_CloseHandle();
    return TRUE;
}

//SPI Verify
BOOL CSPIDlg::XUSB_Verify(CString& portName,CString& m_pathName)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0, reclen, blockcnt =0, i, translen, len=0;
    FILE* fp;
    int pos=0;
    unsigned int total=0,file_len,ack, scnt,rcnt;
    char* lpBuffer;
    char temp[BUF_SIZE];
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage(WM_SPI_PROGRESS,(LPARAM)0,0);
    m_progress.SetBkColor(COLOR_VERIFY);

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }

#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,SPI,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    if(m_type!=ENV)
    {
        fseek(fp,0,SEEK_END);
        file_len=ftell(fp);
        fseek(fp,0,SEEK_SET);
    }
    else
    {
        file_len=0x10000;
    }

    if(!file_len)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    m_fhead->flag=VERIFY_ACTION;

    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write SPI head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    m_progress.SetRange(0,100);

    blockcnt = file_len/(SPI_BLOCK_SIZE);    
    total=0;
    TRACE(_T("VERIFY file_len=%d  blockcnt=%d\n"), file_len, blockcnt); 

    if(m_type == UBOOT) // uboot image, image size = DDR size – 1M - 64k
    {
        scnt=file_len/BUF_SIZE;
        rcnt=file_len%BUF_SIZE;
        TRACE(_T("VERIFY file_len=%d  scnt = %d  rcnt = %d\n"), file_len, scnt, rcnt);
        while(scnt>0)
        {
            if(m_type!=ENV)
            {
                fread(temp,BUF_SIZE,1,fp);
            }
            else
            {
                memcpy(temp,mainWnd->envbuf+total,BUF_SIZE);
            }
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                fclose(fp);
                return FALSE;
            }

            if(bResult==TRUE)
            {
                total+=BUF_SIZE;

                pos=(int)(((float)(((float)total/(float)file_len))*100));
                //posstr.Format(_T("%d%%"),pos);

                if(DataCompare(temp,lpBuffer,BUF_SIZE))
                    ack=BUF_SIZE;
                else
                    ack=0;//compare error
                bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
                if((bResult==FALSE)||(!ack))
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    fclose(fp);
                    return FALSE;
                }

            }
            else
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                return FALSE;
            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                fclose(fp);
                return FALSE;
            }

            scnt--;

            if(pos%5==0)
            {
                PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);
            }
        }

        if(rcnt>0)
        {
            reclen = MIN(BUF_SIZE, rcnt);
            if(m_type!=ENV)
            {
                fread(temp,reclen,1,fp);
            }
            else
            {
                memcpy(temp,mainWnd->envbuf+total,reclen);
            }
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,reclen);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                fclose(fp);
                return FALSE;
            }

            if(bResult==TRUE)
            {
                total+=reclen;
                if(DataCompare(temp,lpBuffer,reclen))
                    ack=reclen;
                else
                    ack=0;//compare error
                bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack, 4);
                if((bResult==FALSE)||(!ack))
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    fclose(fp);
                    return FALSE;

                }

            }
            else
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                return FALSE;
            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                fclose(fp);
                return FALSE;
            }

            pos=(int)(((float)(((float)total/(float)file_len))*100));

            if(pos>=100)
            {
                pos=100;
            }        

			PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);            

        }    
    }
    else // BATCH_Verify
    {
        for(i=0; i<blockcnt; i++)
        {
            translen = SPI_BLOCK_SIZE;
            while(translen>0)
            {
                if(m_type!=ENV)
                {
                    fread(temp,BUF_SIZE,1,fp);
                }
                else
                {
                    memcpy(temp,mainWnd->envbuf+total,BUF_SIZE);
                }
                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer, BUF_SIZE);
                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    fclose(fp);
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    total+=BUF_SIZE;
                    len+=BUF_SIZE;
                    pos=(int)(((float)(((float)len/(float)file_len))*100));

                    if(DataCompare(temp,lpBuffer,BUF_SIZE))
                        ack=BUF_SIZE;
                    else
                        ack=0;//compare error
                    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
                    if((bResult==FALSE)||(!ack))
                    {
                        delete []lpBuffer;
                        NucUsb.NUC_CloseHandle();
                        fclose(fp);
                        return FALSE;
                    }

                }
                else
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    fclose(fp);
                    return FALSE;
                }

                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    fclose(fp);
                    return FALSE;
                }

                translen-=BUF_SIZE;
                if(pos%5==0)
                {
                    PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);
                }
            }
        }


        translen = file_len - blockcnt*SPI_BLOCK_SIZE;
        TRACE(_T("file_len = %d     translen=%d\n"), file_len, translen);
        if (translen != 0)
        {
            while(translen>0)
            {
                reclen = MIN(BUF_SIZE, translen);
                if(m_type!=ENV)
                {
                    fread(temp,reclen,1,fp);
                }
                else
                {
                    memcpy(temp,mainWnd->envbuf+total,reclen);
                }
                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,reclen);
                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    fclose(fp);
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    total+=reclen;
                    len+=reclen;
                    translen-=reclen;
                    TRACE(_T("file_len = %d  total=%d, len=%d,   reclen=%d, translen=%d\n"), file_len, total, len, reclen, translen);
                    if(DataCompare(temp,lpBuffer,reclen))
                        ack=reclen;
                    else
                        ack=0;//compare error
                    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack, 4);
                    if((bResult==FALSE)||(!ack))
                    {
                        delete []lpBuffer;
                        NucUsb.NUC_CloseHandle();
                        fclose(fp);
                        return FALSE;

                    }

                }
                else
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    fclose(fp);
                    return FALSE;
                }

                if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    fclose(fp);
                    return FALSE;
                }

                pos=(int)(((float)(((float)len/(float)file_len))*100));

                if(pos>=100)
                {
                    pos=100;
                }

                PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);
            }

        }
    }
    delete []lpBuffer;
    fclose(fp);
    NucUsb.NUC_CloseHandle();
    return TRUE;

}

BOOL CSPIDlg::XUSB_Read(CString& portName,CString& m_pathName,unsigned int addr,unsigned int len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    int pos=0;
    unsigned int total=0,scnt,rcnt,ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage(WM_SPI_PROGRESS,(LPARAM)0,0);
    m_progress.SetBkColor(COLOR_VERIFY);

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,SPI,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    FILE* tempfp;
    //-----------------------------------
    tempfp=_wfopen(m_pathName,_T("w+b"));
    //-----------------------------------
    if(!tempfp)
    {
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    m_fhead->flag=READ_ACTION;
    m_fhead->flashoffset=addr;
    m_fhead->filelen=len;
    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! Write SPI head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE || ack != 0x85)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    m_progress.SetRange(0,100);
    scnt=len/BUF_SIZE;
    rcnt=len%BUF_SIZE;
    total=0;
    while(scnt>0)
    {
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            return FALSE;
        }
        if(bResult==TRUE)
        {
            total+=BUF_SIZE;
            pos=(int)(((float)(((float)total/(float)len))*100));
            fwrite(lpBuffer,BUF_SIZE,1,tempfp);
            ack=BUF_SIZE;
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }
        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            return FALSE;
        }

        scnt--;
        if(pos%5==0)
        {
            PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);
        }
    }

    if(rcnt>0)
    {
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,rcnt);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            return FALSE;
        }
        if(bResult==TRUE)
        {
            total+=rcnt;
            fwrite(lpBuffer,rcnt,1,tempfp);
            ack=BUF_SIZE;
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if(bResult!=TRUE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }
        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            return FALSE;
        }
        pos=(int)(((float)(((float)total/(float)len))*100));
        if(pos>=100)
        {
            pos=100;
        }
        if(pos%5==0)
        {
            PostMessage(WM_SPI_PROGRESS,(LPARAM)pos,0);
        }
    }
    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    fclose(tempfp);
    /********************************************/
    return TRUE;
}

BOOL CSPIDlg::XUSB_Erase(CString& portName)
{
    BOOL bResult;
    CString tempstr;
    int count=0;
    unsigned int ack,erase_pos=0;
    NORBOOT_NAND_HEAD *fhead;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    fhead=(NORBOOT_NAND_HEAD *)malloc(sizeof(NORBOOT_NAND_HEAD));

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        free(fhead);
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,SPI,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        free(fhead);
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    memset((unsigned char*)fhead,0,sizeof(NORBOOT_NAND_HEAD));
    fhead->flag=ERASE_ACTION;
    fhead->flashoffset = _wtoi(m_sblocks); //start erase block
    fhead->execaddr=_wtoi(m_blocks);  //erase block length
    fhead->type=m_erase_flag; // Decide chip erase mode or erase mode
    if(m_erase_flag==0)
        fhead->no=0xffffffff;//erase all
    else
        fhead->no=0x0;//erase all

    memcpy(lpBuffer,(unsigned char*)fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
    free(fhead);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! Write SPI head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE || ack!=0x85)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        CString msg;
        msg.Format(_T("ACK Error! 0x%08x\n"),ack);
        AfxMessageBox(msg);
        return FALSE;
    }

    m_progress.SetRange(0,100);
    erase_pos=0;
    int wait_pos=0;
    while(erase_pos!=100)
    {
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            AfxMessageBox(_T("Error! WaitForSingleObject Error!"));
            return FALSE;
        }

        //TRACE(_T("SPI wait erase ack"));
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
        if(bResult=FALSE)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
            return FALSE;
        }

        //TRACE(_T("SPI wait erase ack end"));
        if(!((ack>>16)&0xffff))
        {
            erase_pos=ack&0xffff;
            PostMessage(WM_SPI_PROGRESS,(LPARAM)erase_pos,0);
        }
        else
        {

            PostMessage(WM_SPI_PROGRESS,(LPARAM)0,0);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            CString msg;
            msg.Format(_T("Error! Erase Error 0x%08x\n"),ack);
            AfxMessageBox(msg);
            return FALSE;
        }
#if(0)
        if(erase_pos==95)
        {
            wait_pos++;
            if(wait_pos>100)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                AfxMessageBox(_T("Error! Erase error"));
                return FALSE;
            }
        }
#endif
    }

    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    return TRUE;

}
/************** SPI End ************/

/************** MMC Begin ************/
BOOL CMMCDlg::XUSB_PackErase(int id, CString& portName, CString& m_pathName)
{
    BOOL bResult;
    CString tempstr;
    int count=0;
    DWORD  iRet = 0x00;
    unsigned int ack,erase_pos=0;
    NORBOOT_MMC_HEAD *fhead;
    char* lpBuffer;

    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    fhead=(NORBOOT_MMC_HEAD *)malloc(sizeof(NORBOOT_MMC_HEAD));

    //TRACE(_T("XUSB_PackErase start (%d)\n"), id);
    /***********************************************/
    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(bResult==FALSE)
    {
        free(fhead);
        //AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }

#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        free(fhead);
        return FALSE;
    }
#endif

    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(id,MMC,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        free(fhead);
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("Error! Erase NUC_SetType error !!!\n"));
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    //memset((unsigned char*)fhead,0,sizeof(NORBOOT_MMC_HEAD));
    memset(fhead,0,sizeof(NORBOOT_MMC_HEAD));

    FILE* fp;
    unsigned int file_len;
    char* lpReadBuffer;
    unsigned int totalsize;

    fp=_wfopen(m_pathName,_T("rb"));
    if(!fp)
    {
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(!file_len)
    {
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    lpReadBuffer = new char[file_len]; //read file to buffer
    memset(lpReadBuffer,0x00,file_len);
    fread(lpReadBuffer,file_len,1,fp);
    if(lpReadBuffer[0]!=0x5)
    {
        delete []lpReadBuffer;
        fclose(fp);
        return ERR_PACK_FORMAT;
    }
    fclose(fp);

    char *pbuf = lpReadBuffer;
    PACK_HEAD *ppackhead=(PACK_HEAD *)lpReadBuffer;

    pbuf+= sizeof(PACK_HEAD);
    PACK_CHILD_HEAD child;
    PACK_MMC_FORMAT_INFO child_format;
    totalsize = 0;
    BOOL bFormatFlag = FALSE;
    for(int i=0; i<(int)(ppackhead->num); i++)
    {
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        pbuf+= sizeof(PACK_CHILD_HEAD);
        if(child.imagetype == PARTITION)
        {
            bFormatFlag = TRUE;
            memcpy(&child_format,(char *)pbuf,sizeof(PACK_MMC_FORMAT_INFO));
            memset(fhead,0,sizeof(NORBOOT_MMC_HEAD));
            fhead->flag=FORMAT_ACTION;
            fhead->FSType = child_format.MMCFTFS;
            fhead->PartitionNum = child_format.MMCFTPNUM;
            fhead->ReserveSize = child_format.MMCFTPREV;
            fhead->Partition1Size = child_format.MMCFTP1;
            fhead->PartitionS1Size = child_format.MMCFTP1S;
            fhead->Partition2Size = child_format.MMCFTP2;
            fhead->PartitionS2Size = child_format.MMCFTP2S;
            fhead->Partition3Size = child_format.MMCFTP3;
            fhead->PartitionS3Size = child_format.MMCFTP3S;
            fhead->Partition4Size = child_format.MMCFTP4;
            fhead->PartitionS4Size = child_format.MMCFTP4S;
        }

        pbuf+=child.filelen;
        totalsize += child.filelen;
    }
    delete []lpReadBuffer;

    TRACE("Format Info[%d]: %d  %d  %d \n[P1:%dMB  %d  P2:%dMB  %d  P3:%dMB  %d  P4:%dMB  %d]\n", bFormatFlag, fhead->FSType, fhead->PartitionNum, fhead->ReserveSize, fhead->Partition1Size, fhead->PartitionS1Size,
          fhead->Partition2Size, fhead->PartitionS2Size, fhead->Partition3Size, fhead->PartitionS3Size, fhead->Partition4Size, fhead->PartitionS4Size);

    if(bFormatFlag == FALSE) //Without Partition information
    {
        //memset(fhead,0,sizeof(NORBOOT_MMC_HEAD));
        //fhead->flag=FORMAT_ACTION;
        //fhead->FSType = 0; //FAT32
        //fhead->PartitionNum = 1;
        //fhead->ReserveSize = 32768; //16MB
        //fhead->PartitionS1Size = mainWnd->m_info.EMMC_uBlock - 32768; //unit Sector
        //fhead->Partition1Size = fhead->PartitionS1Size/1024*512/1024; //MB
        free(fhead);
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        return TRUE;
    }

    memcpy(lpBuffer,(unsigned char*)fhead,sizeof(NORBOOT_MMC_HEAD));
    free(fhead);

    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)lpBuffer, sizeof(NORBOOT_MMC_HEAD));
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("Error! (%d) Erase eMMC head error\n"), id);
        return FALSE;
    }

    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("Error! (%d) Erase Read ACK error 0x%x, 0x%x\n"), id, ack, GetLastError());
        return FALSE;
    }

    erase_pos=0;
    int wait_pos=0;
    m_progress.SetRange(0,100);

    while(erase_pos!=100)
    {
        if(WaitForSingleObject(m_ExitEvent, EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("Error! (%d) WaitForSingleObject error. 0x%x \n"),id, GetLastError());
            return FALSE;
        }
        Sleep(10);
        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        if(bResult=FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("Error! (%d) Erase Read ACK error 0x%x, 0x%x\n"), id, ack, GetLastError());
            return FALSE;
        }

        if(!((ack>>16)&0xffff))
        {
            erase_pos=ack&0xffff;
            PostMessage(WM_MMC_PROGRESS,(LPARAM)erase_pos,0);
        }
        else
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("Error! (%d) Erase error 0x%x, 0x%x\n"), id, ack, GetLastError());
            return FALSE;
        }

        if(erase_pos==95)
        {
            wait_pos++;
            if(wait_pos>100)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("Error! (%d) Erase error %d\n"), id, wait_pos);
                return FALSE;
            }
        }
    }

    delete []lpBuffer;
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}

BOOL CMMCDlg::XUSB_Pack(CString& portName,CString& m_pathName,int *len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len,scnt,rcnt;
    unsigned int totalsize;
    ULONG ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    bResult = NucUsb.EnableWinUsbDevice();
    if(!bResult)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(0,MMC,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    fp=_wfopen(m_pathName,_T("rb"));
    if(!fp)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(!file_len)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    unsigned int magic;
    fread((unsigned char *)&magic,4,1,fp);
    if(magic!=0x5)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Pack Image Format Error\n"));
        return FALSE;
    }

    /* Pack Start */
    fseek(fp,0,SEEK_SET);
    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);
    memset((unsigned char*)m_fhead,0,sizeof(NORBOOT_MMC_HEAD));
    total=0;
    m_fhead->flag=PACK_ACTION;
    m_fhead->type=m_type;
    m_fhead->filelen=file_len;
    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_MMC_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_MMC_HEAD));
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write eMMC head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }
    TRACE(_T("      image file_len=%d\n"), file_len);
    fread(lpBuffer,m_fhead->filelen,1,fp);
    if(lpBuffer[0]!=0x5)
    {
        delete []lpBuffer;
        fclose(fp);
        AfxMessageBox(_T("Error! This file is not pack image"));
    }
    fclose(fp);

    // Check DDR *ini
    bResult=CheckDDRiniData(lpBuffer, m_fhead->filelen);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! DDR Init select error\n"));
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    char *pbuf = lpBuffer;
    PACK_HEAD *ppackhead=(PACK_HEAD *)lpBuffer;
	totalsize = 0;
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, sizeof(PACK_HEAD));
    Sleep(5);
    if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
    {
		delete []lpBuffer;
		NucUsb.NUC_CloseHandle();
        TRACE(_T("Error! WaitForSingleObject error\n"));
        return FALSE;
    }
    if(bResult==FALSE)
    {
		delete []lpBuffer;
		NucUsb.NUC_CloseHandle();
        TRACE(_T("Error! eMMC Burn Write error! 0x%x\n"), GetLastError());
        return FALSE;
    }

    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
		delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    totalsize += sizeof(PACK_HEAD);
    total+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);
    PACK_CHILD_HEAD child;
    m_progress.SetRange(0,100);
    m_progress.SetBkColor(COLOR_BURN);
    int posnum=0;
    int prepos=0;
    unsigned int blockNum, u32imagetype;
    for(int i=0; i<(int)(ppackhead->num); i++)
    {
        total=0;
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        u32imagetype = child.imagetype;
        TRACE(_T("      child.image(%d): child.filelen=%d,  PACK_CHILD_HEAD size=%d\n"),i, child.filelen, sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
			delete []lpBuffer;
			NucUsb.NUC_CloseHandle();
            TRACE(_T("Error! WaitForSingleObject error 2. 0x%x\n"), bResult);
            return FALSE;
        }
        if(bResult==FALSE)
        {
			delete []lpBuffer;
			NucUsb.NUC_CloseHandle();
            TRACE(_T("Error! eMMC Burn Write error! 0x%x\n"), GetLastError());
            return FALSE;
        }

        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
        if(bResult==FALSE)
        {
			delete []lpBuffer;
			NucUsb.NUC_CloseHandle();
            TRACE(_T("Error! eMMC Burn Read error! 0x%x\n"), GetLastError());
            return FALSE;
        }

        if(u32imagetype == PARTITION)
        {
            pbuf+= sizeof(PACK_CHILD_HEAD)+PACK_FOMRAT_HEADER_LEN;
            totalsize += sizeof(PACK_CHILD_HEAD)+PACK_FOMRAT_HEADER_LEN; // skip partition header
            TRACE(_T("totalsize = %d   file_len =%d   sizeof(PACK_MMC_FORMAT_INFO) =%d \n"), totalsize, file_len, sizeof(PACK_MMC_FORMAT_INFO));
            pos=(int)(((float)(((float)totalsize/(float)file_len))*100));
            if(pos>=100)
            {
                pos=100;
            }

            TRACE(" pos =%d, prepos =%d\n", pos, prepos);
            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage(WM_MMC_PROGRESS,(LPARAM)(pos),0);
            }

            continue;
        }

        pbuf+= sizeof(PACK_CHILD_HEAD);
		totalsize += sizeof(PACK_CHILD_HEAD);
        //Sleep(1);
        scnt=child.filelen/BUF_SIZE;
        rcnt=child.filelen%BUF_SIZE;		
        total=0;

        while(scnt>0)
        {
            //Sleep(1);
            //TRACE(_T("#2442  eMMC Burn scnt %d \n"), scnt);
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
				delete []lpBuffer;
				NucUsb.NUC_CloseHandle();
                TRACE(_T("Error! WaitForSingleObject error 3, scnt= %d\n"), scnt);
                return FALSE;
            }
            if(bResult==FALSE)
            {
				delete []lpBuffer;
				NucUsb.NUC_CloseHandle();
                TRACE(_T("Error! eMMC Burn Write error. scnt= %d ,0x%x\n"), scnt, GetLastError());
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=BUF_SIZE;
                total+=BUF_SIZE;
                totalsize += BUF_SIZE;

                pos=(int)(((float)(((float)totalsize/(float)file_len))*100));

                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                if(bResult==FALSE)
                {
					delete []lpBuffer;
					NucUsb.NUC_CloseHandle();
                    CString tmp;
                    tmp.Format(_T("eMMC Burn ACK error imagenum=%d  scnt= %d  0x%x"),i, scnt, GetLastError());
                    TRACE(_T("Error! %s\n"), tmp);
                    return FALSE;
                }
            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
				delete []lpBuffer;
				NucUsb.NUC_CloseHandle();
                TRACE(_T("Error! WaitForSingleObject error 4. scnt= %d  0x%x\n"), scnt, GetLastError());
                return FALSE;
            }
            scnt--;
            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                if((scnt % 4 == 0) || pos == 100)
                {
                    PostMessage(WM_MMC_PROGRESS,(LPARAM)(pos),0);
                }
            }
        }
        TRACE(_T("totalsize = %d\n"), totalsize);
			
        if(rcnt>0)
        {
            Sleep(20);
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf,rcnt);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
				delete []lpBuffer;
				NucUsb.NUC_CloseHandle();
                TRACE(_T("Error! WaitForSingleObject error 5. rcnt = %d  0x%x\n"), scnt, GetLastError());
                return FALSE;
            }
            if(bResult==FALSE)
            {
				delete []lpBuffer;
				NucUsb.NUC_CloseHandle();
                TRACE(_T("Error! eMMC Burn Write error! rcnt = %d  0x%x\n"), rcnt, GetLastError());
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=rcnt;
                total+=rcnt;
                totalsize += rcnt;

                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                if(bResult==FALSE)
                {
					delete []lpBuffer;
					NucUsb.NUC_CloseHandle();
                    TRACE(_T("Error! eMMC Burn Read error! rcnt = %d  0x%x\n"), rcnt, GetLastError());
                    return FALSE;
                }
            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
				delete []lpBuffer;
				NucUsb.NUC_CloseHandle();
                TRACE(_T("Error! WaitForSingleObject error 6. rcnt = %d  0x%x\n"), rcnt, GetLastError());
                return FALSE;
            }
			TRACE(_T("totalsize = %d\n"), totalsize);
            pos=(int)(((float)(((float)totalsize/(float)file_len))*100));
            if(pos>=100)
            {
                pos=100;
            }

            //TRACE("(%d) #7629 rcnt %d, pos =%d, prepos =%d\n", id, rcnt, pos, prepos);
            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage(WM_MMC_PROGRESS,(LPARAM)(pos),0);
            }

        }

        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&blockNum,4);
        //Sleep(10);
        if(bResult==FALSE)
        {
			delete []lpBuffer;
			NucUsb.NUC_CloseHandle();
            TRACE(_T("Error! eMMC Burn Read error. rcnt = %d  0x%x\n"), rcnt, GetLastError());
            return FALSE;
        }
    }//for(int i=0;i<(int)(ppackhead->num);i++)

    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    return TRUE;

}

BOOL CMMCDlg::XUSB_Burn(CString& portName,CString& m_pathName,int *len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len,scnt,rcnt;
    ULONG ack;

    char* lpBuffer;
    unsigned char *ddrbuf;
    int ddrlen;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    m_progress.SetRange(0,100);
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }

#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif

    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(0,MMC,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    fp=_wfopen(m_pathName,_T("rb"));
    if(!fp)
    {
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(!file_len)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    memset((unsigned char*)m_fhead,0,sizeof(NORBOOT_MMC_HEAD));

    m_fhead->flag=WRITE_ACTION;
    ((NORBOOT_MMC_HEAD *)m_fhead)->initSize=0;

    m_fhead->filelen=file_len;
    switch(m_type)
    {
    case DATA:
    case PACK:
        swscanf_s(_T("0"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        *len=file_len;
        lpBuffer = new char[file_len]; //read file to buffer
        memset(lpBuffer,0xff,file_len);
        ((NORBOOT_MMC_HEAD *)m_fhead)->macaddr[7]=0;
        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);
        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_MMC_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_MMC_HEAD));
        break;
    case ENV:
        swscanf_s(_T("0"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        if(file_len>(0x10000-4))
        {
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            AfxMessageBox(_T("Error! The environment file size is less then 64KB\n"));
            return FALSE;
        }
        lpBuffer = new char[0x10000]; //read file to buffer
        memset(lpBuffer,0x00,0x10000);

        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;

        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);
        m_fhead->filelen=0x10000;
        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_MMC_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_MMC_HEAD));
        break;
    case UBOOT:
        swscanf_s(_T("1"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        //-------------------DDR---------------------
        ddrbuf=DDR2Buf(mainWnd->ShareDDRBuf,mainWnd->DDRLen,&ddrlen);
        file_len=file_len+ddrlen+IBR_HEADER_LEN;
        ((NORBOOT_MMC_HEAD *)m_fhead)->initSize=ddrlen;
        //-------------------------------------------
        *len=file_len;
        lpBuffer = new char[file_len];//read file to buffer
        memset(lpBuffer, 0xff, file_len);
        ((NORBOOT_MMC_HEAD *)m_fhead)->macaddr[7]=0;

        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);

        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_MMC_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_MMC_HEAD));
        TRACE(_T("ddrlen=0x%x(%d)  file_len=0x%x(%d)\n"), ddrlen, ddrlen, file_len, file_len);
        break;
    }

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write eMMC head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    switch(m_type)
    {
    case DATA:
    case PACK:
        fread(lpBuffer,m_fhead->filelen,1,fp);
        break;
    case ENV:
#if 0
        fread(lpBuffer+4,file_len,1,fp);
#else
        {
            char line[256];
            char* ptr=(char *)(lpBuffer+4);
            while (1)
            {
                if (fgets(line,256, fp) == NULL) break;
                if(line[strlen(line)-2]==0x0D || line[strlen(line)-1]==0x0A)
                {
                    strncpy(ptr,line,strlen(line)-1);
                    ptr[strlen(line)-2]=0x0;
                    ptr+=(strlen(line)-1);
                }
                else
                {
                    strncpy(ptr,line,strlen(line));
                    ptr+=(strlen(line));
                }
            }
        }
#endif

        *(unsigned int *)lpBuffer=mainWnd->CalculateCRC32((unsigned char *)(lpBuffer+4),0x10000-4);
        *len=file_len=0x10000;
        if(mainWnd->envbuf!=NULL) free(mainWnd->envbuf);
        mainWnd->envbuf=(unsigned char *)malloc(0x10000);
        memcpy(mainWnd->envbuf,lpBuffer,0x10000);
        break;
    case UBOOT:
        memcpy(lpBuffer,ddrbuf,ddrlen);
        fread(lpBuffer+ddrlen,m_fhead->filelen,1,fp);
        break;
    }

    scnt=file_len/BUF_SIZE;
    rcnt=file_len%BUF_SIZE;

    total=0;
    char *pbuf = lpBuffer;

    int prepos=0;
    while(scnt>0)
    {
        //Sleep(1000);
        //TRACE(_T("scnt %d --> \n"), scnt);
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, BUF_SIZE);
        pbuf+=BUF_SIZE;
        //TRACE(_T("scnt %d <-- \n"), scnt);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            fclose(fp);
            return FALSE;
        }

        if(bResult==FALSE)
        {
            delete []lpBuffer;
            fclose(fp);
            NucUsb.NUC_CloseHandle();
            TRACE(_T("Error! eMMC Burn Write error. %d ,0x%x\n"), scnt, GetLastError());
            return FALSE;
        }
        if(bResult==TRUE)
        {
            //pbuf+=BUF_SIZE;
            total+=BUF_SIZE;

            pos=(int)(((float)(((float)total/(float)file_len))*100));
            //posstr.Format(_T("%d%%"),pos);

            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            //TRACE(_T("eMMC wait ack end, timercnt =%d\n"),timercnt);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                TRACE(_T("Error! eMMC Burn ACK error. %d  0x%x\n"), scnt, GetLastError());
                return FALSE;
            }

        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        scnt--;
        //TRACE(_T("eMMC  scnt =%d\n"), scnt);
        if((pos!=prepos) || pos==100)
        {
            prepos=pos;
            PostMessage(WM_MMC_PROGRESS,(LPARAM)pos,0);
        }
    }
    TRACE(_T("eMMC  rcnt =%d\n"), rcnt);
    if(rcnt>0)
    {
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf,rcnt);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }
        if(bResult==FALSE)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            TRACE(_T("Error! eMMC Burn Write error. %d  0x%x\n"), rcnt, GetLastError());
            return FALSE;
        }
        //TRACE(_T("upload %%.2f\r"),((float)total/file_len)*100);

        if(bResult==TRUE)
        {
            total+=rcnt;

            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            TRACE(_T("eMMC  ack =0x%x\n"), ack);
            if(bResult==FALSE || ack != rcnt)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                TRACE(_T("Error! eMMC Burn Read error! %d,  0x%x\n"), rcnt, GetLastError());
                return FALSE;
            }
        }
        TRACE(_T("eMMC  total =%d    file_len =%d    pos =%d\n"), total, file_len, pos);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            return FALSE;
        }

        pos=(int)(((float)(((float)total/(float)file_len))*100));

        if(pos>=100)
        {
            pos=100;
        }

        if(pos%5==0)
        {

            PostMessage(WM_MMC_PROGRESS,(LPARAM)pos,0);
        }

    }
    delete []lpBuffer;
    fclose(fp);

    unsigned int blockNum;
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&blockNum,4);
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! Read block error"));
        return FALSE;
    }

    NucUsb.NUC_CloseHandle();
    return TRUE;
}

BOOL CMMCDlg::XUSB_Verify(CString& portName,CString& m_pathName)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    unsigned int total=0,file_len,scnt,rcnt,ack;
    char* lpBuffer;
    char temp[BUF_SIZE];
    //char buf[BUF_SIZE];
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage(WM_MMC_PROGRESS,(LPARAM)0,0);
    m_progress.SetBkColor(COLOR_VERIFY);

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,MMC,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }
    if(m_type!=ENV)
    {
        fseek(fp,0,SEEK_END);
        file_len=ftell(fp);
        fseek(fp,0,SEEK_SET);
    }
    else
        file_len=0x10000;

    if(!file_len)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    m_fhead->flag=VERIFY_ACTION;

    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_MMC_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_MMC_HEAD));
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write MMC head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    scnt=(file_len)/BUF_SIZE;
    rcnt=(file_len)%BUF_SIZE;
    //TRACE(_T(" scnt = %d   rcnt = %d, file_len=0x%x(%d)\n"), scnt, rcnt, file_len, file_len);
    total=0;
    int propos=0;
    while(scnt>0)
    {
        if(m_type!=ENV)
        {
            fread(temp,BUF_SIZE,1,fp);
        }
        else
        {
            memcpy(temp,mainWnd->envbuf+total,BUF_SIZE);
        }
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer, BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(bResult==TRUE)
        {

            total+=BUF_SIZE;
            TRACE(_T("scnt = %d   total=0x%x(%d)\n"), scnt, total, total);
            pos=(int)(((float)(((float)total/(float)file_len))*100));


            if(DataCompare(temp,lpBuffer,BUF_SIZE))
                ack=BUF_SIZE;
            else
                ack=0;//compare error
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if((bResult==FALSE)||(!ack))
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                return FALSE;
            }

        }
        else
        {
            TRACE(_T("XXX scnt = %d   total=0x%x(%d)\n"), scnt, total, total);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            fclose(fp);
            return FALSE;
        }

        scnt--;
        //TRACE(_T("scnt = %d   total=0x%x(%d)\n"), scnt, total, total);
        if(pos!=propos || pos==100)
        {
            propos=pos;
            PostMessage(WM_MMC_PROGRESS,(LPARAM)pos,0);
        }
    }

    if(rcnt>0)
    {
        if(m_type!=ENV)
        {
            fread(temp,rcnt,1,fp);
        }
        else
        {
            memcpy(temp,mainWnd->envbuf+total,rcnt);
        }
        //bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,rcnt);
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(bResult==TRUE)
        {
            total+=rcnt;
            TRACE(_T("rcnt = %d   total=0x%x(%d)\n"), rcnt, total, total);
            if(DataCompare(temp,lpBuffer,rcnt))
                ack=BUF_SIZE;
            else
                ack=0;//compare error
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack, 4);
            if((bResult==FALSE)||(!ack))
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                return FALSE;

            }

        }
        else
        {
            TRACE(_T("XXX rcnt = %d\n, "), rcnt);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        pos=(int)(((float)(((float)total/(float)file_len))*100));

        if(pos>=100)
        {
            pos=100;
        }

        if(pos%5==0)
        {
            PostMessage(WM_MMC_PROGRESS,(LPARAM)pos,0);
        }

    }

    delete []lpBuffer;
    fclose(fp);
    NucUsb.NUC_CloseHandle();
    return TRUE;

}

BOOL CMMCDlg::XUSB_Read(CString& portName,CString& m_pathName,unsigned int addr,unsigned int len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    int pos=0;
    unsigned int total=0,scnt,rcnt,ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage(WM_MMC_PROGRESS,(LPARAM)0,0);
    m_progress.SetBkColor(COLOR_VERIFY);

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,MMC,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    FILE* tempfp;
    //-----------------------------------
    tempfp=_wfopen(m_pathName,_T("w+b"));
    //-----------------------------------
    if(!tempfp)
    {
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    m_fhead->flag=READ_ACTION;
    m_fhead->flashoffset=addr;
    m_fhead->filelen=len;
    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_MMC_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_MMC_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! Write MMC head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    scnt=len/BUF_SIZE;
    rcnt=len%BUF_SIZE;
    total=0;
    int prepos=0;
    while(scnt>0)
    {
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            return FALSE;
        }
        if(bResult==TRUE)
        {
            total+=BUF_SIZE;
            pos=(int)(((float)(((float)total/(float)len))*100));
            fwrite(lpBuffer,BUF_SIZE,1,tempfp);
            ack=BUF_SIZE;
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }
        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            return FALSE;
        }

        scnt--;
        if((pos!=prepos) || pos==100 )
        {
            prepos=pos;
            PostMessage(WM_MMC_PROGRESS,(LPARAM)pos,0);
        }
    }

    if(rcnt>0)
    {
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            return FALSE;
        }
        if(bResult==TRUE)
        {
            total+=rcnt;
            fwrite(lpBuffer,rcnt,1,tempfp);
            ack=BUF_SIZE;
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if(bResult!=TRUE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }
        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            return FALSE;
        }
        pos=(int)(((float)(((float)total/(float)len))*100));
        if(pos>=100)
        {
            pos=100;
        }
        if((pos!=prepos) || (pos==100))
        {
            PostMessage(WM_MMC_PROGRESS,(LPARAM)pos,0);
        }
    }
    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    fclose(tempfp);
    /********************************************/
    return TRUE;
}

BOOL CMMCDlg::XUSB_Format(CString& portName)
{
    BOOL bResult;
    CString tempstr;
    int count=0;
    unsigned int ack,format_pos=0;
    NORBOOT_MMC_HEAD *fhead;
    char* lpBuffer;
    CFormatDlg format_dlg;

    m_progress.SetRange(0,100);
    fhead=(NORBOOT_MMC_HEAD *)malloc(sizeof(NORBOOT_MMC_HEAD));

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        free(fhead);
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,MMC,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        free(fhead);
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    memset((unsigned char*)fhead,0,sizeof(NORBOOT_MMC_HEAD));
    fhead->flag=FORMAT_ACTION;
    swscanf_s(m_space,_T("%d"),&fhead->ReserveSize);
    swscanf_s(strPartitionNum,_T("%d"),&fhead->PartitionNum);
    swscanf_s(strPartition1Size,_T("%d"),&fhead->Partition1Size);
    swscanf_s(strPartition2Size,_T("%d"),&fhead->Partition2Size);
    swscanf_s(strPartition3Size,_T("%d"),&fhead->Partition3Size);
    swscanf_s(strPartition4Size,_T("%d"),&fhead->Partition4Size);

    memcpy(lpBuffer,(unsigned char*)fhead,sizeof(NORBOOT_MMC_HEAD));
    free(fhead);

    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_MMC_HEAD));
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! Write eMMC head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    format_pos=0;
    int wait_pos=0;
    while(format_pos!=100)
    {
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        //DbgOut("eMMC wait erase ack");
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
        if(bResult=FALSE)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
            return FALSE;
        }
        //DbgOut("eMMC wait erase ack end");
        if(!((ack>>16)&0xffff))
        {
            format_pos=ack&0xffff;
            PostMessage(WM_MMC_PROGRESS,(LPARAM)format_pos,0);
        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            CString msg;
            msg.Format(_T("Error! Format error 0x%08x\n"),ack);
            AfxMessageBox(msg);
            return FALSE;
        }

        if(format_pos==95)
        {
            wait_pos++;
            if(wait_pos>100)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                AfxMessageBox(_T("Error! Format error"));
                return FALSE;
            }
        }
    }

    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    return TRUE;
}
/************** MMC End ************/

/************** NAND Begin ************/
BOOL CNANDDlg::XUSB_Pack(CString& portName,CString& m_pathName,int *len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len,scnt,rcnt;
    ULONG ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(0,NAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }


    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if(!file_len)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("File length is zero\n"));
        return FALSE;
    }

    unsigned int magic;
    fread((unsigned char *)&magic,4,1,fp);
    if(magic!=0x5)
    {
        fclose(fp);
        AfxMessageBox(_T("Error! Pack Image Format Error\n"));
        return FALSE;
    }
    fseek(fp,0,SEEK_SET);

    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);

    memset((unsigned char*)m_fhead,0,sizeof(NORBOOT_NAND_HEAD));
    total=0;
    m_fhead->flag=PACK_ACTION;
    m_fhead->type=m_type;
    m_fhead->filelen=file_len;
    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    fread(lpBuffer,m_fhead->filelen,1,fp);
    if(lpBuffer[0]!=0x5)
    {
        delete []lpBuffer;
        fclose(fp);
        AfxMessageBox(_T("Error! This file is not pack image"));
    }
    fclose(fp);

    // Check DDR *ini
    bResult=CheckDDRiniData(lpBuffer, m_fhead->filelen);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! DDR Init select error\n"));
        delete []lpBuffer;
        return FALSE;
    }

    char *pbuf = lpBuffer;
    PACK_HEAD *ppackhead=(PACK_HEAD *)lpBuffer;
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, sizeof(PACK_HEAD));
    Sleep(5);
    if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT) return FALSE;
    if(bResult!=TRUE) return FALSE;
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }
    total+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);

    PACK_CHILD_HEAD child;
    m_progress.SetRange(0,short(ppackhead->num*100));
    int posnum=0;
    int prepos=0;

    unsigned int blockNum;
    for(int i=0; i<(int)(ppackhead->num); i++)
    {
        total=0;
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT) return FALSE;
        if(bResult!=TRUE) return FALSE;

        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        pbuf+= sizeof(PACK_CHILD_HEAD);
        //Sleep(1);
        scnt=child.filelen/BUF_SIZE;
        rcnt=child.filelen%BUF_SIZE;
        total=0;
        while(scnt>0)
        {
            //Sleep(20);
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=BUF_SIZE;
                total+=BUF_SIZE;

                pos=(int)(((float)(((float)total/(float)child.filelen))*100));

                TRACE(_T("NAND wait ack"));
                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                TRACE(_T("NAND wait ack end"));

                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    CString tmp;
                    tmp.Format(_T("Error! ACK error %d!"),i);
                    AfxMessageBox(tmp);
                    return FALSE;
                }

            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }

            scnt--;

            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage(WM_NAND_PROGRESS,(LPARAM)(posnum+pos),0);
            }

        }

        if(rcnt>0)
        {
            //Sleep(20);
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf,rcnt);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }

            //TRACE(_T("upload %%.2f\r"),((float)total/file_len)*100);
            if(bResult==TRUE)
            {
                pbuf+=rcnt;
                total+=rcnt;
                TRACE(_T("NAND wait ack"));
                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                TRACE(_T("NAND wait ack end"));

                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    //fclose(fp);
                    CString tmp;
                    tmp.Format(_T("ACK error(rcnt>0) %d!"),i);
                    AfxMessageBox(tmp);
                    return FALSE;

                }

            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }

            pos=(int)(((float)(((float)total/(float)child.filelen))*100));

            if(pos>=100)
            {
                pos=100;
            }
            posstr.Format(_T("%d%%"),pos);

            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage(WM_NAND_PROGRESS,(LPARAM)(posnum+pos),0);
            }

        }
        posnum+=100;

        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&blockNum,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            AfxMessageBox(_T("Error! Read block error !"));
            return FALSE;
        }
    }//for(int i=0;i<(int)(ppackhead->num);i++)

    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    return TRUE;

}

BOOL CNANDDlg::XUSB_Burn(CString& portName,CString& m_pathName,int *len,int *blockNum)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len,scnt,rcnt;
    ULONG ack;
    char* lpBuffer;
    unsigned char *ddrbuf;
    int ddrlen;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    m_progress.SetRange(0,100);
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(0,NAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        fclose(fp);
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if( (file_len>(mainWnd->m_info.Nand_uPagePerBlock*mainWnd->m_info.Nand_uPageSize))&&m_type==UBOOT)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Uboot File length cannot greater than block size\n"));
        return FALSE;
    }

    if(!file_len)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    memset((unsigned char*)m_fhead,0,sizeof(NORBOOT_NAND_HEAD));
    m_fhead->flag=WRITE_ACTION;
    ((NORBOOT_NAND_HEAD *)m_fhead)->initSize=0;

    m_fhead->filelen=file_len;
    switch(m_type)
    {
    case DATA:
    case IMAGE:
    case PACK:
        swscanf_s(_T("0"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        *len=file_len;
        lpBuffer = new char[file_len]; //read file to buffer
        memset(lpBuffer,0xff,file_len);
        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;
        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);
        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    case ENV:
        swscanf_s(_T("0"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        if(file_len>(0x10000-4))
        {
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            AfxMessageBox(_T("The environment file size is less then 64KB\n"));
            return FALSE;
        }
        lpBuffer = new char[0x10000]; //read file to buffer
        memset(lpBuffer,0x00,0x10000);

        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;

        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);
        m_fhead->filelen=0x10000;
        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    case UBOOT:
        swscanf_s(_T("1"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        //-------------------DDR---------------------
        ddrbuf=DDR2Buf(mainWnd->ShareDDRBuf,mainWnd->DDRLen,&ddrlen);
        file_len=file_len+ddrlen;
        ((NORBOOT_NAND_HEAD *)m_fhead)->initSize=ddrlen;
        //-------------------------------------------
        *len=file_len;
        lpBuffer = new char[file_len]; //read file to buffer
        memset(lpBuffer,0xff,file_len);
        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;


        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);

        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    }

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write NOR NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    switch(m_type)
    {
    case DATA:
    case IMAGE:
        fread(lpBuffer,m_fhead->filelen,1,fp);
        break;
    case PACK:
        fread(lpBuffer,m_fhead->filelen,1,fp);
        break;
    case ENV:
#if 0
        fread(lpBuffer+4,file_len,1,fp);
#else
        {
            char line[256];
            char* ptr=(char *)(lpBuffer+4);
            while (1)
            {
                if (fgets(line,256, fp) == NULL) break;
                if(line[strlen(line)-2]==0x0D || line[strlen(line)-1]==0x0A)
                {
                    strncpy(ptr,line,strlen(line)-1);
                    ptr[strlen(line)-2]=0x0;
                    ptr+=(strlen(line)-1);
                }
                else
                {
                    strncpy(ptr,line,strlen(line));
                    ptr+=(strlen(line));
                }
            }

        }
#endif

        *(unsigned int *)lpBuffer=mainWnd->CalculateCRC32((unsigned char *)(lpBuffer+4),0x10000-4);
        *len=file_len=0x10000;
        if(mainWnd->envbuf!=NULL) free(mainWnd->envbuf);
        mainWnd->envbuf=(unsigned char *)malloc(0x10000);
        memcpy(mainWnd->envbuf,lpBuffer,0x10000);
        break;
    case UBOOT:
        memcpy(lpBuffer,ddrbuf,ddrlen);
        fread(lpBuffer+ddrlen,m_fhead->filelen,1,fp);
        break;
    }

    scnt=file_len/BUF_SIZE;
    rcnt=file_len%BUF_SIZE;

    total=0;
    char *pbuf = lpBuffer;
    while(scnt>0)
    {
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, BUF_SIZE);
        pbuf+=BUF_SIZE;
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        if(bResult==TRUE)
        {
            total+=BUF_SIZE;

            pos=(int)(((float)(((float)total/(float)file_len))*100));
            posstr.Format(_T("%d%%"),pos);

            TRACE(_T("NAND wait ack\n"));
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            TRACE(_T("NAND wait ack end\n"));

            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                return FALSE;
            }

        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        scnt--;

        if(pos%5==0)
        {
            PostMessage(WM_NAND_PROGRESS,(LPARAM)pos,0);
        }
    }

    if(rcnt>0)
    {
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf,rcnt);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        //TRACE(_T("upload %%.2f\r"),((float)total/file_len)*100);
        if(bResult==TRUE)
        {
            total+=rcnt;
            TRACE(_T("NAND wait ack\n"));
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            TRACE(_T("NAND wait ack end\n"));

            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                return FALSE;

            }

        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        pos=(int)(((float)(((float)total/(float)file_len))*100));

        if(pos>=100)
        {
            pos=100;
        }

        if(pos%5==0)
        {
            PostMessage(WM_NAND_PROGRESS,(LPARAM)pos,0);
        }

    }
    delete []lpBuffer;
    fclose(fp);

    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)blockNum,4);
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! Read block error."));
        return FALSE;
    }

    NucUsb.NUC_CloseHandle();
    return TRUE;
}

BOOL CNANDDlg::XUSB_BurnWithOOB(CString& portName,CString& m_pathName,int *len,int *blockNum)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len,scnt,rcnt;
    ULONG ack;
    char* lpBuffer;
    unsigned char *ddrbuf;
    int ddrlen;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    m_progress.SetRange(0,100);
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(0,NAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }


    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if(file_len%(mainWnd->m_info.Nand_uPageSize+mainWnd->m_info.Nand_uSpareSize)!=0)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is not Multiple of page+spare size \n"));
        return FALSE;
    }

    if(!file_len)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    memset((unsigned char*)m_fhead,0,sizeof(NORBOOT_NAND_HEAD));
    m_fhead->flag=WRITE_ACTION;
    ((NORBOOT_NAND_HEAD *)m_fhead)->initSize=0;

    m_fhead->filelen=file_len;

    switch(m_type)
    {
    case DATA:
    case IMAGE:
    case PACK:
        swscanf_s(_T("0"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        *len=file_len;
        lpBuffer = new char[file_len]; //read file to buffer
        memset(lpBuffer,0xff,file_len);
        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;
        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);
        m_fhead->type=DATA_OOB;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    case ENV:
        swscanf_s(_T("0"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        if(file_len>(0x10000-4))
        {
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            AfxMessageBox(_T("Error! The environment file size is less then 64KB\n"));
            return FALSE;
        }
        lpBuffer = new char[0x10000]; //read file to buffer
        memset(lpBuffer,0x00,0x10000);

        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;

        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);
        m_fhead->filelen=0x10000;
        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    case UBOOT:
        swscanf_s(_T("1"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        //-------------------DDR---------------------
        ddrbuf=DDR2Buf(mainWnd->ShareDDRBuf,mainWnd->DDRLen,&ddrlen);
        file_len=file_len+ddrlen;
        ((NORBOOT_NAND_HEAD *)m_fhead)->initSize=ddrlen;
        //-------------------------------------------
        *len=file_len;
        lpBuffer = new char[file_len]; //read file to buffer
        memset(lpBuffer,0xff,file_len);
        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;

        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);

        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    }

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write NOR NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    switch(m_type)
    {
    case DATA:
    case IMAGE:
        fread(lpBuffer,m_fhead->filelen,1,fp);
        break;
    case PACK:
        fread(lpBuffer,m_fhead->filelen,1,fp);
        break;
    case ENV:
#if 0
        fread(lpBuffer+4,file_len,1,fp);
#else
        {
            char line[256];
            char* ptr=(char *)(lpBuffer+4);
            while (1)
            {
                if (fgets(line,256, fp) == NULL) break;
                if(line[strlen(line)-2]==0x0D || line[strlen(line)-1]==0x0A)
                {
                    strncpy(ptr,line,strlen(line)-1);
                    ptr[strlen(line)-2]=0x0;
                    ptr+=(strlen(line)-1);
                }
                else
                {
                    strncpy(ptr,line,strlen(line));
                    ptr+=(strlen(line));
                }
            }

        }
#endif

        *(unsigned int *)lpBuffer=mainWnd->CalculateCRC32((unsigned char *)(lpBuffer+4),0x10000-4);
        *len=file_len=0x10000;
        if(mainWnd->envbuf!=NULL) free(mainWnd->envbuf);
        mainWnd->envbuf=(unsigned char *)malloc(0x10000);
        memcpy(mainWnd->envbuf,lpBuffer,0x10000);
        break;
    case UBOOT:
        memcpy(lpBuffer,ddrbuf,ddrlen);
        fread(lpBuffer+ddrlen,m_fhead->filelen,1,fp);
        break;
    }

    int BurnBufLen=(mainWnd->m_info.Nand_uPageSize+mainWnd->m_info.Nand_uSpareSize);
    scnt=file_len/BurnBufLen;
    rcnt=file_len%BurnBufLen;

    if(rcnt!=0)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length error"));
        return 0;
    }

    total=0;
    char *pbuf = lpBuffer;
    while(scnt>0)
    {
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, BurnBufLen);
        pbuf+=BurnBufLen;
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(bResult==TRUE)
        {
            total+=BurnBufLen;

            pos=(int)(((float)(((float)total/(float)file_len))*100));

            TRACE(_T("NAND wait ack"));
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            TRACE(_T("NAND wait ack end"));

            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                return FALSE;
            }

        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        scnt--;

        if(pos%5==0)
        {
            PostMessage(WM_NAND_PROGRESS,(LPARAM)pos,0);
        }

    }

    if(rcnt>0)
    {
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf,rcnt);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        //TRACE(_T("upload %%.2f\r"),((float)total/file_len)*100);
        if(bResult==TRUE)
        {
            total+=rcnt;
            TRACE(_T("NAND wait ack"));
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            TRACE(_T("NAND wait ack end"));

            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                return FALSE;

            }

        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        pos=(int)(((float)(((float)total/(float)file_len))*100));

        if(pos>=100)
        {
            pos=100;
        }

        if(pos%5==0)
        {
            PostMessage(WM_NAND_PROGRESS,(LPARAM)pos,0);
        }

    }
    delete []lpBuffer;
    fclose(fp);

    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)blockNum,4);
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! Read block error!"));
        return FALSE;
    }

    NucUsb.NUC_CloseHandle();
    return TRUE;
}

int CNANDDlg::XUSB_Verify(CString& portName,CString& m_pathName)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    unsigned int total=0,file_len,scnt,rcnt,ack;
    char* lpBuffer;
    char temp[BUF_SIZE];
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage(WM_NAND_PROGRESS,(LPARAM)0,0);
    m_progress.SetBkColor(COLOR_VERIFY);

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,NAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    if(m_type!=ENV)
    {
        fseek(fp,0,SEEK_END);
        file_len=ftell(fp);
        fseek(fp,0,SEEK_SET);
    }
    else
    {
        file_len=0x10000;
    }

    if(!file_len)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    if(m_type==PACK)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        return NAND_VERIFY_PACK_ERROR;
    }

    if(m_type==IMAGE && (file_len%512)!=0)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        return NAND_VERIFY_FILESYSTEM_ERROR;
    }

    m_fhead->flag=VERIFY_ACTION;

    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    scnt=file_len/BUF_SIZE;
    rcnt=file_len%BUF_SIZE;
    total=0;
    int prepos=0;
    while(scnt>0)
    {
        if(m_type!=ENV)
        {
            fread(temp,BUF_SIZE,1,fp);
        }
        else
        {
            memcpy(temp,mainWnd->envbuf+total,BUF_SIZE);
        }
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer, BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(bResult==TRUE)
        {
            total+=BUF_SIZE;

            pos=(int)(((float)(((float)total/(float)file_len))*100));

            if(DataCompare(temp,lpBuffer,BUF_SIZE))
                ack=BUF_SIZE;
            else
                ack=0;//compare error
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if((bResult==FALSE)||(!ack))
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                return FALSE;
            }

        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        scnt--;

        if(pos-prepos>=5 || pos==100)
        {
            prepos=pos;
            PostMessage(WM_NAND_PROGRESS,(LPARAM)pos,0);
        }
    }

    if(rcnt>0)
    {
        if(m_type!=ENV)
        {
            fread(temp,rcnt,1,fp);
        }
        else
        {
            memcpy(temp,mainWnd->envbuf+total,rcnt);
        }

        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(bResult==TRUE)
        {
            total+=rcnt;
            if(DataCompare(temp,lpBuffer,rcnt))
                ack=BUF_SIZE;
            else
                ack=0;//compare error
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack, 4);
            if((bResult==FALSE)||(!ack))
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                return FALSE;

            }

        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        pos=(int)(((float)(((float)total/(float)file_len))*100));

        if(pos>=100)
        {
            pos=100;
        }
        if(pos-prepos>=5 || pos==100)
        {
            prepos=pos;
            PostMessage(WM_NAND_PROGRESS,(LPARAM)pos,0);
        }

    }

    delete []lpBuffer;
    fclose(fp);
    NucUsb.NUC_CloseHandle();
    return TRUE;

}

BOOL CNANDDlg::XUSB_Read_Redunancy(CString& portName,CString& m_pathName,unsigned int addr,unsigned int len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    int pos=0;
    unsigned int total=0,scnt,ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage(WM_NAND_PROGRESS,(LPARAM)0,0);
    m_progress.SetBkColor(COLOR_VERIFY);

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,NAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    FILE* tempfp;
    //-----------------------------------
    tempfp=_wfopen(m_pathName,_T("w+b"));
    //-----------------------------------
    if(!tempfp)
    {
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    unsigned int sparesize,totalsize;
    sparesize=mainWnd->m_info.Nand_uSpareSize;
    totalsize=mainWnd->m_info.Nand_uPagePerBlock*(mainWnd->m_info.Nand_uPageSize+mainWnd->m_info.Nand_uSpareSize);
    m_fhead->flag=READ_ACTION;
    m_fhead->flashoffset=addr;
    m_fhead->filelen=len*(totalsize);
    m_fhead->initSize=1;  //read redundancy data, good block and bad block

    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(tempfp);
        AfxMessageBox(_T("Error! Write NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(tempfp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    unsigned int alen=len*(totalsize);
    total=0;
    for(unsigned int i=0; i<len; i++)
    {
        scnt=totalsize/(mainWnd->m_info.Nand_uPageSize+mainWnd->m_info.Nand_uSpareSize);
        while(scnt>0)
        {
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,mainWnd->m_info.Nand_uPageSize);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }
            if(bResult==TRUE)
            {
                total+=mainWnd->m_info.Nand_uPageSize;
                pos=(int)(((float)(((float)total/(float)alen))*100));
                fwrite(lpBuffer,mainWnd->m_info.Nand_uPageSize,1,tempfp);
                ack=mainWnd->m_info.Nand_uPageSize;
                bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    fclose(tempfp);
                    return FALSE;
                }
            }
            else
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }

            scnt--;
            if(pos%5==0)
            {
                PostMessage(WM_NAND_PROGRESS,(LPARAM)pos,0);
            }
            //spare size---------------------------------------------------------
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,sparesize);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }
            if(bResult==TRUE)
            {
                total+=sparesize;
                pos=(int)(((float)(((float)total/(float)alen))*100));
                fwrite(lpBuffer,sparesize,1,tempfp);
                ack=sparesize;
                bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    fclose(tempfp);
                    return FALSE;
                }
            }
            else
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }
            if(pos%5==0)
            {
                PostMessage(WM_NAND_PROGRESS,(LPARAM)pos,0);
            }
        }//while(scnt>0) end

    }

    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    fclose(tempfp);
    /********************************************/
    return TRUE;
}

BOOL CNANDDlg::XUSB_Read(CString& portName,CString& m_pathName,unsigned int addr,unsigned int len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    int pos=0;
    unsigned int total=0,scnt,rcnt,ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage(WM_NAND_PROGRESS,(LPARAM)0,0);
    m_progress.SetBkColor(COLOR_VERIFY);

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,NAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    FILE* tempfp;
    //-----------------------------------
    tempfp=_wfopen(m_pathName,_T("w+b"));
    //-----------------------------------
    if(!tempfp)
    {
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    m_fhead->flag=READ_ACTION;
    m_fhead->flashoffset=addr;
    m_fhead->filelen=len;
    m_fhead->initSize=0; //read good block
    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(tempfp);
        AfxMessageBox(_T("Error! Write NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(tempfp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    scnt=len/BUF_SIZE;
    rcnt=len%BUF_SIZE;
    total=0;
    while(scnt>0)
    {
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        if(bResult==TRUE)
        {
            total+=BUF_SIZE;
            pos=(int)(((float)(((float)total/(float)len))*100));
            fwrite(lpBuffer,BUF_SIZE,1,tempfp);
            ack=BUF_SIZE;
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }
        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }

        scnt--;
        if(pos%5==0)
        {
            PostMessage(WM_NAND_PROGRESS,(LPARAM)pos,0);
        }
    }

    if(rcnt>0)
    {
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer, rcnt);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        if(bResult==TRUE)
        {
            total+=rcnt;
            fwrite(lpBuffer,rcnt,1,tempfp);
            ack=BUF_SIZE;
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if(bResult!=TRUE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }
        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        pos=(int)(((float)(((float)total/(float)len))*100));
        if(pos>=100)
        {
            pos=100;
        }
        if(pos%5==0)
        {
            PostMessage(WM_NAND_PROGRESS,(LPARAM)pos,0);
        }
    }
    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    fclose(tempfp);
    /********************************************/
    return TRUE;
}

BOOL CNANDDlg::XUSB_Erase(CString& portName)
{
    BOOL bResult;
    CString tempstr;
    int count=0;
    unsigned int ack,erase_pos=0;
    NORBOOT_NAND_HEAD *fhead;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    fhead=(NORBOOT_NAND_HEAD *)malloc(sizeof(NORBOOT_NAND_HEAD));

    m_progress.SetRange(0,100);
    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        free(fhead);
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        free(fhead);
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,NAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        free(fhead);
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    memset((unsigned char*)fhead,0,sizeof(NORBOOT_NAND_HEAD));
    fhead->flag=ERASE_ACTION;

    fhead->flashoffset = _wtoi(m_sblocks); //start erase block
    fhead->execaddr=_wtoi(m_blocks);  //erase block length
    fhead->type=m_erase_flag; // Decide chip erase mode or erase mode

    if(mainWnd->ChipEraseWithBad==0)
        fhead->no=0xFFFFFFFF;//erase good block
    else
        fhead->no=0xFFFFFFFE;//erase good block and bad block
    memcpy(lpBuffer,(unsigned char*)fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
    free(fhead);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! Write NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    erase_pos=0;
    int wait_pos=0;
    while(erase_pos!=100)
    {
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        Sleep(2);
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
        if(bResult=FALSE)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
            return FALSE;
        }

        if(!((ack>>16)&0xffff))
        {
            erase_pos=ack&0xffff;
            PostMessage(WM_NAND_PROGRESS,(LPARAM)erase_pos,0);
        }
        else
        {

            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            CString msg;
            msg.Format(_T("Error! Erase error. 0x%08x\n"),ack);
            AfxMessageBox(msg);
            return FALSE;
        }

        if(erase_pos==95)
        {
            wait_pos++;
            if(wait_pos>100)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                AfxMessageBox(_T("Error! Erase error."));
                return FALSE;
            }
        }
    }


    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    return TRUE;

}
/************** NAND End ************/

/************** Fast Begin ************/

BOOL FastDlg::XUSB_FastNANDErase(int id, CString& portName)
{
    BOOL bResult;
    CString tempstr;
    int count=0;
    unsigned int ack,erase_pos=0;
    NORBOOT_NAND_HEAD *fhead;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    fhead=(NORBOOT_NAND_HEAD *)malloc(sizeof(NORBOOT_NAND_HEAD));

    /***********************************************/
    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(bResult==FALSE)
    {
        free(fhead);
        //AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(id,NAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        free(fhead);
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) NAND Erase error\n"),id);
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    memset((unsigned char*)fhead,0,sizeof(NORBOOT_NAND_HEAD));
    fhead->flag=ERASE_ACTION;

    fhead->flashoffset = 0; //start erase block
    fhead->execaddr= 20;//erase block length
    fhead->type= 0;// chip erase mode

    if(mainWnd->ChipEraseWithBad==0)
        fhead->no=0xFFFFFFFF;//erase good block
    else
        fhead->no=0xFFFFFFFE;//erase good block and bad block

    memcpy(lpBuffer,(unsigned char*)fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
    free(fhead);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        //TRACE(_T("XXX (%d) NAND Erase head error\n"),id);
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE || ack != 0x83)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        //TRACE(_T("XXX (%d) NAND Erase error\n"),id);
        return FALSE;
    }

    erase_pos=0;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,100);
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetBkColor(COLOR_ERASE);
    int wait_pos=0;
    while(erase_pos!=100)
    {
        if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            //TRACE(_T("XXX (%d) NAND Erase error\n"),id);
            return FALSE;
        }

        //TRACE(_T("(%d) NAND wait erase ack\n"), id);
        Sleep(2);
        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        //TRACE("debug xusb.cpp  66   id= %d, bResult = %d, ack=0x%x\n", id, bResult, ack);
        if(ack == 0xffff)    // Storage error
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) NAND Erase error\n"),id);
            return FALSE;
        }

        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            //TRACE(_T("(%d) #5872 NAND Erase error\n"),id);
            return FALSE;
        }

        //TRACE(_T("(%d) NAND wait erase ack end\n"), id);
        //TRACE("debug xusb.cpp  77   ack=0x%x,  ((ack>>16)&0xffff)=0x%x\n", ack, ((ack>>16)&0xffff));
        if(!((ack>>16)&0xffff))
        {
            erase_pos=ack&0xffff;
            PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)erase_pos,0);
        }
        else
        {

            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) NAND Erase error. ack = 0x%x\n"),id, ack);
            return FALSE;
        }

        if(erase_pos==95)
        {
            wait_pos++;
            if(wait_pos>100)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                //TRACE(_T("XXX (%d) NAND Erase error\n"),id);
                return FALSE;
            }
        }
    }

    delete []lpBuffer;
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}


BOOL FastDlg::XUSB_FastNANDBurn(int id, CString& portName,CString& m_pathName,int *len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len,scnt,rcnt, totalsize;
    ULONG ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    bResult=NucUsb.EnableOneWinUsbDevice(id);
    //TRACE("debug xusb.cpp  99   id= %d, bResult = %d \n", id, bResult);
    if(!bResult)
    {
        //AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }

#if(0)
    bResult=NucUsb.NUC_CheckFw(id);

    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif

    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(id,NAND,(UCHAR *)&typeack,sizeof(typeack));
    //TRACE("debug xusb.cpp  aa   id= %d, bResult = %d \n", id, bResult);
    if(bResult==FALSE)
    {
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) NAND Burn NUC_SetType error\n"),id);
        return FALSE;
    }


    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        NucUsb.CloseWinUsbDevice(id);
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if(!file_len)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    unsigned int magic;
    fread((unsigned char *)&magic,4,1,fp);
    if(magic!=0x5)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        //AfxMessageBox(_T("Error! Pack Image Format Error\n"));
        return ERR_PACK_FORMAT;
    }
    fseek(fp,0,SEEK_SET);

    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);

    memset((unsigned char*)m_fhead_nand,0,sizeof(NORBOOT_NAND_HEAD));
    total=0;
    m_fhead_nand->flag=PACK_ACTION;
    m_fhead_nand->type=m_type;
    m_fhead_nand->filelen=file_len;
    memcpy(lpBuffer,(unsigned char*)m_fhead_nand,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) NAND Burn error\n"),id);
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) NAND Burn error\n"),id);
        return FALSE;
    }

    fread(lpBuffer,m_fhead_nand->filelen,1,fp);
    if(lpBuffer[0]!=0x5)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        //AfxMessageBox(_T("This file is not pack image"));
        return ERR_PACK_FORMAT;
    }
    fclose(fp);

    // Check DDR *ini
    bResult=CheckDDRiniData(lpBuffer, m_fhead_nand->filelen);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        //AfxMessageBox(_T("DDR Init select error\n"));
        return ERR_DDRINI_DATACOM;
    }

    char *pbuf = lpBuffer;
    PACK_HEAD *ppackhead=(PACK_HEAD *)lpBuffer;
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, sizeof(PACK_HEAD));
    //Sleep(5);
    if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) NAND Burn error\n"),id);
        return FALSE;
    }

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) NAND Burn error\n"),id);
        return FALSE;
    }

    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) NAND Burn error\n"),id);
        return FALSE;
    }

    total+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);
    PACK_CHILD_HEAD child;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,short(ppackhead->num*100));
    int posnum=0;
    int prepos=0;

    unsigned int blockNum;
    totalsize = 0;
    for(int i=0; i<(int)(ppackhead->num); i++)
    {
        total=0;
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
        //Sleep(1);
        if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) NAND Burn error\n"),id);
            return FALSE;
        }
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) NAND Burn error\n"),id);
            return FALSE;
        }

        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) NAND Burn error\n"),id);
            return FALSE;
        }

        pbuf+= sizeof(PACK_CHILD_HEAD);
        //Sleep(1);
        scnt=child.filelen/BUF_SIZE;
        rcnt=child.filelen%BUF_SIZE;
        total=0;
        while(scnt>0)
        {
            //Sleep(1);
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                //TRACE("event 01\n");
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) NAND Burn error! scnt = %d\n"),id, scnt);
                return FALSE;
            }
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) NAND Burn error! scnt = %d\n"),id, scnt);
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=BUF_SIZE;
                total+=BUF_SIZE;
                totalsize += BUF_SIZE;

                pos=(int)(((float)(((float)totalsize/(float)file_len))*100));

                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
                if(bResult==FALSE || ack==0xffff)    // Storage error
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) NAND Burn error! scnt = %d, ack= 0x%x\n"),id, scnt, ack);
                    return FALSE;
                }
            }

            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject error! scnt = %d, ack= 0x%x\n"),id, scnt, ack);
                return FALSE;
            }

            scnt--;

            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                if((scnt % 4 == 0) || pos == 100)
                {
                    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)(pos),0);
                }
            }

        }
        if(rcnt>0)
        {
            //Sleep(20);
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf,rcnt);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject error! rcnt = %d, ack= 0x%x\n"),id, rcnt, ack);
                return FALSE;
            }

            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) NAND Burn error! rcnt = %d\n"),id, rcnt);
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=rcnt;
                total+=rcnt;
                totalsize +=rcnt;

                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) NAND Burn error! rcnt = %d\n"),id, rcnt);
                    return FALSE;
                }
            }
            //TRACE("OneDeviceXUSB_Pack:device %d,08\n",id);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject error! rcnt = %d\n"),id, rcnt);
                return FALSE;
            }

            pos=(int)(((float)(((float)totalsize/(float)file_len))*100));
            if(pos>=100)
            {
                pos=100;
            }

            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                if(pos%5==0)
                {
                    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
                }
            }

        }
        //posnum+=100;
        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&blockNum,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) XUSB_FastNANDBurn failed!\n"), id);
            return FALSE;
        }
    }//for(int i=0;i<(int)(ppackhead->num);i++)

    delete []lpBuffer;
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}

int FastDlg::XUSB_FastNANDVerify(int id, CString& portName, CString& m_pathName)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    unsigned int total=0,file_len,scnt,rcnt,ack;
    char* lpBuffer;
    char temp[BUF_SIZE];
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)0,0);

    /***********************************************/
    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(bResult==FALSE)
    {
        //AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(id,NAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.CloseWinUsbDevice(id);
        return FALSE;
    }

    fp=_wfopen(m_pathName,_T("rb"));
    if(!fp)
    {
        NucUsb.CloseWinUsbDevice(id);
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(!file_len)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }


    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);

    fread(lpBuffer,file_len,1,fp);
    PACK_HEAD *ppackhead =(PACK_HEAD *)lpBuffer;

    int imagenum = 0;
    ppackhead->fileLength = ((lpBuffer[7]&0xff) << 24 | (lpBuffer[6]&0xff) << 16 | (lpBuffer[5]&0xff) << 8 | (lpBuffer[4]&0xff));
    ppackhead->num = ((lpBuffer[11]&0xff) << 24 | (lpBuffer[10]&0xff) << 16 | (lpBuffer[9]&0xff) << 8 | (lpBuffer[8]&0xff));
    imagenum = ppackhead->num;

    char *pbuf = lpBuffer;
    total+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);

    PACK_CHILD_HEAD child;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,100);
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetBkColor(COLOR_VERIFY);
    int posnum=0;
    int prepos=0;

    memset(m_fhead_nand, 0x00, sizeof(NORBOOT_NAND_HEAD));
    m_fhead_nand->flag=PACK_VERIFY_ACTION;
    //TRACE(_T("%x  %x  %x %x\n"),lpBuffer[16], lpBuffer[17], lpBuffer[18], lpBuffer[19]);
    m_fhead_nand->filelen= ((lpBuffer[19]&0xff) << 24 | (lpBuffer[18]&0xff) << 16 | (lpBuffer[17]&0xff) << 8 | (lpBuffer[16]&0xff)); // child1 file len
    m_fhead_nand->flashoffset = ((lpBuffer[23]&0xff) << 24 | (lpBuffer[22]&0xff) << 16 | (lpBuffer[21]&0xff) << 8 | (lpBuffer[20]&0xff)); // child1 start address
    m_fhead_nand->type = ((lpBuffer[27]&0xff) << 24 | (lpBuffer[26]&0xff) << 16 | (lpBuffer[25]&0xff) << 8 | (lpBuffer[24]&0xff)); // child1 image type
    m_fhead_nand->no = imagenum;
    memcpy(temp,(unsigned char*)m_fhead_nand,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)temp, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) NAND Verify Write head error\n"), id);
        return FALSE;
    }
    //Sleep(5);
    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) NAND Verify Read head error !\n"), id);
        return FALSE;
    }

    for(int i=0; i<(int)imagenum; i++)
    {
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        memcpy(temp,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)temp, sizeof(PACK_CHILD_HEAD));
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            fclose(fp);
            TRACE(_T("XXX (%d) Write NAND head error\n"), id);
            return FALSE;
        }
        //Sleep(5);
        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            fclose(fp);
            TRACE(_T("XXX (%d) NAND Verify error\n"), id);
            return FALSE;
        }
        total+= sizeof(PACK_CHILD_HEAD);
        pbuf+= sizeof(PACK_CHILD_HEAD);

        int ddrlen;
        if(child.imagetype == UBOOT)
        {
            ddrlen = (((mainWnd->DDRLen+8+15)/16)*16);
            pbuf += ddrlen + IBR_HEADER_LEN;
            total+= ddrlen + IBR_HEADER_LEN;
            TRACE(_T("ddrlen = 0x%x(%d)    mainWnd->DDRLen = 0x%x(%d)\n"), ddrlen, ddrlen, mainWnd->DDRLen, mainWnd->DDRLen);
            // send DDR parameter Length
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ddrlen,4);
            //Sleep(5);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) NUC_WritePipe DDR Length error\n"), id);
                return FALSE;
            }

            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) NUC_ReadPipe DDR Length error\n"), id);
                return FALSE;
            }
        }

        if(child.imagetype == UBOOT)
        {
            scnt=(child.filelen-ddrlen-IBR_HEADER_LEN)/BUF_SIZE;
            rcnt=(child.filelen-ddrlen-IBR_HEADER_LEN)%BUF_SIZE;
        }
        else
        {
            scnt=child.filelen/BUF_SIZE;
            rcnt=child.filelen%BUF_SIZE;
        }
        //TRACE("child.filelen = %d, scnt=%d, rcnt =%d\n", child.filelen, scnt, rcnt);
        int prepos=0;
        while(scnt>0)
        {
            //TRACE("scnt=0x%x(%d)\n", scnt, scnt);
            if(child.imagetype !=ENV)
            {
                fseek(fp,total,SEEK_SET);
                fread(temp,BUF_SIZE,1,fp);
            }
            else
            {
                fseek(fp,total,SEEK_SET);
                fread(temp,BUF_SIZE,1,fp);
            }
            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)lpBuffer, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) NAND Verify error. scnt=%d\n"), id, scnt);
                return FALSE;
            }

            if(bResult==TRUE)
            {
                total+=BUF_SIZE;
                pbuf+=BUF_SIZE;

                pos=(int)(((float)(((float)total/(float)file_len))*100));
                if(DataCompare(temp,lpBuffer,BUF_SIZE))
                    ack=BUF_SIZE;
                else
                    ack=0;//compare error

                bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ack,4);
                if((bResult==FALSE)||(!ack))
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) NAND Verify error. scnt=%d\n"), id, scnt);
                    //return FALSE;
                    return ERR_VERIFY_DATACOM;
                }

            }
            else
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) NAND Verify error. scnt=%d\n"), id, scnt);
                return FALSE;
            }

            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) WaitForSingleObject error. scnt=%d\n"), id, scnt);
                return FALSE;
            }

            scnt--;

            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
            }

        }

        int temp_len = 0;
        if(rcnt>0)
        {
            memset((char *)&temp, 0xff, BUF_SIZE);
            if(child.imagetype != ENV)
            {
                fseek(fp,total,SEEK_SET);
                fread(temp,rcnt,1,fp);
                temp_len = rcnt;
            }
            else
            {
                fseek(fp,total,SEEK_SET);
                fread(temp,BUF_SIZE,1,fp);
                temp_len = BUF_SIZE;
            }

            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)lpBuffer,BUF_SIZE);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) WaitForSingleObject error. rcnt=%d\n"), id, rcnt);
                return FALSE;
            }

            if(bResult==TRUE)
            {
                total+=temp_len;
                pbuf+=temp_len;
                if(DataCompare(temp,lpBuffer,temp_len))
                    ack=BUF_SIZE;
                else
                    ack=0;//compare error

                bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ack, 4);
                if((bResult==FALSE)||(!ack))
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) NAND Verify error. rcnt=%d  ack= 0x%x\n"), id, rcnt, ack);
                    return ERR_VERIFY_DATACOM;

                }

            }
            else
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) NAND Verify error. rcnt=%d\n"), id, rcnt);
                return FALSE;
            }

            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) NAND Verify error. rcnt=%d\n"), id, rcnt);
                return FALSE;
            }

            pos=(int)(((float)(((float)total/(float)file_len))*100));
            if(pos>=100)
            {
                pos=100;
            }
            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
            }
        }
    }

    delete []lpBuffer;
    fclose(fp);
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}

BOOL FastDlg::XUSB_FastSPIErase(int id, CString& portName)
{
    BOOL bResult;
    CString tempstr;
    int count=0;
    unsigned int ack,erase_pos=0;
    NORBOOT_NAND_HEAD *fhead;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    fhead=(NORBOOT_NAND_HEAD *)malloc(sizeof(NORBOOT_NAND_HEAD));

    /***********************************************/
    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(bResult==FALSE)
    {
        //AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        free(fhead);
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        free(fhead);
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(id,SPI,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.CloseWinUsbDevice(id);
        free(fhead);
        TRACE(_T("XXX (%d) SPI NUC_SetType error.\n"), id);
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    memset((unsigned char*)fhead,0,sizeof(NORBOOT_NAND_HEAD));
    fhead->flag=ERASE_ACTION;
    fhead->flashoffset = 0; //start erase block
    fhead->execaddr=1; //erase block length
    fhead->type=0; // Decide chip erase mode
    fhead->no=0xffffffff;//erase all

    memcpy(lpBuffer,(unsigned char*)fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
    free(fhead);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) SPI Erase head error.\n"), id);
        return FALSE;
    }

    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE || ack!=0x85)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) SPI Erase head error. ack= 0x%x\n"), id, ack);
        return FALSE;
    }

    erase_pos=0;
    int wait_pos=0;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,100);

    while(erase_pos!=100)
    {
        if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI Erase WaitForSingleObject error"), id);
            return FALSE;
        }

        //TRACE(_T("SPI Erase (%d) wait erase ack\n"), id);
        //Sleep(10);
        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        if(bResult=FALSE || ack==0xFFFFFFFF)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI Erase error. ack= 0x%x\n"), id, ack);
            return FALSE;
        }

        //TRACE(_T("SPI Erase (%d) wait erase ack end\n"), id);
        if(!((ack>>16)&0xffff))
        {
            erase_pos=ack&0xffff;
            PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)erase_pos,0);
        }
        else
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI Erase error. ack= 0x%x\n"), id, ack);
            return FALSE;
        }
#if(0)
        if(erase_pos==95)
        {
            wait_pos++;
            if(wait_pos>100)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) SPI Erase error.\n"), id);
                return FALSE;
            }
        }
#endif
    }

    delete []lpBuffer;
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}

BOOL FastDlg::XUSB_FastSPIBurn(int id, CString& portName,CString& m_pathName,int *len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len;//,scnt,rcnt,totalsize;
    ULONG ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(!bResult)
    {
        //AfxMessageBox(_T("Error! Device Enable error\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(id,SPI,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) SPI Burn NUC_SetType error.\n"), id);
        return FALSE;
    }

    fp=_wfopen(m_pathName,_T("rb"));
    if(!fp)
    {
        NucUsb.CloseWinUsbDevice(id);
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if(!file_len)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    unsigned int magic;
    fread((unsigned char *)&magic,4,1,fp);
    if(magic!=0x5)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        AfxMessageBox(_T("Error! Pack Image Format Error\n"));
        return ERR_PACK_FORMAT;
    }

    fseek(fp,0,SEEK_SET);
    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);
    memset((unsigned char*)m_fhead_spi,0,sizeof(NORBOOT_NAND_HEAD));
    total=0;
    m_fhead_spi->flag=PACK_ACTION;
    m_fhead_spi->type=m_type;
    m_fhead_spi->filelen=file_len;
    memcpy(lpBuffer,(unsigned char*)m_fhead_spi,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) SPI Burn error. 0x%x\n"), id, GetLastError());
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) SPI Burn error. 0x%x\n"), id, GetLastError());
        return FALSE;
    }

    fread(lpBuffer,m_fhead_spi->filelen,1,fp);
    if(lpBuffer[0]!=0x5)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        //AfxMessageBox(_T("Error! This file is not pack image"));
        return ERR_PACK_FORMAT;
    }
    fclose(fp);

    // Check DDR *ini
    bResult=CheckDDRiniData(lpBuffer, m_fhead_spi->filelen);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        //AfxMessageBox(_T("Error! DDR Init select error\n"));
        return ERR_DDRINI_DATACOM;
    }

    char *pbuf = lpBuffer;
    PACK_HEAD *ppackhead=(PACK_HEAD *)lpBuffer;
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, sizeof(PACK_HEAD));
    //Sleep(5);
    if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) SPI Burn WaitForSingleObject error. 0x%x\n"), id, GetLastError());
        return FALSE;
    }
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) SPI Burn error. 0x%x\n"), id, GetLastError());
        return FALSE;
    }

    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) SPI Burn error. 0x%x\n"), id, GetLastError());
        return FALSE;
    }

    total+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);

#if(0) //image size = DDR size – 1M - 64k
    PACK_CHILD_HEAD child;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,short(ppackhead->num*200));

    int posnum=0;
    int prepos=0;
    totalsize = 0;
    for(int i=0; i<(int)(ppackhead->num); i++)
    {
        if(i == 1)
            TRACE(_T("image = 3 \n"));
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        //Sleep(20);
        bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
        if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI Burn WaitForSingleObject error. 0x%x\n"), id, GetLastError());
            return FALSE;
        }
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI Burn error. 0x%x\n"), id, GetLastError());
            return FALSE;
        }

        total+= sizeof(PACK_CHILD_HEAD);
        pbuf+= sizeof(PACK_CHILD_HEAD);
        scnt=child.filelen/BUF_SIZE;
        rcnt=child.filelen%BUF_SIZE;
        //TRACE(_T("(%d) #6768 SPI Burn scnt = %d  rcnt %d \n"), id, scnt, rcnt);
        while(scnt>0)
        {
            //Sleep(20);
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject SPI Burn error. scnt = %d  0x%x \n"), id, scnt, GetLastError());
                return FALSE;
            }
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) SPI Burn error. scnt = %d  0x%x \n"), id, scnt, GetLastError());
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=BUF_SIZE;
                total+=BUF_SIZE;
                totalsize+=BUF_SIZE;

                pos=(int)(((float)(((float)total/(float)child.filelen))*100));

                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
                if(bResult==FALSE || ack==0xffff)    // Storage error
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) SPI Burn error. scnt = %d, ack= 0x%x, 0x%x\n"), id, scnt, ack, GetLastError());
                    return FALSE;
                }
            }

            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) SPI Burn WaitForSingleObject error. scnt = %d, 0x%x\n"), id, scnt, GetLastError());
                return FALSE;
            }

            scnt--;
        }

        if(rcnt>0)
        {
            //Sleep(20);
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf,rcnt);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) SPI Burn WaitForSingleObject error. rcnt = %d, 0x%x\n"), id, rcnt, GetLastError());
                return FALSE;
            }
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) SPI Burn error. rcnt = %d, 0x%x\n"), id, rcnt, GetLastError());
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=rcnt;
                total+=rcnt;
                totalsize+=rcnt;
                //TRACE(_T("SPI (%d) wait Burn rcnt(%d) ack\n"), id, rcnt);
                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
                //TRACE(_T("SPI (%d) wait Burn rcnt ack\n"), id);

                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) SPI Burn error. rcnt = %d, 0x%x\n"), id, rcnt, GetLastError());
                    return FALSE;
                }
            }

            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) SPI Burn WaitForSingleObject error. rcnt = %d, 0x%x\n"), id, rcnt, GetLastError());
                return FALSE;
            }
        }

        posnum+=100;
        burn_pos=0;

        while(burn_pos!=100)
        {
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                return FALSE;
            }

            //DbgOut("SPI wait burn ack");
            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                //AfxMessageBox(_T("ACK Error!"));
                return FALSE;
            }
            DbgOut("SPI wait burn ack end");
            if(!((ack>>16)&0xffff))
            {
                burn_pos=(UCHAR)(ack&0xffff);
                //TRACE("posstr  %d %d \n", burn_pos, posnum);
                PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)(posnum+burn_pos),0);
            }
            else
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                //AfxMessageBox(_T("Burn Error!"));
                return FALSE;
            }
        }
    }
#else // Batch Burn

    PACK_CHILD_HEAD child;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,100);
    int i, j, translen, reclen, blockcnt;
    int posnum=0;

    for(i=0; i<(int)(ppackhead->num); i++)
    {
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
        if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI Burn error\n"),id);
            return FALSE;
        }
        if(bResult!=TRUE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI Burn error\n"),id);
            return FALSE;
        }
        total+= sizeof(PACK_CHILD_HEAD);
        pbuf+= sizeof(PACK_CHILD_HEAD);
        blockcnt = child.filelen/(SPI_BLOCK_SIZE);
        //translen = child.filelen;
        for(j=0; j<blockcnt; j++)
        {
            //TRACE(_T("blockcnt=%d, total = %d, child.filelen=%d  file_len=%d j=%d\n"), blockcnt, total, child.filelen, file_len, j);
            translen = SPI_BLOCK_SIZE;
            while(translen>0)
            {
                bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, BUF_SIZE);
                pbuf+=BUF_SIZE;
                translen -= BUF_SIZE;
                if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
					NucUsb.CloseWinUsbDevice(id);
					TRACE(_T("XXX (%d) SPI Burn error\n"),id);
					return FALSE;
                }

                if(bResult==TRUE)
                {
                    total+=BUF_SIZE;
                    pos=(int)(((float)(((float)total/(float)file_len))*100));

                    //DbgOut("SPI wait ack");
                    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
                    //DbgOut("SPI wait ack end");
                    if(bResult==FALSE)
                    {
                        delete []lpBuffer;
                        NucUsb.CloseWinUsbDevice(id);
                        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                        return FALSE;
                    }
                }

                if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    fclose(fp);
					NucUsb.CloseWinUsbDevice(id);
                    return FALSE;
                }
            }

            TRACE(_T("blockcnt=%d,  j=%d, total = %d  pos=%d\n"), blockcnt, j, total, pos);
            PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);

            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
            if(bResult==TRUE)
            {
                while(ack!=j);
            }
            else
                TRACE(_T("file_len = %d total = %d  remain = %d\n"), file_len, total, file_len-total);

            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
				NucUsb.CloseWinUsbDevice(id);
                return FALSE;
            }
        }
        TRACE(_T("child.filelen = %d     %d\n"), child.filelen, child.filelen % (SPI_BLOCK_SIZE));
        if ((child.filelen % (SPI_BLOCK_SIZE)) != 0)
        {
            translen = child.filelen - (blockcnt*SPI_BLOCK_SIZE);
            while(translen>0)
            {
                reclen = MIN(BUF_SIZE, translen);
                bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, reclen);
                pbuf+=reclen;
                translen -= reclen;
                if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
					NucUsb.CloseWinUsbDevice(id);
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    total+=reclen;
                    pos=(int)(((float)(((float)total/(float)file_len))*100));
                    //TRACE(_T("file_len=%d  total = %d   remin=%d\n"), file_len, total, file_len-total);
                    //DbgOut("SPI wait ack");
                    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
                    //DbgOut("SPI wait ack end");
                    if(bResult==FALSE)
                    {
                        delete []lpBuffer;
                        NucUsb.CloseWinUsbDevice(id);
                        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                        return FALSE;
                    }
                }

                if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
					NucUsb.CloseWinUsbDevice(id);
                    return FALSE;
                }

            }

            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
            if(bResult==TRUE)
            {
                while(ack!=100);
                TRACE(_T("ack %d\n"), ack);
            }
            else
            {
                TRACE(_T("file_len = %d  total = %d  remain = %d\n"), file_len, total, file_len-total);
            }
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
				NucUsb.CloseWinUsbDevice(id);
                return FALSE;
            }

            PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
        }
    }
#endif
    delete []lpBuffer;
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}


//SPI Verify
int FastDlg::XUSB_FastSPIVerify(int id, CString& portName, CString& m_pathName)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0, blockcnt;
    FILE* fp;
    int pos=0, reclen, translen;//len
    unsigned int total=0,file_len,ack,totalsize;//scnt,rcnt
    char* lpBuffer;
    char temp[BUF_SIZE];
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)0,0);

    /***********************************************/
    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(bResult==FALSE)
    {
        //AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }

#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif

    USHORT typeack;
    bResult=NucUsb.NUC_SetType(id,SPI,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.CloseWinUsbDevice(id);
        return FALSE;
    }

    fp=_wfopen(m_pathName,_T("rb"));
    if(!fp)
    {
        NucUsb.CloseWinUsbDevice(id);
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if(!file_len)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);

    fread(lpBuffer,file_len,1,fp);
    PACK_HEAD *ppackhead =(PACK_HEAD *)lpBuffer;

    int imagenum = 0;
    ppackhead->fileLength = ((lpBuffer[7]&0xff) << 24 | (lpBuffer[6]&0xff) << 16 | (lpBuffer[5]&0xff) << 8 | (lpBuffer[4]&0xff));
    ppackhead->num = ((lpBuffer[11]&0xff) << 24 | (lpBuffer[10]&0xff) << 16 | (lpBuffer[9]&0xff) << 8 | (lpBuffer[8]&0xff));
    imagenum = ppackhead->num;

    char *pbuf = lpBuffer;
    total+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);

    PACK_CHILD_HEAD child;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,100);
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetBkColor(COLOR_VERIFY);
    int posnum=0;
    int prepos=0;

    memset(m_fhead_spi, 0x00, sizeof(NORBOOT_NAND_HEAD));
    m_fhead_spi->flag=PACK_VERIFY_ACTION;
    //TRACE("lpBuffer[16~19] = 0x%x, 0x%x, 0x%x, 0x%x\n", (lpBuffer[16] & 0xff), lpBuffer[17], lpBuffer[18], lpBuffer[19]);
    m_fhead_spi->filelen= ((lpBuffer[19]&0xff) << 24 | (lpBuffer[18]&0xff) << 16 | (lpBuffer[17]&0xff) << 8 | (lpBuffer[16]&0xff)); // child1 file len
    m_fhead_spi->flashoffset = ((lpBuffer[23]&0xff) << 24 | (lpBuffer[22]&0xff) << 16 | (lpBuffer[21]&0xff) << 8 | (lpBuffer[20]&0xff)); // child1 start address
    m_fhead_spi->type = ((lpBuffer[27]&0xff) << 24 | (lpBuffer[26]&0xff) << 16 | (lpBuffer[25]&0xff) << 8 | (lpBuffer[24]&0xff));
    m_fhead_spi->no = imagenum;
    memcpy(temp,(unsigned char*)m_fhead_spi,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)temp, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        fclose(fp);
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) Write SPI head error\n"), id);
        return FALSE;
    }
    Sleep(5);
    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        fclose(fp);
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) SPI Verify Write head error\n"), id);
        return FALSE;
    }

    totalsize = 0;

#if(0) //image size = DDR size – 1M - 64k

    for(int i=0; i<(int)imagenum; i++)
    {
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        memcpy(temp,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)temp, sizeof(PACK_CHILD_HEAD));

        if(bResult==FALSE)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) Write SPI head error\n"), id);
            return FALSE;
        }

        //Sleep(5);
        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        if(bResult==FALSE)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI Verify ACK error !\n"), id);
            return FALSE;
        }
        total+= sizeof(PACK_CHILD_HEAD);
        pbuf+= sizeof(PACK_CHILD_HEAD);


        int ddrlen;
        if(child.imagetype == UBOOT)
        {
            ddrlen = (((mainWnd->DDRLen+8+15)/16)*16);
            pbuf += ddrlen + IBR_HEADER_LEN;
            total+= ddrlen + IBR_HEADER_LEN;
            TRACE(_T("ddrlen = 0x%x(%d)    mainWnd->DDRLen = 0x%x(%d)\n"), ddrlen, ddrlen, mainWnd->DDRLen, mainWnd->DDRLen);
            // send DDR parameter Length
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ddrlen,4);
            //Sleep(5);
            if(bResult==FALSE)
            {
                fclose(fp);
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) NUC_WritePipe DDR Length error\n"), id);
                return FALSE;
            }

            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
            if(bResult==FALSE || ack!=0x85)
            {
                fclose(fp);
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) NUC_ReadPipe DDR Length error\n"), id);
                return FALSE;
            }

            scnt=(child.filelen-ddrlen-IBR_HEADER_LEN)/BUF_SIZE;
            rcnt=(child.filelen-ddrlen-IBR_HEADER_LEN)%BUF_SIZE;
        }
        else
        {
            scnt=child.filelen/BUF_SIZE;
            rcnt=child.filelen%BUF_SIZE;
        }

        TRACE("child.filelen = %d, scnt=%d, rcnt =%d\n", child.filelen, scnt, rcnt);
        int prepos=0;
        while(scnt>0)
        {
            if(child.imagetype !=ENV)
            {
                fseek(fp,total,SEEK_SET);
                fread(temp,BUF_SIZE,1,fp);
            }
            else
            {
                fseek(fp,total,SEEK_SET);
                fread(temp,BUF_SIZE,1,fp);
            }
            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)lpBuffer, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                fclose(fp);
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) SPI Verify error. scnt=%d\n"), id, scnt);
                return FALSE;
            }

            if(bResult==TRUE)
            {
                total+=BUF_SIZE;
                pbuf+=BUF_SIZE;
                totalsize += BUF_SIZE;

                pos=(int)(((float)(((float)total/(float)file_len))*100));
                if(DataCompare(temp,lpBuffer,BUF_SIZE))
                    ack=BUF_SIZE;
                else
                    ack=0;//compare error

                bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ack,4);
                if((bResult==FALSE)||(!ack))
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) SPI Verify error. scnt=%d\n"), id, scnt);
                    //return FALSE;
                    return ERR_VERIFY_DATACOM;
                }

            }
            else
            {
                fclose(fp);
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                return FALSE;
            }

            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                fclose(fp);
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject error. scnt=%d\n"), id, scnt);
                return FALSE;
            }

            scnt--;
#if(0)
            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                if((scnt % 4 == 0) || pos == 100)
                {
                    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)(pos),0);
                }
#endif
                if(pos%5==0)
                {
                    prepos=pos;
                    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
                }
            }

            if(rcnt>0)
            {
                memset((char *)&temp, 0xff, BUF_SIZE);
                if(m_fhead_spi->type != ENV)
                {
                    fseek(fp,total,SEEK_SET);
                    fread(temp,rcnt,1,fp);
                }
                else
                {
                    fseek(fp,total,SEEK_SET);
                    fread(temp,BUF_SIZE,1,fp);
                }
                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)lpBuffer,BUF_SIZE);
                if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
                {
                    fclose(fp);
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) WaitForSingleObject error. rcnt=%d\n"), id, rcnt);
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    total+=rcnt;
                    pbuf+=rcnt;
                    totalsize += rcnt;
                    if(DataCompare(temp,lpBuffer,rcnt))
                        ack=BUF_SIZE;
                    else
                        ack=0;//compare error

                    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ack, 4);
                    if((bResult==FALSE)||(!ack))
                    {
                        fclose(fp);
                        delete []lpBuffer;
                        NucUsb.CloseWinUsbDevice(id);
                        TRACE(_T("XXX (%d) SPI Verify error. rcnt=%d  ack= 0x%x\n"), id, rcnt, ack);
                        //return FALSE;
                        return ERR_VERIFY_DATACOM;
                    }

                }
                else
                {
                    fclose(fp);
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) SPI Verify error. rcnt=%d\n"), id, rcnt);
                    return FALSE;
                }

                if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
                {
                    fclose(fp);
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) SPI Verify error. rcnt=%d\n"), id, rcnt);
                    return FALSE;
                }

                pos=(int)(((float)(((float)total/(float)file_len))*100));
                if(pos>=100)
                {
                    pos=100;
                }

                if(pos%5==0)
                {
                    prepos=pos;
                    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
                }
            }
        }

#else // PACK Batch Verify

    for(int i=0; i<(int)imagenum; i++)
    {
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        memcpy(temp,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)temp, sizeof(PACK_CHILD_HEAD));

        // if(i == 1)
            // TRACE(_T("%d  imagenum=%d\n"), i, imagenum);

        if(bResult==FALSE)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) Write SPI head error\n"), id);
            return FALSE;
        }

        //Sleep(5);
        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        if(bResult==FALSE)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI Verify ACK error !\n"), id);
            return FALSE;
        }
        total+= sizeof(PACK_CHILD_HEAD);
        pbuf+= sizeof(PACK_CHILD_HEAD);


        int ddrlen;
        if(child.imagetype == UBOOT)
        {
            ddrlen = (((mainWnd->DDRLen+8+15)/16)*16);
            pbuf += ddrlen + IBR_HEADER_LEN;
            total+= ddrlen + IBR_HEADER_LEN;
            TRACE(_T("ddrlen = 0x%x(%d)    mainWnd->DDRLen = 0x%x(%d)\n"), ddrlen, ddrlen, mainWnd->DDRLen, mainWnd->DDRLen);
            // send DDR parameter Length
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ddrlen,4);
            //Sleep(5);
            if(bResult==FALSE)
            {
                fclose(fp);
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) NUC_WritePipe DDR Length error\n"), id);
                return FALSE;
            }

            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
            if(bResult==FALSE || ack!=0x85)
            {
                fclose(fp);
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) NUC_ReadPipe DDR Length error\n"), id);
                return FALSE;
            }
            blockcnt = (child.filelen-ddrlen-IBR_HEADER_LEN)/(SPI_BLOCK_SIZE);
            //TRACE("UBOOT child.filelen = %d, blockcnt=%d, child.filelen-ddrlen-IBR_HEADER_LEN =%d,  child.filelen-ddrlen=%d\n", child.filelen, blockcnt, child.filelen-ddrlen-IBR_HEADER_LEN, child.filelen-ddrlen);
        }
        else
        {
            blockcnt = child.filelen/(SPI_BLOCK_SIZE);
            //TRACE("      child.filelen = %d, blockcnt=%d \n", child.filelen, blockcnt);
        }


        for(int j=0; j<blockcnt; j++)
        {
            translen = SPI_BLOCK_SIZE;
            while(translen>0)
            {
                if(child.imagetype !=ENV)
                {
                    fseek(fp,total,SEEK_SET);
                    fread(temp,BUF_SIZE,1,fp);
                }
                else
                {
                    fseek(fp,total,SEEK_SET);
                    fread(temp,BUF_SIZE,1,fp);
                }
                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)lpBuffer, BUF_SIZE);
                if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    fclose(fp);
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) SPI Verify error. total=%d\n"), id, total);
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    total+=BUF_SIZE;
                    pbuf+=BUF_SIZE;

                    pos=(int)(((float)(((float)total/(float)file_len))*100));
                    if(DataCompare(temp,lpBuffer,BUF_SIZE))
                        ack=BUF_SIZE;
                    else
                        ack=0;//compare error

                    bResult=NucUsb.NUC_WritePipe(id, (UCHAR *)&ack,4);
                    if((bResult==FALSE)||(!ack))
                    {
                        delete []lpBuffer;
                        NucUsb.CloseWinUsbDevice(id);
                        fclose(fp);
                        TRACE(_T("XXX (%d) SPI Verify error. total=%d\n"), id, total);
                        //return FALSE;
                        return ERR_VERIFY_DATACOM;
                    }

                }
                else
                {
                    fclose(fp);
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    return FALSE;
                }

                if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
                {
                    fclose(fp);
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) WaitForSingleObject error. total=%d\n"), id, total);
                    return FALSE;
                }

                translen-=BUF_SIZE;
                if(pos%5==0)
                {
                    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
                }
                //TRACE(_T("i=%d, child.filelen = %d, translen=%d\n"), i, child.filelen, translen);
            }
        }


        if(child.imagetype == UBOOT)
        {
            translen = (child.filelen-ddrlen-IBR_HEADER_LEN) - (blockcnt*SPI_BLOCK_SIZE);
            TRACE(_T("UBOOT remain: blockcnt=%d, child.filelen = 0x%x(%d)   ddrlen=%d  translen=%d\n"), blockcnt, child.filelen, child.filelen, ddrlen, translen);
        }
        else
            translen = child.filelen - (blockcnt*SPI_BLOCK_SIZE);

        TRACE(_T("remain: blockcnt=%d, child.filelen = 0x%x(%d)     translen=%d\n"), blockcnt, child.filelen, child.filelen, translen);
        if (translen > 0)
        {
            while(translen>0)
            {
                reclen = MIN(BUF_SIZE, translen);
                if(m_fhead_spi->type != ENV)
                {
                    fseek(fp,total,SEEK_SET);
                    fread(temp,reclen,1,fp);
                }
                else
                {
                    fseek(fp,total,SEEK_SET);
                    fread(temp,reclen,1,fp);
                }
                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)lpBuffer,reclen);
                if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
                {
                    fclose(fp);
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) WaitForSingleObject error. total=%d\n"), id, total);
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    total+=reclen;
                    pbuf+=reclen;
                    translen-=reclen;
                    //TRACE(_T("child.filelen = %d  total=%d, reclen=%d, translen=%d\n"), child.filelen, total, reclen, translen);
                    if(DataCompare(temp,lpBuffer,reclen))
                        ack=reclen;
                    else
                        ack=0;//compare error

                    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ack, 4);
                    if((bResult==FALSE)||(!ack))
                    {
                        fclose(fp);
                        delete []lpBuffer;
                        NucUsb.CloseWinUsbDevice(id);
                        TRACE(_T("XXX (%d) SPI Verify error. total=%d  ack= 0x%x\n"), id, total, ack);
                        //return FALSE;
                        return ERR_VERIFY_DATACOM;
                    }

                }
                else
                {
                    fclose(fp);
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) SPI Verify error. total=%d\n"), id, total);
                    return FALSE;
                }

                if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
                {
                    fclose(fp);
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) SPI Verify error. total=%d\n"), id, total);
                    return FALSE;
                }

                pos=(int)(((float)(((float)total/(float)file_len))*100));
                if(pos>=100)
                {
                    pos=100;
                }

                PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
            }

        }
    }

#endif

    fclose(fp);
    delete []lpBuffer;
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;

}

BOOL FastDlg::XUSB_FasteMMCErase(int id, CString& portName, CString& m_pathName)
{
    BOOL bResult;
    CString tempstr;
    int count=0;
    DWORD  iRet = 0x00;
    unsigned int ack,erase_pos=0;
    NORBOOT_MMC_HEAD *fhead;
    char* lpBuffer;

    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    fhead=(NORBOOT_MMC_HEAD *)malloc(sizeof(NORBOOT_MMC_HEAD));

    //TRACE(_T("XUSB_FasteMMCErase start (%d)\n"), id);
    /***********************************************/
    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(bResult==FALSE)
    {
        free(fhead);
        //AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        free(fhead);
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(id,MMC,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        free(fhead);
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX Erase NUC_SetType error !!!\n"));
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    //memset((unsigned char*)fhead,0,sizeof(NORBOOT_MMC_HEAD));
    memset(fhead,0,sizeof(NORBOOT_MMC_HEAD));

    FILE* fp;
    unsigned int file_len;
    char* lpReadBuffer;
    unsigned int totalsize;

    fp=_wfopen(m_pathName,_T("rb"));
    if(!fp)
    {
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(!file_len)
    {
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    lpReadBuffer = new char[file_len]; //read file to buffer
    memset(lpReadBuffer,0x00,file_len);
    fread(lpReadBuffer,file_len,1,fp);
    if(lpReadBuffer[0]!=0x5)
    {
        delete []lpReadBuffer;
        fclose(fp);
        return ERR_PACK_FORMAT;
    }
    fclose(fp);

    char *pbuf = lpReadBuffer;
    PACK_HEAD *ppackhead=(PACK_HEAD *)lpReadBuffer;

    pbuf+= sizeof(PACK_HEAD);
    PACK_CHILD_HEAD child;
    PACK_MMC_FORMAT_INFO child_format;
    totalsize = 0;
    BOOL bFormatFlag = FALSE;
    for(int i=0; i<(int)(ppackhead->num); i++)
    {
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        pbuf+= sizeof(PACK_CHILD_HEAD);
        if(child.imagetype == PARTITION)
        {
            bFormatFlag = TRUE;
            memcpy(&child_format,(char *)pbuf,sizeof(PACK_MMC_FORMAT_INFO));
            memset(fhead,0,sizeof(NORBOOT_MMC_HEAD));
            fhead->flag=FORMAT_ACTION;
            fhead->FSType = child_format.MMCFTFS;
            fhead->PartitionNum = child_format.MMCFTPNUM;
            fhead->ReserveSize = child_format.MMCFTPREV;
            fhead->Partition1Size = child_format.MMCFTP1;
            fhead->PartitionS1Size = child_format.MMCFTP1S;
            fhead->Partition2Size = child_format.MMCFTP2;
            fhead->PartitionS2Size = child_format.MMCFTP2S;
            fhead->Partition3Size = child_format.MMCFTP3;
            fhead->PartitionS3Size = child_format.MMCFTP3S;
            fhead->Partition4Size = child_format.MMCFTP4;
            fhead->PartitionS4Size = child_format.MMCFTP4S;
        }

        pbuf+=child.filelen;
        totalsize += child.filelen;
    }
    delete []lpReadBuffer;
    TRACE("Format Info[%d]: %d  %d  %d \n[P1:%dMB  %d  P2:%dMB  %d  P3:%dMB  %d  P4:%dMB  %d]\n", bFormatFlag,  fhead->FSType, fhead->PartitionNum, fhead->ReserveSize, fhead->Partition1Size, fhead->PartitionS1Size,
          fhead->Partition2Size, fhead->PartitionS2Size, fhead->Partition3Size, fhead->PartitionS3Size, fhead->Partition4Size, fhead->PartitionS4Size);

    if(bFormatFlag == FALSE) //Without Partition information
    {
        // memset(fhead,0,sizeof(NORBOOT_MMC_HEAD));
        // fhead->flag=FORMAT_ACTION;
        // fhead->FSType = 0; //FAT32
        // fhead->PartitionNum = 1;
        // fhead->ReserveSize = 32768; //16MB
        // fhead->PartitionS1Size = mainWnd->m_info.EMMC_uBlock - 32768; //unit Sector
        // fhead->Partition1Size = fhead->PartitionS1Size/1024*512/1024; //MB

        free(fhead);
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        return TRUE;
    }

    memcpy(lpBuffer,(unsigned char*)fhead,sizeof(NORBOOT_MMC_HEAD));
    free(fhead);

    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)lpBuffer, sizeof(NORBOOT_MMC_HEAD));
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("(%d) XXX Erase eMMC head error\n"), id);
        return FALSE;
    }

    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE || ack!=0x85)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("(%d) XXX Erase Read ACK error ack= 0x%x   0x%x\n"), id, ack, GetLastError());
        return FALSE;
    }

    erase_pos=0;
    int wait_pos=0;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,100);
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetBkColor(COLOR_ERASE);

    while(erase_pos!=100)
    {
        if(WaitForSingleObject(m_ExitEventBurn[id], EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) WaitForSingleObject error. 0x%x \n"),id, GetLastError());
            return FALSE;
        }
        Sleep(10);
        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        if(bResult=FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) Erase Read ACK error ack=0x%x,  0x%x\n"), id, ack, GetLastError());
            return FALSE;
        }

        if(!((ack>>16)&0xffff))
        {
            erase_pos=ack&0xffff;
            PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)erase_pos,0);
        }
        else
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) Erase error ack=0x%x,  0x%x\n"), id, ack, GetLastError());
            return FALSE;
        }

        if(erase_pos==95)
        {
            wait_pos++;
            if(wait_pos>100)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) Erase error wait_pos=%d\n"), id, wait_pos);
                return FALSE;
            }
        }
    }

    delete []lpBuffer;
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}

BOOL FastDlg::XUSB_FasteMMCBurn(int id, CString& portName,CString& m_pathName,int *len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len,scnt,rcnt;
    unsigned int totalsize;
    ULONG ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    //TRACE(_T("XUSB_FasteMMCBurn start (%d)  ,%s\n"), id, m_pathName);
    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(!bResult)
    {
        //AfxMessageBox(_T("Error! Device Enable error\n"));
        return FALSE;

    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(id,MMC,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) Burn  NUC_SetType error \n"), id);
        return FALSE;
    }

    fp=_wfopen(m_pathName,_T("rb"));
    if(!fp)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(!file_len)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    unsigned int magic;
    fread((unsigned char *)&magic,4,1,fp);
    if(magic!=0x5)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        AfxMessageBox(_T("Error! Pack Image Format Error\n"));
        return ERR_PACK_FORMAT;
    }

    fseek(fp,0,SEEK_SET);
    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);
    memset((unsigned char*)m_fhead_emmc,0,sizeof(NORBOOT_MMC_HEAD));
    total=0;
    m_fhead_emmc->flag=PACK_ACTION;
    m_fhead_emmc->type=m_type;
    m_fhead_emmc->filelen=file_len;
    memcpy(lpBuffer,(unsigned char*)m_fhead_emmc,sizeof(NORBOOT_MMC_HEAD));
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)lpBuffer, sizeof(NORBOOT_MMC_HEAD));
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) eMMC Burn file format error! 0x%x\n"), id, GetLastError());
        return FALSE;
    }

    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) eMMC Burn Read ACK error! 0x%x\n"), id, GetLastError());
        return FALSE;
    }

    fread(lpBuffer,m_fhead_emmc->filelen,1,fp);
    if(lpBuffer[0]!=0x5)
    {
        delete []lpBuffer;
        fclose(fp);
        //AfxMessageBox(_T("Error! This file is not pack image"));
        return ERR_PACK_FORMAT;
    }
    fclose(fp);

    // Check DDR *ini
    bResult=CheckDDRiniData(lpBuffer, m_fhead_emmc->filelen);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        //AfxMessageBox(_T("Error! DDR Init select error\n"));
        return ERR_DDRINI_DATACOM;
    }

    char *pbuf = lpBuffer;
    PACK_HEAD *ppackhead=(PACK_HEAD *)lpBuffer;
    totalsize = 0;
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, sizeof(PACK_HEAD));
    Sleep(5);
    if(WaitForSingleObject(m_ExitEventBurn[id], EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) WaitForSingleObject error 1\n"),id);
        return FALSE;
    }
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) eMMC Burn Write error! 0x%x\n"), id, GetLastError());
        return FALSE;
    }

    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) eMMC Burn Read error! 0x%x\n"), id, GetLastError());
        return FALSE;
    }

    totalsize += sizeof(PACK_HEAD);
    total+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);
    PACK_CHILD_HEAD child;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,100);
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetBkColor(COLOR_BURN);
    int posnum=0;
    int prepos=0;

    unsigned int blockNum, u32imagetype;
    for(int i=0; i<(int)(ppackhead->num); i++)
    {
        total=0;
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        u32imagetype = child.imagetype;

        bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
        if(WaitForSingleObject(m_ExitEventBurn[id], EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) WaitForSingleObject error 2. 0x%x\n"),id, bResult);
            return FALSE;
        }
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) eMMC Burn Write error! 0x%x\n"), id, GetLastError());
            return FALSE;
        }

        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) eMMC Burn Read error! 0x%x\n"), id, GetLastError());
            return FALSE;
        }

        if(u32imagetype == PARTITION)
        {
            pbuf+= sizeof(PACK_CHILD_HEAD)+PACK_FOMRAT_HEADER_LEN;
            totalsize += sizeof(PACK_CHILD_HEAD)+PACK_FOMRAT_HEADER_LEN; // skip partition header
            TRACE(_T("totalsize = %d   file_len =%d   sizeof(PACK_MMC_FORMAT_INFO) =%d \n"), totalsize, file_len, sizeof(PACK_MMC_FORMAT_INFO));
            pos=(int)(((float)(((float)totalsize/(float)file_len))*100));
            if(pos>=100)
            {
                pos=100;
            }

            TRACE("(%d) pos =%d, prepos =%d\n", id, pos, prepos);
            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)(pos),0);
            }

            continue;
        }

        pbuf+= sizeof(PACK_CHILD_HEAD);
        //Sleep(1);
        scnt=child.filelen/BUF_SIZE;
        rcnt=child.filelen%BUF_SIZE;
        total=0;

        while(scnt>0)
        {
            //Sleep(10);
            //TRACE(_T("#7459 (%d) eMMC Burn 44444444 scnt %d \n"), id, scnt);
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEventBurn[id], EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject error 3, scnt= %d\n"),id, scnt);
                return FALSE;
            }
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) eMMC Burn Write error. scnt= %d ,0x%x\n"),id, scnt, GetLastError());
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=BUF_SIZE;
                total+=BUF_SIZE;
                totalsize += BUF_SIZE;

                pos=(int)(((float)(((float)totalsize/(float)file_len))*100));

                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    CString tmp;
                    tmp.Format(_T("eMMC Burn ACK error imagenum=%d  scnt= %d  0x%x"),i, scnt, GetLastError());
                    TRACE(_T("XXX (%d) %s !\n"), id, tmp);
                    return FALSE;
                }
            }

            if(WaitForSingleObject(m_ExitEventBurn[id], EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject error 4. scnt= %d  0x%x\n"),id, scnt, GetLastError());
                return FALSE;
            }
            scnt--;
            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                if((scnt % 4 == 0) || pos == 100)
                {
                    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)(pos),0);
                }
            }
        }

        if(rcnt>0)
        {
            Sleep(20);
            //TRACE(_T("#7587 (%d) eMMC Burn 66666666  rcnt = %d\n"), id, rcnt);
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf,rcnt);
            if(WaitForSingleObject(m_ExitEventBurn[id], EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject error 5. rcnt = %d  0x%x\n"),id, scnt, GetLastError());
                return FALSE;
            }
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) eMMC Burn Write error! rcnt = %d  0x%x\n"), id, rcnt, GetLastError());
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=rcnt;
                total+=rcnt;
                totalsize += rcnt;

                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) eMMC Burn Read error! rcnt = %d  0x%x\n"), id, rcnt, GetLastError());
                    return FALSE;
                }
            }

            if(WaitForSingleObject(m_ExitEventBurn[id], EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject error 6. rcnt = %d  0x%x\n"),id, rcnt, GetLastError());
                return FALSE;
            }

            pos=(int)(((float)(((float)totalsize/(float)file_len))*100));
            if(pos>=100)
            {
                pos=100;
            }

            //TRACE("(%d) #7629 rcnt %d, pos =%d, prepos =%d\n", id, rcnt, pos, prepos);
            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)(pos),0);
            }

        }

        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&blockNum,4);
        //Sleep(10);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) eMMC Burn Read error. rcnt = %d  0x%x\n"), id, rcnt, GetLastError());
            return FALSE;
        }
    }//for(int i=0;i<(int)(ppackhead->num);i++)

    delete []lpBuffer;
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}

BOOL FastDlg::XUSB_FasteMMCVerify(int id, CString& portName, CString& m_VfypathName)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    unsigned int file_len,scnt,rcnt,ack = 0;
    unsigned int totalsize;
    char* lpBuffer;
    char temp[BUF_SIZE];
    char buf[BUF_SIZE];
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)0,0);

    //TRACE(_T("XUSB_FasteMMCVerify start (%d)  ,%s\n"), id, m_VfypathName);
    /***********************************************/
    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(bResult==FALSE)
    {
        //AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(id,MMC,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) eMMC Verify NUC_SetType error\n"),id);
        return FALSE;
    }

    fp=_wfopen(m_VfypathName,_T("rb"));
    if(!fp)
    {
        NucUsb.CloseWinUsbDevice(id);
        //AfxMessageBox(_T("Error! File Open error\n"));
        fclose(fp);
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(!file_len)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        //AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);
    fread(lpBuffer,file_len,1,fp);
    PACK_HEAD *ppackhead =(PACK_HEAD *)lpBuffer;
    totalsize = 0;

    int imagenum = 0;
    ppackhead->fileLength = ((lpBuffer[7]&0xff) << 24 | (lpBuffer[6]&0xff) << 16 | (lpBuffer[5]&0xff) << 8 | (lpBuffer[4]&0xff));
    ppackhead->num = ((lpBuffer[11]&0xff) << 24 | (lpBuffer[10]&0xff) << 16 | (lpBuffer[9]&0xff) << 8 | (lpBuffer[8]&0xff));
    imagenum = ppackhead->num;

    char *pbuf = lpBuffer;
    totalsize+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);

    PACK_CHILD_HEAD child;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,100);
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetBkColor(COLOR_VERIFY);
    int posnum=0;
    int prepos=0;

    memset(m_fhead_emmc, 0x00, sizeof(NORBOOT_MMC_HEAD));
    m_fhead_emmc->flag=PACK_VERIFY_ACTION;
    //TRACE("lpBuffer[16~19] = 0x%x, 0x%x, 0x%x, 0x%x\n", (lpBuffer[16] & 0xff), lpBuffer[17], lpBuffer[18], lpBuffer[19]);
    m_fhead_emmc->filelen= ((lpBuffer[19] & 0xff) << 24 | (lpBuffer[18] & 0xff) << 16 | (lpBuffer[17] & 0xff) << 8 | (lpBuffer[16] & 0xff));
    m_fhead_emmc->flashoffset = ((lpBuffer[23] & 0xff) << 24 | (lpBuffer[22] & 0xff) << 16 | (lpBuffer[21] & 0xff) << 8 | (lpBuffer[20] & 0xff)); // child1 start address
    m_fhead_emmc->type = (lpBuffer[27] << 24 | lpBuffer[26] << 16 | lpBuffer[25] << 8 | lpBuffer[24]); // child1 image type
    m_fhead_emmc->no = imagenum;
    memcpy(temp,(unsigned char*)m_fhead_emmc,sizeof(NORBOOT_MMC_HEAD));

    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)temp, sizeof(NORBOOT_MMC_HEAD));
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) eMMC Verify head error. 0x%x\n"),id, GetLastError());
        return FALSE;
    }
    Sleep(5);
    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) eMMC Verify Read ACK head error. 0x%x\n"),id, GetLastError());
        return FALSE;
    }

    unsigned int u32imagetype;
    for(int i=0; i<(int)imagenum; i++)
    {
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        memcpy(temp,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        u32imagetype = child.imagetype;
        TRACE("(%d) u32imagetype = %d, totalsize = %d   file_len =%d\n", id, u32imagetype, totalsize, file_len);
        int prepos=0;
        if(u32imagetype == PARTITION)
        {
            totalsize+= sizeof(PACK_CHILD_HEAD)+PACK_FOMRAT_HEADER_LEN;
            pbuf+= sizeof(PACK_CHILD_HEAD)+PACK_FOMRAT_HEADER_LEN;
            pos=(int)(((float)(((float)totalsize/(float)file_len))*100));
            if(pos>=100)
            {
                pos=100;
            }
            TRACE(_T("totalsize= %d  file_len=%d  pos = %d\n"),totalsize, file_len, pos);

            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
            }
        }
        else
        {
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)temp, sizeof(PACK_CHILD_HEAD));
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) eMMC Verify Write error. 0x%x\n"),id, GetLastError());
                return FALSE;
            }

            //Sleep(5);
            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) eMMC Verify Read error. 0x%x\n"),id, GetLastError());
                return FALSE;
            }
            totalsize+= sizeof(PACK_CHILD_HEAD);
            pbuf+= sizeof(PACK_CHILD_HEAD);

            int ddrlen;
            if(child.imagetype == UBOOT)
            {
                ddrlen = (((mainWnd->DDRLen+8+15)/16)*16);
                pbuf += ddrlen + IBR_HEADER_LEN;
                totalsize+= ddrlen + IBR_HEADER_LEN;

                // send DDR parameter Length
                bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ddrlen,4);
                Sleep(5);
                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) eMMC Verify Write error. 0x%x\n"),id, GetLastError());
                    return FALSE;
                }

                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) eMMC Verify Read error. 0x%x\n"),id, GetLastError());
                    return FALSE;
                }
            }

            if(child.imagetype == UBOOT)
            {
                scnt=(child.filelen-ddrlen-IBR_HEADER_LEN)/BUF_SIZE;
                rcnt=(child.filelen-ddrlen-IBR_HEADER_LEN)%BUF_SIZE;
            }
            else
            {
                scnt=child.filelen/BUF_SIZE;
                rcnt=child.filelen%BUF_SIZE;
            }
            TRACE("(%d) scnt = %d, rcnt = %d\n", id, scnt, rcnt);

            while(scnt>0)
            {

                if(child.imagetype !=ENV)
                {
                    fseek(fp,totalsize,SEEK_SET);
                    fread(temp,BUF_SIZE,1,fp);
                }
                else
                {
                    fseek(fp,totalsize,SEEK_SET);
                    fread(temp,BUF_SIZE,1,fp);
                }
                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)buf, BUF_SIZE);
                if(WaitForSingleObject(m_ExitEventBurn[id], EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) WaitForSingleObject error 7,  0x%x\n"),id, GetLastError());
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    totalsize+=BUF_SIZE;
                    pbuf+=BUF_SIZE;

                    pos=(int)(((float)(((float)totalsize/(float)file_len))*100));

                    if(DataCompare(temp,buf,BUF_SIZE))
                        ack=BUF_SIZE;
                    else
                    {
                        //TRACE("XXX (%d) i = %d, totalsize = %d, scnt = %d\n", id, i, totalsize, scnt);
                        ack=0;//compare error
                    }

                    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ack,4);
                    if((bResult==FALSE)||(!ack))
                    {
                        delete []lpBuffer;
                        NucUsb.CloseWinUsbDevice(id);
                        fclose(fp);
                        TRACE(_T("XXX (%d) eMMC Verify error. ImageNum= %d, scnt=%d\n"),id, i, scnt);
                        return ERR_VERIFY_DATACOM;
                    }

                }
                else
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) eMMC Verify error. 0x%x\n"),id, GetLastError());
                    return FALSE;
                }

                if(WaitForSingleObject(m_ExitEventBurn[id], EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) WaitForSingleObject error 8,  0x%x\n"),id, GetLastError());
                    return FALSE;
                }

                scnt--;

                if(pos%5==0)
                {
                    prepos=pos;
                    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
                }
            }

            if(rcnt>0)
            {
                memset((char *)&temp, 0xff, BUF_SIZE);
                if(u32imagetype == PARTITION)
                {
                    totalsize+= PACK_FOMRAT_HEADER_LEN; // skip partition header
                    pbuf+= PACK_FOMRAT_HEADER_LEN; // skip partition header
                    pos=(int)(((float)(((float)totalsize/(float)file_len))*100));
                    if(pos>=100)
                    {
                        pos=100;
                    }
                    TRACE(_T("totalsize= %d  file_len=%d  pos = %d\n"),totalsize, file_len, pos);

                    if((pos!=prepos) || (pos==100))
                    {
                        prepos=pos;
                        PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
                    }
                    continue;
                }
                //else if(m_fhead_emmc->type != ENV) {
                else if(u32imagetype != ENV)
                {
                    fseek(fp,totalsize,SEEK_SET);
                    fread(temp,rcnt,1,fp);
                }
                else
                {
                    fseek(fp,totalsize,SEEK_SET);
                    fread(temp,BUF_SIZE,1,fp);
                }
                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)buf,BUF_SIZE);
                if(WaitForSingleObject(m_ExitEventBurn[id], EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) WaitForSingleObject error 9, 0x%x\n"),id, GetLastError());
                    return FALSE;
                }

                if(bResult==TRUE)
                {
                    totalsize+=rcnt;
                    pbuf+=rcnt;
                    if(DataCompare(temp,buf,rcnt))
                        ack=BUF_SIZE;
                    else
                    {
                        //TRACE("XXX i = %d, totalsize = %d, rcnt = %d\n", i, totalsize, rcnt);
                        ack=0;//compare error
                    }

                    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ack, 4);
                    if((bResult==FALSE)||(!ack))
                    {
                        delete []lpBuffer;
                        NucUsb.CloseWinUsbDevice(id);
                        fclose(fp);
                        TRACE(_T("XXX (%d) eMMC Verify error. 0x%x\n"),id, GetLastError());
                        return ERR_VERIFY_DATACOM;
                    }
                }
                else
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) eMMC Verify error. 0x%x\n"),id, GetLastError());
                    return FALSE;
                }

                if(WaitForSingleObject(m_ExitEventBurn[id], EMMC_RW_PIPE_TIMEOUT) != WAIT_TIMEOUT)
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) WaitForSingleObject error A,  0x%x\n"),id, GetLastError());
                    return FALSE;
                }

                pos=(int)(((float)(((float)totalsize/(float)file_len))*100));
                if(pos>=100)
                {
                    pos=100;
                }
                TRACE(_T("totalsize= %d  file_len=%d  pos = %d\n"),totalsize, file_len, pos);

                if((pos!=prepos) || (pos==100))
                {
                    prepos=pos;
                    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
                }
            }
        }
    }

    delete []lpBuffer;
    fclose(fp);
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}

BOOL FastDlg::XUSB_FastSPINANDErase(int id, CString& portName)
{
    BOOL bResult;
    CString tempstr;
    int count=0;
    unsigned int ack,erase_pos=0, BlockPerFlash;
    NORBOOT_NAND_HEAD *fhead;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    fhead=(NORBOOT_NAND_HEAD *)malloc(sizeof(NORBOOT_NAND_HEAD));

    /***********************************************/
    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(bResult==FALSE)
    {
        free(fhead);
        //AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(id,SPINAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        free(fhead);
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) SPI NAND Erase error\n"),id);
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    memset((unsigned char*)fhead,0,sizeof(NORBOOT_NAND_HEAD));
    fhead->flag=ERASE_ACTION;

    fhead->flashoffset = 0; //start erase block
    fhead->execaddr= mainWnd->m_info.SPINand_BlockPerFlash;//20;//erase block length
    fhead->type= 0;// chip erase mode

    if(mainWnd->ChipEraseWithBad==0)
        fhead->no=0xFFFFFFFF;//erase good block
    else
        fhead->no=0xFFFFFFFE;//erase good block and bad block

    memcpy(lpBuffer,(unsigned char*)fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
    free(fhead);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        //TRACE(_T("XXX (%d) SPI NAND Erase head error\n"),id);
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE || ack != 0x89)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        //TRACE(_T("XXX (%d) SPI NAND Erase error\n"),id);
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(id, (UCHAR *)&BlockPerFlash,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        TRACE(_T("XXX (%d) SPI NAND  BlockPerFlash error !\n"),id);
        return FALSE;
    }
    erase_pos=0;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,100);
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetBkColor(COLOR_ERASE);
    int wait_pos=0;
    while(erase_pos!=100)
    {
        if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            return FALSE;
        }

        //TRACE(_T("(%d) SPI NAND wait erase ack\n"), id);
        Sleep(2);
        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        if(bResult=FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            //TRACE(_T("(%d) #6597 SPI NAND Erase error\n"),id);
            return FALSE;
        }

        if(ack < BlockPerFlash)
        {
            erase_pos = (int)(((float)(((float)ack/(float)BlockPerFlash))*100));
            if(ack == BlockPerFlash-1)
            {
                if(erase_pos < 99)
                    erase_pos = 100;
                else
                    erase_pos++;
            }
            //TRACE(_T("erase_pos =%d, ack =%d\n"), erase_pos, ack);
            PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)erase_pos,0);
        }
        else
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI NAND Erase error. ack = 0x%x\n"),id, ack);
            return FALSE;
        }
    }

    delete []lpBuffer;
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}

BOOL FastDlg::XUSB_FastSPINANDBurn(int id, CString& portName,CString& m_pathName,int *len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len,scnt,rcnt, totalsize;
    ULONG ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    bResult=NucUsb.EnableOneWinUsbDevice(id);
    //TRACE("debug xusb.cpp  99   id= %d, bResult = %d \n", id, bResult);
    if(!bResult)
    {
        //AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }

#if(0)
    bResult=NucUsb.NUC_CheckFw(id);

    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
        }
#endif

    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(id,SPINAND,(UCHAR *)&typeack,sizeof(typeack));
    //TRACE("debug xusb.cpp  aa   id= %d, bResult = %d \n", id, bResult);
    if(bResult==FALSE)
    {
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) NAND Burn NUC_SetType error\n"),id);
        return FALSE;
    }


    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        NucUsb.CloseWinUsbDevice(id);
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if(!file_len)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        AfxMessageBox(_T("File length is zero\n"));
        return FALSE;
    }

    unsigned int magic;
    fread((unsigned char *)&magic,4,1,fp);
    if(magic!=0x5)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        //AfxMessageBox(_T("Error! Pack Image Format Error\n"));
        return FALSE;
    }
    fseek(fp,0,SEEK_SET);

    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);

    memset((unsigned char*)m_fhead_nand,0,sizeof(NORBOOT_NAND_HEAD));
    total=0;
    m_fhead_nand->flag=PACK_ACTION;
    m_fhead_nand->type=m_type;
    m_fhead_nand->filelen=file_len;
    memcpy(lpBuffer,(unsigned char*)m_fhead_nand,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) SPI NAND Burn error\n"),id);
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) SPI NAND Burn error\n"),id);
        return FALSE;
    }

    fread(lpBuffer,m_fhead_nand->filelen,1,fp);
    if(lpBuffer[0]!=0x5)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        //AfxMessageBox(_T("Error! This file is not pack image"));
        return ERR_PACK_FORMAT;
    }
    fclose(fp);

    // Check DDR *ini
    bResult=CheckDDRiniData(lpBuffer, m_fhead_nand->filelen);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        //AfxMessageBox(_T("Error! DDR Init select error\n"));
        return ERR_DDRINI_DATACOM;
    }

    char *pbuf = lpBuffer;
    PACK_HEAD *ppackhead=(PACK_HEAD *)lpBuffer;
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, sizeof(PACK_HEAD));
    //Sleep(5);
    if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) SPI NAND Burn error\n"),id);
        return FALSE;
    }

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) SPI NAND Burn error\n"),id);
        return FALSE;
    }

    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("XXX (%d) SPI NAND Burn error\n"),id);
        return FALSE;
    }

    total+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);
    PACK_CHILD_HEAD child;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,short(ppackhead->num*100));
    int posnum=0;
    int prepos=0;

    unsigned int blockNum;
    totalsize = 0;
    for(int i=0; i<(int)(ppackhead->num); i++)
    {
        total=0;
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
        //Sleep(1);
        if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI NAND Burn error\n"),id);
            return FALSE;
        }
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI NAND Burn error\n"),id);
            return FALSE;
        }

        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) SPI NAND Burn error\n"),id);
            return FALSE;
        }

        pbuf+= sizeof(PACK_CHILD_HEAD);
        //Sleep(1);
        scnt=child.filelen/BUF_SIZE;
        rcnt=child.filelen%BUF_SIZE;
        total=0;
        TRACE(_T("scnt %d, rcnt %d\n"),scnt, rcnt);
        while(scnt>0)
        {
            //Sleep(1);
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                //TRACE("event 01\n");
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) SPI NAND Burn error! scnt = %d\n"),id, scnt);
                return FALSE;
            }
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) SPI NAND Burn error! scnt = %d\n"),id, scnt);
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=BUF_SIZE;
                total+=BUF_SIZE;
                totalsize += BUF_SIZE;

                pos=(int)(((float)(((float)totalsize/(float)file_len))*100));

                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
                if(bResult==FALSE)    // Storage error
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) SPI NAND Burn error! scnt = %d, ack= 0x%x\n"),id, scnt, ack);
                    return FALSE;
                }
            }

            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject error! scnt = %d, ack= 0x%x\n"),id, scnt, ack);
                return FALSE;
            }

            scnt--;

            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                if((scnt % 4 == 0) || pos == 100)
                {
                    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)(pos),0);
                }
            }

        }
        if(rcnt>0)
        {
            //Sleep(20);
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf,rcnt);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject error! rcnt = %d, ack= 0x%x\n"),id, rcnt, ack);
                return FALSE;
            }

            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) SPI NAND Burn error! rcnt = %d\n"),id, rcnt);
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=rcnt;
                total+=rcnt;
                totalsize +=rcnt;

                bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    TRACE(_T("XXX (%d) SPI NAND Burn error! rcnt = %d\n"),id, rcnt);
                    return FALSE;
                }
            }
            //TRACE("OneDeviceXUSB_Pack:device %d,08\n",id);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                TRACE(_T("XXX (%d) WaitForSingleObject error! rcnt = %d\n"),id, rcnt);
                return FALSE;
            }

            pos=(int)(((float)(((float)totalsize/(float)file_len))*100));
            if(pos>=100)
            {
                pos=100;
            }

            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                if(pos%5==0)
                {
                    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
                }
            }
        }
        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&blockNum,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            TRACE(_T("XXX (%d) XUSB_FastSPINANDBurn failed!\n"), id);
            return FALSE;
        }
    }//for(int i=0;i<(int)(ppackhead->num);i++)

    delete []lpBuffer;
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}

int FastDlg::XUSB_FastSPINANDVerify(int id, CString& portName, CString& m_pathName)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    unsigned int total=0,file_len,scnt,rcnt,ack;
    char* lpBuffer;
    char temp[BUF_SIZE];
    char buf[BUF_SIZE];
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)0,0);

    /***********************************************/
    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(bResult==FALSE)
    {
        //AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(id,SPINAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.CloseWinUsbDevice(id);
        return FALSE;
    }

    fp=_wfopen(m_pathName,_T("rb"));
    if(!fp)
    {
        NucUsb.CloseWinUsbDevice(id);
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(!file_len)
    {
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);

    fread(lpBuffer,file_len,1,fp);
    PACK_HEAD *ppackhead =(PACK_HEAD *)lpBuffer;

    int imagenum = 0;
    ppackhead->fileLength = ((lpBuffer[7]&0xff) << 24 | (lpBuffer[6]&0xff) << 16 | (lpBuffer[5]&0xff) << 8 | (lpBuffer[4]&0xff));

    ppackhead->num = ((lpBuffer[11]&0xff) << 24 | (lpBuffer[10]&0xff) << 16 | (lpBuffer[9]&0xff) << 8 | (lpBuffer[8]&0xff));
    imagenum = ppackhead->num;


    char *pbuf = lpBuffer;
    total+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);

    PACK_CHILD_HEAD child;
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetRange(0,100);
    ((CProgressCtrl *)GetDlgItem(iId_Array[id]))->SetBkColor(COLOR_VERIFY);
    int posnum=0;
    int prepos=0;

    memset(m_fhead_nand, 0x00, sizeof(NORBOOT_NAND_HEAD));
    m_fhead_nand->flag=PACK_VERIFY_ACTION;
    //TRACE(_T("%x  %x  %x %x\n"),lpBuffer[16], lpBuffer[17], lpBuffer[18], lpBuffer[19]);
    m_fhead_nand->filelen= ((lpBuffer[19]&0xff) << 24 | (lpBuffer[18]&0xff) << 16 | (lpBuffer[17]&0xff) << 8 | (lpBuffer[16]&0xff)); // child1 file len
    m_fhead_nand->flashoffset = ((lpBuffer[23]&0xff) << 24 | (lpBuffer[22]&0xff) << 16 | (lpBuffer[21]&0xff) << 8 | (lpBuffer[20]&0xff)); // child1 start address
    m_fhead_nand->type = ((lpBuffer[27]&0xff) << 24 | (lpBuffer[26]&0xff) << 16 | (lpBuffer[25]&0xff) << 8 | (lpBuffer[24]&0xff)); // child1 image type
    m_fhead_nand->no = imagenum;
    memcpy(temp,(unsigned char*)m_fhead_nand,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)temp, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) SPI NAND Verify Write head error\n"), id);
        return FALSE;
    }
    //Sleep(5);
    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("XXX (%d) SPI NAND Verify Read head error !\n"), id);
        return FALSE;
    }

    for(int i=0; i<(int)imagenum; i++)
    {
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        memcpy(temp,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)temp, sizeof(PACK_CHILD_HEAD));
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            fclose(fp);
            TRACE(_T("XXX (%d) i=%d SPI NAND Verify error\n"), id, i);
            return FALSE;
        }
        //Sleep(5);
        bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            fclose(fp);
            TRACE(_T("XXX (%d) SPI NAND Verify error\n"), id);
            return FALSE;
        }
        total+= sizeof(PACK_CHILD_HEAD);
        pbuf+= sizeof(PACK_CHILD_HEAD);

        int ddrlen;
        //UCHAR * ddrbuf;
        if(child.imagetype == UBOOT)
        {
            //ddrbuf=DDR2Buf(mainWnd->DDRBuf,mainWnd->DDRLen,&ddrlen);
            ddrlen = (((mainWnd->DDRLen+8+15)/16)*16);
            pbuf += ddrlen + IBR_HEADER_LEN;
            total+= ddrlen + IBR_HEADER_LEN;
            //TRACE(_T("len = %d    mainWnd->DDRLen = %d,  %d\n"), 16 + mainWnd->DDRLen + IBR_HEADER_LEN, mainWnd->DDRLen, 16 + ddrlen + IBR_HEADER_LEN);
            // send DDR parameter Length
            bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ddrlen,4);
            //Sleep(5);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) NUC_WritePipe DDR Length error\n"), id);
                return FALSE;
            }

            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) NUC_ReadPipe DDR Length error\n"), id);
                return FALSE;
            }
        }

        if(child.imagetype == UBOOT)
        {
            scnt=(child.filelen-ddrlen-IBR_HEADER_LEN)/BUF_SIZE;
            rcnt=(child.filelen-ddrlen-IBR_HEADER_LEN)%BUF_SIZE;
        }
        else
        {
            scnt=child.filelen/BUF_SIZE;
            rcnt=child.filelen%BUF_SIZE;
        }
        //TRACE("child.filelen = %d, scnt=%d, rcnt =%d\n", child.filelen, scnt, rcnt);
        int prepos=0;
        while(scnt>0)
        {
            //TRACE("scnt=0x%x(%d)\n", scnt, scnt);
            if(child.imagetype !=ENV)
            {
                fseek(fp,total,SEEK_SET);
                fread(temp,BUF_SIZE,1,fp);
            }
            else
            {
                fseek(fp,total,SEEK_SET);
                fread(temp,BUF_SIZE,1,fp);
            }
            //bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)lpBuffer, BUF_SIZE);
            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)buf, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) NAND Verify error. scnt=%d\n"), id, scnt);
                return FALSE;
            }

            if(bResult==TRUE)
            {
                total+=BUF_SIZE;
                pbuf+=BUF_SIZE;

                pos=(int)(((float)(((float)total/(float)file_len))*100));
                //if(DataCompare(temp,lpBuffer,BUF_SIZE))
                if(DataCompare(temp,buf,BUF_SIZE))
                    ack=BUF_SIZE;
                else
                    ack=0;//compare error

                bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ack,4);
                if((bResult==FALSE)||(!ack))
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) SPI NAND Verify error. scnt=%d\n"), id, scnt);
                    //return FALSE;
                    return ERR_VERIFY_DATACOM;
                }

            }
            else
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) NAND Verify error. scnt=%d\n"), id, scnt);
                return FALSE;
            }

            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) WaitForSingleObject error. scnt=%d\n"), id, scnt);
                return FALSE;
            }

            scnt--;

            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
            }

        }

        int temp_len = 0;
        if(rcnt>0)
        {
            memset((char *)&temp, 0xff, BUF_SIZE);

            if(child.imagetype != ENV)
            {
                fseek(fp,total,SEEK_SET);
                fread(temp,rcnt,1,fp);
                temp_len = rcnt;
            }
            else
            {
                fseek(fp,total,SEEK_SET);
                fread(temp,BUF_SIZE,1,fp);
                temp_len = BUF_SIZE;
            }

            //bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)lpBuffer,BUF_SIZE);
            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)buf,BUF_SIZE);
            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) WaitForSingleObject error. rcnt=%d\n"), id, rcnt);
                return FALSE;
            }

            if(bResult==TRUE)
            {
                total+=temp_len;
                pbuf+=temp_len;
                //if(DataCompare(temp,lpBuffer,temp_len))
                if(DataCompare(temp,buf,temp_len))
                    ack=BUF_SIZE;
                else
                    ack=0;//compare error

                bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&ack, 4);
                if((bResult==FALSE)||(!ack))
                {
                    delete []lpBuffer;
                    NucUsb.CloseWinUsbDevice(id);
                    fclose(fp);
                    TRACE(_T("XXX (%d) SPI NAND Verify error. rcnt=%d  ack= 0x%x\n"), id, rcnt, ack);
                    //return FALSE;
                    return ERR_VERIFY_DATACOM;

                }

            }
            else
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) SPI NAND Verify error. rcnt=%d\n"), id, rcnt);
                return FALSE;
            }

            if(WaitForSingleObject(m_ExitEventBurn[id], 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                TRACE(_T("XXX (%d) SPI NAND Verify error. rcnt=%d\n"), id, rcnt);
                return FALSE;
            }

            pos=(int)(((float)(((float)total/(float)file_len))*100));
            if(pos>=100)
            {
                pos=100;
            }
            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage((WM_FAST_PROGRESS1+id),(LPARAM)pos,0);
            }
        }
    }

    delete []lpBuffer;
    fclose(fp);
    NucUsb.CloseWinUsbDevice(id);
    return TRUE;
}

/************** Fast End ************/

/************** SPI NAND Begin ************/
BOOL CSPINandDlg::XUSB_Burn(CString& portName,CString& m_pathName,int *len,int *blockNum)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len,scnt,rcnt;
    ULONG ack;
    //NORBOOT_NAND_HEAD fhead;
    char* lpBuffer;
    unsigned char *ddrbuf;
    int ddrlen;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    m_progress.SetRange(0,100);
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(0,SPINAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        fclose(fp);
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if( (file_len>(mainWnd->m_info.SPINand_PagePerBlock*mainWnd->m_info.SPINand_PageSize))&&m_type==UBOOT)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Uboot File length cannot greater than block size\n"));
        return FALSE;
    }

    if(!file_len)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    memset((unsigned char*)m_fhead,0,sizeof(NORBOOT_NAND_HEAD));
    m_fhead->flag=WRITE_ACTION;
    ((NORBOOT_NAND_HEAD *)m_fhead)->initSize=0;

    m_fhead->filelen=file_len;
    switch(m_type)
    {
    case DATA:
    case IMAGE:
    case PACK:
        swscanf_s(_T("0"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        *len=file_len;
        lpBuffer = new char[file_len]; //read file to buffer
        memset(lpBuffer,0xff,file_len);
        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;
        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);
        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    case ENV:
        swscanf_s(_T("0"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        if(file_len>(0x20000-4))
        {
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            AfxMessageBox(_T("Error! The environment file size is less then 64KB\n"));
            return FALSE;
        }
        lpBuffer = new char[0x20000]; //read file to buffer
        memset(lpBuffer,0x00,0x20000);

        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;

        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);
        m_fhead->filelen=0x20000;
        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    case UBOOT:
        swscanf_s(_T("1"),_T("%d"),&m_fhead->no);
        swscanf_s(m_execaddr,_T("%x"),&m_fhead->execaddr);
        swscanf_s(m_startblock,_T("%x"),&m_fhead->flashoffset);
        //-------------------DDR---------------------
        ddrbuf=DDR2Buf(mainWnd->ShareDDRBuf,mainWnd->DDRLen,&ddrlen);
        file_len=file_len+ddrlen;
        ((NORBOOT_NAND_HEAD *)m_fhead)->initSize=ddrlen;
        //-------------------------------------------
        *len=file_len;
        lpBuffer = new char[file_len]; //read file to buffer
        memset(lpBuffer,0xff,file_len);
        ((NORBOOT_NAND_HEAD *)m_fhead)->macaddr[7]=0;

        wcstombs( m_fhead->name, (wchar_t *)m_imagename.GetBuffer(), 16);

        m_fhead->type=m_type;
        memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
        break;
    }

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write SPI NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    switch(m_type)
    {
    case DATA:
    case IMAGE:
        fread(lpBuffer,m_fhead->filelen,1,fp);
        break;
    case PACK:
        fread(lpBuffer,m_fhead->filelen,1,fp);
        break;
    case ENV:
#if 0
        fread(lpBuffer+4,file_len,1,fp);
#else
        {
            char line[256];
            char* ptr=(char *)(lpBuffer+4);
            while (1)
            {
                if (fgets(line,256, fp) == NULL) break;
                if(line[strlen(line)-2]==0x0D || line[strlen(line)-1]==0x0A)
                {
                    strncpy(ptr,line,strlen(line)-1);
                    ptr[strlen(line)-2]=0x0;
                    ptr+=(strlen(line)-1);
                }
                else
                {
                    strncpy(ptr,line,strlen(line));
                    ptr+=(strlen(line));
                }
            }

        }
#endif

        *(unsigned int *)lpBuffer=mainWnd->CalculateCRC32((unsigned char *)(lpBuffer+4),0x20000-4);
        *len=file_len=0x20000;
        if(mainWnd->envbuf!=NULL) free(mainWnd->envbuf);
        mainWnd->envbuf=(unsigned char *)malloc(0x20000);
        memcpy(mainWnd->envbuf,lpBuffer,0x20000);
        break;
    case UBOOT:
        memcpy(lpBuffer,ddrbuf,ddrlen);
        fread(lpBuffer+ddrlen,m_fhead->filelen,1,fp);
        break;
    }

    scnt=file_len/BUF_SIZE;
    rcnt=file_len%BUF_SIZE;

    total=0;
    char *pbuf = lpBuffer;
    while(scnt>0)
    {
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, BUF_SIZE);
        pbuf+=BUF_SIZE;
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        if(bResult==TRUE)
        {
            total+=BUF_SIZE;

            pos=(int)(((float)(((float)total/(float)file_len))*100));

            //TRACE(_T("SPI NAND scnt wait ack\n"));
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            //TRACE(_T("SPI NAND scnt wait ack end\n"));

            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                return FALSE;
            }

        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        scnt--;

        if(pos%5==0)
        {
            PostMessage(WM_SPINAND_PROGRESS,(LPARAM)pos,0);
        }

    }

    if(rcnt>0)
    {
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf,rcnt);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        //TRACE(_T("upload %%.2f\r"),((float)total/file_len)*100);
        if(bResult==TRUE)
        {
            total+=rcnt;
            //DbgOut("SPI NAND rcnt wait ack\n");
            bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
            //DbgOut("SPI NAND rcnt wait ack end\n");

            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
                return FALSE;

            }

        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            fclose(fp);
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        pos=(int)(((float)(((float)total/(float)file_len))*100));

        if(pos>=100)
        {
            pos=100;
        }
        if(pos%5==0)
        {
            PostMessage(WM_SPINAND_PROGRESS,(LPARAM)pos,0);
        }

    }
    delete []lpBuffer;
    fclose(fp);

    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)blockNum,4);
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! Read block error"));
        return FALSE;
    }

    NucUsb.NUC_CloseHandle();
    return TRUE;
}


int CSPINandDlg::XUSB_Verify(CString& portName,CString& m_pathName)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    unsigned int total=0,file_len,scnt,rcnt,ack;
    char* lpBuffer;
    char temp[BUF_SIZE];
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage(WM_SPINAND_PROGRESS,(LPARAM)0,0);
    m_progress.SetBkColor(COLOR_VERIFY);

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,SPINAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    if(m_type!=ENV)
    {
        fseek(fp,0,SEEK_END);
        file_len=ftell(fp);
        fseek(fp,0,SEEK_SET);
    }
    else
    {
        file_len=0x20000;
    }

    if(!file_len)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    if(m_type==PACK)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        return NAND_VERIFY_PACK_ERROR;
    }

    if(m_type==IMAGE && (file_len%512)!=0)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        return NAND_VERIFY_FILESYSTEM_ERROR;
    }

    m_fhead->flag=VERIFY_ACTION;

    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write SPI NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    scnt=file_len/BUF_SIZE;
    rcnt=file_len%BUF_SIZE;
    total=0;
    int prepos=0;
    while(scnt>0)
    {
        if(m_type!=ENV)
        {
            fread(temp,BUF_SIZE,1,fp);
        }
        else
        {
            memcpy(temp,mainWnd->envbuf+total,BUF_SIZE);
        }
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer, BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(bResult==TRUE)
        {
            total+=BUF_SIZE;

            pos=(int)(((float)(((float)total/(float)file_len))*100));
            //posstr.Format(_T("%d%%"),pos);

            if(DataCompare(temp,lpBuffer,BUF_SIZE))
                ack=BUF_SIZE;
            else
                ack=0;//compare error
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if((bResult==FALSE)||(!ack))
            {
                TRACE(_T("XXX scnt = %d\n"), scnt);
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                return FALSE;
            }

        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        scnt--;

        if(pos-prepos>=5 || pos==100)
        {
            prepos=pos;
            PostMessage(WM_SPINAND_PROGRESS,(LPARAM)pos,0);
        }
    }

    if(rcnt>0)
    {
        if(m_type!=ENV)
        {
            fread(temp,rcnt,1,fp);
        }
        else
        {
            memcpy(temp,mainWnd->envbuf+total,rcnt);
        }

        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(bResult==TRUE)
        {
            total+=rcnt;
            if(DataCompare(temp,lpBuffer,rcnt))
                ack=BUF_SIZE;
            else
                ack=0;//compare error
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack, 4);
            if((bResult==FALSE)||(!ack))
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(fp);
                return FALSE;

            }

        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(fp);
            return FALSE;
        }

        pos=(int)(((float)(((float)total/(float)file_len))*100));

        if(pos>=100)
        {
            pos=100;
        }

        if(pos-prepos>=5 || pos==100)
        {
            prepos=pos;
            PostMessage(WM_SPINAND_PROGRESS,(LPARAM)pos,0);
        }

    }

    delete []lpBuffer;
    fclose(fp);
    NucUsb.NUC_CloseHandle();
    return TRUE;

}

BOOL CSPINandDlg::XUSB_Read_Redunancy(CString& portName,CString& m_pathName,unsigned int addr,unsigned int len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    int pos=0;
    unsigned int total=0,scnt,rcnt,ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage(WM_SPINAND_PROGRESS,(LPARAM)0,0);
    m_progress.SetBkColor(COLOR_VERIFY);

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,SPINAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    FILE* tempfp;
    //-----------------------------------
    tempfp=_wfopen(m_pathName,_T("w+b"));
    //-----------------------------------
    if(!tempfp)
    {
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    m_fhead->flag=READ_ACTION;
    m_fhead->flashoffset=addr;
    m_fhead->filelen=len;
    m_fhead->initSize=1;  //read redundancy data, good block and bad block
    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(tempfp);
        AfxMessageBox(_T("Error! Write SPI NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(tempfp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    scnt=len/BUF_SIZE;
    rcnt=len%BUF_SIZE;
    total=0;
    while(scnt>0)
    {
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        if(bResult==TRUE)
        {
            total+=BUF_SIZE;
            pos=(int)(((float)(((float)total/(float)len))*100));
            posstr.Format(_T("%d%%"),pos);
            fwrite(lpBuffer,BUF_SIZE,1,tempfp);
            ack=BUF_SIZE;
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }
        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }

        scnt--;
        if(pos%5==0)
        {
            PostMessage(WM_SPINAND_PROGRESS,(LPARAM)pos,0);
        }
    }

    if(rcnt>0)
    {
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        if(bResult==TRUE)
        {
            total+=rcnt;
            fwrite(lpBuffer,rcnt,1,tempfp);
            ack=BUF_SIZE;
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if(bResult!=TRUE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }
        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        pos=(int)(((float)(((float)total/(float)len))*100));
        if(pos>=100)
        {
            pos=100;
        }
        posstr.Format(_T("%d%%"),pos);
        if(pos%5==0)
        {
            PostMessage(WM_SPINAND_PROGRESS,(LPARAM)pos,0);
        }
    }
    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    fclose(tempfp);
    /********************************************/
    return TRUE;
}

BOOL CSPINandDlg::XUSB_Read(CString& portName,CString& m_pathName,unsigned int addr,unsigned int len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    int pos=0;
    unsigned int total=0,scnt,rcnt,ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    PostMessage(WM_SPINAND_PROGRESS,(LPARAM)0,0);
    m_progress.SetBkColor(COLOR_VERIFY);

    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,SPINAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    FILE* tempfp;
    //-----------------------------------
    tempfp=_wfopen(m_pathName,_T("w+b"));
    //-----------------------------------
    if(!tempfp)
    {
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    m_fhead->flag=READ_ACTION;
    m_fhead->flashoffset=addr;
    m_fhead->filelen=len;
    m_fhead->initSize=0; //read good block
    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(tempfp);
        AfxMessageBox(_T("Error! Write SPI NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(tempfp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    scnt=len/BUF_SIZE;
    rcnt=len%BUF_SIZE;
    total=0;
    while(scnt>0)
    {
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        if(bResult==TRUE)
        {
            total+=BUF_SIZE;
            pos=(int)(((float)(((float)total/(float)len))*100));
            posstr.Format(_T("%d%%"),pos);
            fwrite(lpBuffer,BUF_SIZE,1,tempfp);
            ack=BUF_SIZE;
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if(bResult==FALSE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }
        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }

        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }

        scnt--;
        if(pos%5==0)
        {
            PostMessage(WM_SPINAND_PROGRESS,(LPARAM)pos,0);
        }
    }

    if(rcnt>0)
    {
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)lpBuffer,BUF_SIZE);
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        if(bResult==TRUE)
        {
            total+=rcnt;
            fwrite(lpBuffer,rcnt,1,tempfp);
            ack=BUF_SIZE;
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&ack,4);
            if(bResult!=TRUE)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                fclose(tempfp);
                return FALSE;
            }
        }
        else
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            fclose(tempfp);
            return FALSE;
        }
        pos=(int)(((float)(((float)total/(float)len))*100));
        if(pos>=100)
        {
            pos=100;
        }
        posstr.Format(_T("%d%%"),pos);
        if(pos%5==0)
        {
            PostMessage(WM_SPINAND_PROGRESS,(LPARAM)pos,0);
        }
    }
    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    fclose(tempfp);
    /********************************************/
    return TRUE;
}

BOOL CSPINandDlg::XUSB_Erase(CString& portName)
{
    BOOL bResult;
    CString tempstr;
    int count=0;
    unsigned int ack,erase_pos=0, BlockPerFlash;
    NORBOOT_NAND_HEAD *fhead;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    fhead=(NORBOOT_NAND_HEAD *)malloc(sizeof(NORBOOT_NAND_HEAD));

    m_progress.SetRange(0,100);
    /***********************************************/
    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        free(fhead);
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        free(fhead);
        return FALSE;
    }
#endif
    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,SPINAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        free(fhead);
        return FALSE;
    }

    lpBuffer = new char[BUF_SIZE];
    memset((unsigned char*)fhead,0,sizeof(NORBOOT_NAND_HEAD));
    fhead->flag=ERASE_ACTION;

    fhead->flashoffset = _wtoi(m_sblocks); //start erase block
    fhead->execaddr=_wtoi(m_blocks);  //erase block length
    fhead->type=m_erase_flag; // Decide chip erase mode or erase mode

    if(mainWnd->ChipEraseWithBad==0)
        fhead->no=0xFFFFFFFF;//erase good block
    else
        fhead->no=0xFFFFFFFE;//erase good block and bad block
    memcpy(lpBuffer,(unsigned char*)fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));
    free(fhead);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! Write SPI NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! ACK(0x%x) error ! Please reset device and Re-connect now !!!"), ack);
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&BlockPerFlash,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! BlockPerFlash error !"));
        return FALSE;
    }
    TRACE(_T("SPI NAND BlockPerFlash = %d\n"), BlockPerFlash);
    erase_pos=0;
    int wait_pos=0;
    while(erase_pos!= 100)
    {
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }

        //TRACE(_T("SPI NAND wait erase ack\n"));
        //Sleep(2);
        Sleep(10);
        //Sleep(15);
        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
            return FALSE;
        }

        if(m_erase_flag == 0) // erase all
        {
            if(ack != 0xffff)
            {
                erase_pos = (int)(((float)(((float)ack/(float)BlockPerFlash))*100));
                if(ack == BlockPerFlash-1)
                {
                    if(erase_pos < 99)
                        erase_pos = 100;
                    else
                        erase_pos++;
                }
                TRACE(_T("erase_pos =%d, ack =%d\n"), erase_pos, ack);
                PostMessage(WM_SPINAND_PROGRESS,(LPARAM)erase_pos,0);
            }
            else
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                CString msg;
                msg.Format(_T("Erase error ack=0x%08x\n"),ack);
                AfxMessageBox(msg);
                return FALSE;
            }
        }
        else //erase accord start and length blocks.
        {
            if(ack != 0xffff)
            {
                erase_pos=ack&0xffff;
                PostMessage(WM_SPINAND_PROGRESS,(LPARAM)erase_pos,0);
            }
            else
            {

                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                CString msg;
                msg.Format(_T("Erase error ack=0x%08x\n"),ack);
                AfxMessageBox(msg);
                return FALSE;
            }

            if(erase_pos==95)
            {
                wait_pos++;
                if(wait_pos>100)
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    CString msg;
                    msg.Format(_T("Erase error ack=0x%08x\n"),ack);
                    AfxMessageBox(msg);
                    return FALSE;
                }
            }
        }
    }

    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    return TRUE;
}


BOOL CSPINandDlg::XUSB_Pack(CString& portName,CString& m_pathName,int *len)
{
    BOOL bResult;
    CString posstr;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    UCHAR burn_pos=0;
    unsigned int total,file_len,scnt,rcnt;
    ULONG ack;
    char* lpBuffer;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    bResult = NucUsb.EnableWinUsbDevice();
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif
    USHORT typeack=0x0;
    bResult=NucUsb.NUC_SetType(0,SPINAND,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE)
    {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }


    fp=_wfopen(m_pathName,_T("rb"));

    if(!fp)
    {
        NucUsb.NUC_CloseHandle();
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }

    fseek(fp,0,SEEK_END);
    file_len=ftell(fp);
    fseek(fp,0,SEEK_SET);

    if(!file_len)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! File length is zero\n"));
        return FALSE;
    }

    unsigned int magic;
    fread((unsigned char *)&magic,4,1,fp);
    if(magic!=0x5)
    {
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Pack Image Format Error\n"));
        return FALSE;
    }
    fseek(fp,0,SEEK_SET);

    lpBuffer = new char[file_len]; //read file to buffer
    memset(lpBuffer,0x00,file_len);

    memset((unsigned char*)m_fhead,0,sizeof(NORBOOT_NAND_HEAD));
    total=0;
    m_fhead->flag=PACK_ACTION;
    m_fhead->type=m_type;
    m_fhead->filelen=file_len;
    memcpy(lpBuffer,(unsigned char*)m_fhead,sizeof(NORBOOT_NAND_HEAD));
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)lpBuffer, sizeof(NORBOOT_NAND_HEAD));

    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("Error! Write SPI NAND head error\n"));
        return FALSE;
    }
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        fclose(fp);
        AfxMessageBox(_T("ACK Error! Please reset device and Re-connect now !!!"));
        return FALSE;
    }

    fread(lpBuffer,m_fhead->filelen,1,fp);
    if(lpBuffer[0]!=0x5)
    {
        AfxMessageBox(_T("Error! This file is not pack image"));
    }
    fclose(fp);

    // Check DDR *ini
    bResult=CheckDDRiniData(lpBuffer, m_fhead->filelen);
    if(bResult==FALSE)
    {
        AfxMessageBox(_T("Error! DDR Init select error\n"));
        delete []lpBuffer;
        return FALSE;
    }

    char *pbuf = lpBuffer;
    PACK_HEAD *ppackhead=(PACK_HEAD *)lpBuffer;
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, sizeof(PACK_HEAD));
    Sleep(5);
    if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT) return FALSE;
    if(bResult!=TRUE) return FALSE;
    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
    if(bResult==FALSE)
    {
        delete []lpBuffer;
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }
    total+= sizeof(PACK_HEAD);
    pbuf+= sizeof(PACK_HEAD);

    PACK_CHILD_HEAD child;
    m_progress.SetRange(0,short(ppackhead->num*100));
    int posnum=0;
    int prepos=0;

    unsigned int blockNum;
    for(int i=0; i<(int)(ppackhead->num); i++)
    {
        total=0;
        memcpy(&child,(char *)pbuf,sizeof(PACK_CHILD_HEAD));
        bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
        if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT) return FALSE;
        if(bResult!=TRUE) return FALSE;

        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            return FALSE;
        }
        //total+= sizeof(PACK_CHILD_HEAD);
        pbuf+= sizeof(PACK_CHILD_HEAD);
        //Sleep(1);
        scnt=child.filelen/BUF_SIZE;
        rcnt=child.filelen%BUF_SIZE;
        total=0;
        while(scnt>0)
        {
            //Sleep(20);
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf, BUF_SIZE);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }

            if(bResult==TRUE)
            {
                pbuf+=BUF_SIZE;
                total+=BUF_SIZE;

                pos=(int)(((float)(((float)total/(float)child.filelen))*100));
                posstr.Format(_T("%d%%"),pos);

                TRACE(_T("SPI NAND wait ack"));
                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                TRACE(_T("SPI NAND wait ack end"));

                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    CString tmp;
                    tmp.Format(_T("ACK error %d!"),i);
                    AfxMessageBox(tmp);
                    return FALSE;
                }

            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }

            scnt--;

            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage(WM_SPINAND_PROGRESS,(LPARAM)(posnum+pos),0);
            }

        }

        if(rcnt>0)
        {
            //Sleep(20);
            bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)pbuf,rcnt);
            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }

            //TRACE(_T("upload %%.2f\r"),((float)total/file_len)*100);
            if(bResult==TRUE)
            {
                pbuf+=rcnt;
                total+=rcnt;
                TRACE(_T("SPI NAND wait ack"));
                bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&ack,4);
                TRACE(_T("SPI NAND wait ack end"));

                if(bResult==FALSE)
                {
                    delete []lpBuffer;
                    NucUsb.NUC_CloseHandle();
                    fclose(fp);
                    CString tmp;
                    tmp.Format(_T("ACK error(rcnt>0) %d!"),i);
                    AfxMessageBox(tmp);
                    return FALSE;

                }

            }

            if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT)
            {
                delete []lpBuffer;
                NucUsb.NUC_CloseHandle();
                return FALSE;
            }

            pos=(int)(((float)(((float)total/(float)child.filelen))*100));

            if(pos>=100)
            {
                pos=100;
            }

            if((pos!=prepos) || (pos==100))
            {
                prepos=pos;
                PostMessage(WM_SPINAND_PROGRESS,(LPARAM)(posnum+pos),0);
            }

        }
        posnum+=100;

        bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&blockNum,4);
        if(bResult==FALSE)
        {
            delete []lpBuffer;
            NucUsb.NUC_CloseHandle();
            AfxMessageBox(_T("Error! Read block error."));
            return FALSE;
        }
    }//for(int i=0;i<(int)(ppackhead->num);i++)

    delete []lpBuffer;
    NucUsb.NUC_CloseHandle();
    return TRUE;
}

/************** SPI NAND End ************/
