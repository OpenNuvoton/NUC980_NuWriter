// SPINORInfo.cpp : 實作檔
//

#include "stdafx.h"
#include "NuWriter.h"
#include "NuWriterDlg.h"
#include "SPINORInfo.h"
#include "afxdialogex.h"


// SPINORInfo 對話方塊

IMPLEMENT_DYNAMIC(SPINORInfo, CDialogEx)

SPINORInfo::SPINORInfo(CWnd* pParent /*=NULL*/)
	: CDialogEx(SPINORInfo::IDD, pParent)
	, m_quadreadcmd(_T(""))
	, m_readstatuscmd(_T(""))
	, m_writestatuscmd(_T(""))
	, m_statusvalue(_T(""))
	, m_dummybyte(_T(""))
{

}

SPINORInfo::~SPINORInfo()
{
}

void SPINORInfo::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_EDIT_SPIINFO1, m_quadreadcmd);
	DDV_MaxChars(pDX, m_quadreadcmd, 2);
	DDX_Text(pDX, IDC_EDIT_SPIINFO2, m_readstatuscmd);
	DDV_MaxChars(pDX, m_readstatuscmd, 2);
	DDX_Text(pDX, IDC_EDIT_SPIINFO3, m_writestatuscmd);
	DDV_MaxChars(pDX, m_writestatuscmd, 2);
	DDX_Text(pDX, IDC_EDIT_SPIINFO4, m_statusvalue);
	DDV_MaxChars(pDX, m_statusvalue, 2);
	DDX_Text(pDX, IDC_EDIT_SPIINFO5, m_dummybyte);
	DDV_MaxChars(pDX, m_dummybyte, 2);
}


BEGIN_MESSAGE_MAP(SPINORInfo, CDialogEx)
	ON_BN_CLICKED(IDOK, &SPINORInfo::OnBnClickedOk)
END_MESSAGE_MAP()


// SPINORInfo 訊息處理常式


void SPINORInfo::OnBnClickedOk()
{
	// TODO: 在此加入控制項告知處理常式程式碼
	CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
	CString str;
	int num;

	GetDlgItem(IDC_EDIT_SPIINFO1)->GetWindowText(str);
    mainWnd->m_inifile.SetValue(_T("SPI_INFO"),_T("QuadReadCmd"),str);
    mainWnd->m_inifile.WriteFile();

	GetDlgItem(IDC_EDIT_SPIINFO2)->GetWindowText(str);
    mainWnd->m_inifile.SetValue(_T("SPI_INFO"),_T("ReadStatusCmd"),str);
	mainWnd->m_inifile.WriteFile();	

	GetDlgItem(IDC_EDIT_SPIINFO3)->GetWindowText(str);    
    mainWnd->m_inifile.SetValue(_T("SPI_INFO"),_T("WriteStatusCmd"),str);
	mainWnd->m_inifile.WriteFile();

	GetDlgItem(IDC_EDIT_SPIINFO4)->GetWindowText(str);
    mainWnd->m_inifile.SetValue(_T("SPI_INFO"),_T("StatusValue"),str);
	mainWnd->m_inifile.WriteFile();

	GetDlgItem(IDC_EDIT_SPIINFO5)->GetWindowText(str);	
	num =_wtoi(str);
	str.Format(_T("%d"),num);
    mainWnd->m_inifile.SetValue(_T("SPI_INFO"),_T("dummybyte"),str);
	mainWnd->m_inifile.WriteFile();
	

	mainWnd->m_info.SPI_QuadReadCmd = mainWnd->Str2Hex(mainWnd->m_inifile.GetValue(_T("SPI_INFO"),_T("QuadReadCmd")));
	mainWnd->m_info.SPI_ReadStatusCmd = mainWnd->Str2Hex(mainWnd->m_inifile.GetValue(_T("SPI_INFO"),_T("ReadStatusCmd")));
	mainWnd->m_info.SPI_WriteStatusCmd = mainWnd->Str2Hex(mainWnd->m_inifile.GetValue(_T("SPI_INFO"),_T("WriteStatusCmd")));
	mainWnd->m_info.SPI_dummybyte = mainWnd->Str2Hex(mainWnd->m_inifile.GetValue(_T("SPI_INFO"),_T("dummybyte")));
	mainWnd->m_info.SPI_StatusValue = mainWnd->Str2Hex(mainWnd->m_inifile.GetValue(_T("SPI_INFO"),_T("StatusValue")));

	CDialogEx::OnOK();
}


BOOL SPINORInfo::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  在此加入額外的初始化
	CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
	CString str;

	mainWnd->m_inifile.ReadFile();
	
	str.Format(_T("%s"),mainWnd->m_inifile.GetValue(_T("SPI_INFO"),_T("QuadReadCmd")));
    GetDlgItem(IDC_EDIT_SPIINFO1)->SetWindowText(str);
	str.Format(_T("%s"),mainWnd->m_inifile.GetValue(_T("SPI_INFO"),_T("ReadStatusCmd")));
    GetDlgItem(IDC_EDIT_SPIINFO2)->SetWindowText(str);
	str.Format(_T("%s"),mainWnd->m_inifile.GetValue(_T("SPI_INFO"),_T("WriteStatusCmd")));
    GetDlgItem(IDC_EDIT_SPIINFO3)->SetWindowText(str);
	str.Format(_T("%s"),mainWnd->m_inifile.GetValue(_T("SPI_INFO"),_T("StatusValue")));
    GetDlgItem(IDC_EDIT_SPIINFO4)->SetWindowText(str);
	str.Format(_T("%d"),_wtoi(mainWnd->m_inifile.GetValue(_T("SPI_INFO"),_T("dummybyte"))));
    GetDlgItem(IDC_EDIT_SPIINFO5)->SetWindowText(str);


	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX 屬性頁應傳回 FALSE
}
