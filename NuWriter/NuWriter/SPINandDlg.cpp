// SPINandDlg.cpp
//

#include "stdafx.h"
#include "NuWriter.h"
#include "NuWriterDlg.h"
#include "SPINandDlg.h"


// CSPINandDlg

IMPLEMENT_DYNAMIC(CSPINandDlg, CDialog)

CSPINandDlg::CSPINandDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CSPINandDlg::IDD, pParent)
    , m_imagename(_T(""))
    , m_startblock(_T(""))
    , m_execaddr(_T(""))
    , m_type(0)
{
    InitFlag=0;
    TmpOffsetFlag=0;
    m_fhead=(NORBOOT_NAND_HEAD *)malloc(sizeof(NORBOOT_NAND_HEAD));
}

CSPINandDlg::~CSPINandDlg()
{
    free(m_fhead);
}

void CSPINandDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_SPINAND_IMAGENAME_A, m_imagename);
    DDX_Control(pDX, IDC_SPINAND_IMAGELIST, m_imagelist);
    DDX_Control(pDX, IDC_SPINAND_PROGRESS, m_progress);
    DDX_Text(pDX, IDC_SPINAND_FLASHOFFSET_A, m_startblock);
    DDV_MaxChars(pDX, m_startblock, 8);
    DDX_Text(pDX, IDC_SPINAND_EXECADDR_A, m_execaddr);
    DDV_MaxChars(pDX, m_execaddr, 8);
    DDX_Control(pDX, IDC_SPINAND_DOWNLOAD, m_burn);
    DDX_Control(pDX, IDC_SPINAND_VERIFY, m_verify);
    DDX_Control(pDX, IDC_SPINAND_READ, m_read);
    DDX_Control(pDX, IDC_SPINAND_ERASEALL, m_eraseall);
    DDX_Control(pDX, IDC_SPINAND_BROWSE, m_browse);
    DDX_Control(pDX, IDC_SPINAND_INFO, m_info);
    DDX_Radio(pDX, IDC_SPINAND_TYPE_A, m_type);
    DDX_Control(pDX, IDC_SPINAND_STATUS, m_status);
    DDX_Control(pDX, IDC_SPINAND_USRCONFIG, m_spinandflash_check);
}


BEGIN_MESSAGE_MAP(CSPINandDlg, CDialog)
    ON_BN_CLICKED(IDC_SPINAND_INFO, &CSPINandDlg::OnBnClickedSpinandInfo)
    ON_BN_CLICKED(IDC_SPINAND_DOWNLOAD, &CSPINandDlg::OnBnClickedSpinandDownload)
    ON_WM_SHOWWINDOW()
    ON_MESSAGE(WM_SPINAND_PROGRESS,ShowStatus)
    ON_BN_CLICKED(IDC_SPINAND_VERIFY, &CSPINandDlg::OnBnClickedSpinandVerify)
    ON_BN_CLICKED(IDC_SPINAND_BROWSE, &CSPINandDlg::OnBnClickedSpinandBrowse)
    ON_BN_CLICKED(IDC_SPINAND_ERASEALL, &CSPINandDlg::OnBnClickedSpinandEraseall)
    ON_BN_CLICKED(IDC_SPINAND_READ, &CSPINandDlg::OnBnClickedSpinandRead)
    ON_BN_CLICKED(IDC_SPINAND_USRCONFIG, &CSPINandDlg::OnBnClickedSpinandUsrconfig)
END_MESSAGE_MAP()


// CSPINandDlg
LRESULT CSPINandDlg::ShowStatus( WPARAM  pos, LPARAM message)
{
    m_progress.SetPos((int)pos);
    return true;
}

void CSPINandDlg::OnBnClickedSpinandInfo()
{
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    CSPINandInfoDlg spinandinfo_dlg;
    spinandinfo_dlg.DoModal();
    mainWnd->GetDlgItem(IDC_RECONNECT)->EnableWindow(FALSE);
    NucUsb.UsbDevice_Detect();// Re-detert WinUSB number
    mainWnd->OneDeviceInfo(0);// Update nand parameters
}


