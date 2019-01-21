// NuWriterDlg.cpp : implementation file
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include "NuWriter.h"
#include "NuWriterDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


#define LOAD 1
#define SAVE 0

#define AUTO_DOWNLOAD

#define RETRYCNT 5

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
    CAboutDlg();

// Dialog Data
    enum { IDD = IDD_ABOUTBOX };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CNuWriterDlg dialog
DEFINE_GUID(GUID_CLASS_VCOMPORT, 0x9b2095ce, 0x99a1, 0x44d9, 0xa7, 0xc6,
            0xc7, 0xcb, 0x58, 0x41, 0xfe, 0x88);

//extern BOOL Auto_Detect(CString& portName,CString& tempName);
extern BOOL Device_Detect(CString& portName,CString& tempName);

CNuWriterDlg::CNuWriterDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CNuWriterDlg::IDD, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CNuWriterDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO_TYPE, m_gtype);
    DDX_Control(pDX, IDC_COMPORT, m_burntext);
    DDX_Control(pDX, IDC_TYPE, m_type);
    DDX_Control(pDX, IDC_STATIC_DDRFile, m_static_ddrfile);
    DDX_Control(pDX, IDC_RECONNECT, m_reconnect);
    DDX_Control(pDX, IDCANCEL, m_exit);
    DDX_Control(pDX, IDC_VERSION, m_version);
}

BEGIN_MESSAGE_MAP(CNuWriterDlg, CDialog)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    //}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_RECONNECT, &CNuWriterDlg::OnBnClickedReconnect)
    ON_WM_DESTROY()
    ON_CBN_SELCHANGE(IDC_COMBO_TYPE, &CNuWriterDlg::OnCbnSelchangeComboType)
    ON_WM_CTLCOLOR()
    ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


// CNuWriterDlg message handlers
void CNuWriterDlg::FastMode_ProgressControl(int num, BOOL isHIDEType)
{
    int i;
    CDialog* mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());

    if(isHIDEType) {
        for(i=0; i< 8; i++) {
            ((CProgressCtrl *)mainWnd->GetDlgItem(IDC_FAST_PROGRESS1 + i))->ShowWindow(SW_HIDE);
            ((CStatic *)mainWnd->GetDlgItem((IDC_STATIC_FAST_MSG1+i)))->ShowWindow(SW_HIDE);
            ((CStatic *)mainWnd->GetDlgItem((IDC_STATIC_FAST_DEV1+i)))->ShowWindow(SW_HIDE);
        }
    } else {
        for(i=0; i< num; i++) {

            ((CProgressCtrl *)mainWnd->GetDlgItem(IDC_FAST_PROGRESS1 + i))->SetRange(0,100);
            ((CProgressCtrl *)mainWnd->GetDlgItem(IDC_FAST_PROGRESS1 + i))->SetPos(0);
            ((CProgressCtrl *)mainWnd->GetDlgItem(IDC_FAST_PROGRESS1 + i))->ShowWindow(SW_SHOW);
            ((CStatic *)mainWnd->GetDlgItem((IDC_STATIC_FAST_MSG1+i)))->SetWindowText(_T(""));
            ((CStatic *)mainWnd->GetDlgItem((IDC_STATIC_FAST_MSG1+i)))->ShowWindow(SW_SHOW);
            ((CStatic *)mainWnd->GetDlgItem((IDC_STATIC_FAST_DEV1+i)))->ShowWindow(SW_SHOW);
        }
    }
}

void CNuWriterDlg::ShowDeviceConnectState(BOOL isConnect)
{
    COLORREF col = RGB(0xFF, 0x00, 0xFF);

    if(isConnect)
    {
        m_portName.Format(_T("Nuvoton VCOM"));
        m_reconnect.setBitmapId(IDB_RECONNECT0, col);
        m_reconnect.setGradient(true);
        m_burntext.SetWindowText(_T("Device Connected"));
    }
    else
    {
        m_portName="";
        m_reconnect.setBitmapId(IDB_RECONNECT1, col);
        m_reconnect.setGradient(true);
        m_burntext.SetWindowText(_T(" Disconnected"));
    }
}

BOOL CNuWriterDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Add "About..." menu item to system menu.

    // IDM_ABOUTBOX must be in the system command range.
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != NULL) {
        CString strAboutMenu;
        strAboutMenu.LoadString(IDS_ABOUTBOX);
        if (!strAboutMenu.IsEmpty()) {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);        // Set big icon
    SetIcon(m_hIcon, FALSE);       // Set small icon

    //ShowWindow(SW_MINIMIZE);
    g_iDeviceNum = 0;
    g_iCurDevNum = 0;

    // TODO: Add extra initialization here
    envbuf=NULL;
    g_iDeviceNum = NucUsb.UsbDevice_Detect();
    for(int i = 0; i < g_iDeviceNum; i++) {
        m_ExitEvent[i]=CreateEvent(NULL,TRUE,FALSE,NULL);
    }

    this->SetWindowText(PROJECT_NAME);
    page_idx=0;
    TargetChip=0; //init target chip
    ChipEraseWithBad=0;
    ChipReadWithBad=0;
    ChipWriteWithOOB=0;
    DtbEn=0;
    //DDRBuf=NULL;
    //---auto detect device -------------------------------------------------
    //UsbRegisterDeviceNotification(this,OSR_DEVICE_INTERFACE,1);

    TCHAR exeFullPath[MAX_PATH];
    memset(exeFullPath,0,MAX_PATH);
    int len=GetModuleFileName(NULL,exeFullPath,MAX_PATH);
    for(int i=len; i>0; i--) {
        if(exeFullPath[i]=='\\') {
            len=i;
            break;
        }
    }
    lstrcpy(&exeFullPath[len+1],_T("path.ini"));
    m_inifile.SetPath(exeFullPath);

    CRect rc;
    CString title;

    (GetDlgItem(IDC_TYPE))->GetWindowRect(&rc); // get the position for the subforms
    m_SubForms.SetCenterPos(rc);     // if the subdialog needs to be centered

    INItoSaveOrLoad(LOAD);
    if(m_SelChipdlg.DoModal()==IDCANCEL) exit(0);

    m_gtype.AddString(_T("DDR/SRAM"));
    m_SubForms.CreateSubForm(IDD_SDRAM,this); // create the sub forms
    m_gtype.AddString(_T("SPI"));
    m_SubForms.CreateSubForm(IDD_SPI,this);
    m_gtype.AddString(_T("NAND"));
    m_SubForms.CreateSubForm(IDD_NAND,this);
    m_gtype.AddString(_T("eMMC/SD"));
    m_SubForms.CreateSubForm(IDD_MMC,this);
    m_gtype.AddString(_T("SPI NAND"));
    m_SubForms.CreateSubForm(IDD_SPINAND,this);
    m_gtype.AddString(_T("PACK"));
    m_SubForms.CreateSubForm(IDD_PACK,this);
    m_gtype.AddString(_T("Mass Production"));
    m_SubForms.CreateSubForm(IDD_FAST,this);
    m_static_ddrfile.ShowWindow(true);
    //------------------------------------
    LPTSTR charPathName = new TCHAR[DDRFileFullPath.GetLength()+1];
    _tcscpy(charPathName, DDRFileFullPath);
    wchar_t BmpDrive[10];
    wchar_t BmpDir[256];
    wchar_t BmpFName[256];
    wchar_t BmpExt[256];
    _wsplitpath(charPathName, BmpDrive, BmpDir, BmpFName, BmpExt);
    CString showddrname;

    CFont* pFont=m_static_ddrfile.GetFont();
    LOGFONT lf;
    pFont->GetLogFont(&lf);
    int nFontSize=14;
    lf.lfHeight = -MulDiv(nFontSize,96,72); // 96 dpi
    CFont* pNewFont=new CFont;
    pNewFont->CreateFontIndirect(&lf);
    m_static_ddrfile.SetFont(pNewFont);

    // showddrname.Format(_T("%s%s"),BmpFName,BmpExt);
    // m_static_ddrfile.SetWindowText(showddrname);

    UpdateBufForDDR();
    showddrname.Format(_T("%s%s-%s"),BmpFName,BmpExt,DDRFileVer);
    m_static_ddrfile.SetWindowText(showddrname);

    m_SubForms.ShowSubForm(); // show the first one
    m_gtype.SetCurSel(page_idx);
    m_gtype.GetWindowText(title);
    m_type.SetWindowText(title);
    m_SubForms.ShowSubForm(m_gtype.GetCurSel());

    INItoSaveOrLoad(SAVE);

    GetDlgItem(IDC_RECONNECT)->EnableWindow(FALSE);
    COLORREF col = RGB(0xFF, 0x00, 0xFF);
    m_reconnect.setBitmapId(IDB_RECONNECT0, col);
    m_reconnect.setGradient(true);
    m_exit.setBitmapId(IDB_EXIT, col);
    m_exit.setGradient(true);

    CString tempText;
    if(!g_iDeviceNum) {
        ShowDeviceConnectState(0);//Disconnected
        GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);
    } else {
        ShowDeviceConnectState(1);//Connected
        GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);
    }

    g_iCurDevNum =  g_iDeviceNum;
    //m_burntext.SetWindowText(tempText);

