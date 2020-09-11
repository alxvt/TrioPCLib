#ifdef TRIOPC_EXPORT
#define TRIOPC_DECL __declspec(dllexport)
#else
#define TRIOPC_DECL __declspec(dllimport)
#endif

#ifndef __FIREEVENT__
typedef void * fireEventContext_t;
typedef BOOL (__stdcall *fireEventFunction_t)(fireEventContext_t fireEventContext, const int event_type, const long integerData, LPCTSTR message);
#endif

#ifndef __CONTROLLER_TYPES__
#define EVENT_WRITE_FAIL                    0
#define EVENT_READ_FAIL                     1
#define EVENT_MESSAGE                       2
#define EVENT_RECEIVE                       3
#define EVENT_BUFFER_OVERRUN                4
#define EVENT_PROGRESS_CREATE               5
#define EVENT_PROGRESS_DESTROY              6
#define EVENT_PROGRESS_SET_POS              7
#define EVENT_GET_CONTROLLER_KEY            8
#endif

#ifdef __cplusplus
extern "C"
{
#endif

// TrioPC_DLL Context
void TRIOPC_DECL * __stdcall TrioPC_CreateContext();
void TRIOPC_DECL __stdcall TrioPC_DestroyContext(void *Context);

// Connection Data
void TRIOPC_DECL __stdcall TrioPC_SetHost(void *Context, LPCTSTR strData);
INT32 TRIOPC_DECL __stdcall TrioPC_GetBoard(void *Context);
void TRIOPC_DECL __stdcall TrioPC_SetBoard(void *Context, INT32 Board);
BSTR TRIOPC_DECL __stdcall TrioPC_GetHostAddress(void *Context);
void TRIOPC_DECL __stdcall TrioPC_SetHostAddress(void *Context, LPCTSTR Host);
INT32 TRIOPC_DECL __stdcall TrioPC_GetConnectionType(void *Context);
INT32 TRIOPC_DECL __stdcall TrioPC_GetCmdProtocol(void *Context);
void TRIOPC_DECL __stdcall TrioPC_SetFlushBeforeWrite(void *Context, INT32 FlushBeforeWrite);
INT32 TRIOPC_DECL __stdcall TrioPC_GetFlushBeforeWrite(void *Context);
void TRIOPC_DECL __stdcall TrioPC_SetFastSerialMode(void *Context, BOOL FastSerialMode);
BOOL TRIOPC_DECL __stdcall TrioPC_GetFastSerialMode(void *Context);
void TRIOPC_DECL __stdcall TrioPC_SetCmdProtocol(void *Context, INT32 CmdProtocol);
INT32 TRIOPC_DECL __stdcall TrioPC_GetLastError(void *Context);

// Controller Data
BOOL TRIOPC_DECL __stdcall TrioPC_ProductVersion(void *Context, LPTSTR Version, size_t VersionSize);
BOOL TRIOPC_DECL __stdcall TrioPC_ProductName(void *Context, LPTSTR Name, size_t NameSize);

// Connection Management
void TRIOPC_DECL __stdcall TrioPC_Close(void *Context, INT16 PortId);
BOOL TRIOPC_DECL __stdcall TrioPC_Open(void *Context, INT16 PortType, INT16 PortId, fireEventContext_t FireCommunicationsEventContext, fireEventFunction_t FireCommunicationsEvent);

// Variables
BOOL TRIOPC_DECL __stdcall TrioPC_GetAxisVariable(void *Context, LPCTSTR Variable, INT16 Axis, DOUBLE* Value);
BOOL TRIOPC_DECL __stdcall TrioPC_SetAxisVariable(void *Context, LPCTSTR Variable, INT16 Axis, DOUBLE Value);
BOOL TRIOPC_DECL __stdcall TrioPC_GetProcVariable(void *Context, LPCTSTR Variable, INT16 Proc, DOUBLE* Value);
BOOL TRIOPC_DECL __stdcall TrioPC_SetProcVariable(void *Context, LPCTSTR Variable, INT16 Proc, DOUBLE Value);
BOOL TRIOPC_DECL __stdcall TrioPC_GetSlotVariable(void *Context, LPCTSTR Variable, INT16 Slot, DOUBLE* Value);
BOOL TRIOPC_DECL __stdcall TrioPC_SetSlotVariable(void *Context, LPCTSTR Variable, INT16 Slot, DOUBLE Value);
BOOL TRIOPC_DECL __stdcall TrioPC_GetPortVariable(void *Context, LPCTSTR Variable, INT16 Port, DOUBLE* Value);
BOOL TRIOPC_DECL __stdcall TrioPC_SetPortVariable(void *Context, LPCTSTR Variable, INT16 Port, DOUBLE Value);
BOOL TRIOPC_DECL __stdcall TrioPC_GetVr(void *Context, INT16 Variable, double FAR* Value);
BOOL TRIOPC_DECL __stdcall TrioPC_SetVr(void *Context, INT16 Variable, double Value);
BOOL TRIOPC_DECL __stdcall TrioPC_GetVariable(void *Context, LPCTSTR Variable, double FAR* Value);
BOOL TRIOPC_DECL __stdcall TrioPC_SetVariable(void *Context, LPCTSTR Variable, double Value);
BOOL TRIOPC_DECL __stdcall TrioPC_GetProcessVariable(void *Context, INT16 Variable, INT16 Process, DOUBLE* Value);
BOOL TRIOPC_DECL __stdcall TrioPC_GetTable(void *Context, INT32 StartPosition, INT32 NumberOfValues, DOUBLE Values[]);
BOOL TRIOPC_DECL __stdcall TrioPC_SetTable(void *Context, INT32 StartPosition, INT32 NumberOfValues, const DOUBLE Values[]);

// Tokens
BOOL TRIOPC_DECL __stdcall TrioPC_MoveRel(void *Context, INT16 Axes, const DOUBLE* Distance, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_Base(void *Context, INT16 Axes, const INT *Order);
BOOL TRIOPC_DECL __stdcall TrioPC_MoveAbs(void *Context, INT16 Axes, const DOUBLE* Position, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_MoveCirc(void *Context, double FinishBase, double FinishNext, double CentreBase, double CentreNext, INT16 Direction, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_AddAxis(void *Context, INT16 LinkAxis, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_Cambox(void *Context, INT16 TableStart, INT16 TableStop, double TableMultiplier, double LinkDistance, INT16 LinkAxis, INT16 LinkOptions, double LinkPos, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_Cam(void *Context, INT16 TableStart, INT16 TableStop, double TableMultiplier, double LinkDistance, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_Cancel(void *Context, INT16 Mode, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_Connect(void *Context, double Ratio, INT16 LinkAxis, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_Datum(void *Context, INT16 Sequence, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_Forward(void *Context, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_MoveHelical(void *Context, double FinishBase, double FinishNext, double CentreBase, double CentreNext, INT16 Direction, double LinearDistance, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_MoveLink(void *Context, double Distance, double LinkDistance, double LinkAcceleration, double LinkDeceleration, INT16 LinkAxis, INT16 LinkOptions, double LinkPosition, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_MoveModify(void *Context, double Position, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_RapidStop(void *Context);
BOOL TRIOPC_DECL __stdcall TrioPC_Reverse(void *Context, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_Run(void *Context, LPCTSTR Program, const INT Process);
BOOL TRIOPC_DECL __stdcall TrioPC_Stop(void *Context, LPCTSTR Program, const INT Process);
BOOL TRIOPC_DECL __stdcall TrioPC_Op(void *Context, INT16 StartChannel, INT16 StopChannel, INT32 Value);
BOOL TRIOPC_DECL __stdcall TrioPC_Ain(void *Context, INT16 Channel,double *Value);
BOOL TRIOPC_DECL __stdcall TrioPC_Get(void *Context, INT16 Channel, INT16 FAR* Value);
BOOL TRIOPC_DECL __stdcall TrioPC_Input(void *Context, INT16 Channel, double FAR* Value);
BOOL TRIOPC_DECL __stdcall TrioPC_Key(void *Context, INT16 Channel, INT16 *Value);
BOOL TRIOPC_DECL __stdcall TrioPC_Mark(void *Context, INT16 Axis, INT16 *Value);
BOOL TRIOPC_DECL __stdcall TrioPC_MarkB(void *Context, INT16 Axis, INT16 *Value);
BOOL TRIOPC_DECL __stdcall TrioPC_Linput(void *Context, INT16 Channel, INT16 StartVr);
BOOL TRIOPC_DECL __stdcall TrioPC_PswitchOff(void *Context, INT16 Switch, INT16 Hold);
BOOL TRIOPC_DECL __stdcall TrioPC_Pswitch(void *Context, INT16 Switch, INT16 Enable, const INT Axis, const INT OutputNumber, const INT OutputStatus, const double SetPosition, const double ResetPosition);
BOOL TRIOPC_DECL __stdcall TrioPC_ReadPacket(void *Context, INT16 PortNumber, INT16 StartVr, INT16 NumberVr, INT16 Format);
BOOL TRIOPC_DECL __stdcall TrioPC_Regist1(void *Context, INT16 Mode);
BOOL TRIOPC_DECL __stdcall TrioPC_Regist2(void *Context, INT16 Mode, const double Distance);
BOOL TRIOPC_DECL __stdcall TrioPC_Send3(void *Context, INT16 Destination, INT16 Type, INT16 Data1, const VARIANT FAR& Data2);
BOOL TRIOPC_DECL __stdcall TrioPC_Send4(void *Context, INT16 Destination, INT16 Type, INT16 Data1, const VARIANT FAR& Data2);
BOOL TRIOPC_DECL __stdcall TrioPC_Setcom(void *Context, INT32 BaudRate, INT16 DataBits, INT16 StopBits, INT16 Parity, INT16 Port, INT32 Control);
BOOL TRIOPC_DECL __stdcall TrioPC_In(void *Context, INT16 StartChannel, INT16 StopChannel, INT32 * Value);
BOOL TRIOPC_DECL __stdcall TrioPC_Execute(void *Context, LPCTSTR Command);
BOOL TRIOPC_DECL __stdcall TrioPC_New(void *Context, LPCTSTR Program);
BOOL TRIOPC_DECL __stdcall TrioPC_Select(void *Context, LPCTSTR Program);
BOOL TRIOPC_DECL __stdcall TrioPC_Dir(void *Context, LPTSTR Directory, DWORD DirectorySize, LPCTSTR Option);
BOOL TRIOPC_DECL __stdcall TrioPC_InsertLine(void *Context, LPCTSTR Program, INT16 LineNumber, LPCTSTR Line);
BOOL TRIOPC_DECL __stdcall TrioPC_SendData(void *Context, INT16 Channel, LPCTSTR Data);
BOOL TRIOPC_DECL __stdcall TrioPC_GetData(void *Context, INT16 Channel, LPTSTR Data, size_t DataSize);
BOOL TRIOPC_DECL __stdcall TrioPC_MechatroLink(void *Context, INT16 Module, INT16 Function, INT16 NumberOfParameters, const DOUBLE * MLParameters, double * pdResult);
BOOL TRIOPC_DECL __stdcall TrioPC_LoadProject(void *Context, LPCTSTR ProjectFile);
BOOL TRIOPC_DECL __stdcall TrioPC_LoadSystem(void *Context, LPCTSTR SystemFile);
BOOL TRIOPC_DECL __stdcall TrioPC_LoadProgram(void *Context, LPCTSTR SystemFile, BOOL SlowLoad);
BOOL TRIOPC_DECL __stdcall TrioPC_StepRatio(void *Context, INT Numerator, INT Denominator, const INT Axis);
BOOL TRIOPC_DECL __stdcall TrioPC_TextFileLoader(void *Context, const char *p_source_file, int p_destination, const char *p_destination_file, int p_protocol, BOOL p_compression, int p_compression_level, BOOL p_timeout, int p_timeout_seconds, int p_direction);
BOOL TRIOPC_DECL __stdcall TrioPC_IsOpen(void *Context, INT32 lMode);
BOOL TRIOPC_DECL __stdcall TrioPC_ScopeOnOff(void *Context, BOOL OnOff);
BOOL TRIOPC_DECL __stdcall TrioPC_Scope(void *Context, BOOL OnOff, const INT32 SamplePeriod, const INT32 TableStart, const INT32 TableEnd, LPCTSTR CaptureParams);
BOOL TRIOPC_DECL __stdcall TrioPC_Trigger(void *Context);
BOOL TRIOPC_DECL __stdcall TrioPC_ReadOp(void *Context, INT16 StartChannel, INT16 StopChannel, INT32 FAR* Value);

#ifdef __cplusplus
}
#endif
