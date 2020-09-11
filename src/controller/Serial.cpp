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
#include "Serial.h"
#include "ASCII.h"

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

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

CSerial::CSerial()
	: m_remotePort(INVALID_HANDLE_VALUE)
	, m_mpePort(INVALID_HANDLE_VALUE)
{
}

CSerial::~CSerial()
{
	// close the port
	Close(-1);
}

void CSerial::SetInterfaceRS232(const char *p_portName, enum port_mode_t p_portMode) {
	// create the port name
	m_portName.Format(_T("\\\\.\\%s"),p_portName);
	m_portMode = p_portMode;
}

BOOL CSerial::SetMode(enum port_mode_t p_portMode) {
	// only applicatble on MPE connection
	if (COMMUNICATIONS_CAPABILITY_MPE != m_openCapabilities) return FALSE;

	// modify serial parameters
	BOOL success = SetupConnection(m_mpePort, p_portMode);

	// if the parameters set then update the port mode
	if (success) m_portMode = p_portMode;
	// parameters did not set so go back to original mode
	else SetupConnection(m_mpePort, m_portMode);

	return success;
}

BOOL CSerial::OpenSerial(HANDLE &p_port) {
	// local data
	CString			csPortName;
	COMMTIMEOUTS	CommTimeOuts;

	// if the physical port is already open then fail
	if (m_openCapabilities) {
		SetLastErrorMessage("Serial port can only be opened once");
		return FALSE;
	}

	// open the file
	p_port = CreateFile(m_portName,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

	// if the port did not open then fail
	if(INVALID_HANDLE_VALUE == p_port) {
		SetLastErrorMessage(GetLastError());
		return FALSE;
	}

	// get any early notifications
	SetCommMask(p_port, EV_RXCHAR);

	// setup device buffers
	SetupComm(p_port,1024,1024);

	// set up for overlapped I/O
	CommTimeOuts.ReadIntervalTimeout = MAXDWORD;
	CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
	CommTimeOuts.ReadTotalTimeoutConstant = 0;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 10;
	CommTimeOuts.WriteTotalTimeoutConstant = 10;
	SetCommTimeouts(p_port,&CommTimeOuts);

	// if comms parameters don't set then fail
	if(!SetupConnection(p_port, m_portMode)) return FALSE;

	// return successful
	return TRUE;
}

BOOL CSerial::OpenRemote() {
	// we cannot accept port mode normal here!!!!!!
	if (port_mode_normal == m_portMode) {
		SetLastErrorMessage("PORT MODE CANNOT BE NORMAL!!!!");
		return FALSE;
	}

	// open the port
	return OpenSerial(m_remotePort);
}

void CSerial::CloseSerial(HANDLE &p_port)
{
	// if the port is open then close it
	if (p_port != INVALID_HANDLE_VALUE)
	{
		CloseHandle(p_port);
		p_port = INVALID_HANDLE_VALUE;
	}
}

BOOL CSerial::WriteSerial(HANDLE p_port, const char *szBuffer, size_t p_bufferSize) {
	// local data
	BOOL	bWrite;
	DWORD	dwBytesWritten;

	// write the data out
	bWrite = WriteFile(p_port,szBuffer, (DWORD)p_bufferSize,&dwBytesWritten,NULL);
	if (!bWrite) {
		SetLastErrorMessage(GetLastError());
	}

	// return the success status
	return bWrite;
}

BOOL CSerial::ReadSerial(HANDLE p_port, char *p_buffer, size_t *p_bufferSize) {
	// local data
	COMSTAT	commstat;
	DWORD error, bytesRead = 0;
	BOOL retval = TRUE;

	// if there is data to read then
	if(ClearCommError(p_port, &error, &commstat) && commstat.cbInQue) {
		retval = ReadFile(p_port, p_buffer, min((DWORD)*p_bufferSize, commstat.cbInQue), &bytesRead, NULL);
		if (!retval) {
			SetLastErrorMessage(GetLastError());
		}
	}

	// done
	*p_bufferSize = bytesRead;
	return retval;
}

BOOL CSerial::SetupConnection(const HANDLE p_port, enum port_mode_t p_portMode) {
	// local data
	static struct serial_parameter_t {
		long baudrate;
		char dataBits;
		char parity;
		char stopBits;
	} v_serial_parameters[] = {{CBR_9600, 7, 'e', 1}, {CBR_38400, 8, 'e', 1}, {CBR_9600, 8, 'n', 2}};
	int v_index = 0;
	DCB		dcb;

	// set the dcb size
	dcb.DCBlength = sizeof(DCB);

	// get the current comms state
	if(!GetCommState(p_port, &dcb)) {
		SetLastErrorMessage(GetLastError());
		return FALSE;
	}

	// select the serial parameters
	switch (p_portMode) {
	case port_mode_normal:
		switch (m_portMode) {
		case port_mode_slow: v_index = 0; break;
		case port_mode_fast: v_index = 1; break;
		default: SetLastErrorMessage((const char *)"Invalid port mode"); return FALSE;
		}
		break;
	case port_mode_slow: v_index = 0; break;
	case port_mode_fast: v_index = 1; break;
	case port_mode_download:
		switch (m_portMode) {
		case port_mode_slow: v_index = 2; break;
		case port_mode_fast: v_index = 1; break;
		default: SetLastErrorMessage((const char *)"Invalid port mode"); return FALSE;
		}
		break;
	}

	// set the baud rate
	dcb.BaudRate = v_serial_parameters[v_index].baudrate;
	dcb.ByteSize = v_serial_parameters[v_index].dataBits;
	switch (v_serial_parameters[v_index].stopBits) {
	case 1: dcb.StopBits = ONESTOPBIT; break;
	case 15: dcb.StopBits = ONE5STOPBITS; break;
	case 2: dcb.StopBits = TWOSTOPBITS; break;
	}
	switch (tolower(v_serial_parameters[v_index].parity)) {
	case 'e': dcb.Parity = EVENPARITY; break;
	case 'o': dcb.Parity = ODDPARITY; break;
	case 'n': dcb.Parity = NOPARITY; break;
	case 'm': dcb.Parity = MARKPARITY; break;
	case 's': dcb.Parity = SPACEPARITY; break;
	}
	dcb.fInX = dcb.fOutX = ((m_remotePort == p_port) || (port_mode_download == p_portMode)) ? FALSE : TRUE;
	dcb.fParity = (NOPARITY != dcb.Parity);
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.XonChar = ASCII_XON;
	dcb.XoffChar = ASCII_XOFF;
	dcb.XonLim = 100;
	dcb.XoffLim = 100;
	dcb.fBinary = TRUE;
	dcb.fNull = FALSE;
	dcb.fErrorChar = FALSE;

	// set the comms parameters
	if(!SetCommState(p_port, &dcb)) {
		SetLastErrorMessage(GetLastError());
		return FALSE;
	}

	// purge any information in the buffer
	Sleep(100);
	PurgeComm(p_port,PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

	// return success status
	return(TRUE);
}