#ifdef AUTO_DOWNLOAD
    CString t_type;
    iDevice=0;
    if(!m_portName.IsEmpty()) {
        m_initdlg.SetData();
        m_initdlg.DoModal();
        if(m_initdlg.GetVersion() == _T("xxxx"))
        //if(!g_iDeviceNum)
        {
            ShowDeviceConnectState(0);//Disconnected
            GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);
            g_iCurDevNum -= 1;
            g_iDeviceNum -= 1;

            m_gtype.GetWindowText(t_type);
            if( !t_type.Compare(_T("Mass Production")) ) {
                FastMode_ProgressControl(8, 1); // HIDE
            }
        } else {
            m_version.SetWindowText(m_initdlg.GetVersion());
            g_iCurDevNum =  g_iDeviceNum;
            Sleep(FirstDelay);
            GetInfo();
        }
    }

#endif

    delete [] charPathName;
    delete pNewFont;
    return TRUE;  // return TRUE  unless you set the focus to a control
}

void CNuWriterDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    } else {
        CDialog::OnSysCommand(nID, lParam);
    }
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.
void CNuWriterDlg::OnPaint()
{
    if (IsIconic()) {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    } else {
        CDialog::OnPaint();
    }
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CNuWriterDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}


void CNuWriterDlg::OnBnClickedReconnect()
{
    CDialog* mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
    GetDlgItem(IDC_RECONNECT)->EnableWindow(FALSE);
    CString tempText;
    //COLORREF col = RGB(0xFF, 0x00, 0xFF);

    CString t_type;
    int i;

    m_gtype.GetWindowText(t_type);

    g_iDeviceNum = NucUsb.UsbDevice_Detect();
    TRACE(_T("\n@@@@@ CNuWriterDlg::OnBnClickedReconnect, g_iDeviceNum =%d\n"), g_iDeviceNum);
    if(!g_iDeviceNum) {
        AfxMessageBox(_T("No VCOM Port found !"));
        ShowDeviceConnectState(0);//Disconnected
        GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);
        g_iCurDevNum = 0;
        g_iDeviceNum = 0;

        if( !t_type.Compare(_T("Mass Production")) ) {
            FastMode_ProgressControl(8, 1); // HIDE
        }

        //AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return ;
    } else {
        ShowDeviceConnectState(1);//Connected
        if( !t_type.Compare(_T("Mass Production")) ) {
            FastMode_ProgressControl(8, 1); // HIDE
        }
    }
    //m_burntext.SetWindowText(tempText);
    //m_reconnect.setBitmapId(IDB_RECONNECT0, col);
    //m_reconnect.setGradient(true);

#ifdef AUTO_DOWNLOAD
    m_initdlg.SetData();
    m_initdlg.DoModal();
    iDevice=0;

    if(m_initdlg.GetVersion() == _T("xxxx") || g_iDeviceNum == 0) {
        ShowDeviceConnectState(0);//Disconnected
        GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);
        g_iCurDevNum -= 1;
        g_iDeviceNum -= 1;

        if( !t_type.Compare(_T("Mass Production")) ) {
            FastMode_ProgressControl(8, 1); // HIDE
        }

        return;
    } else {
        m_version.SetWindowText(m_initdlg.GetVersion());
        GetInfo();
#if(0)
        TRACE("CNuWriterDlg::OnBnClickedReconnect:g_iDeviceNum =%d\n",g_iDeviceNum);
        for(i=0;  i < g_iDeviceNum; i++) {
           OneDeviceInfo(i);
        }
#endif
        g_iDeviceNum = NucUsb.WinUsbNumber;
        TRACE(_T("g_iDeviceNum = %d\n"), g_iDeviceNum);
    }
#endif

    CString tmpstr;
    m_burntext.GetWindowText(tmpstr);

    //GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);

    if(m_gtype.GetCurSel()==0) /* SDRAM mode */
        m_SubForms.GetSubForm(0)->PostMessage(WM_SDRAM_PROGRESS,(LPARAM)0,0);

    if( !t_type.Compare(_T("Mass Production")) ) {

        FastMode_ProgressControl(8, 1);// all HIDE
        FastMode_ProgressControl(g_iDeviceNum, 0);// show

        ((CComboBox*)mainWnd->GetDlgItem(IDC_COMBO_FAST_ID))->ResetContent();
        for(i = 0; i < g_iDeviceNum; i++) {
            tempText.Format(_T("%d"),i);
            ((CComboBox*)mainWnd->GetDlgItem(IDC_COMBO_FAST_ID))->AddString(tempText);
        }

        ((CComboBox*)mainWnd->GetDlgItem(IDC_COMBO_FAST_ID))->SetCurSel(0);
        mainWnd->GetDlgItem(IDC_BTN_FAST_START)->EnableWindow(TRUE);
    }

}

void CNuWriterDlg::OnDestroy()
{
    CDialog::OnDestroy();
    INItoSaveOrLoad(SAVE);
    //TRACE(_T("CNuWriterDlg::OnDestroy() CNuWriterDlg::OnDestroy() CNuWriterDlg::OnDestroy() CNuWriterDlg::OnDestroy()"));
}

BYTE CNuWriterDlg::Str2Hex(CString strSource)
{
    BYTE num;
    char *str;
    CStringA strA(strSource.GetBuffer(0));
    strSource.ReleaseBuffer();
    string s = strA.GetBuffer(0);
    const char* pc = s.c_str();
    num = (int)strtol(pc, &str, 16);
    return num;
}