BOOL CSPINandDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
    //TmpOffset
    UpdateData(TRUE);
    if(m_type==UBOOT) {
        if(TmpOffsetFlag==0) {
            TmpOffsetFlag=1;
            GetDlgItem(IDC_SPINAND_FLASHOFFSET_A)->GetWindowText(TmpOffset);
            GetDlgItem(IDC_SPINAND_FLASHOFFSET_A)->SetWindowText(_T("0"));
            GetDlgItem(IDC_SPINAND_FLASHOFFSET_A)->EnableWindow(FALSE);
            GetDlgItem(IDC_SPINAND_EXECADDR_A)->EnableWindow(TRUE);
        }

    } else {

        if(TmpOffsetFlag==1) {
            TmpOffsetFlag=0;
            GetDlgItem(IDC_SPINAND_FLASHOFFSET_A)->SetWindowText(TmpOffset);
            GetDlgItem(IDC_SPINAND_FLASHOFFSET_A)->EnableWindow(TRUE);
        }


        if(m_type==PACK) {
            GetDlgItem(IDC_SPINAND_EXECADDR_A)->EnableWindow(FALSE);
            GetDlgItem(IDC_SPINAND_FLASHOFFSET_A)->EnableWindow(FALSE);
        } else {
            GetDlgItem(IDC_SPINAND_EXECADDR_A)->EnableWindow(FALSE);
            GetDlgItem(IDC_SPINAND_FLASHOFFSET_A)->EnableWindow(TRUE);
        }
    }
    return CDialog::OnCommand(wParam, lParam);
}


BOOL CSPINandDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    m_imagelist.SetHeadings(_T("Name, 70; Type, 50;Start, 60;End, 60;Block, 60"));
    m_imagelist.SetGridLines(TRUE);

    m_ExitEvent=CreateEvent(NULL,TRUE,FALSE,NULL);

    m_progress.SetRange(0,100);
    m_progress.SetBkColor(COLOR_DOWNLOAD);
    m_filename="";
    GetDlgItem(IDC_SPINAND_VERIFY)->EnableWindow(FALSE);

    COLORREF col = RGB(0xFF, 0x00, 0xFF);
    m_eraseall.setBitmapId(IDB_ERASEALL, col);
    m_eraseall.setGradient(true);
    m_read.setBitmapId(IDB_READ_DEVICE, col);
    m_read.setGradient(true);
    m_verify.setBitmapId(IDB_VERIFY, col);
    m_verify.setGradient(true);
    m_burn.setBitmapId(IDB_WRITE_DEVICE, col);
    m_burn.setGradient(true);
    m_browse.setBitmapId(IDB_BROWSE, col);
    m_browse.setGradient(true);
    m_info.setBitmapId(IDB_READ_DEVICE, col);
    m_info.setGradient(true);

    //memset(m_spinandinfo, 0, sizeof(SPINAND_INFO_T));
    //m_spinandinfo->PageSize = 2048;
    //m_spinandinfo->SpareArea = 64;


    UpdateData(FALSE);
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX return FALSE
}

BOOL CSPINandDlg::InitFile(int flag)
{

    CString tmp;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    if(!mainWnd->m_inifile.ReadFile()) return false;
    switch(flag) {
    case 0:
        tmp=mainWnd->m_inifile.GetValue(_T("SPINAND"),_T("PATH"));
        m_filename=tmp;

        tmp=m_filename.Mid(m_filename.ReverseFind('\\')+1, m_filename.GetLength());
        if(tmp.ReverseFind('.')>0)
            m_imagename=tmp.Mid(0,tmp.ReverseFind('.'));
        else
            m_imagename=tmp;
        //if(m_imagename.GetLength()>16)
        //    m_imagename = m_imagename.Mid(0,15);
        GetDlgItem(IDC_SPINAND_IMAGENAME_A)->SetWindowText(m_imagename);

        tmp=mainWnd->m_inifile.GetValue(_T("SPINAND"),_T("TYPE"));
        ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A))->SetCheck(FALSE);
        switch(_wtoi(tmp)) {
        case 0:
            ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A))->SetCheck(TRUE);
            break;
        case 1:
            ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A2))->SetCheck(TRUE);
            break;
        case 2:
            ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A4))->SetCheck(TRUE);
            break;
        case 3:
            ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A3))->SetCheck(TRUE);
            break;

        }

        tmp=mainWnd->m_inifile.GetValue(_T("SPINAND"),_T("EXECADDR"));
        GetDlgItem(IDC_SPINAND_EXECADDR_A)->SetWindowText(tmp);
        m_execaddr=tmp;
        tmp=mainWnd->m_inifile.GetValue(_T("SPINAND"),_T("OFFSET"));
        GetDlgItem(IDC_SPINAND_FLASHOFFSET_A)->SetWindowText(tmp);
        //m_startblock=tmp;
        break;
    case 1:
        mainWnd->m_inifile.SetValue(_T("SPINAND"),_T("PATH"),m_filename);
        tmp.Format(_T("%d"),m_type);
        mainWnd->m_inifile.SetValue(_T("SPINAND"),_T("TYPE"),tmp);
        mainWnd->m_inifile.SetValue(_T("SPINAND"),_T("EXECADDR"),m_execaddr);
        mainWnd->m_inifile.SetValue(_T("SPINAND"),_T("OFFSET"),m_startblock);

        mainWnd->m_inifile.WriteFile();
        break;
    default:
        break;
    }

    if(mainWnd->ChipWriteWithOOB==1) {
        ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A))->SetCheck(FALSE);
        ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A2))->SetCheck(FALSE);
        ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A4))->SetCheck(FALSE);
        ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A3))->SetCheck(FALSE);
        ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A))->SetCheck(TRUE);

        ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A))->EnableWindow(FALSE);
        ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A2))->EnableWindow(FALSE);
        ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A4))->EnableWindow(FALSE);
        ((CButton *)GetDlgItem(IDC_SPINAND_TYPE_A3))->EnableWindow(FALSE);
    }
    return true;
}

