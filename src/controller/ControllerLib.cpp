/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

#include "StdAfx.h"

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "ControllerLib.h"
#include "Controller.h"

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

//
//  Note!
//
//      If this DLL is dynamically linked against the MFC
//      DLLs, any functions exported from this DLL which
//      call into MFC must have the AFX_MANAGE_STATE macro
//      added at the very beginning of the function.
//
//      For example:
//
//      extern "C" BOOL PASCAL EXPORT ExportedFunction()
//      {
//          AFX_MANAGE_STATE(AfxGetStaticModuleState());
//          // normal function body here
//      }
//
//      It is very important that this macro appear in each
//      function, prior to any calls into MFC.  This means that
//      it must appear as the first statement within the
//      function, even before any object variable declarations
//      as their constructors may generate calls into the MFC
//      DLL.
//
//      Please see MFC Technical Notes 33 and 58 for additional
//      details.
//

//-- controller object manipulation
void * ControllerClassNew(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    return new CController(p_fireEventClass, p_fireEventFunction);
}

void ControllerClassDelete(void *p_controllerClass)
{
    if (NULL != p_controllerClass) {
        ControllerDestroy(p_controllerClass);
        delete (CController *)p_controllerClass;
    }
}

void ControllerSetFireEventFunction(void *p_controllerClass, fireEventFunction_t p_fireEventFunction)
{
    if (NULL != p_controllerClass) ((CController *)p_controllerClass)->SetFireEventFunction(p_fireEventFunction);
}

void ControllerSetFireEventClass(void *p_controllerClass, fireEventContext_t p_fireEventClass)
{
    if (NULL != p_controllerClass) ((CController *)p_controllerClass)->SetFireEventClass(p_fireEventClass);
}

fireEventFunction_t ControllerGetFireEventFunction(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? NULL : ((CController *)p_controllerClass)->GetFireEventFunction();
}

fireEventContext_t ControllerGetFireEventClass(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? NULL : ((CController *)p_controllerClass)->GetFireEventClass();
}

//-- connect/disconnect
#ifdef INTERFACE_USB
BOOL ControllerCreateUSB(void *p_controllerClass, int p_driverInstance)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->CreateUSB(p_driverInstance);
}
#endif

#ifdef INTERFACE_SERIAL
BOOL ControllerCreateRS232(void *p_controllerClass, LPCTSTR p_port, enum port_mode_t p_mode)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->CreateRS232(p_port, p_mode);
}
#endif

#ifdef INTERFACE_ETHERNET
BOOL ControllerCreateTCP(void *p_controllerClass, LPCTSTR p_hostname, short p_port)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->CreateTCP(p_hostname, p_port);
}
#endif

#ifdef INTERFACE_PCI
BOOL ControllerCreatePCI(void *p_controllerClass, int p_driverInstance)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->CreatePCI(p_driverInstance);
}
#endif

#ifdef INTERFACE_FINS
BOOL ControllerCreateFins(void *p_controllerClass, LPCTSTR p_hostname, short p_port, char p_unitAddress)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->CreateFins(p_hostname, p_port, p_unitAddress);
}
#endif

#ifdef INTERFACE_PATH
BOOL ControllerCreatePath(void *p_controllerClass, LPCTSTR p_path)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->CreatePath(p_path);
}
#endif

BOOL ControllerOpen(void *p_controllerClass, int p_capabilities)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->Open(p_capabilities);
}

void ControllerClose(void *p_controllerClass, int p_capabilities)
{
    if (NULL != p_controllerClass) ((CController *)p_controllerClass)->Close(p_capabilities);
}

void ControllerDestroy(void *p_controllerClass)
{
    ControllerClose(p_controllerClass, -1);
    if (NULL != p_controllerClass) ((CController *)p_controllerClass)->Destroy();
}

//-- controller status
void ControllerSetChannelMode(void *p_controllerClass, enum channel_mode_t p_channelMode, BOOL p_force)
{
    if (NULL != p_controllerClass) ((CController *)p_controllerClass)->SetChannelMode(p_channelMode, p_force);
}

const enum channel_mode_t ControllerGetChannelMode(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? channel_mode_off : ((CController *)p_controllerClass)->GetChannelMode();
}

BOOL ControllerIsOpen(void *p_controllerClass, int p_capabilities)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->IsOpen(p_capabilities);
}

BOOL ControllerIsError(void *p_controllerClass, int p_capabilities)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->IsError(p_capabilities);
}

//-- port status
BOOL ControllerGetStatus(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->GetStatus();
}

enum port_type_t ControllerGetPortType(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? port_type_none : ((CController *)p_controllerClass)->GetPortType();
}

//-- port capabilities
BOOL ControllerCanRemote(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->CanRemote();
}

BOOL ControllerCanDownload(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->CanDownload();
}

BOOL ControllerCanMemory(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->CanMemory();
}

//-- port operations
BOOL ControllerGetTable(void *p_controllerClass, long p_start, long p_points, double p_values[])
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->GetTable(p_start, p_points, p_values);
}

