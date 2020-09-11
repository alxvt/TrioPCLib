/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __CONTROLLERDLL_H__
#define __CONTROLLERDLL_H__
#if defined(_WIN32) && !defined(__AFXWIN_H__)
    #error include 'stdafx.h' before including this file for PCH
#endif

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

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif
extern long g_lMPEMode;

//-- controller object manipulation
void * ControllerClassNew(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEvent);
void ControllerClassDelete(void *p_controllerClass);
void ControllerSetFireEventFunction(void *p_controllerClass, fireEventFunction_t p_fireEventFunction);
void ControllerSetFireEventClass(void *p_controllerClass, fireEventContext_t p_fireEventClass);
fireEventFunction_t ControllerGetFireEventFunction(void *p_controllerClass);
fireEventContext_t ControllerGetFireEventClass(void *p_controllerClass);

//-- connect/disconnect
BOOL ControllerCreateUSB(void *p_controllerClass, int p_driverInstance);
BOOL ControllerCreatePCI(void *p_controllerClass, int p_driverInstance);
BOOL ControllerCreateRS232(void *p_controllerClass, LPCTSTR p_port, enum port_mode_t p_mode);
BOOL ControllerCreateTCP(void *p_controllerClass, LPCTSTR p_hostname, short p_port);
BOOL ControllerSetInterfaceTCP(void *p_controllerClass, int p_sndbufSize);
BOOL ControllerCreateFins(void *p_controllerClass, LPCTSTR p_hostname, short p_port, char p_unitAddress);
BOOL ControllerCreatePath(void *p_controllerClass, LPCTSTR p_path);
BOOL ControllerOpen(void *p_controllerClass, int p_capabilities);
void ControllerClose(void *p_controllerClass, int p_capabilities);
void ControllerDestroy(void *p_controllerClass);

//-- controller status
void ControllerSetChannelMode(void *p_controllerClass, enum channel_mode_t p_channelMode, BOOL p_force);
const enum channel_mode_t ControllerGetChannelMode(void *p_controllerClass);
BOOL ControllerIsOpen(void *p_controllerClass, int p_capabilities);
BOOL ControllerIsError(void *p_controllerClass, int p_capabilities);

//-- port status
BOOL ControllerGetStatus(void *p_controllerClass);
enum port_type_t ControllerGetPortType(void *p_controllerClass);

//-- port capabilities
int ControllerGetOpenCapabilities(void *p_controllerClass);
BOOL ControllerCanRemote(void *p_controllerClass);
BOOL ControllerCanDownload(void *p_controllerClass);
BOOL ControllerCanMemory(void *p_controllerClass);
BOOL ControllerIsEscapeRemoteChannel(void *p_controllerClass);

//-- port operations
BOOL ControllerGetTable(void *p_controllerClass, long p_start, long p_points, double p_values[]);
BOOL ControllerSetTable(void *p_controllerClass, long p_start, long p_points, const double p_values[]);
BOOL ControllerGetVr(void *p_controllerClass, long p_vr, double *p_value);
BOOL ControllerSetVr(void *p_controllerClass, long p_vr, double p_value);

//-- capture
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
BOOL ControllerOpenCaptureFile(void *p_controllerClass, const CString &p_captureFile);
void ControllerStartCapture(void *p_controllerClass, char p_channel);
void ControllerStopCapture(void *p_controllerClass, char p_channel);
void ControllerCloseCaptureFile(void *p_controllerClass);
#endif

//-- I/O
BOOL ControllerFlush(void *p_controllerClass, char p_channel);
BOOL ControllerRead(void *p_controllerClass, char p_channel, char *p_buffer, size_t *p_bufferSize);
BOOL ControllerWrite(void *p_controllerClass, char p_channel, const char *p_buffer, size_t p_bufferSize);
BOOL ControllerDispatchControllerToPc(void *p_controllerClass);
BOOL ControllerDispatchPcToController(void *p_controllerClass);

//-- Project
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
BOOL ControllerProjectLoad(void *p_controllerClass, LPCTSTR p_fullProjectPath, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
BOOL ControllerProjectSave(void *p_controllerClass, LPCTSTR p_fullProjectPath, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
BOOL ControllerProjectCheck(void *p_controllerClass, LPCTSTR p_fullProjectPath, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
BOOL ControllerProgramLoad(void *p_controllerClass, LPCTSTR p_fullProgramPath, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
BOOL ControllerProgramSave(void *p_controllerClass, LPCTSTR p_fullProgramPath, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);

//-- file transfer
BOOL ControllerTextFileLoader(void *p_controllerClass, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction, LPCTSTR p_source_file, int p_destination, LPCTSTR p_destination_file, int p_protocol, BOOL p_compression, int p_compression_level, BOOL p_timeout, int p_timeout_seconds, int p_direction);
#endif
#if !defined(_WIN32_WCE)
BOOL ControllerSystemSoftwareLoad(void *p_controllerClass, LPCTSTR p_coffFile, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction);
#endif

//-- Support routines
void ControllerSetCancel(void *p_controllerClass, BOOL p_cancel);
BOOL ControllerGetCancel(void *p_controllerClass);
void ControllerGetLastErrorMessage(void *p_controllerClass, CString &p_errorMessage);

// DIRTY HACKS
void * ControllerGetPort(void *p_controllerClass);
#ifdef __cplusplus
}
#endif

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/
#endif