void CSPINandDlg::Download()
{
    int i=0;
    BOOLEAN ret;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    if(!m_filename.IsEmpty()) {
        int len,startblock,endblock;
        mainWnd->m_gtype.EnableWindow(FALSE);
        GetDlgItem(IDC_SPINAND_VERIFY)->EnableWindow(FALSE);
        ResetEvent(m_ExitEvent);
        GetDlgItem(IDC_SPINAND_DOWNLOAD)->SetWindowText(_T("Abort"));

        if(m_type!=PACK) {
            int blockNum;
            //if(mainWnd->ChipWriteWithOOB==0)
                ret=XUSB_Burn(mainWnd->m_portName,m_filename,&len,&blockNum);
            //else
            //    ret=XUSB_BurnWithOOB(mainWnd->m_portName,m_filename,&len,&blockNum);

            if(ret) {
                GetDlgItem(IDC_SPINAND_VERIFY)->EnableWindow(TRUE);
                AfxMessageBox(_T("Burn successfully"));
                CString flagstr;
                switch(m_type) {
                case DATA :
                    flagstr.Format(_T("DATA"));
                    break;
                case ENV  :
                    flagstr.Format(_T("ENV"));
                    break;
                case UBOOT:
                    flagstr.Format(_T("uBOOT"));
                    break;
                case PACK :
                    flagstr.Format(_T("Pack"));
                    break;
                case IMAGE:
                    flagstr.Format(_T("FS"));
                    break;
                }
                _stscanf_s(m_startblock,_T("%x"),&startblock);
                //endblock=startblock+((len+(NAND_SIZE-1))/(NAND_SIZE))*(NAND_SIZE);
                endblock=startblock+len;

                CString _startblock,_endblock,_blockNum;
                _startblock.Format(_T("0x%x"),startblock);
                _endblock.Format(_T("0x%x"),endblock);
                _blockNum.Format(_T("0x%x"),blockNum);
                m_imagelist.InsertItem(0,m_imagename,flagstr,_startblock,_endblock,_blockNum);
            }
            else
                m_progress.SetPos(0);
            //  AfxMessageBox("Burn unsuccessfully!! Please check device");
        } else {
            ret=XUSB_Pack(mainWnd->m_portName,m_filename,&len);
            if(ret) {
                GetDlgItem(IDC_SPINAND_VERIFY)->EnableWindow(TRUE);
                AfxMessageBox(_T("Burn successfully"));
            }
            else
                m_progress.SetPos(0);
        }

        GetDlgItem(IDC_SPINAND_DOWNLOAD)->EnableWindow(TRUE);

        GetDlgItem(IDC_SPINAND_DOWNLOAD)->SetWindowText(_T("Program"));
        //UpdateData(FALSE);
        mainWnd->m_gtype.EnableWindow(TRUE);

    } else
        AfxMessageBox(_T("Please choose image file !"));
    return ;

}

unsigned WINAPI CSPINandDlg:: Download_proc(void* args)
{
    CSPINandDlg* pThis = reinterpret_cast<CSPINandDlg*>(args);
    pThis->Download();
    return 0;
}

