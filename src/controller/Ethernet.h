/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __ETHERNET_H__
#define __ETHERNET_H__

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
class CEthernet : public CCommunications
{
protected:
    SOCKET m_mpeSocket, m_remoteSocket, m_tflSocket;
	DWORD m_ipAddress;
	short m_mpePort, m_remotePort, m_tflPort, m_tflnpPort;
    int m_sndbufSize;

    BOOL OpenTCP(SOCKET &p_socket, short p_port);
	BOOL OpenMPE() { return OpenTCP(m_mpeSocket, m_mpePort); }
	BOOL OpenRemote() { return OpenTCP(m_remoteSocket, m_remotePort); }
	BOOL OpenMemory() { return FALSE; }
	BOOL OpenTextFileLoader() { return OpenTCP(m_tflSocket, m_tflPort); }
	BOOL OpenTextFileLoaderNP() { return OpenTCP(m_tflSocket, m_tflnpPort); }
	
	void CloseTCP(SOCKET &p_socket);
	void CloseMPE() { CloseTCP(m_mpeSocket); }
	void CloseRemote() { CloseTCP(m_remoteSocket); }
	void CloseMemory() {}
	void CloseTextFileLoader() { CloseTCP(m_tflSocket); }
	void CloseTextFileLoaderNP() { CloseTCP(m_tflSocket); }

	BOOL ReadTCP(SOCKET p_socket, char *p_buffer, size_t *p_bufferSize);
	BOOL ReadMPE(char *p_buffer, size_t *p_bufferSize) { return ReadTCP(m_mpeSocket, p_buffer, p_bufferSize); }
	BOOL ReadRemote(char *p_buffer, size_t *p_bufferSize) { return ReadTCP(m_remoteSocket, p_buffer, p_bufferSize); }
	BOOL ReadTfl(char *p_buffer, size_t *p_bufferSize) { return ReadTCP(m_tflSocket, p_buffer, p_bufferSize); }

	BOOL WriteTCP(SOCKET p_socket, const char *p_buffer,size_t p_bufferSize);
	BOOL WriteMPE(const char *p_buffer,size_t p_bufferSize) { return WriteTCP(m_mpeSocket, p_buffer, p_bufferSize); }
	BOOL WriteRemote(const char *p_buffer,size_t p_bufferSize) { return WriteTCP(m_remoteSocket, p_buffer, p_bufferSize); }
	BOOL WriteTfl(const char *p_buffer,size_t p_bufferSize) { return WriteTCP(m_tflSocket, p_buffer, p_bufferSize); }

public:
    CEthernet();
    virtual ~CEthernet();

	//-- set parameters
	void SetInterfaceTCP(const char *p_hostname, short p_mpePort, short p_remotePort, short p_tflPort, short p_tflnpPort);
	void SetInterfaceTCP(DWORD p_ipAddress, short p_mpePort, short p_remotePort, short p_tflPort, short p_tflnpPort);
    void SetInterfaceTCP(int p_sndbufSize) { m_sndbufSize = p_sndbufSize; }

	//-- capabilities
	enum port_type_t GetType() const { return port_type_ethernet; }

	//------------------------------------------------------
	// CAUTION:
	// Nagle is enabled on the controller. This means that ACKs will be sent on the next outgoing packet,
	// or after the Delayed ACK timeout (200 ms). As a consequence we have to send a complete command to the 
	// controller so that it will generate an output packet so we can get the ACK on the outgoing packet. If this
	// FIFO length is too small then we don't send a complete command and so we get the Delayed ACK. I saw this
	// with FIFO length = 64 doing a SetTable.
	size_t GetFifoLength() const { return 128; }
	//------------------------------------------------------
	
	size_t GetSystemSoftwareDownloadFifoLength() const { return 64; }
	BOOL CanRemote() const { return TRUE; }
	BOOL CanDownload() const { return TRUE; }
	BOOL CanMemory() const { return FALSE; }
	BOOL CanTextFileLoader() const { return TRUE; }
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