void CNuWriterDlg::INItoSaveOrLoad(int Flag)
{

    if(!m_inifile.ReadFile()) //Fail
        return;

    CString idx;
    memset((char *)&m_info,0xff,sizeof(INFO_T));
    if(Flag==LOAD) {
#if 0
        idx=m_inifile.GetValue(_T("DEFAULT"),_T("PAGE_IDX"));
        if(!idx.IsEmpty())
            m_gtype.SetCurSel(_wtoi(idx));
#endif
        page_idx = _wtoi(m_inifile.GetValue(_T("DEFAULT"),_T("PAGE_IDX")));
        TargetChip=_wtoi(m_inifile.GetValue(_T("TARGET"),_T("CHIP")));
        DDR_Idx=_wtoi(m_inifile.GetValue(_T("TARGET"),_T("DDR")));
        DDRAddress=_wtoi(m_inifile.GetValue(_T("TARGET"),_T("DDRADDRESS")));
        TimeEn=_wtoi(m_inifile.GetValue(_T("TARGET"),_T("TimeEnable")));
        Timeus=_wtoi(m_inifile.GetValue(_T("TARGET"),_T("Timeus")));
        ChipEraseWithBad=_wtoi(m_inifile.GetValue(_T("TARGET"),_T("ChipEraseWithBad")));
        ChipReadWithBad=_wtoi(m_inifile.GetValue(_T("TARGET"),_T("ChipReadWithBad")));
        ChipWriteWithOOB=_wtoi(m_inifile.GetValue(_T("TARGET"),_T("ChipWriteWithOOB")));
        DtbEn=_wtoi(m_inifile.GetValue(_T("SDRAM"),_T("DTBEN")));
        FirstDelay=_wtoi(m_inifile.GetValue(_T("TARGET"),_T("FirstDelay")));

        Nand_uBlockPerFlash=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uBlockPerFlash")));
        Nand_uPagePerBlock=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uPagePerBlock")));

        //memset((char *)&m_info,0xff,sizeof(INFO_T));
        m_info.SPI_QuadReadCmd = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("QuadReadCmd")));
        m_info.SPI_ReadStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("ReadStatusCmd")));
        m_info.SPI_WriteStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("WriteStatusCmd")));
        m_info.SPI_dummybyte = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("dummybyte")));
        m_info.SPI_StatusValue = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("StatusValue")));

        //(char*)(LPCTSTR)strSource
        m_info.SPINand_PageSize = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PageSize")));
        m_info.SPINand_SpareArea = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("SpareArea")));

        m_info.SPINand_QuadReadCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("QuadReadCmd")));
        m_info.SPINand_ReadStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("ReadStatusCmd")));
        m_info.SPINand_WriteStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("WriteStatusCmd")));
        m_info.SPINand_dummybyte = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("dummybyte")));
        m_info.SPINand_StatusValue = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("StatusValue")));

        //m_info.SPINand_ReadStatusCmd = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("ReadStatusCmd")));
        //m_info.SPINand_WriteStatusCmd = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("WriteStatusCmd")));
        //m_info.SPINand_dummybyte = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("dummybyte")));
        //m_info.SPINand_StatusValue = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("StatusValue")));

        m_info.SPINand_BlockPerFlash=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("BlockPerFlash")));
        m_info.SPINand_PagePerBlock=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PagePerBlock")));

        if(Timeus==0) Timeus=5000;

    } else if(Flag==SAVE) {
#if 0
        idx.Format(_T("%d"),m_gtype.GetCurSel());
        m_inifile.SetValue(_T("DEFAULT"),_T("PAGE_IDX"),idx);
        m_inifile.WriteFile();
#endif

        idx.Format(_T("%d"),TargetChip);
        m_inifile.SetValue(_T("TARGET"),_T("CHIP"),idx);

        idx.Format(_T("%d"),DDR_Idx);
        m_inifile.SetValue(_T("TARGET"),_T("DDR"),idx);

        idx.Format(_T("%d"),TimeEn);
        m_inifile.SetValue(_T("TARGET"),_T("TimeEnable"),idx);

        idx.Format(_T("%d"),Timeus);
        m_inifile.SetValue(_T("TARGET"),_T("Timeus"),idx);

        idx.Format(_T("%d"),m_gtype.GetCurSel());
        m_inifile.SetValue(_T("DEFAULT"),_T("PAGE_IDX"),idx);

        m_inifile.WriteFile();
    }

}
void CNuWriterDlg::UpdateBufForDDR()
{
    LoadDDRInit(DDRFileFullPath,&DDRLen);
}

//char * CNuWriterDlg::LoadDDRInit(CString pathName,int *len)
void CNuWriterDlg::LoadDDRInit(CString pathName,int *len)
{
    ifstream Read;
    int length,tmpbuf_size;
    char *ptmpbuf,*ptmp,tmp[256],cvt[128];
    unsigned int * puint32_t;
    Read.open(pathName,ios::binary | ios::in);

    if(!Read.is_open())
        AfxMessageBox(_T("open DDR initial file error\n"));

    Read.seekg (0, Read.end);
    length=(int)Read.tellg();
    ptmpbuf=(char *)malloc(sizeof(char)*length);
    //TRACE(_T("LoadDDRInit --> length = %d\n"), length);
    Read.seekg(0,Read.beg);
    unsigned int val=0;

    puint32_t=(unsigned int *)ptmpbuf;
    tmpbuf_size=0;
    while(!Read.eof()) {
        Read.getline(tmp,256);
        ptmp=strchr(tmp,'=');
        if(ptmp==NULL) {
            AfxMessageBox(_T("DDR initial format error\n"));
            break;
        }
        strncpy(cvt,tmp,(unsigned int)ptmp-(unsigned int)tmp);
        cvt[(unsigned int)ptmp-(unsigned int)tmp]='\0';
        val=strtoul(cvt,NULL,0);
        *puint32_t=val;
        puint32_t++;
        tmpbuf_size+=sizeof(unsigned int *);

        strncpy(cvt,++ptmp,strlen(ptmp));
        cvt[strlen(ptmp)]='\0';
        val=strtoul(cvt,NULL,0);
        *puint32_t=val;
        puint32_t++;
        tmpbuf_size+=sizeof(unsigned int *);
    }
    Read.close();


#if 0 //for test
    ofstream Write;
    Write.open("C:\\tmp.bin",ios::binary | ios::out);
    Write.write(ptmpbuf,tmpbuf_size);
    Write.close();
#endif
    TRACE(_T("ptmpbuf[0~3] = 0x%x 0x%x 0x%x 0x%x\n"), ptmpbuf[0], ptmpbuf[1], ptmpbuf[2], ptmpbuf[3]);
    if(((ptmpbuf[0]&0xff) == 0) && ((ptmpbuf[1]&0xff) == 0) && ((ptmpbuf[2]&0xff) == 0) && ((ptmpbuf[3]&0xff) == 0xB0))
    {
        *len=(tmpbuf_size-8);// ddr version 8 byte
        DDRFileVer.Format(_T("V%d.0"), ptmpbuf[4]);
        memcpy((char *)ShareDDRBuf, ptmpbuf+8, *len);
    }
    else
    {
        *len=tmpbuf_size;
        DDRFileVer.Format(_T("No version"));
        memcpy((char *)ShareDDRBuf, ptmpbuf, *len);
    }

    free(ptmpbuf);
    return ;
}

void  CNuWriterDlg::GetExeDir(CString & path)
{
    wchar_t szDir[MAX_PATH];
    GetModuleFileName(NULL,szDir,MAX_PATH);

    wchar_t drive[_MAX_DRIVE];
    wchar_t dir[_MAX_DIR];
    _wsplitpath(szDir, drive, dir, NULL,NULL);
    CString strPath;
    path.Format(_T("%s%s"), drive, dir);
}

HBRUSH CNuWriterDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    if (pWnd->GetDlgCtrlID() == IDC_COMPORT) {
        CString word;
        GetDlgItem(IDC_COMPORT)->GetWindowText(word);
        if(!word.Compare(_T(" Disconnected")))
            pDC->SetTextColor(RGB(255,0,0));
        else
            pDC->SetTextColor(RGB(0,0,0));
    }

    return hbr;
}

