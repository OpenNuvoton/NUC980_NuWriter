// NandInfoDlg.cpp
//

#include "stdafx.h"
#include "NuWriter.h"
#include "NuWriterDlg.h"
#include "NandInfoDlg.h"
#include "afxdialogex.h"


// CNandInfoDlg

IMPLEMENT_DYNAMIC(CNandInfoDlg, CDialogEx)

CNandInfoDlg::CNandInfoDlg(CWnd* pParent /*=NULL*/)
    : CDialogEx(CNandInfoDlg::IDD, pParent)
    , m_BlockPerFlash(_T(""))
    , m_PagePerBlock(_T(""))
{

}

CNandInfoDlg::~CNandInfoDlg()
{
}

void CNandInfoDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT_NANDINFO1, m_BlockPerFlash);
    DDX_Text(pDX, IDC_EDIT_NANDINFO2, m_PagePerBlock);
}


BEGIN_MESSAGE_MAP(CNandInfoDlg, CDialogEx)
    ON_BN_CLICKED(IDOK, &CNandInfoDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// CNandInfoDlg


void CNandInfoDlg::OnBnClickedOk()
{
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    CString str;

    GetDlgItem(IDC_EDIT_NANDINFO1)->GetWindowText(str);
    mainWnd->m_inifile.SetValue(_T("NAND_INFO"),_T("uBlockPerFlash"),str);
    mainWnd->m_inifile.WriteFile();

    GetDlgItem(IDC_EDIT_NANDINFO2)->GetWindowText(str);
    mainWnd->m_inifile.SetValue(_T("NAND_INFO"),_T("uPagePerBlock"),str);
    mainWnd->m_inifile.WriteFile();
    BlockPerFlash = _wtoi(mainWnd->m_inifile.GetValue(_T("NAND_INFO"),_T("uBlockPerFlash")));
    PagePerBlock = _wtoi(mainWnd->m_inifile.GetValue(_T("NAND_INFO"),_T("uPagePerBlock")));

    mainWnd->m_info.Nand_uBlockPerFlash=_wtoi(mainWnd->m_inifile.GetValue(_T("NAND_INFO"),_T("uBlockPerFlash")));
    mainWnd->m_info.Nand_uPagePerBlock=_wtoi(mainWnd->m_inifile.GetValue(_T("NAND_INFO"),_T("uPagePerBlock")));

    CDialogEx::OnOK();
}


BOOL CNandInfoDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    CNuWriterDlg* mainWnd=(CNuWriterDlg*)(AfxGetApp()->m_pMainWnd);
    CString str;

    mainWnd->m_inifile.ReadFile();

    str.Format(_T("%d"),_wtoi(mainWnd->m_inifile.GetValue(_T("NAND_INFO"),_T("uBlockPerFlash"))));
    GetDlgItem(IDC_EDIT_NANDINFO1)->SetWindowText(str);
    str.Format(_T("%d"),_wtoi(mainWnd->m_inifile.GetValue(_T("NAND_INFO"),_T("uPagePerBlock"))));
    GetDlgItem(IDC_EDIT_NANDINFO2)->SetWindowText(str);

    mainWnd->m_info.Nand_uBlockPerFlash=_wtoi(mainWnd->m_inifile.GetValue(_T("NAND_INFO"),_T("uBlockPerFlash")));
    mainWnd->m_info.Nand_uPagePerBlock=_wtoi(mainWnd->m_inifile.GetValue(_T("NAND_INFO"),_T("uPagePerBlock")));

    return TRUE;  // return TRUE unless you set the focus to a control
}
