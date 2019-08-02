// PACKDlg.cpp : implementation file
//

#include "stdafx.h"
#include "NuWriter.h"
#include "NuWriterDlg.h"
#include "PACKDlg.h"
#include "PackTab1.h"

// CPACKDlg dialog
#define PACK_PAR 0
#define PACK_MTP 1


#define PACK_Mode(val)          (((val)&0xF   )>> 0)
#define PACK_Option(val)        (((val)&0xF0  )>> 4)
#define PACK_Encrypt(val)       (((val)&0xF00 )>> 8)
#define PACK_Enable(val)        (((val)&0xF000)>>12)

#define Enc_PACK_Mode(val)      (((val)&0xF)<< 0)
#define Enc_PACK_Option(val)    (((val)&0xF)<< 4)
#define Enc_PACK_Encrypt(val)   (((val)&0xF)<< 8)
#define Enc_PACK_Enable(val)    (((val)&0xF)<<12)


IMPLEMENT_DYNAMIC(CPACKDlg, CDialog)

CPACKDlg::CPACKDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CPACKDlg::IDD, pParent)
{
    TmpOffsetFlag=0;
    InitFlag=0;
    modifyflag=0;
}

CPACKDlg::~CPACKDlg()
{
}

void CPACKDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_PACK_ADD, m_add);
    DDX_Control(pDX, IDC_PACK_MODIFY, m_modify);
    DDX_Control(pDX, IDC_PACK_DELETE, m_delete);
    DDX_Control(pDX, IDC_PACK_OUTPUT, m_output);
    DDX_Control(pDX, IDC_PACK_DOWNPROGRESS, m_progress);
    DDX_Control(pDX, IDC_PACK_IMAGELIST, m_imagelist);
    DDX_Control(pDX, IDC_PACK_TABCONTROL, m_pack_tabcontrol);
    DDX_Control(pDX, IDC_PACK_STATUS, m_status);
}


BEGIN_MESSAGE_MAP(CPACKDlg, CDialog)
    ON_BN_CLICKED(IDC_PACK_ADD, &CPACKDlg::OnBnClickedPackAdd)
    ON_BN_CLICKED(IDC_PACK_MODIFY, &CPACKDlg::OnBnClickedPackModify)
    ON_BN_CLICKED(IDC_PACK_DELETE, &CPACKDlg::OnBnClickedPackDelete)
    ON_BN_CLICKED(IDC_PACK_OUTPUT, &CPACKDlg::OnBnClickedPackOutput)
    ON_WM_SHOWWINDOW()
    ON_NOTIFY(NM_DBLCLK, IDC_PACK_IMAGELIST, &CPACKDlg::OnNMDblclkPackImagelist)
    ON_NOTIFY(TCN_SELCHANGE, IDC_PACK_TABCONTROL, &CPACKDlg::OnTcnSelchangePackTabcontrol)
END_MESSAGE_MAP()


// CPACKDlg message handlers
BOOL CPACKDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_imagelist.SetHeadings(_T("Name, 180 ; Type,70 ; BootType,80 ; Start Address(Hex),130 ; End Address(Hex),130 ; Exec Address(Hex),136 ; SPI User Defined,140"));
    m_imagelist.SetGridLines(TRUE);

    COLORREF col = RGB(0xFF, 0x00, 0xFF);
    m_add.setBitmapId(IDB_ADD, col);
    m_add.setGradient(true);
    m_modify.setBitmapId(IDB_MODIFY, col);
    m_modify.setGradient(true);
    m_delete.setBitmapId(IDB_DELETE, col);
    m_delete.setGradient(true);
    m_output.setBitmapId(IDB_OUTPUT, col);
    m_output.setGradient(true);
    //m_browse.setBitmapId(IDB_BROWSE, col);
    //m_browse.setGradient(true);

    m_packtab1.Create(CPackTab1::IDD, GetDlgItem(IDC_PACK_TABCONTROL));

    m_pack_tabcontrol.InsertItem(0, _T("Parameter"));

    SwapTab();
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CPACKDlg::SwapTab(void)
{
    CRect rTab, rItem;
    m_pack_tabcontrol.GetItemRect(0, &rItem);
    m_pack_tabcontrol.GetClientRect(&rTab);
    int x = rItem.left;
    int y = rItem.bottom + 1;
    int cx = rTab.right - rItem.left - 3;
    int cy = rTab.bottom - y -2;
    int tab = m_pack_tabcontrol.GetCurSel();
    m_packtab1.SetWindowPos(NULL, x, y, cx, cy, SWP_HIDEWINDOW);
    m_packtab1.SetWindowPos(NULL, x, y, cx, cy, SWP_SHOWWINDOW);

}

void CPACKDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CDialog::OnShowWindow(bShow, nStatus);


    // TODO: Add your message handler code here
    if(InitFlag==0) {
        InitFlag=1;
        InitFile(0);
    }
    //UpdateData (TRUE);
}

extern unsigned char * DDR2Buf(char *buf,int buflen,int *ddrlen);