void CNuWriterDlg::LoadDirINI(CString szDir,wchar_t * pext, vector<CString> &sList) // Dir 就是你要掃瞄的目錄, sList 拿來存放檔名
{
    //pext=".ini"
    WIN32_FIND_DATA filedata; // Structure for file data
    HANDLE filehandle; // Handle for searching
    CString szFileName,szFileFullPath;
    wchar_t filename[_MAX_FNAME];
    wchar_t ext[_MAX_EXT];
    //szDir = IncludeTrailingPathDelimiter(Dir);

    filehandle = FindFirstFile((szDir + _T("*.*")).GetBuffer(0), &filedata);
    if (filehandle != INVALID_HANDLE_VALUE) {
        do {
            if ((filedata.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0 ||
                    lstrcmp(filedata.cFileName, _T(".")) == 0 ||
                    lstrcmp(filedata.cFileName, _T("..")) == 0)
                continue;
            if ((filedata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                szFileFullPath = szDir + filedata.cFileName;
                LoadDirINI(szFileFullPath,pext,sList);
            } else {
                szFileFullPath = szDir + filedata.cFileName;
                _wsplitpath(szFileFullPath, NULL, NULL, filename,ext);
                if(lstrcmp(ext,pext)==0) {
                    szFileName.Format(_T("%s%s"),filename,ext);
                    sList.push_back(szFileName);
                }

            }
        } while (FindNextFile(filehandle, &filedata));

        FindClose(filehandle);
    }
}

void CNuWriterDlg::OnCbnSelchangeComboType()
{
    CString title;
    int i = 0;
    m_gtype.GetWindowText(title);
    m_type.SetWindowText(title);
    m_SubForms.ShowSubForm(m_gtype.GetCurSel());

    CString t_type, str;

    //Default
    //m_info.Nand_uIsUserConfig = 0;
    //m_info.Nand_uPagePerBlock =  Nand_uPagePerBlock;
    //m_info.Nand_uBlockPerFlash = Nand_uBlockPerFlash;
    m_gtype.GetWindowText(t_type);
    if( !t_type.Compare(_T("NAND")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
        CString tmp;
        unsigned int val=m_info.Nand_uPagePerBlock * m_info.Nand_uPageSize;
        if(val==0 || val>0x800000) {
            tmp.Format(_T("Alignment : N/A"));
            Nand_size.Format(_T("N/A"));
        } else {
            tmp.Format(_T("Alignment : 0x%x"),val);
            Nand_size.Format(_T("0x%x"),val);
        }
        ((CNANDDlg *)mainWnd)->m_status.SetWindowText(tmp);

        if(m_info.Nand_uBlockPerFlash == 1023)
            m_info.Nand_uBlockPerFlash = 1024;
        else if(m_info.Nand_uBlockPerFlash == 2047)
            m_info.Nand_uBlockPerFlash = 2048;
        else if(m_info.Nand_uBlockPerFlash == 4095)
            m_info.Nand_uBlockPerFlash = 4096;
        else if(m_info.Nand_uBlockPerFlash == 8191)
            m_info.Nand_uBlockPerFlash = 8192;

        if(((CNANDDlg *)mainWnd)->m_nandflash_check.GetCheck()!=TRUE) // Auto Detect
        {
            m_info.Nand_uIsUserConfig = 0;
            str.Format(_T("%d"), m_info.Nand_uBlockPerFlash);
            m_inifile.SetValue(_T("NAND_INFO"),_T("uBlockPerFlash"),str);
            m_inifile.WriteFile();
            str.Format(_T("%d"), m_info.Nand_uPagePerBlock);
            m_inifile.SetValue(_T("NAND_INFO"),_T("uPagePerBlock"),str);
            m_inifile.WriteFile();
        }
        else // User Config
        {
            m_info.Nand_uIsUserConfig = 1;
            m_info.Nand_uBlockPerFlash=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uBlockPerFlash")));
            str.Format(_T("%d"), m_info.Nand_uBlockPerFlash);
            m_inifile.SetValue(_T("NAND_INFO"),_T("uBlockPerFlash"),str);
            m_inifile.WriteFile();
            m_info.Nand_uPagePerBlock=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uPagePerBlock")));
            str.Format(_T("%d"), m_info.Nand_uPagePerBlock);
            m_inifile.SetValue(_T("NAND_INFO"),_T("uPagePerBlock"),str);
            m_inifile.WriteFile();
        }
    }

    if( !t_type.Compare(_T("SPI")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());

        if( !t_type.Compare(_T("SPI")) ) {
            CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
            if(((CSPIDlg *)mainWnd)->m_spinor_check.GetCheck()!=TRUE)
            {
                m_info.SPI_uIsUserConfig = 0;
            }
            else
            {
                m_info.SPI_uIsUserConfig = 1;
            }
        }
    }

    if( !t_type.Compare(_T("SPI NAND")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
        CString tmp;
        unsigned int val= m_info.SPINand_PagePerBlock * m_info.SPINand_PageSize;
        if(val==0 || val>0x800000) {
            tmp.Format(_T("Alignment : N/A"));
            SPINand_size.Format(_T("N/A"));
        } else {
            tmp.Format(_T("Alignment : 0x%x"),val);
            SPINand_size.Format(_T("0x%x"),val);
        }
        ((CSPINandDlg *)mainWnd)->m_status.SetWindowText(tmp);

        if( !t_type.Compare(_T("SPI NAND")) ) {
            CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
            if(((CSPINandDlg *)mainWnd)->m_spinandflash_check.GetCheck()!=TRUE)
            {
                m_info.SPINand_uIsUserConfig = 0;
                // m_info.SPINand_PagePerBlock=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PagePerBlock")));
                // m_info.SPINand_BlockPerFlash=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("BlockPerFlash")));
            }
            else
            {
                m_info.SPINand_uIsUserConfig = 1;
                // m_info.SPINand_PageSize = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PageSize")));
                // m_info.SPINand_SpareArea = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("SpareArea")));
                // m_info.SPINand_QuadReadCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("QuadReadCmd")));
                // m_info.SPINand_ReadStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("ReadStatusCmd")));
                // m_info.SPINand_WriteStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("WriteStatusCmd")));
                // m_info.SPINand_dummybyte = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("dummybyte")));
                // m_info.SPINand_StatusValue = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("StatusValue")));
                // m_info.SPINand_BlockPerFlash=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("BlockPerFlash")));
                // m_info.SPINand_PagePerBlock=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PagePerBlock")));
            }
        }
    }

    if( !t_type.Compare(_T("PACK")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
        unsigned int val=m_info.Nand_uPagePerBlock * m_info.Nand_uPageSize;
        CString tmp;
        if(val==0 || val>0x2000000)
            tmp.Format(_T("Alignment : SPI(0x1000)/eMMC(0x200)/NAND(n/a)"));
        else
            tmp.Format(_T("Alignment : SPI(0x1000)/eMMC(0x200)/NAND(0x%x)"),val);
        ((CPACKDlg *)mainWnd)->m_status.SetWindowText(tmp);
    }

    if( !t_type.Compare(_T("eMMC")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
        CString tmp;
        int val=m_info.EMMC_uReserved * 0x200;
        if(val<=0)
            tmp.Format(_T("Alignment : 0x200, Reserved Size : N/A"),val);
        else
            tmp.Format(_T("Alignment : 0x200, Reserved Size : 0x%x"),val);
        ((CMMCDlg *)mainWnd)->m_status.SetWindowText(tmp);
    }

    if( !t_type.Compare(_T("Mass Production")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());

        if(m_info.Nand_uBlockPerFlash == 1023)
            m_info.Nand_uBlockPerFlash = 1024;
        else if(m_info.Nand_uBlockPerFlash == 2047)
            m_info.Nand_uBlockPerFlash = 2048;
        else if(m_info.Nand_uBlockPerFlash == 4095)
            m_info.Nand_uBlockPerFlash = 4096;
        else if(m_info.Nand_uBlockPerFlash == 8191)
            m_info.Nand_uBlockPerFlash = 8192;

        if(((FastDlg *)mainWnd)->m_nandflashInfo_check.GetCheck()!=TRUE) // Auto Detect
        {
            m_info.Nand_uIsUserConfig = 0;
            str.Format(_T("%d"), m_info.Nand_uBlockPerFlash);
            m_inifile.SetValue(_T("NAND_INFO"),_T("uBlockPerFlash"),str);
            m_inifile.WriteFile();
            str.Format(_T("%d"), m_info.Nand_uPagePerBlock);
            m_inifile.SetValue(_T("NAND_INFO"),_T("uPagePerBlock"),str);
            m_inifile.WriteFile();
        }
        else // User Config
        {
            m_info.Nand_uIsUserConfig = 1;
            m_info.Nand_uBlockPerFlash=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uBlockPerFlash")));
            str.Format(_T("%d"), m_info.Nand_uBlockPerFlash);
            m_inifile.SetValue(_T("NAND_INFO"),_T("uBlockPerFlash"),str);
            m_inifile.WriteFile();
            m_info.Nand_uPagePerBlock=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uPagePerBlock")));
            str.Format(_T("%d"), m_info.Nand_uPagePerBlock);
            m_inifile.SetValue(_T("NAND_INFO"),_T("uPagePerBlock"),str);
            m_inifile.WriteFile();
        }

        if(((FastDlg *)mainWnd)->m_spinor_check.GetCheck()!=TRUE)
        {
            m_info.SPI_uIsUserConfig = 0;
        }
        else // User Config
        {
            m_info.SPI_uIsUserConfig = 1;
        }

        if(((FastDlg *)mainWnd)->m_spinandflashInfo_check.GetCheck()!=TRUE)
        {
            m_info.SPINand_uIsUserConfig = 0;
        }
        else // User Config
        {
            m_info.SPINand_uIsUserConfig = 1;
        }

        for(i=0; i< NucUsb.WinUsbNumber; i++) {
            ((CProgressCtrl *)mainWnd->GetDlgItem(IDC_FAST_PROGRESS1 + i))->ShowWindow(SW_SHOW);
            ((CStatic *)mainWnd->GetDlgItem((IDC_STATIC_FAST_MSG1+i)))->ShowWindow(SW_SHOW);
            ((CStatic *)mainWnd->GetDlgItem((IDC_STATIC_FAST_DEV1+i)))->ShowWindow(SW_SHOW);
            ((CProgressCtrl *)mainWnd->GetDlgItem(IDC_FAST_PROGRESS1 + i))->SetRange(0,100);
            ((CProgressCtrl *)mainWnd->GetDlgItem(IDC_FAST_PROGRESS1 + i))->SetPos(0);
            ((CStatic *)mainWnd->GetDlgItem((IDC_STATIC_FAST_MSG1+i)))->SetWindowText(_T(""));
        }
    }
}

void CNuWriterDlg::OneSelchangeComboType(int id)
{
    CString title;
    m_gtype.GetWindowText(title);
    m_type.SetWindowText(title);
    m_SubForms.ShowSubForm(m_gtype.GetCurSel());

    CString t_type;
    m_gtype.GetWindowText(t_type);

    if( !t_type.Compare(_T("Mass Production")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());

        ((CProgressCtrl *)mainWnd->GetDlgItem(IDC_FAST_PROGRESS1 + id))->ShowWindow(SW_SHOW);
        ((CStatic *)mainWnd->GetDlgItem((IDC_STATIC_FAST_MSG1+id)))->ShowWindow(SW_SHOW);
        ((CStatic *)mainWnd->GetDlgItem((IDC_STATIC_FAST_DEV1+id)))->ShowWindow(SW_SHOW);
        ((CProgressCtrl *)mainWnd->GetDlgItem(IDC_FAST_PROGRESS1 + id))->SetRange(0,100);
        ((CProgressCtrl *)mainWnd->GetDlgItem(IDC_FAST_PROGRESS1 + id))->SetPos(0);
        ((CStatic *)mainWnd->GetDlgItem((IDC_STATIC_FAST_MSG1+id)))->SetWindowText(_T(""));

    }
}

int  CNuWriterDlg::Read_File_Line(HANDLE hFile)
{
    DWORD   i, nBytesRead;
    char    data_byte;
    BOOL    bResult;

    for (i = 0; i < LINE_BUFF_LEN; i++) {
        bResult = ReadFile(hFile, (LPVOID)&data_byte, 1, &nBytesRead, NULL);
        if (bResult &&  nBytesRead == 0) {
            if (i == 0) {
                return -1;
            } else {
                _FileLineBuff[i] = 0;
                return 0;
            }
        }

        if ((data_byte == '\n') || (data_byte == '\r'))
            break;

        _FileLineBuff[i] = data_byte;
    }
    _FileLineBuff[i] = 0;

    while (1) {
        bResult = ReadFile(hFile, (LPVOID)&data_byte, 1, &nBytesRead, NULL);
        if (bResult &&  nBytesRead == 0)
            return 0;

        if ((data_byte != '\n') && (data_byte != '\r'))
            break;
    }
    SetFilePointer(hFile, -1, NULL, FILE_CURRENT);
    return 0;
}

void CNuWriterDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CDialog::OnShowWindow(bShow, nStatus);

    // TODO: Add your message handler code here
    m_gtype.SetFocus();
}

BOOL CNuWriterDlg:: Info()
{
    BOOL bResult;

    bResult = NucUsb.EnableWinUsbDevice();
    if(!bResult) {
        //AfxMessageBox(_T("CNuWriterDlg:: Info(): Device Enable error\n"));
        //AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }

#if(0)
    bResult=NucUsb.NUC_CheckFw(0);
    ResetEvent(m_ExitEvent[0]);
    if(bResult==FALSE) {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif

    USHORT typeack;
    bResult=NucUsb.NUC_SetType(0,INFO,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE) {
        NucUsb.NUC_CloseHandle();
        return FALSE;
    }

#if(1)
    CString t_type;
    m_gtype.GetWindowText(t_type);
    if( !t_type.Compare(_T("NAND")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
        if(((CNANDDlg *)mainWnd)->m_nandflash_check.GetCheck()!=TRUE)
        {
            m_info.Nand_uIsUserConfig = 0;
            m_info.Nand_uPagePerBlock =  Nand_uPagePerBlock;
            m_info.Nand_uBlockPerFlash = Nand_uBlockPerFlash;
        }
        else
        {
            m_info.Nand_uIsUserConfig = 1;
            m_info.Nand_uPagePerBlock=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uPagePerBlock")));
            m_info.Nand_uBlockPerFlash=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uBlockPerFlash")));
        }

        TRACE(_T("Info: BlockPerFlash =%d, PagePerBlock = %d\n"),  m_info.Nand_uBlockPerFlash, m_info.Nand_uPagePerBlock);
    }

    m_gtype.GetWindowText(t_type);
    if( !t_type.Compare(_T("SPI NAND")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
        if(((CSPINandDlg *)mainWnd)->m_spinandflash_check.GetCheck()!=TRUE)
        {
            m_info.SPINand_uIsUserConfig = 0;
            // m_info.SPINand_PagePerBlock=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PagePerBlock")));
            // m_info.SPINand_BlockPerFlash=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("BlockPerFlash")));
        }
        else
        {
            m_info.SPINand_uIsUserConfig = 1;
            // m_info.SPINand_PagePerBlock=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PagePerBlock")));
            // m_info.SPINand_BlockPerFlash=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("BlockPerFlash")));
            m_info.SPINand_PageSize = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PageSize")));
            m_info.SPINand_SpareArea = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("SpareArea")));
            m_info.SPINand_QuadReadCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("QuadReadCmd")));
            m_info.SPINand_ReadStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("ReadStatusCmd")));
            m_info.SPINand_WriteStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("WriteStatusCmd")));
            m_info.SPINand_dummybyte = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("dummybyte")));
            m_info.SPINand_StatusValue = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("StatusValue")));
            m_info.SPINand_BlockPerFlash=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("BlockPerFlash")));
            m_info.SPINand_PagePerBlock=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PagePerBlock")));
        }

        TRACE(_T("Info: BlockPerFlash =%d, PagePerBlock = %d\n"),  m_info.Nand_uBlockPerFlash, m_info.Nand_uPagePerBlock);
    }
