/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

#include "StdAfx.h"

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "Controller.h"
#include "Serial.h"
#include "PCI.h"
#include "Ethernet.h"
#ifdef _WIN32
#include "USB.h"
#endif
#include "FINS.h"
#include "Path.h"
#include "Ascii.h"
#include "Project.h"
#include "LoadSystemSoftware.h"
#include "TextFileLoader.h"

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
extern "C"
{
    long g_lMPEMode = 1;
}

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/
CController::CController(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
    : m_port(NULL)
    , m_fireEventClass(p_fireEventClass)
    , m_fireEventFunction(p_fireEventFunction)
    , m_syncActive(FALSE)
    , m_channelMode(channel_mode_off)
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
    , m_capture(FALSE)
#endif
    , m_mpeReceiveStatus(0)
    , m_mpeReceiveBytes(0)
    , m_mpeReceiveByte(0)
    , m_remoteReceiveBytes(0)
    , m_remoteReceiveByte(0)
    , m_tflReceiveBytes(0)
    , m_tflReceiveByte(0)
{
    char channelId[CHANNEL_ID_SIZE];
    int buffer;

    // initialise the critical sections
    InitializeCriticalSection(&m_pcToControllerCriticalSection);
    InitializeCriticalSection(&m_controllerToPcCriticalSection);

    // initialise capture data
    for (buffer = 0;buffer < BUFFERS; buffer++) {
        _snprintf(channelId, sizeof(channelId), "Channel[%i][pc_to_controller]",
            BufferToChannel(buffer));
        m_channelBuffer[pc_to_controller][buffer].SetChannelId(channelId);
        _snprintf(channelId, sizeof(channelId), "Channel[%i][controller_to_pc]",
            BufferToChannel(buffer));
        m_channelBuffer[controller_to_pc][buffer].SetChannelId(channelId);
    }
    _snprintf(m_channelId[pc_to_controller], sizeof(m_channelId[pc_to_controller]),
        "RAW[pc_to_controller]");
    _snprintf(m_channelId[controller_to_pc], sizeof(m_channelId[controller_to_pc]),
        "RAW[controller_to_pc]");

    // initialise channel data
    m_activeChannel[pc_to_controller] = 0;
    m_activeChannel[controller_to_pc] = 0;
}

CController::~CController(void)
{
    Destroy();

    // free the critical sections
    DeleteCriticalSection(&m_pcToControllerCriticalSection);
    DeleteCriticalSection(&m_controllerToPcCriticalSection);
}

//-- connect/disconnect
#ifdef INTERFACE_USB
BOOL CController::CreateUSB(int p_driverInstance) {
    // create interface
    if (NULL == m_port) {
        m_port = new CUsb;
        if(!m_port) {
            m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, _T("Cannot create USB port"));
            return FALSE;
        }
    }

    // set interface characteristics
    m_port->SetInterfaceUSB(p_driverInstance);

    return TRUE;
}
#endif

#ifdef INTERFACE_SERIAL
BOOL CController::CreateRS232(const wchar_t *p_port, enum port_mode_t p_mode)
{
    CStringA ASCIIPort(p_port);
    return CreateRS232(ASCIIPort, p_mode);
}

BOOL CController::CreateRS232(const char *p_port, enum port_mode_t p_mode)
{
    // create interface
    m_port = new CSerial;
    if(!m_port) {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, _T("Cannot create serial port"));
        return FALSE;
    }

    // set interface characteristics
    m_port->SetInterfaceRS232(p_port, p_mode);

    return TRUE;
}
#endif

#ifdef INTERFACE_ETHERNET
#if !defined(__GNUC__)
BOOL CController::CreateTCP(const wchar_t *p_hostname, short p_port)
{
    CStringA ASCIIHostname(p_hostname);
    return CreateTCP(ASCIIHostname, p_port);
}
#endif