void CPACKDlg::OnBnClickedPackAdd()
{
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    if(m_pack_tabcontrol.GetCurSel()==PACK_PAR) {
        m_packtab1.UpdateData(TRUE);

        // TODO: Add your control notification handler code here
        CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
        //szFileName.Format(_T("%s%s"),filename,ext);
        //sList.push_back(szFileName); // 將完整路徑加到 sList 裡頭
        if(m_packtab1.m_type != PARTITION) {
            if(m_packtab1.m_imagename.IsEmpty()) {
                AfxMessageBox(_T("Error! Please input image file"));
                return;
            }

            if((m_packtab1.m_type!=UBOOT)&&((m_packtab1.m_execaddr.IsEmpty())||(m_packtab1.m_startblock.IsEmpty()) )) {

                AfxMessageBox(_T("Error! Please input image information"));
                return;
            }
        }
        else {
            m_packtab1.m_filename = _T("Partition_INFO");
            m_packtab1.m_ubootflashtype = TYPE_EMMC;
            m_packtab1.m_execaddr = _T("0x0");
            m_packtab1.m_startblock = _T("0x0");
            m_packtab1.m_isUserConfig = 0;
        }

        ImageName.push_back(m_packtab1.m_filename);
        ImageType.push_back(m_packtab1.m_type);

        CString ubootflashtype;
        switch(m_packtab1.m_ubootflashtype) {
        case TYPE_NAND:
            ubootflashtype.Format(_T("NAND"));
            break;
        case TYPE_SPI:
            ubootflashtype.Format(_T("SPI"));
            break;
        case TYPE_EMMC:
            ubootflashtype.Format(_T("eMMC/SD"));
            break;
        case TYPE_SPINAND:
            ubootflashtype.Format(_T("SPI NAND"));
            break;
        }
        FlashType.push_back(m_packtab1.m_ubootflashtype);

        ImageExec.push_back(m_packtab1.m_execaddr);
        ImageStartblock.push_back(m_packtab1.m_startblock);

        CString userstr;
        switch(m_packtab1.m_isUserConfig) {
        case 1: {
            userstr.Format(_T("Yes"));
            mainWnd->m_info.SPI_uIsUserConfig = 1;
            mainWnd->m_info.SPINand_uIsUserConfig = 1;
            break;
        }
        case 0:
        default: {
            userstr.Format(_T("No"));
            mainWnd->m_info.SPI_uIsUserConfig = 0;
            mainWnd->m_info.SPINand_uIsUserConfig = 0;
            break;
        }
        }
        UserConfig.push_back(m_packtab1.m_isUserConfig);

        CString flagstr;
        switch(m_packtab1.m_type) {
        case DATA :
            flagstr.Format(_T("DATA"));
            break;
        case ENV  :
            flagstr.Format(_T("ENV"));
            break;
        case UBOOT:
            flagstr.Format(_T("uBOOT"));
            break;
        case PARTITION:
            flagstr.Format(_T("FORMAT"));
            break;
#if(0)
        case PACK :
            flagstr.Format(_T("Pack"));
            break;
#endif
        case IMAGE :
            flagstr.Format(_T("FS"));
            break;
        }
        CString len;
        if(m_packtab1.m_type != PARTITION) {
            FILE* rfp;
            int total;
            int startblock;
            rfp=_wfopen(m_packtab1.m_filename,_T("rb"));
            fseek(rfp,0,SEEK_END);
            _stscanf_s(m_packtab1.m_startblock,_T("%x"),&startblock);
            total=ftell(rfp)+startblock;
            if(m_packtab1.m_type==UBOOT) {
                int ddrlen;
                UCHAR * ddrbuf;
                ddrbuf=DDR2Buf(mainWnd->ShareDDRBuf,mainWnd->DDRLen,&ddrlen);
                total+=(ddrlen+32);
            }

            len.Format(_T("%x"),total);
            fclose(rfp);
        }
        switch(m_packtab1.m_type) {
        case DATA:
            m_imagelist.InsertItem(m_imagelist.GetItemCount(),m_packtab1.m_imagename,flagstr,_T(""),m_packtab1.m_startblock,len,_T(""),_T(""));
            break;
        case ENV:
            m_imagelist.InsertItem(m_imagelist.GetItemCount(),m_packtab1.m_imagename,flagstr,ubootflashtype,m_packtab1.m_startblock,len,_T(""),_T(""));
            break;
        case PARTITION:
            m_imagelist.InsertItem(m_imagelist.GetItemCount(),_T("Partition_INFO"),flagstr,_T(""),_T(""),_T(""),_T(""),_T(""));
            break;
        case UBOOT:
            m_imagelist.InsertItem(m_imagelist.GetItemCount(),m_packtab1.m_imagename,flagstr,ubootflashtype,m_packtab1.m_startblock,len,m_packtab1.m_execaddr, userstr);
            break;
        }
    }

    //vector<CString>::iterator item;
    //for(item=ImageExec.begin();item!=ImageExec.end();item++)
    //{
    //  AfxMessageBox(*item);
    //}
}