#endif
    //m_info.Nand_uPagePerBlock =  Nand_uPagePerBlock;
    //m_info.Nand_uBlockPerFlash = Nand_uBlockPerFlash;
    bResult=NucUsb.NUC_WritePipe(0,(UCHAR *)&m_info, sizeof(INFO_T));
    if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT) bResult=FALSE;
    if(bResult!=TRUE)   bResult=FALSE;

    bResult=NucUsb.NUC_ReadPipe(0,(UCHAR *)&m_info, sizeof(INFO_T));
    if(WaitForSingleObject(m_ExitEvent, 0) != WAIT_TIMEOUT) bResult=FALSE;
    if(bResult!=TRUE)   bResult=FALSE;


    NucUsb.NUC_CloseHandle();
    OnCbnSelchangeComboType();
    return bResult;
}

BOOL CNuWriterDlg::OneDeviceInfo(int id)
{
    BOOL bResult, bRet;
    int count=0;

    //TRACE(_T("\n@@@@@(%d) CNuWriterDlg::OneDeviceInfo\n"), id);
    ResetEvent(m_ExitEvent[id]);
    bRet = FALSE;
    for(count =0; count < RETRYCNT; count++) {
        //TRACE(_T("@@@@@ (%d) OneDeviceInfo Retry %d\n"), id, count);
        bResult=NucUsb.EnableOneWinUsbDevice(id);
        //TRACE(_T("@@@@@ (%d) bResult %d\n\n"), id, bResult);
        if(!bResult) {
            //TRACE(_T("@@@@@ !!!!! Start (%d) bResult %d\n"), id, bResult);
            bResult=NucUsb.EnableOneWinUsbDevice(id);
            //TRACE(_T("@@@@@ !!!!! End   (%d) bResult %d\n"), id, bResult);
            if(bResult == FALSE) {
                bResult =NucUsb.NUC_ResetFW(id);
                TRACE(_T("@@@@@ (%d) RESET NUC980 \n"), id);
                Sleep(100);//for Mass production
                if(bResult == FALSE) {
                    TRACE(_T("@@@@@ XXXX (%d) RESET NUC980 failed\n"), id);
                    continue;
                }
                else
                {
                    bRet = TRUE;
                    //GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);
                    break;
                }
                Sleep(200);//for Mass production
                bResult = FWDownload(id);
                //TRACE(_T("@@@@@ (%d) FWDownload NUC980 \n"), id);
                Sleep(200);//for Mass production
                if(bResult == FALSE) {
                    TRACE(_T("@@@@@ XXXX (%d) Download FW failed\n"), id);
                    continue;
                }
            }
            else
            {
                bRet = TRUE;
                break;
            }
        }
        else
        {
            bRet = TRUE;
            //GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);
            break;
        }
    }

    if(bRet == FALSE) // Retry Fail
    {
        TRACE(_T("@@@@@ XXX (%d) EnableOneWinUsbDevice RETRY ERROR\n"), id);
        NucUsb.CloseWinUsbDevice(id);
        ShowDeviceConnectState(0);//Disconnected
        GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);
        //AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }


#if(0)
    bResult=NucUsb.NUC_CheckFw(id);
    ResetEvent(m_ExitEvent[id]);
    if(bResult==FALSE) {
        AfxMessageBox(_T("Please reset device and Re-connect now !!!\n"));
        return FALSE;
    }
#endif

    USHORT typeack;
    bResult=NucUsb.NUC_SetType(id,INFO,(UCHAR *)&typeack,sizeof(typeack));
    if(bResult==FALSE) {
        NucUsb.CloseWinUsbDevice(id);
        ShowDeviceConnectState(0);//Disconnected
        GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);
        return FALSE;
    }

