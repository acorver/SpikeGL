
// MFCApplication2.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// MEAControl:
// See MFCApplication2.cpp for the implementation of this class
//

class MEAControl : public CWinApp
{
public:
	MEAControl();

// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern MEAControl theApp;

extern BOOL SpikeGL_Mode;
