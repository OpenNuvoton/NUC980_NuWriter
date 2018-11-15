// SPINandInfoDlg.cpp
//

#include "stdafx.h"
#include "NuWriter.h"
#include "NuWriterDlg.h"
#include "SPINandInfoDlg.h"
#include "afxdialogex.h"


// CSPINandInfoDlg

IMPLEMENT_DYNAMIC(CSPINandInfoDlg, CDialog)

CSPINandInfoDlg::CSPINandInfoDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CSPINandInfoDlg::IDD, pParent)
    , m_pagesize(_T(""))
    , m_sparearea(_T(""))
    , m_quadreadcmd(_T(""))
    , m_readstatuscmd(_T(""))
    , m_writestatuscmd(_T(""))
    , m_statusvalue(_T(""))
    , m_dummybyte(_T(""))
    , m_pageperblock(_T(""))
    , m_blockperflash(_T(""))
{

}

CSPINandInfoDlg::~CSPINandInfoDlg()
{
}

void CSPINandInfoDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT_SPINANDINFO1, m_pagesize);
    DDV_MaxChars(pDX, m_pagesize, 8);
    DDX_Text(pDX, IDC_EDIT_SPINANDINFO2, m_sparearea);
    DDX_Text(pDX, IDC_EDIT_SPINANDINFO3, m_quadreadcmd);
    DDV_MaxChars(pDX, m_quadreadcmd, 8);
    DDX_Text(pDX, IDC_EDIT_SPINANDINFO4, m_readstatuscmd);
    DDV_MaxChars(pDX, m_readstatuscmd, 8);
    DDX_Text(pDX, IDC_EDIT_SPINANDINFO5, m_writestatuscmd);
    DDV_MaxChars(pDX, m_writestatuscmd, 8);
    DDX_Text(pDX, IDC_EDIT_SPINANDINFO6, m_statusvalue);
    DDV_MaxChars(pDX, m_statusvalue, 8);
    DDX_Text(pDX, IDC_EDIT_SPINANDINFO7, m_dummybyte);
    DDV_MaxChars(pDX, m_dummybyte, 8);
    DDX_Text(pDX, IDC_EDIT_SPINANDINFO9, m_pageperblock);
    DDV_MaxChars(pDX, m_pageperblock, 8);
    DDX_Text(pDX, IDC_EDIT_SPINANDINFO8, m_blockperflash);
    DDV_MaxChars(pDX, m_blockperflash, 8);
}


BEGIN_MESSAGE_MAP(CSPINandInfoDlg, CDialog)
    ON_BN_CLICKED(IDOK, &CSPINandInfoDlg::OnBnClickedOk)
    ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


// CSPINandInfoDlg


