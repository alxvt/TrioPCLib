/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __COMMUNICATIONS_H__
#define __COMMUNICATIONS_H__

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "ControllerTypes.h"

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
class CCommunications
{
protected:
	enum port_mode_t m_portMode;
	enum port_type_t m_portType;
	CString m_lastErrorMessage;
    int m_openCapabilities, m_errorCapabilities;

protected:
	//-- open
	virtual BOOL OpenMPE() = 0;
	virtual BOOL OpenRemote() = 0;
	virtual BOOL OpenMemory() = 0;
	virtual BOOL OpenTextFileLoader() = 0;
	virtual BOOL OpenTextFileLoaderNP() = 0;

	//-- close
	virtual void CloseMPE() = 0;
	virtual void CloseRemote() = 0;
	virtual void CloseMemory() = 0;
	virtual void CloseTextFileLoader() = 0;
	virtual void CloseTextFileLoaderNP() = 0;

	//-- io
	virtual BOOL ReadMPE(char *p_buffer, size_t *p_bufferSize) = 0;
	virtual BOOL ReadRemote(char *p_buffer, size_t *p_bufferSize) = 0;
	virtual BOOL ReadTfl(char *p_buffer, size_t *p_bufferSize) = 0;
	virtual BOOL WriteMPE(const char *p_buffer,size_t p_bufferSize) = 0;
	virtual BOOL WriteRemote(const char *p_buffer,size_t p_bufferSize) = 0;
	virtual BOOL WriteTfl(const char *p_buffer,size_t p_bufferSize) = 0;

	//-- utilities
#ifdef _WIN32
	void SpinWait(DWORD p_microSeconds) const {
        FILETIME ft = {0};
		ULARGE_INTEGER t1, t0;

		// system time is in 100 nanosecond increments
		p_microSeconds *= 10;

		// spin for 1 microsecond
#ifdef _WIN32_WCE
        SYSTEMTIME st;
		GetSystemTime(&st);
        SystemTimeToFileTime(&st, &ft);
#else
		GetSystemTimeAsFileTime(&ft);
#endif
		t0.HighPart = ft.dwHighDateTime;
		t0.LowPart = ft.dwLowDateTime;
		do {
#ifdef _WIN32_WCE
            SYSTEMTIME st;
		    GetSystemTime(&st);
            SystemTimeToFileTime(&st, &ft);
#else
			GetSystemTimeAsFileTime(&ft);
#endif
			t1.HighPart = ft.dwHighDateTime;
			t1.LowPart = ft.dwLowDateTime;
		}
		while ((t1.QuadPart - t0.QuadPart) < p_microSeconds);
	}
#else
    #define SpinWait(micro_seconds) usleep(micro_seconds)
#endif

#ifdef _WIN32
	void SetLastErrorMessage(DWORD p_errorCode) {
		TCHAR *buffer;

		// convert the error code to a string
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            p_errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
            (TCHAR *)&buffer,
            0,
            NULL
        );

		// store the error message
		m_lastErrorMessage = (char *)buffer;

		// remove trailing carriage return
		while (m_lastErrorMessage.Right(2) == "\r\n"){
			m_lastErrorMessage = m_lastErrorMessage.Left(m_lastErrorMessage.GetLength() - 2);
		}

        // Free the buffer.
        LocalFree(buffer);
	}
#else
    void SetLastErrorMessage(int error_code)
    {
        m_lastErrorMessage = strerror(error_code);
    }
#endif

	void SetLastErrorMessage(CString &p_errorMessage) { m_lastErrorMessage = p_errorMessage; }
	void SetLastErrorMessage(const char *p_errorMessage) { m_lastErrorMessage = p_errorMessage; }

