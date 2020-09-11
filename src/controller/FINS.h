/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __FINS_H__
#define __FINS_H__
#define WINDOW_SIZE 2

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
class CFins : public CCommunications
{
protected:
	char m_destinationNodeAddress, m_destinationUnitAddress, m_sourceNodeAddress, m_openCommand;
    SOCKET m_mpeSocket;
	DWORD m_ipAddress;
	short m_mpePort;
	ULARGE_INTEGER m_lastPoll, m_lastWrite;
    char m_receive_buffer[10240];
    volatile size_t m_receive_bufferHead, m_receive_bufferTail;
    char m_transmit_buffer[WINDOW_SIZE][1024];
    size_t m_transmit_bufferLength[WINDOW_SIZE];
    volatile size_t m_transmit_bufferHead, m_transmit_bufferTail;
    HANDLE write_mutex;

	BOOL OpenMPE();
	BOOL OpenRemote() { return FALSE; }
	BOOL OpenMemory() { return FALSE; }
	BOOL OpenTextFileLoader() { return FALSE; }
	BOOL OpenTextFileLoaderNP() { return FALSE; }

	void CloseMPE();
	void CloseRemote() {}
	void CloseMemory() {}
	void CloseTextFileLoader() {}
	void CloseTextFileLoaderNP() {}

	BOOL ReadMPE(char *p_buffer, size_t *p_bufferSize);
	BOOL ReadRemote(char *p_buffer, size_t *p_bufferSize) { return FALSE; }
	BOOL ReadTfl(char *p_buffer, size_t *p_bufferSize) { return FALSE; }

	size_t WriteHeader(char *packet, size_t packetSize) const;
	BOOL WriteMPE(const char *p_buffer,size_t p_bufferSize);
	BOOL WriteRemote(const char *p_buffer,size_t p_bufferSize) { return FALSE; }
	BOOL WriteTfl(const char *p_buffer,size_t p_bufferSize) { return FALSE; }

public:
	CFins(void);
	virtual ~CFins(void);

	//-- set parameters
	void SetInterfaceFins(const char *p_hostname, short p_port, char p_unitAddress);
	void SetInterfaceFins(DWORD p_ipAddress, short p_port, char p_unitAddress);

	//-- capabilities
	enum port_type_t GetType() const { return port_type_fins; }
	size_t GetFifoLength() const { return 514; }
	size_t GetSystemSoftwareDownloadFifoLength() const { return 0; }
	BOOL CanRemote() const { return FALSE; }
	BOOL CanDownload() const { return FALSE; }
	BOOL CanMemory() const { return FALSE; }
	BOOL CanTextFileLoader() const { return FALSE; }
	BOOL IsEscapeRemoteChannel() const { return TRUE; }

	//-- direct transfer operations
	BOOL GetTable(long p_start, long p_points, double p_values[]) { return FALSE; }
	BOOL SetTable(long p_start, long p_points, const double p_values[]) { return FALSE; }
	BOOL GetVr(long p_vr, double *p_value) { return FALSE; }
	BOOL SetVr(long p_vr, double p_value) { return FALSE; }

	//-- DIRTY HACKS FOR FINSTOOLS.CPP
	BOOL WritePacket(const char *p_packet, size_t p_packetLength);
	BOOL ReadPacket(char *p_packet, size_t *p_packetSize);
	BOOL IsResponseError(const char *p_packet, size_t p_packetLength, CStringA &message);
	void SetDestinationUnitAddress(char p_destinationUnitAddress) { m_destinationUnitAddress = p_destinationUnitAddress; }
	char GetDestinationUnitAddress() const { return m_destinationUnitAddress; }
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
