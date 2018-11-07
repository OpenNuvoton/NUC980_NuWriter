#pragma once
#include "afxwin.h"
#include "afxcmn.h"

// CSPINandDlg 對話方塊

class CSPINandDlg : public CDialog
{
	DECLARE_DYNAMIC(CSPINandDlg)

public:
	CSPINandDlg(CWnd* pParent = NULL);   // 標準建構函式
	virtual ~CSPINandDlg();

// 對話方塊資料
	enum { IDD = IDD_SPINAND };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支援
	LRESULT ShowStatus( WPARAM  pos, LPARAM message);

	DECLARE_MESSAGE_MAP()
public:
	CString m_imagename;
    CReportCtrl	m_imagelist;
	CString m_startblock;
	CString m_execaddr;
	CProgressCtrl	m_progress;
	FooButton m_burn;
	FooButton m_verify;
	FooButton m_read;
	FooButton m_eraseall;
	FooButton m_browse;
	FooButton m_info;
	int m_type;
	CStatic m_status;
	CString m_filename;
	CString m_filename2;
	HANDLE m_ExitEvent;
	int InitFlag;
	NORBOOT_NAND_HEAD *m_fhead;
	SPINAND_INFO_T m_spinandinfo;
    CString TmpOffset;
	int TmpOffsetFlag;
	CString m_blocks;
	CString m_sblocks;
	int m_erase_flag;

	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnBnClickedSpinandInfo();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	virtual BOOL OnInitDialog();
	BOOL InitFile(int flag);

	afx_msg void OnBnClickedSpinandDownload();
	static unsigned WINAPI Download_proc(void* args);
	void Download();	
	BOOL XUSB_Burn(CString& portName,CString& m_pathName,int *len,int *blockNum);

	int XUSB_Verify(CString& portName,CString& m_pathName);
	static unsigned WINAPI Verify_proc(void* args);
	void Verify();

	BOOL XUSB_Erase(CString& portName);
	static unsigned WINAPI Erase_proc(void* args);
	void Erase();

	BOOL XUSB_Pack(CString& portName,CString& m_pathName,int *len);

	BOOL XUSB_Read(CString& portName,CString& m_pathName,unsigned int addr,unsigned int len);
	BOOL XUSB_Read_Redunancy(CString& portName,CString& m_pathName,unsigned int addr,unsigned int len);
	static unsigned WINAPI Read_proc(void* args);
	void Read();

	afx_msg void OnBnClickedSpinandVerify();
	afx_msg void OnBnClickedSpinandBrowse();
	afx_msg void OnBnClickedSpinandEraseall();
	afx_msg void OnBnClickedSpinandRead();
	afx_msg void OnBnClickedSpinandUsrconfig();
	CButton m_spinandflash_check;
};