BOOL CController::CreateTCP(const char *p_hostname, short p_port)
{
    // create interface
    m_port = new CEthernet;
    if(!m_port) {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, _T("Cannot create ethernet port"));
        return FALSE;
    }

    // set interface characteristics
    m_port->SetInterfaceTCP(p_hostname, p_port, TRIO_TCP_TOKEN_SOCKET, TRIO_TCP_TFL_SOCKET, TRIO_TCP_TFLNP_SOCKET);

    return TRUE;
}

BOOL CController::SetInterfaceTCP(int p_sndbufSize)
{
    if (!m_port)
        return FALSE;
    m_port->SetInterfaceTCP(p_sndbufSize);
    return TRUE;
}
#endif

#ifdef INTERFACE_PCI
BOOL CController::CreatePCI(int p_driverInstance) {
    // create interface
    m_port = new CPCI;
    if(!m_port) {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, _T("Cannot create pci port"));
        return FALSE;
    }

    // set interface characteristics
    m_port->SetInterfacePCI(p_driverInstance);

    return TRUE;
}
#endif

#ifdef INTERFACE_FINS
BOOL CController::CreateFins(const wchar_t *p_hostname, short p_port, char p_unitAddress)
{
    CStringA ASCIIHostname(p_hostname);
    return CreateFins(ASCIIHostname, p_port, p_unitAddress);
}

BOOL CController::CreateFins(const char *p_hostname, short p_port, char p_unitAddress)
{
    // create interface
    m_port = new CFins;
    if(!m_port) {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, _T("Cannot create fins port"));
        return FALSE;
    }

    // set interface characteristics
    m_port->SetInterfaceFins(p_hostname, p_port, p_unitAddress);

    return TRUE;
}
#endif

#ifdef INTERFACE_PATH
BOOL CController::CreatePath(const wchar_t *p_path)
{
    CStringA ASCIIPath;
    return CreatePath(ASCIIPath);
}

BOOL CController::CreatePath(const char *p_path)
{
    // create interface
    m_port = new CPath;
    if(!m_port) {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, _T("Cannot create path port"));
        return FALSE;
    }

    // set interface characteristics
    m_port->SetInterfacePath(p_path);

    return TRUE;
}
#endif

BOOL CController::Open(int p_capabilities) {
    // if no interface then fail
    if (NULL == m_port) {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, _T("No port. Please call a create function first"));
        return FALSE;
    }

    // open interface
    if(!m_port->Open(p_capabilities)) {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, _T("Cannot open port"));
        Close();
        return FALSE;
    }

    return TRUE;
}

void CController::Close(int p_capabilities) {
    if (p_capabilities & COMMUNICATIONS_CAPABILITY_MPE) {
        // reset MPE mode
        if (channel_mode_on == m_channelMode) {
            Write(0, "MPE(0)\r", 7);
            Flush(0);
        }
    }

    // close port
    if (m_port) {
        // close requested capabilities
        m_port->Close(p_capabilities);
    }
}

void CController::Destroy() {
    // if we have a port then destroy it
    if (m_port) {
        // make sure all ports are closed
        Close(-1);

        // delete the port
        delete m_port;
        m_port = NULL;
    }
}

//-- controller status
void CController::SetChannelMode(enum channel_mode_t p_channelMode, BOOL p_force) {
    // if we are already in this mode then we are done
    if(!p_force && (p_channelMode == m_channelMode)) return;

    // change the mode
    switch(p_channelMode) {
    case channel_mode_on:
        m_port->SetMode(port_mode_normal);
        if (p_force || (channel_mode_on != m_channelMode)) {
            char mpeCommand[] = "\x1b\x30\rMPE(1)\r";
			mpeCommand[7] = '0' + (char)g_lMPEMode;
            Write(0, mpeCommand, strlen(mpeCommand));
            m_activeChannel[controller_to_pc] = 0;
            m_mpeReceiveStatus = 0;
        }
        break;
    case channel_mode_off:
        m_port->SetMode(port_mode_normal);
        if (p_force || (channel_mode_on == m_channelMode)) {
            const char *mpeCommand = "\x1b\x30\rMPE(0)\r";
            Write(0, mpeCommand, strlen(mpeCommand));
            m_activeChannel[controller_to_pc] = 0;
            m_mpeReceiveStatus = 0;
        }
        break;
    case channel_mode_download:
        m_port->SetMode(port_mode_download);
        break;
    }

    m_channelMode = p_channelMode;
}