#if(1)
    CString t_type;
    m_gtype.GetWindowText(t_type);

    //Default
    m_info.Nand_uIsUserConfig = 0;
    m_info.Nand_uPagePerBlock =  Nand_uPagePerBlock;
    m_info.Nand_uBlockPerFlash = Nand_uBlockPerFlash;

    if( !t_type.Compare(_T("NAND")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
        if(((CNANDDlg *)mainWnd)->m_nandflash_check.GetCheck()!=TRUE)
        {
            m_info.Nand_uIsUserConfig = 0;
            m_info.Nand_uPagePerBlock =  Nand_uPagePerBlock;
            m_info.Nand_uBlockPerFlash = Nand_uBlockPerFlash;
        }
        else
        {
            m_info.Nand_uIsUserConfig = 1;
            m_info.Nand_uPagePerBlock=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uPagePerBlock")));
            m_info.Nand_uBlockPerFlash=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uBlockPerFlash")));
        }

        //TRACE(_T("OneDeviceInfo: IsUserConfig =%d, BlockPerFlash =%d, PagePerBlock = %d\n"),  m_info.Nand_uIsUserConfig, m_info.Nand_uBlockPerFlash, m_info.Nand_uPagePerBlock);
    }

    if( !t_type.Compare(_T("SPI")) ) {

        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
        if(((CSPIDlg *)mainWnd)->m_spinor_check.GetCheck()!=TRUE)
        {
            m_info.SPI_uIsUserConfig = 0;
        }
        else
        {
            m_info.SPI_uIsUserConfig = 1;
            m_info.SPI_QuadReadCmd = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("QuadReadCmd")));
            m_info.SPI_ReadStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("ReadStatusCmd")));
            m_info.SPI_WriteStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("WriteStatusCmd")));
            m_info.SPI_StatusValue = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("StatusValue")));
            m_info.SPI_dummybyte = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("dummybyte")));
        }
    }

    if( !t_type.Compare(_T("SPI NAND")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
        if(((CSPINandDlg *)mainWnd)->m_spinandflash_check.GetCheck()!=TRUE)
        {
            m_info.SPINand_uIsUserConfig = 0;
            // m_info.SPINand_PagePerBlock=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PagePerBlock")));
            // m_info.SPINand_BlockPerFlash=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("BlockPerFlash")));
        }
        else
        {
            m_info.SPINand_uIsUserConfig = 1;
            m_info.SPINand_PageSize = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PageSize")));
            m_info.SPINand_SpareArea = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("SpareArea")));
            m_info.SPINand_QuadReadCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("QuadReadCmd")));
            m_info.SPINand_ReadStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("ReadStatusCmd")));
            m_info.SPINand_WriteStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("WriteStatusCmd")));
            m_info.SPINand_dummybyte = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("dummybyte")));
            m_info.SPINand_StatusValue = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("StatusValue")));
            m_info.SPINand_BlockPerFlash=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("BlockPerFlash")));
            m_info.SPINand_PagePerBlock=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PagePerBlock")));
        }

        //TRACE(_T("OneDeviceInfo: IsUserConfig =%d, BlockPerFlash =%d, PagePerBlock = %d\n"),  m_info.SPINand_uIsUserConfig, m_info.SPINand_BlockPerFlash, m_info.SPINand_PagePerBlock);
    }

    if( !t_type.Compare(_T("Mass Production")) ) {
        CDialog * mainWnd=m_SubForms.GetSubForm(m_gtype.GetCurSel());
        if(((FastDlg *)mainWnd)->m_nandflashInfo_check.GetCheck()!=TRUE) // Auto Detect
        {
            m_info.Nand_uIsUserConfig = 0;
            m_info.Nand_uPagePerBlock =  Nand_uPagePerBlock;
            m_info.Nand_uBlockPerFlash = Nand_uBlockPerFlash;
        }
        else // User Config
        {
            m_info.Nand_uIsUserConfig = 1;
            m_info.Nand_uPagePerBlock=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uPagePerBlock")));
            m_info.Nand_uBlockPerFlash=_wtoi(m_inifile.GetValue(_T("NAND_INFO"),_T("uBlockPerFlash")));
        }

        if(((FastDlg *)mainWnd)->m_spinor_check.GetCheck()!=TRUE)
        {
            m_info.SPI_uIsUserConfig = 0;
        }
        else
        {
            m_info.SPI_uIsUserConfig = 1;
            m_info.SPI_QuadReadCmd = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("QuadReadCmd")));
            m_info.SPI_ReadStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("ReadStatusCmd")));
            m_info.SPI_WriteStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("WriteStatusCmd")));
            m_info.SPI_StatusValue = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("StatusValue")));
            m_info.SPI_dummybyte = Str2Hex(m_inifile.GetValue(_T("SPI_INFO"),_T("dummybyte")));
        }

        if(((FastDlg *)mainWnd)->m_spinandflashInfo_check.GetCheck()!=TRUE) // Auto Detect
        {
            m_info.SPINand_uIsUserConfig = 0;
        }
        else // User Config
        {
            m_info.SPINand_uIsUserConfig = 1;
            m_info.SPINand_PageSize = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PageSize")));
            m_info.SPINand_SpareArea = _wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("SpareArea")));
            m_info.SPINand_QuadReadCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("QuadReadCmd")));
            m_info.SPINand_ReadStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("ReadStatusCmd")));
            m_info.SPINand_WriteStatusCmd = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("WriteStatusCmd")));
            m_info.SPINand_dummybyte = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("dummybyte")));
            m_info.SPINand_StatusValue = Str2Hex(m_inifile.GetValue(_T("SPINAND_INFO"),_T("StatusValue")));
            m_info.SPINand_BlockPerFlash=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("BlockPerFlash")));
            m_info.SPINand_PagePerBlock=_wtoi(m_inifile.GetValue(_T("SPINAND_INFO"),_T("PagePerBlock")));
        }
        //TRACE(_T("OneDeviceInfo: IsUserConfig =%d, BlockPerFlash =%d, PagePerBlock = %d\n"),  m_info.Nand_uIsUserConfig, m_info.Nand_uBlockPerFlash, m_info.Nand_uPagePerBlock);
    }

