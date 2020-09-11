/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

#define BUFFER_CHANNEL_0        0
#define BUFFER_CHANNEL_1        INVALID_BUFFER
#define BUFFER_CHANNEL_2        INVALID_BUFFER
#define BUFFER_CHANNEL_3        INVALID_BUFFER
#define BUFFER_CHANNEL_4        INVALID_BUFFER
#define BUFFER_CHANNEL_5        1
#define BUFFER_CHANNEL_6        2
#define BUFFER_CHANNEL_7        3
#define BUFFER_CHANNEL_8        4
#define BUFFER_CHANNEL_9        5
#define BUFFER_CHANNEL_MINUS_1  6
#define BUFFER_CHANNEL_REMOTE   7
#define BUFFER_CHANNEL_TFL      8
#define BUFFERS 9
#define BUFFER_INVALID          99

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "ControllerTypes.h"
#include "Communications.h"
#include "FIFO.h"
#include "CaptureFile.h"

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
class CController
{
protected:
    //-- low level communications
    class CCommunications *m_port;
    fireEventContext_t m_fireEventClass;
    fireEventFunction_t m_fireEventFunction;
    BOOL m_syncActive;

    //-- buffers between low level communications and high level communications
    CFIFO m_channelBuffer[channel_directions][BUFFERS];
    channel_mode_t m_channelMode;
    CRITICAL_SECTION m_controllerToPcCriticalSection, m_pcToControllerCriticalSection;

    //-- file capture
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
    CCaptureFile m_captureFile;
    BOOL m_capture;
#endif
    char m_channelId[channel_directions][CHANNEL_ID_SIZE]; // stores the name that will be reported in the capture file

    //-- mpe receive
    char m_activeChannel[channel_directions];
    char m_mpeReceiveBuffer[1024];
    int m_mpeReceiveStatus;
    size_t m_mpeReceiveBytes, m_mpeReceiveByte;

    //-- remote receive
    char m_remoteReceiveBuffer[512];
    size_t m_remoteReceiveBytes, m_remoteReceiveByte;

    //-- tfl receive
    char m_tflReceiveBuffer[512];
    size_t m_tflReceiveBytes, m_tflReceiveByte;

    //-- buffer access
    int ChannelToBuffer(char p_channel) const;
    char BufferToChannel(int p_buffer) const;

