#pragma once
#include "afxwin.h"
#include "afxcmn.h"

// CSPINandInfoDlg ��ܤ��

class CSPINandInfoDlg : public CDialog
{
	DECLARE_DYNAMIC(CSPINandInfoDlg)

public:
	CSPINandInfoDlg(CWnd* pParent = NULL);   // �зǫغc�禡
	virtual ~CSPINandInfoDlg();

// ��ܤ�����
	enum { IDD = IDD_SPINANDINFO };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �䴩

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	CString m_pagesize;
	CString m_sparearea;
	CString m_quadreadcmd;
	CString m_readstatuscmd;
	CString m_writestatuscmd;
	CString m_statusvalue;
	CString m_dummybyte;
	CString m_pageperblock;
	CString m_blockperflash;
	virtual BOOL OnInitDialog();
};
