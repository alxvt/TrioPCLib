/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#if !defined(__LOADSYSTEMSOFTWARE_H__) && !defined(_WIN32_WCE)
#define __LOADSYSTEMSOFTWARE_H__

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "Controller.h"

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
class CLoadSystemSoftware
{
protected:
    CString m_coff_file;
    volatile BOOL m_run;
    class CController *m_controller;
    fireEventFunction_t m_fireEventFunction;
    fireEventContext_t m_fireEventClass;

protected:
#ifdef _WIN32
    static DWORD __stdcall LoadThread(LPVOID p_this) { return ((CLoadSystemSoftware *)p_this)->LoadThread(); }
#else
    static void *LoadThread(LPVOID p_this) { return (void *)((CLoadSystemSoftware *)p_this)->LoadThread(); }
#endif
    DWORD LoadThread();
    BOOL write(long p_value, long p_recv);
    BOOL write(const char *p_send, size_t p_send_len, const char *p_recv, size_t p_recv_len);
    BOOL write(const char *p_send, const char *p_recv);

public:
    CLoadSystemSoftware(class CController *p_controller, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
    virtual ~CLoadSystemSoftware();

    BOOL Load(const char *p_coffFile);
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