public:
    volatile BOOL m_cancel;

	//-- constructor/destructor
	CCommunications(void) 
		: m_portMode(port_mode_normal)
        , m_portType(port_type_none)
		, m_openCapabilities(0) 
        , m_errorCapabilities(0)
        , m_cancel(0) {}
	
	virtual ~CCommunications(void) {}

	//-- set parameters
    virtual void SetInterfaceUSB(int p_driverInstance) {};
	virtual void SetInterfaceRS232(const char *p_portName, port_mode_t p_portMode) {};
    virtual void SetInterfaceTCP(const char *p_hostname, short p_mpePortNumber, short p_remotePortNumber, short p_tflPort, short p_tflnpPort) {};
	virtual void SetInterfaceTCP(DWORD p_ipAddress, short p_mpePortNumber, short p_remotePortNumber, short p_tflPort, short p_tflnpPort) {}
    virtual void SetInterfaceTCP(int p_sndbufSize) {};
    virtual void SetInterfacePCI(int p_driverInstance) {};
	virtual void SetInterfaceFins(const char *p_hostname, short p_port, char p_unitAddress) {}
	virtual void SetInterfaceFins(DWORD p_ipAddress, short p_port,char p_unitAddress) {}
	virtual void SetInterfacePath(const char *p_path) {}
	virtual BOOL SetMode(enum port_mode_t p_portMode) { return FALSE; }

	//-- open/close
	virtual BOOL Open(int p_capabilities = COMMUNICATIONS_CAPABILITY_MPE) {
		BOOL isOpen = TRUE;

		// if the port is already open then we are done
		if(IsOpen(p_capabilities)) return TRUE;

		// open all the requested interfaces
		if (isOpen && (p_capabilities & COMMUNICATIONS_CAPABILITY_MPE)        && (isOpen = OpenMPE()))    
            m_openCapabilities |= COMMUNICATIONS_CAPABILITY_MPE;
		if (isOpen && (p_capabilities & COMMUNICATIONS_CAPABILITY_REMOTE)     && (isOpen = OpenRemote())) 
            m_openCapabilities |= COMMUNICATIONS_CAPABILITY_REMOTE;
		if (isOpen && (p_capabilities & COMMUNICATIONS_CAPABILITY_MEMORY)     && (isOpen = OpenMemory())) 
            m_openCapabilities |= COMMUNICATIONS_CAPABILITY_MEMORY;
        // we only allow one of these two
	    if (isOpen && (p_capabilities & COMMUNICATIONS_CAPABILITY_TFL)        && (isOpen = OpenTextFileLoader())) 
            m_openCapabilities |= COMMUNICATIONS_CAPABILITY_TFL;
	    else if (isOpen && (p_capabilities & COMMUNICATIONS_CAPABILITY_TFLNP) && (isOpen = OpenTextFileLoaderNP())) 
            m_openCapabilities |= COMMUNICATIONS_CAPABILITY_TFL;

		// return open status
		return isOpen;
	}
	virtual void Close(int p_capabilities = -1) {
		// close all the requested interfaces that are open
		p_capabilities &= m_openCapabilities;

		// close all the requested interfaces
		if (p_capabilities & COMMUNICATIONS_CAPABILITY_MPE) {
			// close port
			CloseMPE();

			// clear status
			m_openCapabilities &= ~COMMUNICATIONS_CAPABILITY_MPE;
			m_errorCapabilities &= ~COMMUNICATIONS_CAPABILITY_MPE;
		}
		if (p_capabilities & COMMUNICATIONS_CAPABILITY_REMOTE) {
			// close port
			CloseRemote();

			// clear status
			m_openCapabilities &= ~COMMUNICATIONS_CAPABILITY_REMOTE;
			m_errorCapabilities &= ~COMMUNICATIONS_CAPABILITY_REMOTE;
		}
		if (p_capabilities & COMMUNICATIONS_CAPABILITY_MEMORY) {
			// close port
			CloseMemory();

			// clear status
			m_openCapabilities &= ~COMMUNICATIONS_CAPABILITY_MEMORY;
			m_errorCapabilities &= ~COMMUNICATIONS_CAPABILITY_MEMORY;
		}
		if (p_capabilities & COMMUNICATIONS_CAPABILITY_TFL) {
			// close port
			CloseTextFileLoader();

			// clear status
			m_openCapabilities &= ~COMMUNICATIONS_CAPABILITY_TFL;
			m_errorCapabilities &= ~COMMUNICATIONS_CAPABILITY_TFL;
		}
		if (p_capabilities & COMMUNICATIONS_CAPABILITY_TFLNP) {
			// close port
			CloseTextFileLoaderNP();

			// clear status
			m_openCapabilities &= ~COMMUNICATIONS_CAPABILITY_TFLNP;
			m_errorCapabilities &= ~COMMUNICATIONS_CAPABILITY_TFLNP;
		}
	}

	//-- status
	BOOL IsOpen(int p_capabilities = COMMUNICATIONS_CAPABILITY_MPE) const { return (m_openCapabilities & p_capabilities) ? TRUE : FALSE; }
	BOOL IsError(int p_capabilities = COMMUNICATIONS_CAPABILITY_MPE) const { return m_errorCapabilities & p_capabilities ? TRUE : FALSE; }
	unsigned long GetOpenCapabilities() const { return m_openCapabilities; }
	virtual enum port_type_t GetType() const = 0;
	virtual size_t GetFifoLength() const = 0;
	virtual size_t GetSystemSoftwareDownloadFifoLength() const = 0;
	virtual BOOL CanRemote() const = 0;
	virtual BOOL CanDownload() const = 0;
	virtual BOOL CanMemory() const = 0;
	virtual BOOL CanTextFileLoader() const = 0;
	virtual BOOL IsEscapeRemoteChannel() const = 0;
	void GetLastErrorMessage(CString &p_message) const { p_message = m_lastErrorMessage; }

	//-- direct transfer operations
	virtual BOOL GetTable(long p_start, long p_points, double p_values[]) = 0;
	virtual BOOL SetTable(long p_start, long p_points, const double p_values[]) = 0;
	virtual BOOL GetVr(long p_vr, double *p_value) = 0;
	virtual BOOL SetVr(long p_vr, double p_value) = 0;

	//-- IO
	virtual BOOL Write(int p_capabilities, const char *p_buffer, size_t p_bufferSize) {
		// write all the requested interfaces
		BOOL retval = (!(p_capabilities & COMMUNICATIONS_CAPABILITY_MPE) || !IsOpen(COMMUNICATIONS_CAPABILITY_MPE) || WriteMPE(p_buffer, p_bufferSize)) &&
					  (!(p_capabilities & COMMUNICATIONS_CAPABILITY_REMOTE) || !IsOpen(COMMUNICATIONS_CAPABILITY_REMOTE) || WriteRemote(p_buffer, p_bufferSize)) &&
					  (!(p_capabilities & COMMUNICATIONS_CAPABILITY_TFL) || !IsOpen(COMMUNICATIONS_CAPABILITY_TFL) || WriteTfl(p_buffer, p_bufferSize));

		// set or clear the error status depending on what happened
		if (!retval) m_errorCapabilities |= p_capabilities;
		else m_errorCapabilities &= ~p_capabilities;

		// return the success status
		return retval;
	}
	virtual BOOL Read(int p_capabilities, char *p_buffer, size_t *p_bufferSize) {
		// read all the requested interfaces
		BOOL retval = (!(p_capabilities & COMMUNICATIONS_CAPABILITY_MPE) || !IsOpen(COMMUNICATIONS_CAPABILITY_MPE) || ReadMPE(p_buffer, p_bufferSize)) &&
				      (!(p_capabilities & COMMUNICATIONS_CAPABILITY_REMOTE) || !IsOpen(COMMUNICATIONS_CAPABILITY_REMOTE) || ReadRemote(p_buffer, p_bufferSize)) &&
				      (!(p_capabilities & COMMUNICATIONS_CAPABILITY_TFL) || !IsOpen(COMMUNICATIONS_CAPABILITY_TFL) || ReadTfl(p_buffer, p_bufferSize));

		// set or clear the error status depending on what happened
		if (!retval) m_errorCapabilities |= p_capabilities;
		else m_errorCapabilities &= ~p_capabilities;

		// return the success status
		return retval;
	}
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
