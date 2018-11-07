#pragma once
#include "afxwin.h"


// SPINORInfo 對話方塊

class SPINORInfo : public CDialogEx
{
	DECLARE_DYNAMIC(SPINORInfo)

public:
	SPINORInfo(CWnd* pParent = NULL);   // 標準建構函式
	virtual ~SPINORInfo();

// 對話方塊資料
	enum { IDD = IDD_SPIINFO };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支援

	DECLARE_MESSAGE_MAP()
public:

	afx_msg void OnBnClickedOk();
	CString m_quadreadcmd;
	CString m_readstatuscmd;
	CString m_writestatuscmd;
	CString m_statusvalue;
	CString m_dummybyte;
	virtual BOOL OnInitDialog();
};