void CPACKDlg::OnBnClickedPackModify()
{
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    vector<CString>::iterator itemName;
    vector<int>::iterator itemType;
    vector<int>::iterator itemFlash;
    vector<CString>::iterator itemExec;
    vector<CString>::iterator itemStartblock;
    vector<int>::iterator itemUserconfig;

    if(m_pack_tabcontrol.GetCurSel()==PACK_PAR) {
        m_packtab1.UpdateData(TRUE);

        if(m_packtab1.m_imagename.IsEmpty()) {
            AfxMessageBox(_T("Error! Please input image file"));
            return;
        }

        if((m_packtab1.m_type!=UBOOT)&&((m_packtab1.m_execaddr.IsEmpty())||(m_packtab1.m_startblock.IsEmpty()) )) {
            CString tmp;
            tmp.Format(_T("%d 0x%08x 0x%08x"),m_packtab1.m_type,m_packtab1.m_execaddr,m_packtab1.m_startblock);
            AfxMessageBox(tmp);

            //AfxMessageBox(_T("Please input image information"));
            return;
        }

        int i;
        int modify_idx;
        int imagelen=m_imagelist.GetItemCount();

        for(i=0; i<imagelen; i++) {
            if(m_imagelist.IsItemSelected(i)==TRUE)
                break;
        }
        if(i==imagelen || imagelen==0) {
            AfxMessageBox(_T("Error! Please select image item to modify first"));
            return;
        }
        modify_idx=i;
        //------------------------------------------------------------------------------
        itemName=ImageName.begin()+modify_idx;
        itemType=ImageType.begin()+modify_idx;
        itemFlash=FlashType.begin()+modify_idx;
        itemExec=ImageExec.begin()+modify_idx;
        itemStartblock=ImageStartblock.begin()+modify_idx;
        itemUserconfig=UserConfig.begin()+modify_idx;

        *itemName=m_packtab1.m_filename;
        *itemExec=m_packtab1.m_execaddr;

        *itemType=m_packtab1.m_type;
        *itemFlash=m_packtab1.m_ubootflashtype;
        *itemExec=m_packtab1.m_execaddr;
        *itemStartblock=m_packtab1.m_startblock;
        *itemUserconfig=m_packtab1.m_isUserConfig;
    }

    //------------------------------------------------------------------------------
    int imagelen=m_imagelist.GetItemCount();
    CString flagstr,flashstr,userstr;
    m_imagelist.DeleteAllItems();
    for(int i=0; i<imagelen; i++) {
        itemName=ImageName.begin()+i;
        itemType=ImageType.begin()+i;
        itemFlash=FlashType.begin()+i;
        itemExec=ImageExec.begin()+i;
        itemStartblock=ImageStartblock.begin()+i;
        itemUserconfig=UserConfig.begin()+i;

        CString filename,tmp;
        tmp=(*itemName).Mid((*itemName).ReverseFind('\\')+1, (*itemName).GetLength());
        if(tmp.ReverseFind('.')>0)
            filename=tmp.Mid(0,tmp.ReverseFind('.'));
        else
            filename=tmp;
        // if(filename.GetLength()>16)
        //     filename = filename.Mid(0,15);

        switch(*itemType) {
        case DATA :
            flagstr.Format(_T("DATA"));
            break;
        case ENV  :
            flagstr.Format(_T("ENV"));
            break;
        case UBOOT:
            flagstr.Format(_T("uBOOT"));
            break;
        case PARTITION:
            flagstr.Format(_T("FORMAT"));
            break;
#if(0)
        case PACK :
            flagstr.Format(_T("Pack"));
            break;
#endif
        case IMAGE :
            flagstr.Format(_T("FS"));
            break;
        }

        switch(*itemFlash) {
        case TYPE_NAND:
            flashstr.Format(_T("NAND"));
            break;
        case TYPE_SPI:
            flashstr.Format(_T("SPI"));
            break;
        case TYPE_EMMC:
            flashstr.Format(_T("eMMC/SD"));
            break;
        case TYPE_SPINAND:
            flashstr.Format(_T("SPI NAND"));
            break;
        }

        switch(*itemUserconfig) {
        case 1: {
            userstr.Format(_T("Yes"));
            mainWnd->m_info.SPI_uIsUserConfig = 1;
            mainWnd->m_info.SPINand_uIsUserConfig = 1;
            break;
        }
        case 0:
        default: {
            userstr.Format(_T("No"));
            mainWnd->m_info.SPI_uIsUserConfig = 0;
            mainWnd->m_info.SPINand_uIsUserConfig = 0;
            break;
        }
        }

        CString len;
        if(*itemType != PARTITION) {
        //if(*itemType!=PMTP) {
            FILE* rfp;
            int total;
            int startblock;
            rfp=_wfopen(*itemName,_T("rb"));
            fseek(rfp,0,SEEK_END);
            _stscanf_s(*itemStartblock,_T("%x"),&startblock);
            total=ftell(rfp)+startblock;
            if(*itemType==UBOOT) {
                int ddrlen;
                UCHAR * ddrbuf;
                ddrbuf=DDR2Buf(mainWnd->ShareDDRBuf,mainWnd->DDRLen,&ddrlen);
                total+=(ddrlen+32);
            }
            len.Format(_T("%x"),total);
            fclose(rfp);
        }

        switch(*itemType) {
        case DATA:
            m_imagelist.InsertItem(m_imagelist.GetItemCount(),filename,flagstr,_T(""),*itemStartblock,len,_T(""),_T(""));
            break;
        case ENV:
            m_imagelist.InsertItem(m_imagelist.GetItemCount(),filename,flagstr,flashstr,*itemStartblock,len,_T(""),_T(""));
            break;
        case PARTITION:
            m_imagelist.InsertItem(m_imagelist.GetItemCount(),_T("Partition_INFO"),flagstr,_T(""),_T(""),_T(""),_T(""),_T(""));
            break;
        case UBOOT:
            m_imagelist.InsertItem(m_imagelist.GetItemCount(),filename,flagstr,flashstr,*itemStartblock,len,*itemExec,userstr);
        }
    }

}