#endif
    //m_info.Nand_uPagePerBlock =  Nand_uPagePerBlock;
    //m_info.Nand_uBlockPerFlash = Nand_uBlockPerFlash;

    bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)&m_info, sizeof(INFO_T));
    if(WaitForSingleObject(m_ExitEvent[id], 0) != WAIT_TIMEOUT) bResult=FALSE;
    if(bResult==FALSE) {
        TRACE(_T("XXXX Error NUC_WritePipe: %d.\n"), GetLastError());
        //return FALSE;
    }

    Sleep(300);// Delay for INFO complete
    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&m_info, sizeof(INFO_T));
    if(WaitForSingleObject(m_ExitEvent[id], 0) != WAIT_TIMEOUT) bResult=FALSE;
    if(bResult==FALSE) {
        TRACE(_T("XXXX Error NUC_ReadPipe: %d.\n"), GetLastError());
        //return FALSE;
    }

    NucUsb.CloseWinUsbDevice(id);
    OnCbnSelchangeComboType();

    GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);

	if(bResult==FALSE)
	{
		 ShowDeviceConnectState(0);//Disconnected
	}

    return bResult;
}

unsigned WINAPI CNuWriterDlg::Info_proc(void* args)
{
    CNuWriterDlg* pThis = reinterpret_cast<CNuWriterDlg*>(args);
#if(0)
    int time[]= {100,200,300,400,500};
    for(int i=0; i<sizeof(time)/sizeof(int); i++) {
        //if(pThis->Info()==TRUE) break;
        if(pThis->g_iDeviceNum < NucUsb.WinUsbNumber) {
            TRACE("Info_proc:idevice =%d\n",pThis->iDevice);
            pThis->OneDeviceInfo(pThis->iDevice++);
        }
        Sleep(time[i]);
    }
#else
    TRACE("Info_proc:: %d   %d\n",pThis->g_iDeviceNum, NucUsb.WinUsbNumber);
    //if(pThis->g_iDeviceNum < NucUsb.WinUsbNumber) {
    //if(pThis->iDevice <= pThis->g_iDeviceNum) {
        TRACE("Info_proc:idevice =%d\n",pThis->iDevice);
        pThis->OneDeviceInfo(pThis->iDevice++);
    //}
#endif
    return 0;
}
void CNuWriterDlg::GetInfo()
{
    unsigned Thread1;
    HANDLE hThread;
    hThread=(HANDLE)_beginthreadex(NULL,0,Info_proc,(void*)this,0,&Thread1);
}

BOOL CNuWriterDlg:: FWDownload(int id)
{
    int i=0;
    TCHAR path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    CString temp=path;
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    BOOL bResult=1;

    COLORREF col = RGB(0xFF, 0x00, 0xFF);
    m_reconnect.setBitmapId(IDB_RECONNECT0, col);
    m_reconnect.setGradient(true);
    m_exit.setBitmapId(IDB_EXIT, col);
    m_exit.setGradient(true);

    temp = temp.Left(temp.ReverseFind('\\') + 1);
    CString filename=NULL;

    switch(mainWnd->DDRFileName.GetAt(8)) {
    case '5':
        filename.Format(_T("%sxusb.bin"),temp);
        break;
    case '6':
        filename.Format(_T("%sxusb64.bin"),temp);
        break;
    case '7':
        filename.Format(_T("%sxusb128.bin"),temp);
        break;
    default:
        filename.Format(_T("%sxusb16.bin"),temp);
        break;
    };

    bResult = XUSB(id, filename);

    if(!bResult) {
        ShowDeviceConnectState(0);//Disconnected
        GetDlgItem(IDC_RECONNECT)->EnableWindow(TRUE);
        NucUsb.CloseWinUsbDevice(id);
    }

    return bResult;
}

DWORD GetFWRamAddress(FILE* fp)
{
    BINHEADER head;
    UCHAR SIGNATURE[]= {'W','B',0x5A,0xA5};

    fread((CHAR*)&head,sizeof(head),1,fp);

    if(head.signature==*(DWORD*)SIGNATURE) {
        return head.address;
    } else
        return 0xFFFFFFFF;
}