void CSPINandInfoDlg::OnBnClickedOk()
{
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    CString str;
    int num;

    mainWnd->m_inifile.ReadFile();
    GetDlgItem(IDC_EDIT_SPINANDINFO1)->GetWindowText(str);
    str.Format(_T("%d"),_wtoi(str));
    mainWnd->m_inifile.SetValue(_T("SPINAND_INFO"),_T("PageSize"),str);
    mainWnd->m_inifile.WriteFile();

    GetDlgItem(IDC_EDIT_SPINANDINFO2)->GetWindowText(str);
    str.Format(_T("%d"),_wtoi(str));
    mainWnd->m_inifile.SetValue(_T("SPINAND_INFO"),_T("SpareArea"),str);
    mainWnd->m_inifile.WriteFile();

    GetDlgItem(IDC_EDIT_SPINANDINFO3)->GetWindowText(str);
    if(str.GetLength() > 2)
    {
        str= _T("ff");
    }
    mainWnd->m_inifile.SetValue(_T("SPINAND_INFO"),_T("QuadReadCmd"),str);
    mainWnd->m_inifile.WriteFile();

    GetDlgItem(IDC_EDIT_SPINANDINFO4)->GetWindowText(str);
    if(str.GetLength() > 2)
    {
        str= _T("ff");
    }
    mainWnd->m_inifile.SetValue(_T("SPINAND_INFO"),_T("ReadStatusCmd"),str);
    mainWnd->m_inifile.WriteFile();

    GetDlgItem(IDC_EDIT_SPINANDINFO5)->GetWindowText(str);
    if(str.GetLength() > 2)
    {
        str= _T("ff");
    }
    mainWnd->m_inifile.SetValue(_T("SPINAND_INFO"),_T("WriteStatusCmd"),str);
    mainWnd->m_inifile.WriteFile();

    GetDlgItem(IDC_EDIT_SPINANDINFO6)->GetWindowText(str);
    if(str.GetLength() > 2)
    {
        str= _T("ff");
    }
    mainWnd->m_inifile.SetValue(_T("SPINAND_INFO"),_T("StatusValue"),str);
    mainWnd->m_inifile.WriteFile();

    GetDlgItem(IDC_EDIT_SPINANDINFO7)->GetWindowText(str);
    num =_wtoi(str);
    //if(num > 1)
    //  num = 1;
    str.Format(_T("%d"),num);
    mainWnd->m_inifile.SetValue(_T("SPINAND_INFO"),_T("dummybyte"),str);
    mainWnd->m_inifile.WriteFile();

    GetDlgItem(IDC_EDIT_SPINANDINFO8)->GetWindowText(str);
    str.Format(_T("%d"),_wtoi(str));
    mainWnd->m_inifile.SetValue(_T("SPINAND_INFO"),_T("BlockPerFlash"),str);
    mainWnd->m_inifile.WriteFile();

    GetDlgItem(IDC_EDIT_SPINANDINFO9)->GetWindowText(str);
    str.Format(_T("%d"),_wtoi(str));
    mainWnd->m_inifile.SetValue(_T("SPINAND_INFO"),_T("PagePerBlock"),str);
    mainWnd->m_inifile.WriteFile();


    mainWnd->m_info.SPINand_PageSize = _wtoi(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("PageSize")));
    mainWnd->m_info.SPINand_SpareArea = _wtoi(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("SpareArea")));

    mainWnd->m_info.SPINand_QuadReadCmd = mainWnd->Str2Hex(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("QuadReadCmd")));
    mainWnd->m_info.SPINand_ReadStatusCmd = mainWnd->Str2Hex(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("ReadStatusCmd")));
    mainWnd->m_info.SPINand_WriteStatusCmd = mainWnd->Str2Hex(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("WriteStatusCmd")));
    mainWnd->m_info.SPINand_dummybyte = mainWnd->Str2Hex(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("dummybyte")));
    mainWnd->m_info.SPINand_StatusValue = mainWnd->Str2Hex(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("StatusValue")));

    mainWnd->m_info.SPINand_BlockPerFlash=_wtoi(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("BlockPerFlash")));
    mainWnd->m_info.SPINand_PagePerBlock=_wtoi(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("PagePerBlock")));

    //mainWnd->OneDeviceInfo(0);
    CDialog::OnOK();
}



void CSPINandInfoDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CDialog::OnShowWindow(bShow, nStatus);

}

BOOL CSPINandInfoDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    CString str;

    mainWnd->m_inifile.ReadFile();
    str.Format(_T("%d"),_wtoi(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("PageSize"))));
    GetDlgItem(IDC_EDIT_SPINANDINFO1)->SetWindowText(str);
    str.Format(_T("%d"),_wtoi(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("SpareArea"))));
    GetDlgItem(IDC_EDIT_SPINANDINFO2)->SetWindowText(str);
    str.Format(_T("%s"),mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("QuadReadCmd")));
    GetDlgItem(IDC_EDIT_SPINANDINFO3)->SetWindowText(str);
    str.Format(_T("%s"),mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("ReadStatusCmd")));
    GetDlgItem(IDC_EDIT_SPINANDINFO4)->SetWindowText(str);
    str.Format(_T("%s"),mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("WriteStatusCmd")));
    GetDlgItem(IDC_EDIT_SPINANDINFO5)->SetWindowText(str);
    str.Format(_T("%s"),mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("StatusValue")));
    GetDlgItem(IDC_EDIT_SPINANDINFO6)->SetWindowText(str);
    str.Format(_T("%d"),_wtoi(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("dummybyte"))));
    GetDlgItem(IDC_EDIT_SPINANDINFO7)->SetWindowText(str);
    str.Format(_T("%d"),_wtoi(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("BlockPerFlash"))));
    GetDlgItem(IDC_EDIT_SPINANDINFO8)->SetWindowText(str);
    str.Format(_T("%d"),_wtoi(mainWnd->m_inifile.GetValue(_T("SPINAND_INFO"),_T("PagePerBlock"))));
    GetDlgItem(IDC_EDIT_SPINANDINFO9)->SetWindowText(str);

    return TRUE;  // return TRUE unless you set the focus to a control
}
