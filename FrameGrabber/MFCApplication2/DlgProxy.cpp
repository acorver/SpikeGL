
// DlgProxy.cpp : implementation file
//

#include "stdafx.h"
#include "MFCApplication2.h"
#include "DlgProxy.h"
#include "MFCApplication2Dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// MEAControlDlgAutoProxy

IMPLEMENT_DYNCREATE(MEAControlDlgAutoProxy, CCmdTarget)

MEAControlDlgAutoProxy::MEAControlDlgAutoProxy()
{
	EnableAutomation();
	
	// To keep the application running as long as an automation 
	//	object is active, the constructor calls AfxOleLockApp.
	AfxOleLockApp();

	// Get access to the dialog through the application's
	//  main window pointer.  Set the proxy's internal pointer
	//  to point to the dialog, and set the dialog's back pointer to
	//  this proxy.
	ASSERT_VALID(AfxGetApp()->m_pMainWnd);
	if (AfxGetApp()->m_pMainWnd)
	{
		ASSERT_KINDOF(MEAControlDlg, AfxGetApp()->m_pMainWnd);
		if (AfxGetApp()->m_pMainWnd->IsKindOf(RUNTIME_CLASS(MEAControlDlg)))
		{
			m_pDialog = reinterpret_cast<MEAControlDlg*>(AfxGetApp()->m_pMainWnd);
			m_pDialog->m_pAutoProxy = this;
		}
	}
}

MEAControlDlgAutoProxy::~MEAControlDlgAutoProxy()
{
	// To terminate the application when all objects created with
	// 	with automation, the destructor calls AfxOleUnlockApp.
	//  Among other things, this will destroy the main dialog
	if (m_pDialog != NULL)
		m_pDialog->m_pAutoProxy = NULL;
	AfxOleUnlockApp();
}

void MEAControlDlgAutoProxy::OnFinalRelease()
{
	// When the last reference for an automation object is released
	// OnFinalRelease is called.  The base class will automatically
	// deletes the object.  Add additional cleanup required for your
	// object before calling the base class.

	CCmdTarget::OnFinalRelease();
}

BEGIN_MESSAGE_MAP(MEAControlDlgAutoProxy, CCmdTarget)
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(MEAControlDlgAutoProxy, CCmdTarget)
END_DISPATCH_MAP()

// Note: we add support for IID_IMFCApplication2 to support typesafe binding
//  from VBA.  This IID must match the GUID that is attached to the 
//  dispinterface in the .IDL file.

// {04C4B1CF-A11B-44B3-912D-CD6E3C3CD139}
static const IID IID_IMFCApplication2 =
{ 0x4C4B1CF, 0xA11B, 0x44B3, { 0x91, 0x2D, 0xCD, 0x6E, 0x3C, 0x3C, 0xD1, 0x39 } };

BEGIN_INTERFACE_MAP(MEAControlDlgAutoProxy, CCmdTarget)
	INTERFACE_PART(MEAControlDlgAutoProxy, IID_IMFCApplication2, Dispatch)
END_INTERFACE_MAP()

// The IMPLEMENT_OLECREATE2 macro is defined in StdAfx.h of this project
// {3DE44035-086A-4983-9CC8-FA829C918770}
IMPLEMENT_OLECREATE2(MEAControlDlgAutoProxy, "MFCApplication2.Application", 0x3de44035, 0x86a, 0x4983, 0x9c, 0xc8, 0xfa, 0x82, 0x9c, 0x91, 0x87, 0x70)


// MEAControlDlgAutoProxy message handlers