BOOL CNuWriterDlg::DDRtoDevice(int id, char *buf,unsigned int len)
{
    BOOL bResult;
    char *pbuf;
    unsigned int scnt,rcnt,ack;
    AUTOTYPEHEAD head;

    head.address=DDRAddress;  // 0x10
    head.filelen=len;   // *.ini length = 384

    //TRACE(_T("(%d)  0x%x,  %d\n"), id, head.address, head.filelen);
    bResult=NucUsb.NUC_WritePipe(id,(unsigned char*)&head,sizeof(AUTOTYPEHEAD));
    if(bResult==FALSE) {
        TRACE(_T("XXX (%d) CNuWriterDlg::DDRtoDevice error.  0x%x\n"), id, GetLastError());
        goto failed;
    }
    Sleep(5);
    pbuf=buf;

    scnt=len/BUF_SIZE;
    rcnt=len%BUF_SIZE;
    while(scnt>0) {
        bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf,BUF_SIZE);
        if(bResult==TRUE) {
            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);

            if(bResult==FALSE || (int)ack==(BUF_SIZE+1)) {

                if(bResult==TRUE && (int)ack==(BUF_SIZE+1)) {
                    // xub.bin is running on device
                    NucUsb.CloseWinUsbDevice(id);
                    return FW_CODE;
                } else
                    goto failed;
            }
        } else {
            NucUsb.CloseWinUsbDevice(id);
            //TRACE(_T("(%d) DDRtoDevice scnt%d error! 0x%x\n"), id, scnt, GetLastError());
            return FALSE;
        }

        scnt--;
        pbuf+=BUF_SIZE;
    }

    if(rcnt>0) {
        bResult=NucUsb.NUC_WritePipe(id,(UCHAR *)pbuf,rcnt);
        if(bResult==TRUE) {
            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
            if(bResult==FALSE || (int)ack==(BUF_SIZE+1)) {

                if(bResult==TRUE && (int)ack==(BUF_SIZE+1)) {
                    // xub.bin is running on device
                    NucUsb.CloseWinUsbDevice(id);
                    return FW_CODE;
                } else
                    goto failed;
            }
        } else {
            NucUsb.CloseWinUsbDevice(id);
            //TRACE(_T("XXX (%d) DDRtoDevice rcnt%d error! 0x%x\n"), id, rcnt, GetLastError());
            return FALSE;
        }
    }

    bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
    if(bResult==TRUE && ack==0x55AA55AA)
        return TRUE;

failed:
    NucUsb.CloseWinUsbDevice(id);
    return FALSE;
}

// FW bin download
BOOL CNuWriterDlg::XUSB(int id, CString& m_BinName)
{
    BOOL bResult;
    CString posstr, str;
    CString tempstr;
    int count=0;
    FILE* fp;
    int pos=0;
    AUTOTYPEHEAD fhead;
    XBINHEAD xbinhead;  // 16bytes for IBR using
    DWORD version;
    unsigned int total,file_len,scnt,rcnt,ack;

    /***********************************************/
    //TRACE(_T("CNuWriterDlg::XUSB (%d), %s\n"), id, m_BinName);
    bResult=NucUsb.EnableOneWinUsbDevice(id);
    if(!bResult) {
        TRACE(_T("XXX (%d) NuWriterDlg.cpp  XUSB Device Open error\n"),id);
        //AfxMessageBox(str);
        return FALSE;
    }

    int fw_flag[8];
    fw_flag[id]=DDRtoDevice(id, ShareDDRBuf,DDRLen);

    if(fw_flag[id]==FALSE) {
        NucUsb.WinUsbNumber -= 1;
        NucUsb.CloseWinUsbDevice(id);
        return FALSE;
    }

    ULONG cbSize = 0;
    unsigned char* lpBuffer = new unsigned char[BUF_SIZE];
    fp=_wfopen(m_BinName,_T("rb"));

    if(!fp) {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        AfxMessageBox(_T("Bin File Open error\n"));
        //TRACE(_T("XXX Bin File Open error\n"));
        return FALSE;
    }

    fread((char*)&xbinhead,sizeof(XBINHEAD),1,fp);
    version=xbinhead.version;

    if(fw_flag[id]==FW_CODE) {
        fclose(fp);
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        return TRUE;
    }

    //Get File Length
    fseek(fp,0,SEEK_END);
    file_len=ftell(fp)-sizeof(XBINHEAD);
    fseek(fp,0,SEEK_SET);

    if(!file_len) {
        fclose(fp);
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        TRACE(_T("File length is zero\n"));
        return FALSE;
    }

    fhead.filelen = file_len;
    fhead.address = GetFWRamAddress(fp);//0x8000;

    if(fhead.address==0xFFFFFFFF) {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        TRACE(_T("Invalid Image !\n"));
        return FALSE;
    }

    memcpy(lpBuffer,(unsigned char*)&fhead,sizeof(fhead)); // 8 bytes
    bResult=NucUsb.NUC_WritePipe(id,(unsigned char*)&fhead,sizeof(fhead));
    if(bResult==FALSE) {
        delete []lpBuffer;
        NucUsb.CloseWinUsbDevice(id);
        fclose(fp);
        return FALSE;
    }
    scnt=file_len/BUF_SIZE;
    rcnt=file_len%BUF_SIZE;

    total=0;
    //TRACE(_T("(%d) FW scnt %d    rcnt = %d\n"), id, scnt, rcnt);
    while(scnt>0) {
        fread(lpBuffer,BUF_SIZE,1,fp);
        bResult=NucUsb.NUC_WritePipe(id,lpBuffer,BUF_SIZE);
        if(bResult==TRUE) {
            total+=BUF_SIZE;

            scnt--;

            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
            if(bResult==FALSE || (int)ack!=BUF_SIZE) {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);

                if(bResult==TRUE && (int)ack==(BUF_SIZE+1)) {
                    // xub.bin is running on device
                    return TRUE;
                } else
                    return FALSE;
            }
        } else {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            fclose(fp);
            //TRACE(_T("XXX (%d) FW scnt error. 0x%x\n"), id, GetLastError());
            return FALSE;
        }
    }

    memset(lpBuffer,0x0,BUF_SIZE);
    if(rcnt>0) {
        fread(lpBuffer,rcnt,1,fp);
        bResult=NucUsb.NUC_WritePipe(id,lpBuffer,BUF_SIZE);
        if(bResult==TRUE) {
            total+=rcnt;
            bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)&ack,4);
            if(bResult==FALSE) {
                delete []lpBuffer;
                NucUsb.CloseWinUsbDevice(id);
                fclose(fp);
                return FALSE;
            }
        } else {
            delete []lpBuffer;
            NucUsb.CloseWinUsbDevice(id);
            fclose(fp);
            //TRACE(_T("XXX (%d) FW rcnt error. 0x%x\n"), id, GetLastError());
            return FALSE;
        }
    }

    delete []lpBuffer;
    NucUsb.CloseWinUsbDevice(id);
    fclose(fp);

    return TRUE;
}

#define CRC32POLY 0x04c11db7
static const unsigned long crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
    0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
    0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
    0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
    0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
    0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
    0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
    0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
    0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
    0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
    0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
    0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
    0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
    0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
    0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
    0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
    0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
    0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
    0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
    0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
    0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
    0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
    0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
    0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
    0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
    0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
    0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
    0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
    0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
    0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
    0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
    0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
    0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
    0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
    0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
    0x2d02ef8d
};

unsigned int CNuWriterDlg::CalculateCRC32(unsigned char * buf,unsigned int len)
{
#if 0
    unsigned char *end;
    unsigned int crc=CRC32POLY;
    crc = ~crc;
    for (end = buf + len; buf < end; ++buf)
        crc = crc32_table[(crc ^ *buf) & 0xff] ^ (crc >> 8);
    return ~crc;
#else
    unsigned int i;
    unsigned int crc32;
    unsigned char* byteBuf;
    crc32 = 0 ^ 0xFFFFFFFF;
    byteBuf = (unsigned char *) buf;
    for (i=0; i < len; i++) {
        crc32 = (crc32 >> 8) ^ crc32_table[ (crc32 ^ byteBuf[i]) & 0xFF ];
    }
    return ( crc32 ^ 0xFFFFFFFF );
#endif
}

//---------------------------------------------------------------------------------------------
