/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

#include "stdafx.h"

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "Usb.h"
#include "winioctl.h"
#include "TrioIOCTL.h"
#include "ascii.h"

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CUsb::CUsb()
	: m_remotePort(INVALID_HANDLE_VALUE)
	, m_mpePort(INVALID_HANDLE_VALUE)
	, m_driverInstance(-1)
{
}

CUsb::~CUsb()
{
    // close the port
    Close();
}

void CUsb::SetInterfaceUSB(int p_driverInstance) {
	m_driverInstance = p_driverInstance;
}

BOOL CUsb::OpenUSB(const char *p_mode, HANDLE &p_port) {
    int nConfiguration = 1, driverInstance;
	DWORD dwBytesReturned;
	CString driverInstanceName;

	// try all controllers
	p_port = INVALID_HANDLE_VALUE;
	for (driverInstance = 0; (INVALID_HANDLE_VALUE == p_port) && (driverInstance < 1000); driverInstance++) {
		// select the driver for this mode
		driverInstanceName.Format(_T("\\\\.\\Trio-%03i\\%s"), driverInstance, p_mode);

		// open the file
		p_port = CreateFile(driverInstanceName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	// if the port did not open then fail
	if(INVALID_HANDLE_VALUE == p_port) {
		SetLastErrorMessage(GetLastError());
        return FALSE;
	}

    // if the comnfiguration does not select then fail
    //if(!DeviceIoControl(m_port,IOCTL_TRIO_SELECT_CONFIGURATION,&nConfiguration,sizeof(nConfiguration),NULL,0,&dwBytesReturned,NULL))
    //  return FALSE;
    // THIS IS A FRIG!!!!! ######################
    // If MP2 is open on the USB port then the select will fail as the driver is already open. There should be a change to
    // the USB Driver to check the configuration being selected against the one already selected and if they are the same
    // then return ok
    DeviceIoControl(p_port,IOCTL_TRIO_SELECT_CONFIGURATION,&nConfiguration,sizeof(nConfiguration),NULL,0,&dwBytesReturned,NULL);

    // return successful
    return TRUE;
}

void CUsb::CloseUSB(HANDLE &p_port) {
    // if the port is open then close it
    if(INVALID_HANDLE_VALUE != p_port) {
        // close the handle
        CloseHandle(p_port);

        // invalidate port
        p_port = INVALID_HANDLE_VALUE;
    }
}

BOOL CUsb::WriteUSB(HANDLE p_port, const char *p_buffer, size_t p_bufferSize) {
    // local data
    DWORD dwBytesWritten;
    const int nBlockSize = 32;
    BOOL bRetval = TRUE;

    // send the blocks
	for(dwBytesWritten = 0;bRetval && (dwBytesWritten < p_bufferSize);dwBytesWritten += nBlockSize) {
	    bRetval = WriteBlock(p_port, &p_buffer[dwBytesWritten],(DWORD)min(p_bufferSize - dwBytesWritten,nBlockSize));
	}

    // return the success status
    return bRetval;
}

BOOL CUsb::WriteBlock(HANDLE p_port, const char *p_buffer, DWORD p_bufferSize) {
    // local data
    BOOL    bWrite = FALSE;
    DWORD   dwBytesWritten;

	// write the data out
	bWrite = WriteFile(p_port, p_buffer, p_bufferSize, &dwBytesWritten, NULL);
	if (!bWrite) SetLastErrorMessage(GetLastError());

    // return successful
    return bWrite && (dwBytesWritten == p_bufferSize);
}

BOOL CUsb::ReadUSB(HANDLE p_port, char *p_buffer, size_t *p_bufferSize) {
    // local data
    BOOL    bRead = FALSE;
    DWORD   dwBytesRead = 0;

	// we must have a 32 byte buffer otherwise the USB stack crashes
	ASSERT(*p_bufferSize == 32);

	// read the port
	bRead = ReadFile(p_port, p_buffer, (DWORD)*p_bufferSize, &dwBytesRead, NULL);
	if (!bRead) {
		SetLastErrorMessage(GetLastError());
		*p_bufferSize = 0;
	}
	else {
		*p_bufferSize = dwBytesRead;
	}

    // return success
    return bRead;
}
