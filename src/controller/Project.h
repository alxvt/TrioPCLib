/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

#if !defined(__PROJECT_H__) && !defined(_WIN32_WCE) && !defined(__GNUC__)
#define __PROJECT_H__

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include "Controller.h"

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
class CProject
{
protected:
    CStringList m_programPending;
    char m_projectPath[MAX_PATH];
    volatile unsigned long m_exitFlags, m_runFlags;
    CString m_currentFileName, m_currentProgramName, m_commandRequest, m_commandResponse;
    size_t m_commandResponseIndex;
    CController *m_controller;
    fireEventFunction_t m_fireEventFunction;
    fireEventContext_t m_fireEventClass;
    BOOL m_slowLoad;

    BOOL write(const char p_channel, const char *p_send, const char *p_recv);
    BOOL write(const char p_channel, const char *p_send, size_t p_send_len, const char *p_recv, size_t p_recv_len);
    int base64(char *p_buffer,int p_value);

    void AnalyzeExitStatus();

    void transmitThreadFast();
    void receiveThreadFast();

    BOOL SendAndReceiveCommand(const int channel);
    BOOL New(const int channel, const CString &program);
    BOOL Select(const int channel, const CString &program, const CString &programType);
    void transmitThreadSlow();
    void receiveThreadSlow();

    DWORD transmitThread();
    DWORD receiveThread();

#ifdef _WIN32
    static DWORD __stdcall transmitThread(LPVOID p_params) { return ((CProject *)p_params)->transmitThread(); }
    static DWORD __stdcall receiveThread(LPVOID p_params)  { return ((CProject *)p_params)->receiveThread(); }
#else
    static void *transmitThread(LPVOID p_params) { return (void *)((CProject *)p_params)->transmitThread(); }
    static void *receiveThread(LPVOID p_params)  { return (void *)((CProject *)p_params)->receiveThread(); }
#endif

public:
    CProject(CController *p_controller, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
    ~CProject(void);

    BOOL loadProject(const char *p_fullProjectPath);
    BOOL saveProject(const char *p_fullProjectPath);
    BOOL checkProject(const char *p_fullProjectPath);
    BOOL loadProgram(const char *p_fullProgramPath);
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
