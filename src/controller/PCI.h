/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __PCI_H__
#define __PCI_H__

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
class CPCI : public CCommunications
{
protected:
    HANDLE m_mpePort, m_remotePort, m_memoryPort;
    int m_driverInstance;

    BOOL OpenPCI(const char *p_portName, HANDLE &p_port);
#ifdef _WIN32
    BOOL OpenMPE() { return OpenPCI("MPE", m_mpePort); }
    BOOL OpenRemote() { return OpenPCI("REMOTE", m_remotePort); }
    BOOL OpenMemory() { return OpenPCI("MEMORY", m_memoryPort); }
#else
    BOOL OpenMPE() { return OpenPCI("mpe", m_mpePort); }
    BOOL OpenRemote() { return OpenPCI("remote", m_remotePort); }
    BOOL OpenMemory() { return OpenPCI("memory", m_memoryPort); }
#endif
	BOOL OpenTextFileLoader() { return FALSE; }
	BOOL OpenTextFileLoaderNP() { return FALSE; }

    void ClosePCI(HANDLE &p_port);
    void CloseMPE() { ClosePCI(m_mpePort); }
    void CloseRemote() { ClosePCI(m_remotePort); }
    void CloseMemory() { ClosePCI(m_memoryPort); }
	void CloseTextFileLoader() {}
	void CloseTextFileLoaderNP() {}

    BOOL WritePCI(HANDLE p_port, const char *p_buffer,size_t p_bufferSize);
    BOOL WriteMPE(const char *p_buffer,size_t p_bufferSize) { return WritePCI(m_mpePort, p_buffer, p_bufferSize); }
    BOOL WriteRemote(const char *p_buffer,size_t p_bufferSize) { return WritePCI(m_remotePort, p_buffer, p_bufferSize); }
	BOOL WriteTfl(const char *p_buffer,size_t p_bufferSize) { return FALSE; }

    BOOL ReadPCI(HANDLE p_port, char *p_buffer,size_t *p_bufferSize);
    BOOL ReadMPE(char *p_buffer,size_t *p_bufferSize) { return ReadPCI(m_mpePort, p_buffer, p_bufferSize); }
    BOOL ReadRemote(char *p_buffer,size_t *p_bufferSize) { return ReadPCI(m_remotePort, p_buffer, p_bufferSize); }
	BOOL ReadTfl(char *p_buffer, size_t *p_bufferSize) { return FALSE; }

    BOOL GetTablePoints(long p_start, long p_points, float p_values[]);
    BOOL SetTablePoints(long p_start, long p_points, const double p_values[]);
    BOOL SetTableBase(long base);

public:
    CPCI();
    virtual ~CPCI(void);

    //-- set parameters
    void SetInterfacePCI(int p_driverInstance);

    //-- port capabilities
    enum port_type_t GetType() const { return port_type_pci; }
    size_t GetFifoLength() const { return 160; }
    size_t GetSystemSoftwareDownloadFifoLength() const { return 32; }
    BOOL CanMemory() const { return TRUE; }
    BOOL CanRemote() const { return TRUE; }
    BOOL CanDownload() const { return TRUE; }
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