void CSPINandDlg::OnBnClickedSpinandDownload()
{
    InitFile(1);

    CString dlgText;
    int _startblock=0;

    //UpdateData(TRUE);
    if(m_imagename.IsEmpty()) {
        AfxMessageBox(_T("Please input image file"));
        return;
    }

    _stscanf_s(m_startblock,_T("%x"),&_startblock);
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    unsigned int val=mainWnd->m_info.SPINand_PagePerBlock*mainWnd->m_info.SPINand_PageSize;//64*2048;

#if 1 // cfli why???
    if(mainWnd->ChipWriteWithOOB!=1) {
        if(val>0 && val<=0x800000) {
            if((m_type!=UBOOT)&&(_startblock%(val)!=0) && (m_type!=PACK)) {
                CString tmp;
                tmp.Format(_T("Start offset must be %dK (0x%X) boundary! Do you confirm this operation ?"),val/1024,val);
#if 0
                AfxMessageBox(tmp);
                return;
#else
                if(::MessageBox(this->m_hWnd,tmp,_T("Nu Writer"),MB_OKCANCEL|MB_ICONWARNING)==IDCANCEL)
                    return;
#endif

            }
            if( (m_type!=UBOOT)&&(((unsigned int)_startblock<(val*4)) ) && (m_type!=PACK)) {
                CString tmp;
                tmp.Format(_T("Between 0x0 ~ 0x%X use by uBOOT. Do you confirm this operation ?"),4*val-1);
#if 0
                AfxMessageBox(tmp);
                return;
#else
                if(::MessageBox(this->m_hWnd,tmp,_T("Nu Writer"),MB_OKCANCEL|MB_ICONWARNING)==IDCANCEL)
                    return;
#endif
            }
        }
    } else {

    }
#endif

    GetDlgItem(IDC_SPINAND_DOWNLOAD)->GetWindowText(dlgText);


    if(dlgText.CompareNoCase(_T("Program"))==0) {

        UpdateData(TRUE);

        if(::MessageBox(this->m_hWnd,_T("Do you confirm this operation ?"),_T("Nu Writer"),MB_OKCANCEL|MB_ICONWARNING)==IDCANCEL)
            return;

        unsigned Thread1;
        HANDLE hThread;

        m_progress.SetPos(0);
        m_progress.SetRange(0,100);
        m_progress.SetBkColor(COLOR_DOWNLOAD);

        hThread=(HANDLE)_beginthreadex(NULL,0,Download_proc,(void*)this,0,&Thread1);
        CloseHandle(hThread);
    } else {
        //m_vcom.Close();
        SetEvent(m_ExitEvent);
        GetDlgItem(IDC_SPINAND_DOWNLOAD)->EnableWindow(FALSE);
        m_progress.SetPos(0);
    }
}


void CSPINandDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CDialog::OnShowWindow(bShow, nStatus);

    m_imagelist.DeleteAllItems();
    if(InitFlag==0) {
        InitFlag=1;
        InitFile(0);

    }
}

void CSPINandDlg:: Verify()
{
    int i=0;
    int ret;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    //UpdateData(FALSE);

    if(!m_filename.IsEmpty()) {
        mainWnd->m_gtype.EnableWindow(FALSE);
        ResetEvent(m_ExitEvent);
        GetDlgItem(IDC_SPINAND_VERIFY)->SetWindowText(_T("Abort"));
        ret=XUSB_Verify(mainWnd->m_portName,m_filename);
        if(ret==1)
            AfxMessageBox(_T("Verify OK !"));
        else {
            if(ret==NAND_VERIFY_FILESYSTEM_ERROR)
                AfxMessageBox(_T("This File System can't verify"));
            else if(ret==NAND_VERIFY_PACK_ERROR)
                AfxMessageBox(_T("Pack image can't verify"));
            else
                AfxMessageBox(_T("Verify Error !"));
            m_progress.SetPos(0);
        }
        GetDlgItem(IDC_SPINAND_VERIFY)->EnableWindow(TRUE);
        GetDlgItem(IDC_SPINAND_VERIFY)->SetWindowText(_T("Verify"));
        //UpdateData(FALSE);
        mainWnd->m_gtype.EnableWindow(TRUE);


    } else
        AfxMessageBox(_T("Please choose comparing file !"));

    return ;

}

unsigned WINAPI CSPINandDlg:: Verify_proc(void* args)
{
    CSPINandDlg* pThis = reinterpret_cast<CSPINandDlg*>(args);
    pThis->Verify();
    return 0;
}