    // dispatchers
    BOOL DispatchPcToControllerMpe();
    BOOL DispatchPcToControllerRemote();
    BOOL DispatchPcToControllerTfl();
    BOOL DispatchControllerToPcMpe();
    BOOL DispatchControllerToPcRemote();
    BOOL DispatchControllerToPcTfl();
    BOOL AccumulateAndSend(int p_capability, char p_chr, char *p_buffer, size_t p_bufferSize, size_t *p_bufferLength);
    BOOL FlushAccumulate(int p_capability, char *p_buffer, size_t *p_bufferLength);

public:
    // class constructor/desctructor
    CController(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
    virtual ~CController(void);
    void SetFireEventFunction(fireEventFunction_t p_fireEventFunction) { m_fireEventFunction = p_fireEventFunction; }
    void SetFireEventClass(fireEventContext_t p_fireEventClass) { m_fireEventClass = p_fireEventClass; }
    fireEventFunction_t GetFireEventFunction() const { return m_fireEventFunction; }
    fireEventContext_t GetFireEventClass() const { return m_fireEventClass; }

    //-- connect/disconnect
    BOOL CreateUSB(int p_driverInstance);
    BOOL CreateRS232(const char *p_port, enum port_mode_t p_mode);
    BOOL CreateRS232(const wchar_t *p_port, enum port_mode_t p_mode);
    BOOL CreateTCP(const char *p_hostname, short p_port);
    BOOL CreateTCP(const wchar_t *p_hostname, short p_port);
    BOOL SetInterfaceTCP(int p_sndbufSize);
    BOOL CreatePCI(int p_driverInstance);
    BOOL CreateFins(const char *p_hostname, short p_port, char p_unitAddress);
    BOOL CreateFins(const wchar_t *p_hostname, short p_port, char p_unitAddress);
    BOOL CreatePath(const char *p_path);
    BOOL CreatePath(const wchar_t *p_path);
    BOOL Open(int p_capabilities = COMMUNICATIONS_CAPABILITY_MPE);
    void Close(int p_capabilities = -1);
    void Destroy();

    //-- controller status
    void SetChannelMode(enum channel_mode_t p_channelMode, BOOL p_force = FALSE);
    const enum channel_mode_t GetChannelMode() const { return m_channelMode; }
    BOOL IsOpen(int p_capabilities) const { return (m_port != NULL) && m_port->IsOpen(p_capabilities); }
    BOOL IsError(int p_capabilities) const { return (m_port != NULL) && m_port->IsError(p_capabilities); }

    //-- port status
    BOOL GetStatus() const;
    enum port_type_t GetPortType() const { return m_port->GetType(); }
    void GetLastErrorMessage(CString &p_errorMessage) const { m_port->GetLastErrorMessage(p_errorMessage); }

    //-- port capabilities
    size_t GetFifoLength() const { return m_port ? m_port->GetFifoLength() : 0; }
    size_t GetSystemSoftwareDownloadFifoLength() const { return m_port ? m_port->GetSystemSoftwareDownloadFifoLength() : 0; }
    BOOL CanRemote() const { return m_port ? m_port->CanRemote() : FALSE; }
    BOOL CanDownload() const { return m_port ? m_port->CanDownload() : FALSE; }
    BOOL CanMemory() const { return m_port ? m_port->CanMemory() : FALSE; }
    BOOL IsEscapeRemoteChannel() const { return m_port ? m_port->IsEscapeRemoteChannel() : TRUE; }
    int GetOpenCapabilities() const { return m_port ? m_port->GetOpenCapabilities() : 0; }

    //-- port operations
    BOOL GetTable(long p_start, long p_points, double p_values[]) { return m_port ? m_port->GetTable(p_start, p_points, p_values) : FALSE; }
    BOOL SetTable(long p_start, long p_points, const double p_values[]) { return m_port ? m_port->SetTable(p_start, p_points, p_values) : FALSE; }
    BOOL GetVr(long p_vr, double *p_value) { return m_port ? m_port->GetVr(p_vr, p_value) : FALSE; }
    BOOL SetVr(long p_vr, double p_value) { return m_port ? m_port->SetVr(p_vr, p_value) : FALSE; }

    //-- capture
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
    BOOL OpenCaptureFile(const CString &p_captureFile);
    void StartCapture(char p_channel);
    void StopCapture(char p_channel);
    void CloseCaptureFile();
#endif

    //-- IO
    BOOL Flush(char p_channel);
    BOOL Read(char p_channel, char *p_buffer, size_t *p_bufferSize);
    BOOL Write(char p_channel, const char *p_buffer, size_t p_bufferSize);
#if __SIZEOF_SIZE_T__ != __SIZEOF_LONG__
    BOOL Read(char p_channel, char *p_buffer, unsigned long *p_bufferSize);
#endif
    BOOL DispatchPcToController();
    BOOL DispatchControllerToPc();

    //-- Project
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
    BOOL ProjectLoad(const char *p_project, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
    BOOL ProjectSave(const char *p_project, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
    BOOL ProjectCheck(const char *p_project, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
    BOOL ProgramLoad(const char *p_program, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
    BOOL ProgramSave(const char *p_program, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
#endif

    //-- file operations
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
    BOOL TextFileLoader(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction, const char *p_source_file, int p_destination, const char *p_destination_file, int p_protocol, BOOL p_compression, int p_compression_level, BOOL p_timeout, int p_timeout_seconds, int p_direction);
#endif
#if !defined(_WIN32_WCE)
    BOOL SystemSoftwareLoad(const char *p_coffFile, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
#endif

    // utilities
    BOOL GetCancel() const { return m_port ? m_port->m_cancel : FALSE; }
    void SetCancel(BOOL p_cancel) { if (m_port) m_port->m_cancel = p_cancel; }

    // DIRTY HACKS
    void *GetPort() const { return m_port; }
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
