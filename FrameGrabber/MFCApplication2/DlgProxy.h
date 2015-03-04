
// DlgProxy.h: header file
//

#pragma once

class MEAControlDlg;


// MEAControlDlgAutoProxy command target

class MEAControlDlgAutoProxy : public CCmdTarget
{
	DECLARE_DYNCREATE(MEAControlDlgAutoProxy)

	MEAControlDlgAutoProxy();           // protected constructor used by dynamic creation

// Attributes
public:
	MEAControlDlg* m_pDialog;

// Operations
public:

// Overrides
	public:
	virtual void OnFinalRelease();

// Implementation
protected:
	virtual ~MEAControlDlgAutoProxy();

	// Generated message map functions

	DECLARE_MESSAGE_MAP()
	DECLARE_OLECREATE(MEAControlDlgAutoProxy)

	// Generated OLE dispatch map functions

	DECLARE_DISPATCH_MAP()
	DECLARE_INTERFACE_MAP()
};