void CSPINandDlg::OnBnClickedSpinandVerify()
{
    CString dlgText;

    UpdateData(TRUE);

    GetDlgItem(IDC_SPINAND_VERIFY)->GetWindowText(dlgText);

    if(dlgText.CompareNoCase(_T("Verify"))==0) {

        UpdateData(TRUE);

        unsigned Thread1;
        HANDLE hThread;

        hThread=(HANDLE)_beginthreadex(NULL,0,Verify_proc,(void*)this,0,&Thread1);
        CloseHandle(hThread);
    } else {
        //m_vcom.Close();
        SetEvent(m_ExitEvent);
        GetDlgItem(IDC_SPINAND_VERIFY)->EnableWindow(FALSE);
        m_progress.SetPos(0);
    }
}


void CSPINandDlg::OnBnClickedSpinandBrowse()
{
    UpdateData(TRUE);
    UpdateData(FALSE);

    CString temp;
    CAddFileDialog dlg(TRUE,NULL,NULL,OFN_FILEMUSTEXIST | OFN_HIDEREADONLY ,_T("Bin Files (*.bin)|*.bin|All Files (*.*)|*.*||"));

    dlg.m_ofn.lpstrTitle=_T("Choose burning file...");
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);

    if(!mainWnd->m_inifile.ReadFile())
        dlg.m_ofn.lpstrInitialDir=_T("c:");
    else {
        CString _path;
        _path=mainWnd->m_inifile.GetValue(_T("SPINAND"),_T("PATH"));
        if(_path.IsEmpty())
            dlg.m_ofn.lpstrInitialDir=_T("c:");
        else
            dlg.m_ofn.lpstrInitialDir=_path;
    }


    BOOLEAN ret=dlg.DoModal();

    if(ret==IDCANCEL) {
        return;
    }

    m_filename=dlg.GetPathName();
    temp=dlg.GetFileName();
    if(temp.ReverseFind('.')>0)
        m_imagename=temp.Mid(0,temp.ReverseFind('.'));
    else
        m_imagename=temp;

    //if(m_imagename.GetLength()>16)
    //    m_imagename = m_imagename.Mid(0,15);
    this->GetDlgItem(IDC_SPINAND_IMAGENAME_A)->SetWindowText(m_imagename);


    CString filepath=m_filename.Left(m_filename.GetLength()-dlg.GetFileName().GetLength()-1);

    mainWnd->m_inifile.SetValue(_T("SPINAND"),_T("PATH"),filepath);
    mainWnd->m_inifile.WriteFile();

}

void CSPINandDlg:: Erase()
{
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    BOOLEAN ret;
    //UpdateData(FALSE);

    mainWnd->m_gtype.EnableWindow(FALSE);
    ResetEvent(m_ExitEvent);
    ret=XUSB_Erase(mainWnd->m_portName);
    if(ret)
        AfxMessageBox(_T("Erase successfully"));
    else
    {
        //AfxMessageBox("Erase unsuccessfully!! Please check device");
        SetEvent(m_ExitEvent);
        m_progress.SetPos(0);
    }
    GetDlgItem(IDC_SPINAND_ERASEALL)->EnableWindow(TRUE);
    //UpdateData(FALSE);
    mainWnd->m_gtype.EnableWindow(TRUE);

    return ;
}

unsigned WINAPI CSPINandDlg::Erase_proc(void* args)
{
    CSPINandDlg* pThis = reinterpret_cast<CSPINandDlg*>(args);
    pThis->Erase();
    return 0;
}

void CSPINandDlg::OnBnClickedSpinandEraseall()
{
    CString dlgText;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
#if 1
    CEraseDlg erase_dlg;
    //read_dlg.SizeName.Format(_T("Blocks(1 block is 0x%05x bytes)"),NAND_SIZE);
    erase_dlg.SizeName.Format(_T("blocks"));
    erase_dlg.type=3; /* 0 => SPI, 1 =>NAND, 2=>MMC, 3 =>SPI NAND */
    erase_dlg.EraseInfo.Format(_T("1 block is %s bytes"),mainWnd->SPINand_size);
    if(erase_dlg.DoModal()==IDCANCEL) return;

    m_blocks=erase_dlg.block;
    m_sblocks=erase_dlg.sblock;
    m_erase_flag=erase_dlg.m_erase_type;
#else
    if(::MessageBox(this->m_hWnd,_T("Do you confirm this operation ?"),_T("Nu Writer"),MB_OKCANCEL|MB_ICONWARNING)==IDCANCEL)
        return;
#endif
    UpdateData(TRUE);

    GetDlgItem(IDC_SPINAND_ERASEALL)->GetWindowText(dlgText);

    if(dlgText.CompareNoCase(_T("Erase"))==0) {

        UpdateData(TRUE);

        unsigned Thread1;
        HANDLE hThread;

        m_progress.SetPos(0);
        m_progress.SetBkColor(COLOR_ERASE);
        hThread=(HANDLE)_beginthreadex(NULL,0,Erase_proc,(void*)this,0,&Thread1);
        CloseHandle(hThread);
    } else {
        //m_vcom.Close();
        SetEvent(m_ExitEvent);
        GetDlgItem(IDC_SPINAND_ERASEALL)->EnableWindow(FALSE);
        m_progress.SetPos(0);
    }
}