BOOL ControllerSetTable(void *p_controllerClass, long p_start, long p_points, const double p_values[])
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->SetTable(p_start, p_points, p_values);
}

BOOL ControllerGetVr(void *p_controllerClass, long p_vr, double *p_value)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->GetVr(p_vr, p_value);
}

BOOL ControllerSetVr(void *p_controllerClass, long p_vr, double p_value)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->SetVr(p_vr, p_value);
}

//-- capture
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
BOOL ControllerOpenCaptureFile(void *p_controllerClass, const CString &p_captureFile)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->OpenCaptureFile(p_captureFile);
}

void ControllerStartCapture(void *p_controllerClass, char p_channel)
{
    if (NULL != p_controllerClass) ((CController *)p_controllerClass)->StartCapture(p_channel);
}

void ControllerStopCapture(void *p_controllerClass, char p_channel)
{
    if (NULL != p_controllerClass) ((CController *)p_controllerClass)->StopCapture(p_channel);
}

void ControllerCloseCaptureFile(void *p_controllerClass)
{
    if (NULL != p_controllerClass) ((CController *)p_controllerClass)->CloseCaptureFile();
}
#endif

//-- IO
BOOL ControllerFlush(void *p_controllerClass, char p_channel)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->Flush(p_channel);
}

BOOL ControllerRead(void *p_controllerClass, char p_channel, char *p_buffer, size_t *p_bufferSize)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->Read(p_channel, p_buffer, p_bufferSize);
}

BOOL ControllerWrite(void *p_controllerClass, char p_channel, const char *p_buffer, size_t p_bufferSize)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->Write(p_channel, p_buffer, p_bufferSize);
}

BOOL ControllerDispatchControllerToPc(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->DispatchControllerToPc();
}

BOOL ControllerDispatchPcToController(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->DispatchPcToController();
}

#if !defined(_WIN32_WCE) && !defined(__GNUC__)
BOOL ControllerProjectLoad(void *p_controllerClass, LPCTSTR p_fullProjectPath, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->ProjectLoad(p_fullProjectPath, p_slowLoad, p_fireEventClass, p_fireEventFunction);
}

BOOL ControllerProjectSave(void *p_controllerClass, LPCTSTR p_project, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->ProjectSave(p_project, p_slowLoad, p_fireEventClass, p_fireEventFunction);
}

BOOL ControllerProjectCheck(void *p_controllerClass, LPCTSTR p_project, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->ProjectCheck(p_project, p_slowLoad, p_fireEventClass, p_fireEventFunction);
}

BOOL ControllerProgramLoad(void *p_controllerClass, LPCTSTR p_fullProgramPath, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->ProgramLoad(p_fullProgramPath, p_slowLoad, p_fireEventClass, p_fireEventFunction);
}

BOOL ControllerProgramSave(void *p_controllerClass, LPCTSTR p_program, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->ProgramSave(p_program, p_slowLoad, p_fireEventClass, p_fireEventFunction);
}

BOOL ControllerTextFileLoader(void *p_controllerClass, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction, LPCTSTR p_source_file, int p_destination, LPCTSTR p_destination_file, int p_protocol, BOOL p_compression, int p_compression_level, BOOL p_timeout, int p_timeout_seconds, int p_direction)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->TextFileLoader(p_fireEventClass, p_fireEventFunction, p_source_file, p_destination, p_destination_file, p_protocol, p_compression, p_compression_level, p_timeout, p_timeout_seconds, p_direction);
}
#endif

#if !defined(_WIN32_WCE)
BOOL ControllerSystemSoftwareLoad(void *p_controllerClass, LPCTSTR p_coffFile, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->SystemSoftwareLoad(p_coffFile, p_fireEventClass, p_fireEventFunction);
}
#endif

void ControllerSetCancel(void *p_controllerClass, BOOL p_cancel)
{
    if (NULL != p_controllerClass) ((CController *)p_controllerClass)->SetCancel(p_cancel);
}

BOOL ControllerGetCancel(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->GetCancel();
}

BOOL ControllerIsEscapeRemoteChannel(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->IsEscapeRemoteChannel();
}

int ControllerGetOpenCapabilities(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? 0: ((CController *)p_controllerClass)->GetOpenCapabilities();
}

void * ControllerGetPort(void *p_controllerClass)
{
    return (NULL == p_controllerClass) ? NULL: ((CController *)p_controllerClass)->GetPort();
}

void ControllerGetLastErrorMessage(void *p_controllerClass, CString &p_errorMessage)
{
    if (NULL != p_controllerClass) ((CController *)p_controllerClass)->GetLastErrorMessage(p_errorMessage);
}

BOOL ControllerSetInterfaceTCP(void *p_controllerClass, int p_sndbufSize)
{
    return (NULL == p_controllerClass) ? FALSE : ((CController *)p_controllerClass)->SetInterfaceTCP(p_sndbufSize);
}