//-- buffer status
int CController::ChannelToBuffer(char p_channel) const {
    int buffer = BUFFER_INVALID;

    switch (p_channel) {
    case -1: buffer = BUFFER_CHANNEL_MINUS_1; break;
    case 0: buffer = BUFFER_CHANNEL_0; break;
    case 5: buffer = BUFFER_CHANNEL_5; break;
    case 6: buffer = BUFFER_CHANNEL_6; break;
    case 7: buffer = BUFFER_CHANNEL_7; break;
    case 8: buffer = BUFFER_CHANNEL_8; break;
    case 9: buffer = BUFFER_CHANNEL_9; break;
    case CHANNEL_REMOTE: buffer = BUFFER_CHANNEL_REMOTE; break;
    case CHANNEL_TFL: buffer = BUFFER_CHANNEL_TFL; break;
    default: buffer = BUFFER_INVALID;
    }

    ASSERT(BUFFER_INVALID != buffer);

    return buffer;
}

char CController::BufferToChannel(int p_buffer) const {
    char channel;

    switch (p_buffer) {
    case BUFFER_CHANNEL_MINUS_1: channel = -1; break;
    case BUFFER_CHANNEL_0: channel = 0; break;
    case BUFFER_CHANNEL_5: channel = 5; break;
    case BUFFER_CHANNEL_6: channel = 6; break;
    case BUFFER_CHANNEL_7: channel = 7; break;
    case BUFFER_CHANNEL_8: channel = 8; break;
    case BUFFER_CHANNEL_9: channel = 9; break;
    case BUFFER_CHANNEL_REMOTE: channel = CHANNEL_REMOTE; break;
    case BUFFER_CHANNEL_TFL: channel = CHANNEL_TFL; break;
    default: channel = CHANNEL_INVALID;
    }

    ASSERT(CHANNEL_INVALID != channel);

    return channel;
}

