#pragma once
#include "afxwin.h"


// SPINORInfo ��ܤ��

class SPINORInfo : public CDialogEx
{
	DECLARE_DYNAMIC(SPINORInfo)

public:
	SPINORInfo(CWnd* pParent = NULL);   // �зǫغc�禡
	virtual ~SPINORInfo();

// ��ܤ�����
	enum { IDD = IDD_SPIINFO };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �䴩

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
