/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __SERIAL_H__
#define __SERIAL_H__

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
class CSerial : public CCommunications  
{
protected:
	CString m_portName;
	HANDLE m_remotePort, m_mpePort;

	BOOL SetupConnection(const HANDLE p_port, enum port_mode_t p_portMode);
	BOOL OpenSerial(HANDLE &p_port);
	BOOL OpenMPE() { return OpenSerial(m_mpePort); }
	BOOL OpenRemote();
	BOOL OpenMemory() { return FALSE; }
	BOOL OpenTextFileLoader() { return FALSE; }
	BOOL OpenTextFileLoaderNP() { return FALSE; }

	void CloseSerial(HANDLE &p_port);
	void CloseMPE() { CloseSerial(m_mpePort); }
	void CloseRemote() { CloseSerial(m_remotePort); }
	void CloseMemory() {}
	void CloseTextFileLoader() {}
	void CloseTextFileLoaderNP() {}

	BOOL ReadSerial(HANDLE p_port, char *p_buffer, size_t *p_bufferSize);
	BOOL ReadMPE(char *p_buffer, size_t *p_bufferSize) { return ReadSerial(m_mpePort, p_buffer, p_bufferSize); }
	BOOL ReadRemote(char *p_buffer, size_t *p_bufferSize) { return ReadSerial(m_remotePort, p_buffer, p_bufferSize); }
	BOOL ReadTfl(char *p_buffer, size_t *p_bufferSize) { return FALSE; }

	BOOL WriteSerial(HANDLE p_port, const char *p_buffer,size_t p_bufferSize);
	BOOL WriteMPE(const char *p_buffer,size_t p_bufferSize) { return WriteSerial(m_mpePort, p_buffer, p_bufferSize); }
	BOOL WriteRemote(const char *p_buffer,size_t p_bufferSize) { return WriteSerial(m_remotePort, p_buffer, p_bufferSize); }
	BOOL WriteTfl(const char *p_buffer,size_t p_bufferSize) { return FALSE; }

public:
	CSerial();
	virtual ~CSerial();

	//-- set parameters
	void SetInterfaceRS232(const char *p_portName, port_mode_t p_portMode);
	BOOL SetMode(enum port_mode_t p_portMode);

	//-- port status
	enum port_type_t GetType() const { return port_type_rs232; }
	size_t GetFifoLength() const { return 16; }
	size_t GetSystemSoftwareDownloadFifoLength() const { return 16; }
	BOOL CanRemote() const { return TRUE; }
	BOOL CanDownload() const { return TRUE; }
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

#endif