BOOL CController::DispatchControllerToPcMpe()
{
    int bufferIndex = ChannelToBuffer(m_activeChannel[controller_to_pc]);
    BOOL run = TRUE, error = FALSE;

    // process until stopped
    while (run)
    {
        // if our buffer is empty then read some more
        if (m_mpeReceiveByte >= m_mpeReceiveBytes)
        {
            m_mpeReceiveBytes = min(sizeof(m_mpeReceiveBuffer), m_port->GetFifoLength());
            if (m_port->Read(COMMUNICATIONS_CAPABILITY_MPE, m_mpeReceiveBuffer, &m_mpeReceiveBytes))
            {
                // data to process
                if (m_mpeReceiveBytes)
                {
                    // capture
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
                    if (m_capture)
                    {
                        CString header;

                        header.Format("Get %s: ", m_channelId[controller_to_pc]);
                        m_captureFile.Capture(header, m_mpeReceiveBuffer, m_mpeReceiveBytes, TRUE);
                    }
#endif

                    // process from start of buffer
                    m_mpeReceiveByte = 0;
                }
                // no data to process
                else
                {
                    run = FALSE;
                }
            }
            // error reading
            else
            {
                m_fireEventFunction(m_fireEventClass, EVENT_READ_FAIL, 0, _T("Error reading port"));
                m_mpeReceiveBytes = 0;
                run = FALSE;
                error = TRUE;
            }
        }

        // process all the received characters
        while (run && !error && (m_mpeReceiveByte < m_mpeReceiveBytes))
        {
            char chr;
            static const char esc = ASCII_ESC;

            // get this character
            chr = m_mpeReceiveBuffer[m_mpeReceiveByte++];

            // process this status
            switch(m_mpeReceiveStatus)
            {
            case 0:
                // if channels are active and this is a channel change then process it
                if ((channel_mode_on == m_channelMode) && (ASCII_ESC == chr)) {
                    m_mpeReceiveStatus = 1;
                }
                // if there is no more room on this channel to store the data then stop
                else if (m_channelBuffer[controller_to_pc][bufferIndex].IsFull())
                {
                    // tell everyone whats going on
                    m_fireEventFunction(m_fireEventClass, EVENT_BUFFER_OVERRUN, m_activeChannel[controller_to_pc], NULL);

                    // unconsume this character
                    m_mpeReceiveByte--;
                    run = FALSE;
                }
                // store this character
                else
                    m_channelBuffer[controller_to_pc][bufferIndex].Put(&chr, 1);
                break;
            case 1:
                m_mpeReceiveStatus = 0;
                switch(chr) {
                case '/': case '0': case '5': case '6': case '7': case '8': case '9':
                    // we are leaving this channel so process any pending characters
                    m_fireEventFunction(m_fireEventClass, EVENT_RECEIVE, m_activeChannel[controller_to_pc], NULL);

                    // set next channel
                    bufferIndex = ChannelToBuffer(m_activeChannel[controller_to_pc] = (chr - '0'));
                    break;
                default:
                    if (m_channelBuffer[controller_to_pc][bufferIndex].IsFull())
                        m_fireEventFunction(m_fireEventClass, EVENT_BUFFER_OVERRUN, m_activeChannel[controller_to_pc], NULL);
                    m_channelBuffer[controller_to_pc][bufferIndex].Put(&esc, 1);
                    m_channelBuffer[controller_to_pc][bufferIndex].Put(&chr, 1);
                }
                break;
            } // end switch
        } // end while

        // if characters were processed then fire the event
        if (m_mpeReceiveBytes) {
            m_fireEventFunction(m_fireEventClass, EVENT_RECEIVE, m_activeChannel[controller_to_pc], NULL);
        }
    } // end while

    return !error;
}

BOOL CController::DispatchControllerToPcRemote()
{
    BOOL run = TRUE, success = TRUE;

    // process until stopped
    while (run  && !m_channelBuffer[controller_to_pc][BUFFER_CHANNEL_REMOTE].IsFull())
    {
        // if our buffer is empty then read some more
        if (m_remoteReceiveByte >= m_remoteReceiveBytes)
        {
            // read bytes
            m_remoteReceiveBytes = min(sizeof(m_remoteReceiveBuffer), m_port->GetFifoLength());
            if (m_port->Read(COMMUNICATIONS_CAPABILITY_REMOTE, m_remoteReceiveBuffer, &m_remoteReceiveBytes))
            {
                if (m_remoteReceiveBytes)
                {
                    // report event
                    m_fireEventFunction(m_fireEventClass, EVENT_RECEIVE, CHANNEL_REMOTE, NULL);

                    // capture
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
                    if (m_capture)
                    {
                        m_captureFile.Capture("Get REMOTE: ", m_remoteReceiveBuffer, m_remoteReceiveBytes, TRUE);
                    }
#endif
                    // process from start of buffer
                    m_remoteReceiveByte = 0;
                }
                else
                {
                    run = FALSE;
                }
            }
            // error reading so reopen port
            else
            {
                m_fireEventFunction(m_fireEventClass, EVENT_READ_FAIL, 0, _T("Error reading port"));
                m_remoteReceiveBytes = 0;
                run = FALSE;
                success = FALSE;
            }
        }

        // process all the received characters
        while (run && success && (m_remoteReceiveByte < m_remoteReceiveBytes) && !m_channelBuffer[controller_to_pc][BUFFER_CHANNEL_REMOTE].IsFull())
        {
            m_channelBuffer[controller_to_pc][BUFFER_CHANNEL_REMOTE].Put(&m_remoteReceiveBuffer[m_remoteReceiveByte++], 1);
        }
    } // end while

    return success;
}

