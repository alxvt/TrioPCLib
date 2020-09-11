// TrioPC_DLL.h : main header file for the TrioPC_DLL DLL
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CTrioPC_DLLApp
// See TrioPC_DLL.cpp for the implementation of this class
//

class CTrioPC_DLLApp : public CWinApp
{
public:
	CTrioPC_DLLApp();
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