void CSPINandDlg:: Read()
{
    int i=0;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    int blocksize = mainWnd->m_info.SPINand_PagePerBlock*mainWnd->m_info.SPINand_PageSize;
    m_progress.SetRange(0,100);
    if(blocksize==0) {
        AfxMessageBox(_T("Can't get SPI NAND flash size, Please reconnet to device\n"));
        return;
    }
    //UpdateData(FALSE);

    if(!m_filename2.IsEmpty()) {
        mainWnd->m_gtype.EnableWindow(FALSE);
        ResetEvent(m_ExitEvent);
        GetDlgItem(IDC_SPINAND_READ)->EnableWindow(FALSE);

        int blocks,sblocks;
        _stscanf_s(m_blocks,_T("%d"),&blocks);
        _stscanf_s(m_sblocks,_T("%d"),&sblocks);

        if(mainWnd->ChipReadWithBad==0) { //read good block
            if(XUSB_Read(mainWnd->m_portName,m_filename2,sblocks,blocks*blocksize))
                AfxMessageBox(_T("Read OK !"));
            else
            {
                m_progress.SetPos(0);
                AfxMessageBox(_T("Read Error !"));
            }
        } else { //read redunancy data, good block and bad block
#if(0) //cfli to do
            if(XUSB_Read_Redunancy(mainWnd->m_portName,m_filename2,sblocks,blocks))
                AfxMessageBox(_T("Read OK !"));
            else
                AfxMessageBox(_T("Read Error !"));
#endif
        }
        GetDlgItem(IDC_SPINAND_READ)->EnableWindow(TRUE);

        //UpdateData(FALSE);
        mainWnd->m_gtype.EnableWindow(TRUE);

    } else
        AfxMessageBox(_T("Please choose read file !"));

    return ;
}

unsigned WINAPI CSPINandDlg::Read_proc(void* args)
{
    CSPINandDlg* pThis = reinterpret_cast<CSPINandDlg*>(args);
    pThis->Read();
    return 0;
}

void CSPINandDlg::OnBnClickedSpinandRead()
{
    CReadDlg read_dlg;
    //read_dlg.SizeName.Format(_T("Blocks(1 block is 0x%05x bytes)"),NAND_SIZE);
    read_dlg.SizeName.Format(_T("Blocks"));
    read_dlg.type=3; /* 0 => SPI, 1 =>NAND, 2=>MMC, 3 =>SPI NAND, */
    if(read_dlg.DoModal()==IDCANCEL) return;
    m_filename2=read_dlg.m_filename2;
    m_blocks=read_dlg.block;
    m_sblocks=read_dlg.sblock;

    CString dlgText;

    UpdateData(TRUE);

    GetDlgItem(IDC_SPINAND_READ)->GetWindowText(dlgText);
    if(dlgText.CompareNoCase(_T("Read"))==0) {
        UpdateData(TRUE);
        unsigned Thread1;
        HANDLE hThread;
        m_progress.SetPos(0);
        m_progress.SetRange(0,100);
        m_progress.SetBkColor(COLOR_VERIFY);
        hThread=(HANDLE)_beginthreadex(NULL,0,Read_proc,(void*)this,0,&Thread1);
        CloseHandle(hThread);
    } else {
        //SetEvent(m_ExitEvent);
        //GetDlgItem(IDC_SPINAND_READ)->EnableWindow(FALSE);
        m_progress.SetPos(0);
    }
}


void CSPINandDlg::OnBnClickedSpinandUsrconfig()
{
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    CSPINandInfoDlg spinandinfo_dlg;

    if(m_spinandflash_check.GetCheck()==TRUE)
    {
        spinandinfo_dlg.DoModal();
    }

    mainWnd->GetDlgItem(IDC_RECONNECT)->EnableWindow(FALSE);
    NucUsb.UsbDevice_Detect();// Re-detert WinUSB number
    mainWnd->OneDeviceInfo(0);// Update nand parameters
}