BOOL CController::DispatchControllerToPcTfl()
{
    BOOL run = TRUE, success = TRUE;

    // process until stopped
    while (run  && !m_channelBuffer[controller_to_pc][BUFFER_CHANNEL_TFL].IsFull())
    {
        // if our buffer is empty then read some more
        if (m_tflReceiveByte >= m_tflReceiveBytes)
        {
            // read bytes
            m_tflReceiveBytes = min(sizeof(m_tflReceiveBuffer), m_port->GetFifoLength());
            if (m_port->Read(COMMUNICATIONS_CAPABILITY_TFL, m_tflReceiveBuffer, &m_tflReceiveBytes))
            {
                if (m_tflReceiveBytes)
                {
                    // report event
                    m_fireEventFunction(m_fireEventClass, EVENT_RECEIVE, CHANNEL_TFL, NULL);

                    // capture
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
                    if (m_capture)
                    {
                        m_captureFile.Capture("Get TFL: ", m_tflReceiveBuffer, m_tflReceiveBytes, TRUE);
                    }
#endif

                    // process from start of buffer
                    m_tflReceiveByte = 0;
                }
                else
                {
                    run = FALSE;
                }
            }
            // error reading so reopen port
            else
            {
                m_fireEventFunction(m_fireEventClass, EVENT_READ_FAIL, 0, _T("Error reading port"));
                m_tflReceiveBytes = 0;
                run = FALSE;
                success = FALSE;
            }
        }

        // process all the received characters
        while (run && success && (m_tflReceiveByte < m_tflReceiveBytes) && !m_channelBuffer[controller_to_pc][BUFFER_CHANNEL_TFL].IsFull())
        {
            m_channelBuffer[controller_to_pc][BUFFER_CHANNEL_TFL].Put(&m_tflReceiveBuffer[m_tflReceiveByte++], 1);
        }
    } // end while

    return success;
}

BOOL CController::DispatchControllerToPc() {
    BOOL success = TRUE;
    static BOOL inUse = 0;

    // dispatch (only one dispatcher at any one time)
    if (!inUse && TryEnterCriticalSection(&m_controllerToPcCriticalSection)) {
        // this is safe because only one thread at a time can be in here so there is no race condition
        inUse = TRUE;
        if (IsOpen(COMMUNICATIONS_CAPABILITY_MPE))
            success = DispatchControllerToPcMpe();
        if (success && IsOpen(COMMUNICATIONS_CAPABILITY_REMOTE))
            success = DispatchControllerToPcRemote();
        if (success && IsOpen(COMMUNICATIONS_CAPABILITY_TFL | COMMUNICATIONS_CAPABILITY_TFLNP))
            success = DispatchControllerToPcTfl();
        inUse = FALSE;

        // let some other thread dispatch for us
        LeaveCriticalSection(&m_controllerToPcCriticalSection);
    }

    return success;
}

//-- IO
BOOL CController::Read(char p_channel, char *p_buffer, size_t *p_bufferSize) {
    size_t bufferLength = 0;
    BOOL retval;

    // dispatch data from the controller to the pc
    if(!DispatchControllerToPc()) return FALSE;

    // read from my buffer
    bufferLength = *p_bufferSize;
    retval = m_channelBuffer[controller_to_pc][ChannelToBuffer(p_channel)].Get(p_buffer, &bufferLength);

    // store the buffer length read
    *p_bufferSize = bufferLength;
    return retval;
}

#if __SIZEOF_SIZE_T__ != __SIZEOF_LONG__
BOOL CController::Read(char p_channel, char *p_buffer, unsigned long *p_bufferSize) {
    size_t bufferSize = *p_bufferSize;
    BOOL retval = Read(p_channel, p_buffer, &bufferSize);
    *p_bufferSize = (unsigned long)bufferSize;
    return retval;
}
#endif

