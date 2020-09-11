/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __USB_H__
#define __USB_H__

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "Communications.h"

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
class CUsb : public CCommunications
{
protected:
    HANDLE  m_remotePort, m_mpePort;
	int m_driverInstance;

protected:
	BOOL OpenUSB(const char *p_driverInstanceName, HANDLE &p_port);
	BOOL OpenMPE() { return OpenUSB("MPE", m_mpePort); }
	BOOL OpenRemote() { return OpenUSB("REMOTE", m_remotePort); }
	BOOL OpenMemory() { return FALSE; }
	BOOL OpenTextFileLoader() { return FALSE; }
	BOOL OpenTextFileLoaderNP() { return FALSE; }

	void CloseUSB(HANDLE &p_port);
	void CloseMPE() { CloseUSB(m_mpePort); }
	void CloseRemote() { CloseUSB(m_remotePort); }
	void CloseMemory() {}
	void CloseTextFileLoader() {}
	void CloseTextFileLoaderNP() {}

	BOOL ReadUSB(HANDLE p_port, char *p_buffer, size_t *p_bufferSize);
	BOOL ReadMPE(char *p_buffer, size_t *p_bufferSize) { return ReadUSB(m_mpePort, p_buffer, p_bufferSize); }
	BOOL ReadRemote(char *p_buffer, size_t *p_bufferSize) { return ReadUSB(m_remotePort, p_buffer, p_bufferSize); }
	BOOL ReadTfl(char *p_buffer, size_t *p_bufferSize) { return FALSE; }

    BOOL WriteBlock(HANDLE p_port, const char *szBuffer,DWORD dwBufferLength);
	BOOL WriteUSB(HANDLE p_port, const char *p_buffer,size_t p_bufferSize);
	BOOL WriteMPE(const char *p_buffer,size_t p_bufferSize) { return WriteUSB(m_mpePort, p_buffer, p_bufferSize); }
	BOOL WriteRemote(const char *p_buffer,size_t p_bufferSize) { return WriteUSB(m_remotePort, p_buffer, p_bufferSize); }
	BOOL WriteTfl(const char *p_buffer,size_t p_bufferSize) { return FALSE; }

public:
    CUsb();
    virtual ~CUsb();

	//-- set parameters
    void SetInterfaceUSB(int p_driverInstance);

	//-- port status
	enum port_type_t GetType() const { return port_type_usb; }
	size_t GetFifoLength() const { return 32; }
	size_t GetSystemSoftwareDownloadFifoLength() const { return 32; }
	BOOL CanRemote() const { return TRUE; }
	BOOL CanDownload() const { return FALSE; }
	BOOL CanMemory() const { return FALSE; }
	BOOL CanTextFileLoader() const { return FALSE; }
	BOOL IsEscapeRemoteChannel() const { return TRUE; }

	//-- direct transfer operations
	BOOL GetTable(long p_start, long p_points, double p_values[]) { return FALSE; }
	BOOL SetTable(long p_start, long p_points, const double p_values[]) { return FALSE; }
	BOOL GetVr(long p_vr, double *p_value) { return FALSE; }
	BOOL SetVr(long p_vr, double p_value) { return FALSE; }
};

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/
#endif // !defined(AFX_USB_H__4CFAA5D5_6063_11D3_8A13_00403393B236__INCLUDED_)