void CPACKDlg::OnBnClickedPackDelete()
{
#if 1
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    m_packtab1.UpdateData(TRUE);

    vector<CString>::iterator itemName;
    vector<int>::iterator itemType;
    vector<int>::iterator itemFlash;
    vector<CString>::iterator itemExec;
    vector<CString>::iterator itemStartblock;
    vector<int>::iterator itemUserconfig;
    int modify_idx;
    int i;
    int imagelen=m_imagelist.GetItemCount();

    for(i=0; i<imagelen; i++) {
        if(m_imagelist.IsItemSelected(i)==TRUE)
            break;
    }
    if(i==imagelen || imagelen==0) {
        AfxMessageBox(_T("Error! Please select image item to modify first"));
        return;
    }
    modify_idx=i;
    ImageName.erase(ImageName.begin() + modify_idx);
    ImageType.erase(ImageType.begin() + modify_idx);
    FlashType.erase(FlashType.begin() + modify_idx);
    ImageExec.erase(ImageExec.begin() + modify_idx);
    ImageStartblock.erase(ImageStartblock.begin() + modify_idx);
    UserConfig.erase(UserConfig.begin() + modify_idx);

    //--- re-show image list ---
    m_imagelist.DeleteAllItems();

    i=0;
    for(itemName=ImageName.begin(); itemName!=ImageName.end(); itemName++) {
        itemType=(ImageType.begin()+i);
        itemFlash=(FlashType.begin()+i);
        itemExec=(ImageExec.begin()+i);
        itemStartblock=(ImageStartblock.begin()+i);
        itemUserconfig=(UserConfig.begin()+i);
        i++;

        CString flagstr,flashstr,userstr;
        switch(*itemType) {
        case DATA :
            flagstr.Format(_T("DATA"));
            break;
        case ENV  :
            flagstr.Format(_T("ENV"));
            break;
        case UBOOT:
            flagstr.Format(_T("uBOOT"));
            switch(*itemUserconfig) {
            case 1: {
                userstr.Format(_T("Yes"));
                mainWnd->m_info.SPI_uIsUserConfig = 1;
                mainWnd->m_info.SPINand_uIsUserConfig = 1;
                break;
            }
            case 0:
            default: {
                userstr.Format(_T("No"));
                mainWnd->m_info.SPI_uIsUserConfig = 0;
                mainWnd->m_info.SPINand_uIsUserConfig = 0;
                break;
            }
            }
            break;
        case PARTITION:
            flagstr.Format(_T("FORMAT"));
            break;
#if(0)
        case PACK :
            flagstr.Format(_T("Pack"));
            break;
#endif
        case IMAGE :
            flagstr.Format(_T("FS"));
            break;
        }

        switch(*itemFlash) {
        case TYPE_NAND:
            flashstr.Format(_T("NAND"));
            break;
        case TYPE_SPI:
            flashstr.Format(_T("SPI"));
            break;
        case TYPE_EMMC:
            flashstr.Format(_T("eMMC/SD"));
            break;
        case TYPE_SPINAND:
            flashstr.Format(_T("SPI NAND"));
            break;
        }


        CString filename,tmp;
        tmp=(*itemName).Mid((*itemName).ReverseFind('\\')+1, (*itemName).GetLength());
        if(tmp.ReverseFind('.')>0)
            filename=tmp.Mid(0,tmp.ReverseFind('.'));
        else
            filename=tmp;
        //if(filename.GetLength()>16)
        //    filename = filename.Mid(0,15);

        CString len;
        if(*itemType!=PMTP) {

            if(*itemType != PARTITION) {
            FILE* rfp;
            int total;
            int startblock;
            rfp=_wfopen(*itemName,_T("rb"));
            fseek(rfp,0,SEEK_END);
            _stscanf_s(*itemStartblock,_T("%x"),&startblock);
            total=ftell(rfp)+startblock;
            len.Format(_T("%x"),total);
            fclose(rfp);
            }
        }
        if(*itemType!=PMTP) {
            if(*itemType==UBOOT)
                m_imagelist.InsertItem(m_imagelist.GetItemCount(),filename,flagstr,flashstr,*itemStartblock,len,*itemExec,userstr);
            else if(*itemType==PARTITION)
                m_imagelist.InsertItem(m_imagelist.GetItemCount(),_T("Partition_INFO"),flagstr,_T(""),_T(""),_T(""),_T(""),_T(""));
            else if(*itemType==DATA)
                m_imagelist.InsertItem(m_imagelist.GetItemCount(),filename,flagstr,_T(""),*itemStartblock,len,_T(""),_T(""));
            else
                m_imagelist.InsertItem(m_imagelist.GetItemCount(),filename,flagstr,flashstr,*itemStartblock,len,_T(""),_T(""));
        }
    }
#else
    ImageName.clear();
    ImageType.clear();
    ImageExec.clear();
    ImageStartblock.clear();
    m_imagelist.DeleteAllItems();
#endif
}