BOOL CController::Flush(char p_channel)
{
    size_t bufferSize;
    BOOL success = TRUE;

    do
    {
        char dump[256];

        // delay to pick up characters
        Sleep(100);

        // dispatch and dump data from the controller to the pc
        bufferSize = sizeof(dump);
        if(!DispatchControllerToPc()) success = FALSE;
        else if (!m_channelBuffer[controller_to_pc][ChannelToBuffer(p_channel)].Get(dump, &bufferSize)) success = FALSE;
    }
    while (success && bufferSize);

    return success;
}

BOOL CController::FlushAccumulate(int p_capability, char *p_buffer, size_t *p_bufferLength)
{
    BOOL success = TRUE;

    // if we have not been canceled then send the buffer
    if (!GetCancel()) {
        // send buffer
        if (!m_port->Write(p_capability, p_buffer, *p_bufferLength))
        {
            m_fireEventFunction(m_fireEventClass, EVENT_WRITE_FAIL, 0, _T("Error writing port"));
            success = FALSE;
        }

        // capture
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
        if (m_capture) {
            CString header;

            header.Format("Put %s: ", m_channelId[pc_to_controller]);
            m_captureFile.Capture(header, p_buffer, *p_bufferLength, TRUE);
        }
#endif
    }

    // buffer is now empty
    *p_bufferLength = 0;

    return success;
}

BOOL CController::AccumulateAndSend(int p_capability, char p_chr, char *p_buffer, size_t p_bufferSize, size_t *p_bufferLength)
{
    // append this character to the buffer
    p_buffer[(*p_bufferLength)++] = p_chr;

    // if the buffer is now full then send it
    return
        (*p_bufferLength >= min(p_bufferSize, m_port->GetFifoLength())) ?
            FlushAccumulate(p_capability, p_buffer, p_bufferLength) :
            TRUE;
}

BOOL CController::DispatchPcToControllerMpe()
{
    int bufferIndex;
    char inBuffer[256];
    char outBuffer[256];
    size_t inBytes = 0, outBytes = 0;
    BOOL transfer = FALSE, success = TRUE;

    // dispatch for all channels
    do {
        // nothing transferred yet
        transfer = FALSE;

        for (bufferIndex = 0;bufferIndex < BUFFERS;bufferIndex++) {
            const char channelIndex = BufferToChannel(bufferIndex);

            // if this is the remote channel the skip it
            if ((CHANNEL_REMOTE == channelIndex) || (CHANNEL_TFL == channelIndex))
            {
                continue;
            }

            // if there is no data in this buffer then skip it
            if (m_channelBuffer[pc_to_controller][bufferIndex].IsEmpty()) {
                continue;
            }

            // we have transferred data
            transfer = TRUE;

            // if channels are active then process them
            if (channel_mode_on == m_channelMode) {
                // if we are in MPE mode and there is an active sync message then ignore this buffer
                if (m_syncActive && (channelIndex != m_activeChannel[pc_to_controller])) {
                    continue;
                }

                // if this is not the active channel then change
                if (m_activeChannel[pc_to_controller] != channelIndex) {
                    // send channel change
                    success = AccumulateAndSend(COMMUNICATIONS_CAPABILITY_MPE, ASCII_ESC, outBuffer, sizeof(outBuffer), &outBytes);
                    if (success) success = AccumulateAndSend(COMMUNICATIONS_CAPABILITY_MPE, '0' + channelIndex, outBuffer, sizeof(outBuffer), &outBytes);
                    if (success) m_activeChannel[pc_to_controller] = channelIndex;
                }

                // if this is a sync channel then sync is active
                m_syncActive = ((8 == channelIndex) || (9 == channelIndex));
            }

            // send characters
            inBytes = sizeof(inBuffer);
            if (m_channelBuffer[pc_to_controller][bufferIndex].Get(inBuffer, &inBytes) && inBytes) {
                size_t inByte;

                // process
                for (inByte = 0;inByte < inBytes;inByte++) {
                    char chr = inBuffer[inByte];

                    // if this is a sync send then terminate it
                    if (m_syncActive && ('\r' == chr)) {
                        success = AccumulateAndSend(COMMUNICATIONS_CAPABILITY_MPE, '\n', outBuffer, sizeof(outBuffer), &outBytes);
                        if (success) success = AccumulateAndSend(COMMUNICATIONS_CAPABILITY_MPE, ASCII_DC4, outBuffer, sizeof(outBuffer), &outBytes);
                        if (success) m_syncActive = FALSE;
                    }
                    // not a sync termination so just store it
                    else {
                        success = AccumulateAndSend(COMMUNICATIONS_CAPABILITY_MPE, chr, outBuffer, sizeof(outBuffer), &outBytes);
                    }
                }
            }
        }
    } while (transfer && success);

    // if anything not sent then send it
    if (outBytes && success)
    {
        success = FlushAccumulate(COMMUNICATIONS_CAPABILITY_MPE, outBuffer, &outBytes);
    }

    return success;
}

