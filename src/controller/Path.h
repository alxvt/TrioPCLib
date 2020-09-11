/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __PATH_H__
#define __PATH_H__

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include "Communications.h"

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
class CPath : public CCommunications
{
protected:
	HANDLE m_mpePort, m_remotePort, m_memoryPort;
	CString m_path;

	BOOL OpenPath(const char *p_portName, HANDLE &p_port);
	BOOL OpenMPE() { return OpenPath("mpe", m_mpePort); }
	BOOL OpenRemote() { return OpenPath("remote", m_remotePort); }
	BOOL OpenMemory() { return OpenPath("memory", m_memoryPort); }
	BOOL OpenTextFileLoader() { return FALSE; }
	BOOL OpenTextFileLoaderNP() { return FALSE; }

	void ClosePath(HANDLE &p_port);
	void CloseMPE() { ClosePath(m_mpePort); }
	void CloseRemote() { ClosePath(m_remotePort); }
	void CloseMemory() { ClosePath(m_memoryPort); }
	void CloseTextFileLoader() {}
	void CloseTextFileLoaderNP() {}

	BOOL WritePath(HANDLE p_port, const char *p_buffer,size_t p_bufferSize);
	BOOL WriteMPE(const char *p_buffer,size_t p_bufferSize) { return WritePath(m_mpePort, p_buffer, p_bufferSize); }
	BOOL WriteRemote(const char *p_buffer,size_t p_bufferSize) { return WritePath(m_remotePort, p_buffer, p_bufferSize); }
	BOOL WriteTfl(const char *p_buffer,size_t p_bufferSize) { return FALSE; }

	BOOL ReadPath(HANDLE p_port, char *p_buffer,size_t *p_bufferSize);
	BOOL ReadMPE(char *p_buffer,size_t *p_bufferSize) { return ReadPath(m_mpePort, p_buffer, p_bufferSize); }
	BOOL ReadRemote(char *p_buffer,size_t *p_bufferSize) { return ReadPath(m_remotePort, p_buffer, p_bufferSize); }
	BOOL ReadTfl(char *p_buffer, size_t *p_bufferSize) { return FALSE; }

	BOOL GetTablePoints(long p_start, long p_points, float p_values[]);
	BOOL SetTablePoints(long p_start, long p_points, const double p_values[]);
	BOOL SetTableBase(long base);

public:
	CPath();
	virtual ~CPath(void);

	//-- set parameters
	void SetInterfacePath(const char *p_path);

	//-- port capabilities
	enum port_type_t GetType() const { return port_type_path; }
	size_t GetFifoLength() const { return 160; }
	size_t GetSystemSoftwareDownloadFifoLength() const { return 32; }
	BOOL CanRemote() const { return TRUE; }
	BOOL CanDownload() const { return TRUE; }
	BOOL CanMemory() const { return TRUE; }
	BOOL CanTextFileLoader() const { return FALSE; }
	BOOL IsEscapeRemoteChannel() const { return FALSE; }

	//-- direct transfer operations
	BOOL GetTable(long p_start, long p_points, double p_values[]);
	BOOL SetTable(long p_start, long p_points, const double p_values[]);
	BOOL GetVr(long p_vr, double *p_value);
	BOOL SetVr(long p_vr, double p_value);
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