extern unsigned char * DDR2Buf(char *buf,int buflen,int *ddrlen);
int CPACKDlg::Output()
{
    int i;
    int tmp;
    //m_packtab1.UpdateData(TRUE);
    PostMessage(WM_PACK_PROGRESS,(LPARAM)0,0);
    m_progress.SetBkColor(COLOR_DOWNLOAD);
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    FILE* wfp,*rfp;
    //-----------------------------------
    wfp=_wfopen(m_filename2,_T("w+b"));
    if(!wfp) {
        AfxMessageBox(_T("Error! File Open error\n"));
        return FALSE;
    }
    //-----------------------------------
    int storageType=0;
    int storageSize=64*1024;
    vector<CString>::iterator itemName;
    vector<int>::iterator itemType;
    vector<int>::iterator itemFlash;
    vector<CString>::iterator itemExec;
    vector<CString>::iterator itemStartblock;
    vector<CString>::iterator itemuBoot;
    vector<int>::iterator itemUserConfig;

    mainWnd->INItoSaveOrLoad(1);//load path.ini to get SPI header

    int imagelen=m_imagelist.GetItemCount();

    //-----------------------------------------
    for(i=0; i<imagelen; i++) {
        itemType=(ImageType.begin()+i);
        if(*itemType==UBOOT) break;
    }
    if(i==imagelen) {
        AfxMessageBox(_T("Error! Can't find uBoot image, please input uboot for startup"));
    }
    //-----------------------------------------

    int total=0;
    for(i=0; i<imagelen; i++) {
        itemType=(ImageType.begin()+i);
        if(*itemType!=PMTP) {
            if(*itemType!=PARTITION) {
                itemName=(ImageName.begin()+i);
                rfp=_wfopen(*itemName,_T("rb"));
                fseek(rfp,0,SEEK_END);
                total+=((ftell(rfp)+storageSize-1)/storageSize)*storageSize;
                fclose(rfp);
            }
            else {
                total+=(((PACK_FOMRAT_HEADER_LEN)+storageSize-1)/storageSize)*storageSize;
            }
        } else
            total+=(256/8);
    }

    PACK_HEAD pack_head;
    memset((char *)&pack_head,0xff,sizeof(pack_head));
    pack_head.actionFlag=PACK_ACTION;
    pack_head.fileLength=total;
    pack_head.num=imagelen;

    //write  pack_head
    fwrite((char *)&pack_head,sizeof(PACK_HEAD),1,wfp);

    PACK_CHILD_HEAD child;
    unsigned int len;
    for(i=0; i<imagelen; i++) {
        itemName=(ImageName.begin()+i);
        itemType=(ImageType.begin()+i);
        itemFlash=(FlashType.begin()+i);
        itemExec=(ImageExec.begin()+i);
        itemUserConfig=(UserConfig.begin()+i);

        if(*itemType!=PMTP) {
            itemStartblock=(ImageStartblock.begin()+i);
            if(*itemType!=PARTITION) {
                rfp=_wfopen(*itemName,_T("rb"));
                fseek(rfp,0,SEEK_END);
                len= ftell(rfp);
                fseek(rfp,0,SEEK_SET);
            }
            else
                len= PACK_FOMRAT_HEADER_LEN; // partition header length

            char *pBuffer=NULL;
            char magic[4]= {' ','T','V','N'};
            switch(*itemType) {
            case UBOOT: {
                int ddrlen;
                unsigned char *ddrbuf;

                ddrbuf=DDR2Buf(mainWnd->ShareDDRBuf,mainWnd->DDRLen,&ddrlen);
                memset((char *)&child,0xff,sizeof(PACK_CHILD_HEAD));

                //write  pack_child_head
                child.filelen=len+ddrlen+32;
                child.startaddr=0;
                child.imagetype=UBOOT;
                fwrite((char *)&child,1,sizeof(PACK_CHILD_HEAD),wfp);

                //write uboot head
                fwrite((char *)magic,1,sizeof(magic),wfp);
                _stscanf_s(*itemExec,_T("%x"),&tmp);
                fwrite((char *)&tmp,1,4,wfp);
                fwrite((char *)&len,1,4,wfp);
                tmp=0xffffffff;
                fwrite((char *)&tmp,1,4,wfp);
#if(1)
                //Add IBR header for NUC980 SPI NOR/NAND
                if(*itemFlash== TYPE_SPINAND) { //SPI NAND
                    tmp = mainWnd->m_info.SPINand_PageSize;
                    fwrite((char *)&tmp,1,2,wfp);
                    tmp = mainWnd->m_info.SPINand_SpareArea;
                    fwrite((char *)&tmp,1,2,wfp);
                    tmp = mainWnd->m_info.SPINand_QuadReadCmd;
                    fwrite((char *)&tmp,1,1,wfp);
                    tmp = mainWnd->m_info.SPINand_ReadStatusCmd;
                    fwrite((char *)&tmp,1,1,wfp);
                    tmp = mainWnd->m_info.SPINand_WriteStatusCmd;
                    fwrite((char *)&tmp,1,1,wfp);
                    tmp = mainWnd->m_info.SPINand_StatusValue;
                    fwrite((char *)&tmp,1,1,wfp);
                    tmp = mainWnd->m_info.SPINand_dummybyte;
                    fwrite((char *)&tmp,1,1,wfp);
                    tmp = 0xffffff;
                    fwrite((char *)&tmp,1,3,wfp);
                    tmp = 0xffffffff;
                    fwrite((char *)&tmp,1,4,wfp);
                } else //SPI NOR
#endif
                {
                    tmp = 0x800;
                    fwrite((char *)&tmp,1,2,wfp);
                    tmp = 0x40;
                    fwrite((char *)&tmp,1,2,wfp);
                    tmp = mainWnd->m_info.SPI_QuadReadCmd;
                    fwrite((char *)&tmp,1,1,wfp);
                    tmp = mainWnd->m_info.SPI_ReadStatusCmd;
                    fwrite((char *)&tmp,1,1,wfp);
                    tmp = mainWnd->m_info.SPI_WriteStatusCmd;
                    fwrite((char *)&tmp,1,1,wfp);
                    tmp = mainWnd->m_info.SPI_StatusValue;
                    fwrite((char *)&tmp,1,1,wfp);
                    tmp = mainWnd->m_info.SPI_dummybyte;
                    fwrite((char *)&tmp,1,1,wfp);
                    tmp = 0xffffff;
                    fwrite((char *)&tmp,1,3,wfp);
                    tmp = 0xffffffff;
                    fwrite((char *)&tmp,1,4,wfp);

                }

                //write DDR
                fwrite(ddrbuf,1,ddrlen,wfp);

                pBuffer=(char *)malloc(len);

                fread(pBuffer,1,len,rfp);
                fwrite((char *)pBuffer,1,len,wfp);
            }
            break;
            case ENV  :
                memset((char *)&child,0xff,sizeof(PACK_CHILD_HEAD));
                if(*itemFlash == TYPE_SPINAND) {//spi nand
                    child.filelen=0x20000;
                    child.imagetype=ENV;
                    pBuffer=(char *)malloc(0x20000);
                    memset(pBuffer,0x0,0x20000);
                } else {
                    child.filelen=0x10000;
                    child.imagetype=ENV;
                    pBuffer=(char *)malloc(0x10000);
                    memset(pBuffer,0x0,0x10000);
                }
                _stscanf_s(*itemStartblock,_T("%x"),&child.startaddr);
                //-----------------------------------------------
                fwrite((char *)&child,sizeof(PACK_CHILD_HEAD),1,wfp);
#if 0
                fread(pBuffer+4,1,len,rfp);
#else
                {
                    char line[256];
                    char* ptr=(char *)(pBuffer+4);
                    while (1) {
                        if (fgets(line,256, rfp) == NULL) break;
                        if(line[strlen(line)-2]==0x0D || line[strlen(line)-1]==0x0A) {
                            strncpy(ptr,line,strlen(line)-1);
                            ptr[strlen(line)-2]=0x0;
                            ptr+=(strlen(line)-1);
                        } else {
                            strncpy(ptr,line,strlen(line));
                            ptr+=(strlen(line));
                        }
                    }

                }
#endif
                if(*itemFlash == 3) {//spi nand
                    *(unsigned int *)pBuffer=mainWnd->CalculateCRC32((unsigned char *)(pBuffer+4),0x20000-4);
                    fwrite((char *)pBuffer,1,0x20000,wfp);
                } else {
                    *(unsigned int *)pBuffer=mainWnd->CalculateCRC32((unsigned char *)(pBuffer+4),0x10000-4);
                    fwrite((char *)pBuffer,1,0x10000,wfp);
                }
                break;
            case DATA : {
                memset((char *)&child,0xff,sizeof(PACK_CHILD_HEAD));

                child.filelen=len;
                child.imagetype=DATA;
                pBuffer=(char *)malloc(child.filelen);
                _stscanf_s(*itemStartblock,_T("%x"),&child.startaddr);
                //-----------------------------------------------
                fwrite((char *)&child,sizeof(PACK_CHILD_HEAD),1,wfp);
                fread(pBuffer,1,len,rfp);
                fwrite((char *)pBuffer,1,len,wfp);
            }
            break;
            case PARTITION:
                //if(!mainWnd->m_inifile.ReadFile())
                //    return FALSE;
                memset((char *)&child,0xff,sizeof(PACK_CHILD_HEAD));
                child.filelen= PACK_FOMRAT_HEADER_LEN; // partition header length
                child.imagetype=PARTITION;
                fwrite((char *)&child,1,sizeof(PACK_CHILD_HEAD),wfp);

                tmp=_wtoi(mainWnd->m_inifile.GetValue(_T("PACK"),_T("MMCFTFS")));
                fwrite((char *)&tmp,1,4,wfp);
                tmp=_wtoi(mainWnd->m_inifile.GetValue(_T("PACK"),_T("MMCFTPNUM")));
                TRACE(_T("MMCFTPNUM=%d "), tmp);
                fwrite((char *)&tmp,1,4,wfp);
                tmp=_wtoi(mainWnd->m_inifile.GetValue(_T("PACK"),_T("MMCFTPREV")));
                TRACE(_T("MMCFTPREV=%d "), tmp);
                fwrite((char *)&tmp,1,4,wfp);
                tmp = 0xffffffff;
                fwrite((char *)&tmp,1,4,wfp);
                tmp=_wtoi(mainWnd->m_inifile.GetValue(_T("PACK"),_T("MMCFTP1")));
                TRACE(_T("MMCFTP1=%dMB "), tmp);
                fwrite((char *)&tmp,1,4,wfp);
                tmp = tmp*2*1024;//PartitionS1Size
                fwrite((char *)&tmp,1,4,wfp);
                tmp=_wtoi(mainWnd->m_inifile.GetValue(_T("PACK"),_T("MMCFTP2")));
                TRACE(_T("MMCFTP2=%dMB "), tmp);
                fwrite((char *)&tmp,1,4,wfp);
                tmp = tmp*2*1024;//PartitionS2Size
                fwrite((char *)&tmp,1,4,wfp);
                tmp=_wtoi(mainWnd->m_inifile.GetValue(_T("PACK"),_T("MMCFTP3")));
                TRACE(_T("MMCFTP3=%dMB "), tmp);
                fwrite((char *)&tmp,1,4,wfp);
                tmp = tmp*2*1024;//PartitionS3Size
                fwrite((char *)&tmp,1,4,wfp);
                tmp=_wtoi(mainWnd->m_inifile.GetValue(_T("PACK"),_T("MMCFTP4")));
                TRACE(_T("MMCFTP4=%dMB\n"), tmp);
                fwrite((char *)&tmp,1,4,wfp);
                tmp = tmp*2*1024;//PartitionS4Size
                fwrite((char *)&tmp,1,4,wfp);
            break;

            case IMAGE:
                memset((char *)&child,0xff,sizeof(PACK_CHILD_HEAD));
                child.filelen=len;
                child.imagetype=IMAGE;
                pBuffer=(char *)malloc(child.filelen);
                _stscanf_s(*itemStartblock,_T("%x"),&child.startaddr);
                //-----------------------------------------------
                fwrite((char *)&child,sizeof(PACK_CHILD_HEAD),1,wfp);
                fread(pBuffer,1,len,rfp);
                fwrite((char *)pBuffer,1,len,wfp);
                break;
            }
            fclose(rfp);
            if(pBuffer!=NULL) free(pBuffer);
        }
    }
    fclose(wfp);
    AfxMessageBox(_T("Output finish "));

    return TRUE;
}