BOOL CController::DispatchPcToControllerRemote()
{
    char buffer[256];
    size_t bytes;
    BOOL success = TRUE;

    // dispatch for all channels
    bytes = min(sizeof(buffer), m_port->GetFifoLength());
    while (success && m_channelBuffer[pc_to_controller][BUFFER_CHANNEL_REMOTE].Get(buffer, &bytes) && bytes)
    {
        // capture
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
        if (m_capture)
        {
            CString header;

            header.Format("Put %s: ", m_channelId[pc_to_controller]);
            m_captureFile.Capture(header, buffer, bytes, TRUE);
        }
#endif

        // process
        if (!m_port->Write(COMMUNICATIONS_CAPABILITY_REMOTE, buffer, bytes))
        {
            m_fireEventFunction(m_fireEventClass, EVENT_WRITE_FAIL, 0, _T("Error writing port"));
            m_channelBuffer[pc_to_controller][BUFFER_CHANNEL_REMOTE].Flush();
            success = FALSE;
        }

        // set buffer length for next try
        bytes = min(sizeof(buffer), m_port->GetFifoLength());
    }

    return success;
}

BOOL CController::DispatchPcToControllerTfl()
{
    char buffer[256];
    size_t bytes;
    BOOL success = TRUE;

    // dispatch for all channels
    bytes = min(sizeof(buffer), m_port->GetFifoLength());
    while (success && m_channelBuffer[pc_to_controller][BUFFER_CHANNEL_TFL].Get(buffer, &bytes) && bytes)
    {
        // capture
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
        if (m_capture)
        {
            CString header;

            header.Format("Put %s: ", m_channelId[pc_to_controller]);
            m_captureFile.Capture(header, buffer, bytes, TRUE);
        }
#endif

        // process
        if (!m_port->Write(COMMUNICATIONS_CAPABILITY_TFL, buffer, bytes))
        {
            m_fireEventFunction(m_fireEventClass, EVENT_WRITE_FAIL, 0, _T("Error writing port"));
            m_channelBuffer[pc_to_controller][BUFFER_CHANNEL_REMOTE].Flush();
            success = FALSE;
        }

        // set buffer length for next try
        bytes = min(sizeof(buffer), m_port->GetFifoLength());
    }

    return success;
}

BOOL CController::DispatchPcToController()
{
    BOOL success = TRUE;

    // dispatch (only one dispatcher at any one time)
    if (TryEnterCriticalSection(&m_pcToControllerCriticalSection)) {
        // dispatch
        if (IsOpen(COMMUNICATIONS_CAPABILITY_MPE))
            success = DispatchPcToControllerMpe();
        if (success && IsOpen(COMMUNICATIONS_CAPABILITY_REMOTE))
            success = DispatchPcToControllerRemote();
        if (IsOpen(COMMUNICATIONS_CAPABILITY_TFL | COMMUNICATIONS_CAPABILITY_TFLNP))
            success = DispatchPcToControllerTfl();

        // let some other thread dispatch for us
        LeaveCriticalSection(&m_pcToControllerCriticalSection);
    }

    return success;
}

