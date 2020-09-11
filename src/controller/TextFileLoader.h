/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

#if !defined(__TEXTFILELOADER_H__) && !defined(_WIN32_WCE)
#define __TEXTFILELOADER_H__

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#define RECEIVE_FLAG_TIMEOUT 1
#define RECEIVE_FLAG_CANCEL 2
#define RECEIVE_FLAG_CRC 4
#define RECEIVE_FLAG_DONE 8

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include "Crc.h"

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
class CTextFileLoader
{
protected:
    class CController *m_controller;
    fireEventContext_t m_fireEventClass;
    fireEventFunction_t m_fireEventFunction;
	volatile unsigned long m_receiveFlags, m_receivedCrc, m_xoffFlag, m_transfer_successfull;
    int m_protocol, m_compression_level, m_destination, m_timeout_seconds;
    BOOL m_compression, m_decompression, m_timeout, m_direction;
	CString m_source_file, m_destination_file;
	unsigned long m_calculatedCrc;
    CFile m_transfer_file;
    CCRC m_crc;

	static DWORD __stdcall SendThread(LPVOID p_params) { 
		((CTextFileLoader *)p_params)->SendFile();
		((CTextFileLoader *)p_params)->m_receiveFlags |= RECEIVE_FLAG_DONE;
		return 0;
	}
	static DWORD __stdcall RecvThread(LPVOID p_params) { 
		((CTextFileLoader *)p_params)->RecvFile();
		((CTextFileLoader *)p_params)->m_receiveFlags |= RECEIVE_FLAG_DONE;
		return 0;
	}
	void SendFile();
	void RecvFile();
    template<class T> void swap(T &a, T &b) { T c(a); a = b; b = c; }

public:
    CTextFileLoader(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction, void *p_ControllerClass);   // constructor

    BOOL Send(
        const CString &p_source_file, 
        int p_destination, const CString &p_destination_file,
        int p_protocol,
        BOOL p_compression, int p_compression_level, BOOL p_decompression,
        BOOL p_timeout, int p_timeout_seconds,
		int p_direction
        );
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