unsigned WINAPI CPACKDlg:: Output_proc(void* args)
{
    CPACKDlg* pThis = reinterpret_cast<CPACKDlg*>(args);
    pThis->Output();
    return 0;
}

void CPACKDlg::OnBnClickedPackOutput()
{
    InitFile(1);
    if(m_imagelist.GetItemCount()==0) {
        AfxMessageBox(_T("Error! Please add image item to output binary file\n"));
        return;
    }

    //CAddFileDialog dlg(TRUE,NULL,NULL,OFN_CREATEPROMPT | OFN_HIDEREADONLY ,_T("Bin,Img Files  (*.bin)|*.bin|All Files (*.*)|*.*||"));
    CAddFileDialog dlg(FALSE,NULL,NULL,OFN_CREATEPROMPT | OFN_HIDEREADONLY ,_T("Bin Files (*.bin)|*.bin|All Files (*.*)|*.*||"));
    //CAddFileDialog dlg(TRUE,NULL,NULL,OFN_FILEMUSTEXIST | OFN_HIDEREADONLY ,_T("Bin Files (*.bin)|*.bin|All Files (*.*)|*.*||"));

    dlg.m_ofn.lpstrTitle=_T("Choose burning file...");
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    if(!mainWnd->m_inifile.ReadFile())
        dlg.m_ofn.lpstrInitialDir=_T("c:");
    else {
        CString _path;
        _path=mainWnd->m_inifile.GetValue(_T("PACK"),_T("SAVEPATH"));
        if(_path.IsEmpty())
            dlg.m_ofn.lpstrInitialDir=_T("c:");
        else
            dlg.m_ofn.lpstrInitialDir=_path;
    }

    BOOLEAN ret=dlg.DoModal();

    if(ret==IDCANCEL) {
        return;
    }

    m_filename2=dlg.GetPathName();
    //AfxMessageBox(m_filename2);

    unsigned Thread1;
    HANDLE hThread;
    hThread=(HANDLE)_beginthreadex(NULL,0,Output_proc,(void*)this,0,&Thread1);

}

BOOL CPACKDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
    return CDialog::OnCommand(wParam, lParam);
}

BOOL CPACKDlg::FileExist(CString strFileName)
{
    CFileFind fFind;
    return fFind.FindFile(strFileName);
}