BOOL CController::Write(char p_channel, const char *p_buffer, size_t p_bufferSize) {
	while(p_bufferSize) {
        // write into my buffer
        size_t sent = m_channelBuffer[pc_to_controller][ChannelToBuffer(p_channel)].Put(p_buffer, p_bufferSize);

        // dispatch data from the pc to the controller
        if(!DispatchPcToController()) break;

        // update indices
        p_buffer += sent;
        p_bufferSize -= sent;

    }

	return !p_bufferSize;
}

//-- Capture
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
BOOL CController::OpenCaptureFile(const CString &p_captureFile) {
    return m_captureFile.Open(p_captureFile);
}

void CController::StartCapture(char p_channel) {
    int buffer;

    // if capture is being started on all channels then it is mine
    if (CHANNEL_INVALID == p_channel) {
        m_capture = TRUE;
    }
    // capture is starting on only one channel so store it
    else {
        // get the buffer for this channel
        buffer = ChannelToBuffer(p_channel);

        // set the buffer data
        if(BUFFER_INVALID != buffer) {
            m_channelBuffer[pc_to_controller][buffer].SetCaptureFile(&m_captureFile, NULL);
            m_channelBuffer[controller_to_pc][buffer].SetCaptureFile(NULL, &m_captureFile);
        }
    }
}

void CController::StopCapture(char p_channel) {
    int buffer;

    // if capture is being stopped on all channels then it is mine
    if (CHANNEL_INVALID == p_channel) {
        m_capture = FALSE;
    }
    // capture is stopping on only one channel so store it
    else {
        // get the buffer for this channel
        buffer = ChannelToBuffer(p_channel);

        // set the buffer data
        if(BUFFER_INVALID != buffer) {
            m_channelBuffer[pc_to_controller][buffer].SetCaptureFile(NULL, NULL);
            m_channelBuffer[controller_to_pc][buffer].SetCaptureFile(NULL, NULL);
        }
    }
}

void CController::CloseCaptureFile() {
    m_captureFile.Close();
}
#endif

//-- controller status
BOOL CController::GetStatus() const {
    COLORREF colour;

    if(!m_port) colour = 0;
    else if (m_port->IsError(-1)) colour =  RGB(255, 0, 0);
    else if (!m_port->IsOpen(-1)) colour = RGB(255, 192, 0);
    else if (m_syncActive) colour = RGB(255, 255, 0);
    else colour = RGB(0, 255, 0);

    return colour;
}

#if !defined(_WIN32_WCE) && !defined(__GNUC__)
BOOL CController::ProjectLoad(const char *p_project, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    CProject project(this, p_slowLoad, p_fireEventClass, p_fireEventFunction);
    return project.loadProject(p_project);
}

BOOL CController::ProjectSave(const char *p_project, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    return FALSE;
}

BOOL CController::ProjectCheck(const char *p_project, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    return FALSE;
}

BOOL CController::ProgramLoad(const char *p_program, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    CProject project(this, p_slowLoad, p_fireEventClass, p_fireEventFunction);
    return project.loadProgram(p_program);
}

BOOL CController::ProgramSave(const char *p_program, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    return FALSE;
}

BOOL CController::TextFileLoader(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction, const char *p_source_file, int p_destination, const char *p_destination_file, int p_protocol, BOOL p_compression, int p_compression_level, BOOL p_timeout, int p_timeout_seconds, int p_direction)
{
    CTextFileLoader tfl(p_fireEventClass, p_fireEventFunction, this);
    return tfl.Send(p_source_file, p_destination, p_destination_file, p_protocol, p_compression, TRUE, p_compression_level, p_timeout, p_timeout_seconds, p_direction);
}
#endif
#if !defined(_WIN32_WCE)
BOOL CController::SystemSoftwareLoad(const char *p_coffFile, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    CLoadSystemSoftware loader(this, p_fireEventClass, p_fireEventFunction);
    return loader.Load(p_coffFile);
}
#endif