BOOL CPACKDlg::InitFile(int flag)
{
    CString tName,tType,tFlashType,tExec,tStartblock,tUserConfig;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    CPackTab1* packTabWnd=(CPackTab1*)(AfxGetApp()->m_pMainWnd);

    vector<CString>::iterator itemName;
    vector<int>::iterator itemType;
    vector<int>::iterator itemFlash;
    vector<CString>::iterator itemExec;
    vector<CString>::iterator itemStartblock;
    vector<int>::iterator itemUserConfig;


    if(!mainWnd->m_inifile.ReadFile()) return false;
    switch(flag) {
    case 0: {
        int imagelen=_wtoi( mainWnd->m_inifile.GetValue(_T("PACK"),_T("IMAGENUM")) );
        CString NameTemp;
        int counter=0;
        for(int i=0; i<imagelen; i++) {
            tName.Format(_T("NAME%02d"),i);
            NameTemp = mainWnd->m_inifile.GetValue(_T("PACK"),tName);

            if((FileExist(NameTemp)==NULL) && (NameTemp!=_T("Partition_INFO")))
            {
                AfxMessageBox(_T("Error! Please check file is exist"));
                return true;
                //continue;
            }

            ImageName.push_back( NameTemp );

            tType.Format(_T("TYPE%02d"),i);
            ImageType.push_back( _wtoi(mainWnd->m_inifile.GetValue(_T("PACK"),tType)) );

            tFlashType.Format(_T("BOOTTYPE%02d"),i);
            FlashType.push_back( _wtoi(mainWnd->m_inifile.GetValue(_T("PACK"),tFlashType)) );

            tExec.Format(_T("EXEC%02d"),i);
            ImageExec.push_back( mainWnd->m_inifile.GetValue(_T("PACK"),tExec) );

            tStartblock.Format(_T("STARTBLOCK%02d"),i);
            ImageStartblock.push_back( mainWnd->m_inifile.GetValue(_T("PACK"),tStartblock) );

            tUserConfig.Format(_T("USERCONFIG%02d"),i);
            UserConfig.push_back( _wtoi(mainWnd->m_inifile.GetValue(_T("PACK"),tUserConfig)) );

            itemName=(ImageName.begin()+i);
            itemType=(ImageType.begin()+i);
            itemFlash=(FlashType.begin()+i);
            itemExec=(ImageExec.begin()+i);
            itemStartblock=(ImageStartblock.begin()+i);
            itemUserConfig=(UserConfig.begin()+i);

            CString flagstr,flashstr,userstr;
            switch(*itemType) {
            case DATA   :
                flagstr.Format(_T("DATA"));
                break;
            case ENV    :
                flagstr.Format(_T("ENV"));
                break;
            case UBOOT  :
                flagstr.Format(_T("uBOOT"));
                break;
            case PARTITION  :
                flagstr.Format(_T("FORMAT"));
                break;
#if(0)
            case PACK   :
                flagstr.Format(_T("Pack"));
                break;
#endif
            case IMAGE  :
                flagstr.Format(_T("FS"));
                break;
            }

            switch(*itemFlash) {
            case TYPE_NAND:
                flashstr.Format(_T("NAND"));
                break;
            case TYPE_SPI:
                flashstr.Format(_T("SPI"));
                break;
            case TYPE_EMMC:
                flashstr.Format(_T("eMMC/SD"));
                break;
            case TYPE_SPINAND:
                flashstr.Format(_T("SPI NAND"));
                break;
            }

            switch(*itemUserConfig) {
            case 1: {
                userstr.Format(_T("Yes"));
                mainWnd->m_info.SPI_uIsUserConfig = 1;
                mainWnd->m_info.SPINand_uIsUserConfig = 1;
                break;
            }
            case 0:
            default: {
                userstr.Format(_T("No"));
                mainWnd->m_info.SPI_uIsUserConfig = 0;
                mainWnd->m_info.SPINand_uIsUserConfig = 0;
                break;
            }
            }

            CString filename,tmp;
            tmp=(*itemName).Mid((*itemName).ReverseFind('\\')+1, (*itemName).GetLength());
            if(tmp.ReverseFind('.')>0)
                filename=tmp.Mid(0,tmp.ReverseFind('.'));
            else
                filename=tmp;
            //if(filename.GetLength()>16)
            //    filename = filename.Mid(0,15);

            CString len;
            if(*itemType!=PARTITION) {
                FILE* rfp;
                int total;
                int startblock;
                rfp=_wfopen(*itemName,_T("rb"));
                fseek(rfp,0,SEEK_END);
                _stscanf_s(*itemStartblock,_T("%x"),&startblock);
                total=ftell(rfp)+startblock;

                if(*itemType==UBOOT) {
                    int ddrlen;
                    UCHAR * ddrbuf;
                    ddrbuf=DDR2Buf(mainWnd->ShareDDRBuf,mainWnd->DDRLen,&ddrlen);
                    total+=(ddrlen+32);
                }

                len.Format(_T("%x"),total);
                fclose(rfp);
            }

            switch(*itemType) {
            case DATA:
                m_imagelist.InsertItem(m_imagelist.GetItemCount(),filename,flagstr,_T(""),*itemStartblock,len,_T(""),_T(""));
                break;
            case ENV:
                m_imagelist.InsertItem(m_imagelist.GetItemCount(),filename,flagstr,flashstr,*itemStartblock,len,_T(""),_T(""));
                break;
            case PARTITION:
                m_imagelist.InsertItem(m_imagelist.GetItemCount(),_T("Partition_INFO"),flagstr,_T(""),_T(""),_T(""),_T(""),_T(""));
                break;
            case UBOOT:
                m_imagelist.InsertItem(m_imagelist.GetItemCount(),filename,flagstr,flashstr,*itemStartblock,len,*itemExec,userstr);
                break;
            }
        }
    }
    break;

    case 1: {
        int imagelen=m_imagelist.GetItemCount();
        CString tmp;
        tmp.Format(_T("%d"),imagelen);
        mainWnd->m_inifile.SetValue(_T("PACK"),_T("IMAGENUM"),tmp);

        for(int i=0; i<imagelen; i++) {

            itemName=(ImageName.begin()+i);
            itemType=(ImageType.begin()+i);
            itemFlash=(FlashType.begin()+i);
            itemExec=(ImageExec.begin()+i);
            itemStartblock=(ImageStartblock.begin()+i);
            itemUserConfig=(UserConfig.begin()+i);

            tName.Format(_T("NAME%02d"),i);
            mainWnd->m_inifile.SetValue(_T("PACK"),tName,*itemName);

            tType.Format(_T("TYPE%02d"),i);
            tmp.Format(_T("%d"),*itemType);
            mainWnd->m_inifile.SetValue(_T("PACK"),tType,tmp);

            tFlashType.Format(_T("BOOTTYPE%02d"),i);
            tmp.Format(_T("%d"),*itemFlash);
            mainWnd->m_inifile.SetValue(_T("PACK"),tFlashType,tmp);

            tExec.Format(_T("EXEC%02d"),i);
            mainWnd->m_inifile.SetValue(_T("PACK"),tExec,*itemExec);

            tStartblock.Format(_T("STARTBLOCK%02d"),i);
            mainWnd->m_inifile.SetValue(_T("PACK"),tStartblock,*itemStartblock);

            tUserConfig.Format(_T("USERCONFIG%02d"),i);
            tmp.Format(_T("%d"),*itemUserConfig);
            mainWnd->m_inifile.SetValue(_T("PACK"),tUserConfig,tmp);
        }
        mainWnd->m_inifile.WriteFile();
    }
    break;
    }
    return true;
}
void CPACKDlg::OnNMDblclkPackImagelist(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<NMITEMACTIVATE *>(pNMHDR);
    // TODO: Add your control notification handler code here
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    *pResult = 0;

    vector<CString>::iterator itemName;
    vector<int>::iterator itemType;
    vector<int>::iterator itemFlash;
    vector<CString>::iterator itemExec;
    vector<CString>::iterator itemStartblock;
    vector<int>::iterator itemUserConfig;

    int i;
    int modify_idx;
    int imagelen=m_imagelist.GetItemCount();

    for(i=0; i<imagelen; i++) {
        if(m_imagelist.IsItemSelected(i)==TRUE)
            break;
    }
    if(i==imagelen || imagelen==0) {
        AfxMessageBox(_T("Error! Please select image item to modify first"));
        return;
    }
    modify_idx=i;

    CString tmp;
    //-----------------------------------------------------------------------------
    itemName=ImageName.begin()+modify_idx;
    m_packtab1.m_filename=(*itemName);
    tmp=m_packtab1.m_filename.Mid(m_packtab1.m_filename.ReverseFind('\\')+1, m_packtab1.m_filename.GetLength());
    if(tmp.ReverseFind('.')>0)
        m_packtab1.m_imagename=tmp.Mid(0,tmp.ReverseFind('.'));
    else
        m_packtab1.m_imagename=tmp;
    //if(m_packtab1.m_imagename.GetLength()>16)
    //    m_packtab1.m_imagename = m_packtab1.m_imagename.Mid(0,15);

    m_pack_tabcontrol.SetCurSel(PACK_PAR);
    //SwapTab();
    m_packtab1.GetDlgItem(IDC_PACK_IMAGENAME_A)->SetWindowText(m_packtab1.m_imagename);
    //-----------------------------------------------------------------------------
    itemType=ImageType.begin()+modify_idx;
    ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_TYPE_A))->SetCheck(FALSE);
    ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_TYPE_A2))->SetCheck(FALSE);
    ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_TYPE_A4))->SetCheck(FALSE);
    ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_TYPE_A3))->SetCheck(FALSE);
    switch((*itemType)) {
    case DATA:
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_TYPE_A ))->SetCheck(TRUE);
        break;
    case ENV:
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_TYPE_A2))->SetCheck(TRUE);
        break;
    case UBOOT:
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_TYPE_A4))->SetCheck(TRUE);
        break;
    case PARTITION:
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_TYPE_A ))->SetCheck(TRUE);
        //AfxMessageBox(_T("Error! Please delete this item and re-format"));
        break;
    }
    //-----------------------------------------------------------------------------
    itemFlash=FlashType.begin()+modify_idx;
    ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_FLASHTYPE_1))->SetCheck(FALSE);
    ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_FLASHTYPE_2))->SetCheck(FALSE);
    ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_FLASHTYPE_3))->SetCheck(FALSE);
    ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_FLASHTYPE_4))->SetCheck(FALSE);
    switch((*itemFlash)) {
    case TYPE_NAND:
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_FLASHTYPE_1 ))->SetCheck(TRUE);
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_USRCONFIG ))->EnableWindow(FALSE);
        break;
    case TYPE_SPI:
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_FLASHTYPE_2))->SetCheck(TRUE);
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_USRCONFIG ))->EnableWindow(TRUE);
        break;
    case TYPE_EMMC:
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_FLASHTYPE_3))->SetCheck(TRUE);
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_USRCONFIG ))->EnableWindow(FALSE);
        break;
    case TYPE_SPINAND:
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_FLASHTYPE_4))->SetCheck(TRUE);
        ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_USRCONFIG ))->EnableWindow(TRUE);
        break;
    }
    //-----------------------------------------------------------------------------
    if((*itemType) == PARTITION) {
        m_packtab1.GetDlgItem(IDC_PACK_EXECADDR_A)->SetWindowText(_T("0"));
        m_packtab1.GetDlgItem(IDC_PACK_FLASHOFFSET_A)->SetWindowText(_T("0"));
    }
    else {
        itemExec=ImageExec.begin()+modify_idx;
        m_packtab1.GetDlgItem(IDC_PACK_EXECADDR_A)->SetWindowText(*itemExec);
    //-----------------------------------------------------------------------------
        itemStartblock=ImageStartblock.begin()+modify_idx;
        m_packtab1.GetDlgItem(IDC_PACK_FLASHOFFSET_A)->SetWindowText(*itemStartblock);
    //-----------------------------------------------------------------------------
    }

    itemUserConfig=UserConfig.begin()+modify_idx;
    switch((*itemUserConfig)) {
        case 1: {
            ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_USRCONFIG ))->SetCheck(TRUE);
            mainWnd->m_info.SPI_uIsUserConfig = 1;
            mainWnd->m_info.SPINand_uIsUserConfig = 1;
            break;
        }
        case 0:
        default: {
            ((CButton *)m_packtab1.GetDlgItem(IDC_PACK_USRCONFIG ))->SetCheck(FALSE);
            mainWnd->m_info.SPI_uIsUserConfig = 0;
            mainWnd->m_info.SPINand_uIsUserConfig = 0;
            break;
        }
    }
}

void CPACKDlg::OnTcnSelchangePackTabcontrol(NMHDR *pNMHDR, LRESULT *pResult)
{
    // TODO: Add your control notification handler code here
    *pResult = 0;
    //SwapTab();
}


