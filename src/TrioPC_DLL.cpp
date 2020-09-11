// TrioPC_DLL.cpp : Defines the initialization routines for the DLL.
//

#include "StdAfx.h"
#include "TrioPC_DLL.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(*a))

// CTrioPC_DLLApp

BEGIN_MESSAGE_MAP(CTrioPC_DLLApp, CWinApp)
END_MESSAGE_MAP()


// CTrioPC_DLLApp construction

CTrioPC_DLLApp::CTrioPC_DLLApp()
{
    // TODO: add construction code here,
    // Place all significant initialization in InitInstance
}


// The one and only CTrioPC_DLLApp object

CTrioPC_DLLApp theApp;


// CTrioPC_DLLApp initialization

BOOL CTrioPC_DLLApp::InitInstance()
{
    CWinApp::InitInstance();

    if (!AfxSocketInit())
    {
        AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
        return FALSE;
    }

    return TRUE;
}

// #####################################################################################

#define TRIOPC_EXPORT

#include "ControllerTypes.h"
#include "ControllerLib.h"
#include "TokenLib.h"
#include "TrioPC.h"

#define TPC_STATIC_TABLE_SIZE                   512000
#define TPC_TOKEN_TABLE_PROCESS_TIME    100     // milliseconds
#define MAX_CONTROLLER_AXES              24
#define CONTROLLER_CLASS(c) (((TrioPC_Context_t *)c)->m_controllerClass)
#define TOKEN_CLASS(c) (((TrioPC_Context_t *)c)->m_tokenClass)

enum ECommLinkType {
    eCLT_NoLink = -1, eCLT_USB = 0, eCLT_Serial, eCLT_Ethernet, eCLT_PCI, eCLT_PATH, eCLT_FINS
};

typedef class TrioPC_Context
{
public:
    CString             m_strHost;
    INT32               m_lBoard, m_lCmdProtocol;
    BOOL                m_bFastSerialMode;
    int                 m_PortType;
    void                *m_controllerClass, *m_tokenClass;
    volatile BOOL       m_asyncDispatch;
    HANDLE              m_asyncDispatchThread;
    fireEventFunction_t m_FireCommunicationsEvent;
    void *              m_FireCommunicationsEventContext;

    TrioPC_Context() :
        m_strHost("192.168.0.250"), m_lBoard(0), m_lCmdProtocol(0), m_bFastSerialMode(FALSE),
        m_PortType(-1), m_controllerClass(NULL), m_tokenClass(NULL), m_asyncDispatch(FALSE),
        m_asyncDispatchThread(NULL), m_FireCommunicationsEvent(NULL), m_FireCommunicationsEventContext(NULL)
    {}
} TrioPC_Context_t;

static __declspec(thread) volatile long g_active = 0;

void TRIOPC_DECL *__stdcall TrioPC_CreateContext()
{
    return new TrioPC_Context();
}

void TRIOPC_DECL __stdcall TrioPC_DestroyContext(void *Context)
{
    delete (TrioPC_Context_t *)Context;
}

static BOOL Check(void *Context, BOOL bValue)
{
    // if the command failed then clean up
    if (!bValue) {
        ControllerFlush(CONTROLLER_CLASS(Context), CHANNEL_REMOTE);
    }

    // return status
    return bValue;
}

static BOOL LockMethod()
{
    return InterlockedIncrement(&g_active) == 1;
}

static void UnlockMethod()
{
    InterlockedDecrement(&g_active);
}

static int ConvertPortToCapability(const int portType, const int portId)
{
    int capability = 0;

    // select the capability from the connection parameters
    switch (portType) {
    case eCLT_Serial:
        if (portId > 0) capability = COMMUNICATIONS_CAPABILITY_REMOTE;
        else capability = COMMUNICATIONS_CAPABILITY_MPE;
        break;
    case eCLT_USB:
    case eCLT_PATH:
        capability = (portId == 0) ? COMMUNICATIONS_CAPABILITY_REMOTE : COMMUNICATIONS_CAPABILITY_MPE;
        break;
    case eCLT_FINS:
        capability = COMMUNICATIONS_CAPABILITY_MPE;
        break;
    case eCLT_PCI:
        capability = (portId == 0) ? COMMUNICATIONS_CAPABILITY_REMOTE | COMMUNICATIONS_CAPABILITY_MEMORY : COMMUNICATIONS_CAPABILITY_MPE;
        break;
    case eCLT_Ethernet:
        capability = (!portId || (portId == TRIO_TCP_TOKEN_SOCKET)) ? COMMUNICATIONS_CAPABILITY_REMOTE : COMMUNICATIONS_CAPABILITY_MPE;
        break;
    }

    return capability;
}

/////////////////////////////////////////////////////////////////////////////
// nPortId identifies which port(s) to close =>
//  -1  = all ports
//  0 or TRIO_TOKEN_TCP_PORT   = sync port
//  >1  = async port
//
static void CloseCapability(void *Context, const int p_capability)
{
    // if the controller class exists then delete it
    if (CONTROLLER_CLASS(Context)) {
        // close REMOTE connection
        if ((p_capability & COMMUNICATIONS_CAPABILITY_REMOTE) && ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) {
            // close the port
            ControllerClose(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE | COMMUNICATIONS_CAPABILITY_MEMORY);
        }

        // close MPE connection
        if ((p_capability & COMMUNICATIONS_CAPABILITY_MPE) && ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_MPE)) {
            // stop dispatch thread
            if (((TrioPC_Context_t *)Context)->m_asyncDispatchThread) {
                ((TrioPC_Context_t *)Context)->m_asyncDispatch = FALSE;
                while (WAIT_OBJECT_0 != WaitForSingleObject(((TrioPC_Context_t *)Context)->m_asyncDispatchThread, 100)) {
                    MSG msg;

                    if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) AfxGetApp()->PumpMessage();
                    else Sleep(100);
                }
                CloseHandle(((TrioPC_Context_t *)Context)->m_asyncDispatchThread);
                ((TrioPC_Context_t *)Context)->m_asyncDispatchThread = NULL;
            }

            // close the port
            ControllerClose(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_MPE);
        }

        // if the communications port exists then close it

        // And, if we've just closed both ports, then ...
        if (!ControllerIsOpen(CONTROLLER_CLASS(Context), -1)) {
            // if the token class exists and we have no token connection then delete it
            if (NULL != TOKEN_CLASS(Context)) {
                TokenClassDelete(TOKEN_CLASS(Context));
                TOKEN_CLASS(Context) = NULL;
            }

            // delete the controller
            if (NULL != CONTROLLER_CLASS(Context)) {
                ControllerDestroy(CONTROLLER_CLASS(Context));
                ControllerClassDelete(CONTROLLER_CLASS(Context));
                CONTROLLER_CLASS(Context) = NULL;
            }

            // set color
            ((TrioPC_Context_t *)Context)->m_PortType = -1;
        }
    }
}

static DWORD __stdcall AsyncDispatch(LPVOID Context)
{
    while (((TrioPC_Context_t *)Context)->m_asyncDispatch && CONTROLLER_CLASS(Context)) {
        ControllerDispatchControllerToPc(CONTROLLER_CLASS(Context));
        Sleep(10);
    }

    return 0;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Open(void *Context, INT16 PortType, INT16 PortId, fireEventContext_t FireCommunicationsEventContext, fireEventFunction_t FireCommunicationsEvent)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL    success = FALSE;
    double  f;
    const ECommLinkType     evLinkType = ( ECommLinkType )PortType;
    const int capability = ConvertPortToCapability(PortType, PortId);

    if (!LockMethod()) goto fail;

    ((TrioPC_Context_t *)Context)->m_FireCommunicationsEvent = FireCommunicationsEvent;
    ((TrioPC_Context_t *)Context)->m_FireCommunicationsEventContext = FireCommunicationsEventContext;

    // if there is no controller yet then create it
    if (NULL == CONTROLLER_CLASS(Context)) {
        CONTROLLER_CLASS(Context) = ControllerClassNew(((TrioPC_Context_t *)Context)->m_FireCommunicationsEventContext, ((TrioPC_Context_t *)Context)->m_FireCommunicationsEvent);
    }

    // if there is no token yet then create it
    if (NULL == TOKEN_CLASS(Context)) {
        TOKEN_CLASS(Context) = TokenClassNew(((TrioPC_Context_t *)Context)->m_FireCommunicationsEventContext, ((TrioPC_Context_t *)Context)->m_FireCommunicationsEvent, CONTROLLER_CLASS(Context));
    }

    // if we do not have a token and controller then fail
    if ((NULL == CONTROLLER_CLASS(Context)) || (NULL == TOKEN_CLASS(Context))) {
        success = FALSE;
        OutputDebugString("FAIL (NULL == m_controllerClass) || (NULL == m_tokenClass)\n");
    }
    // if already open then we are done
    else if (ControllerIsOpen(CONTROLLER_CLASS(Context), capability)) {
        success = TRUE;
    }
    else {
        // Ethernet - We can currently only have one communications port open
        // Serial - We can currently only have one communications port open
        if (((evLinkType != eCLT_Ethernet) && (evLinkType != eCLT_Serial)) || !ControllerIsOpen(CONTROLLER_CLASS(Context), -1)) {
            CString port;

            // create interface
            switch (evLinkType) {
            case eCLT_USB:
                success = ControllerCreateUSB(CONTROLLER_CLASS(Context), 0);
                g_lFlushBeforeWrite = 0;
                break;
            case eCLT_Serial:
                port.Format("COM%i", abs(PortId));
                if (capability == COMMUNICATIONS_CAPABILITY_MPE)
                    success = ControllerCreateRS232(CONTROLLER_CLASS(Context), port, ((TrioPC_Context_t *)Context)->m_bFastSerialMode ? port_mode_fast : port_mode_slow);
                else
                    success = ControllerCreateRS232(CONTROLLER_CLASS(Context), port, port_mode_fast);
                g_lFlushBeforeWrite = 0;
                break;
            case eCLT_Ethernet:
                success = ControllerCreateTCP(CONTROLLER_CLASS(Context), ((TrioPC_Context_t *)Context)->m_strHost, PortId == 0 ? 23 : PortId);
                g_lFlushBeforeWrite = 0;
                break;
            case eCLT_PCI:
                success = ControllerCreatePCI(CONTROLLER_CLASS(Context), ((TrioPC_Context_t *)Context)->m_lBoard);
                g_lFlushBeforeWrite = 0;
                break;
            }

            // if success then connect
            if (success)
                success = ControllerOpen(CONTROLLER_CLASS(Context), capability);

            // if success then start token comms
            if (success && (capability & COMMUNICATIONS_CAPABILITY_REMOTE)) {
                // Load token table
                if (TokenGetTokenTable(TOKEN_CLASS(Context)))
                {
                    Sleep(TPC_TOKEN_TABLE_PROCESS_TIME);

                    // Get system version and controller type
                    if (TokenGetVariable(TOKEN_CLASS(Context), "VERSION", &f ))
                    {
                        TokenSetControllerVersion(TOKEN_CLASS(Context), f );

                        if (TokenGetVariable(TOKEN_CLASS(Context), "CONTROL", &f ))
                            TokenSetControllerType(TOKEN_CLASS(Context), (int)f );
                    }
                    else
                        success = FALSE;
                }
                else
                    success = FALSE;
            }

            // if success then start the async comms
            if (success && (capability & COMMUNICATIONS_CAPABILITY_MPE)) {
                // go into MPE mode
                ControllerSetChannelMode(CONTROLLER_CLASS(Context), channel_mode_on, TRUE);

                // start dispatch thread
                ((TrioPC_Context_t *)Context)->m_asyncDispatch = TRUE;
                DWORD threadId;
                ((TrioPC_Context_t *)Context)->m_asyncDispatchThread = CreateThread(NULL, 0, AsyncDispatch, Context, 0, &threadId);
                if (NULL == ((TrioPC_Context_t *)Context)->m_asyncDispatchThread) success = FALSE;
            }

            if (!success) {
                CloseCapability(Context, capability);
            }
        } // if ( *ppCommunicationsPort == NULL)
    }

    if (success) {
        ((TrioPC_Context_t *)Context)->m_PortType = PortType;
    }
    else {
        ((TrioPC_Context_t *)Context)->m_PortType = -1;
    }

    fail: UnlockMethod();
    return success;
}

/////////////////////////////////////////////////////////////////////////////
//
static enum ECommLinkType ConvertPortType(enum port_type_t controllerPortType) {
    enum ECommLinkType ocxPortType = eCLT_NoLink;

    switch (controllerPortType) {
    case port_type_none:            ocxPortType = eCLT_NoLink; break;
    case port_type_rs232:           ocxPortType = eCLT_Serial; break;
    case port_type_usb:             ocxPortType = eCLT_USB; break;
    case port_type_pci:             ocxPortType = eCLT_PCI; break;
    case port_type_ethernet:        ocxPortType = eCLT_Ethernet; break;
    case port_type_fins:            ocxPortType = eCLT_FINS; break;
    case port_type_path:            ocxPortType = eCLT_PATH; break;
    }

    return ocxPortType;
}

void TRIOPC_DECL __stdcall TrioPC_Close(void *Context, INT16 PortId)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    if (!LockMethod()) return;

    if (-1 == PortId) {
        CloseCapability(Context, -1);
    }
    else {
        CloseCapability(Context, ConvertPortToCapability(ConvertPortType(ControllerGetPortType(CONTROLLER_CLASS(Context))), PortId));
    }

    UnlockMethod();
}

BOOL TRIOPC_DECL __stdcall TrioPC_MoveRel(void *Context, INT16 Axes, const DOUBLE* Distance, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL    success = FALSE;
    double  fReturnValue;
    INT32    lAxis = -1; // has to be INT32 for DELPHI

    if (!LockMethod()) goto fail;

    if (ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) {
        if (Axes <= MAX_CONTROLLER_AXES) {
			success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "MOVE", Axes, Distance, Axis, &fReturnValue));
        } // if (Axes <= MAX_CONTROLLER_AXES)
        else
            success = FALSE;
    } // if (ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE) )
    else
        success = FALSE;

    fail: UnlockMethod();

    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Base(void *Context, INT16 Axes, const DOUBLE* Order)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL    success = FALSE;
    double  fReturnValue;

    if (!LockMethod()) goto fail;

    if (ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) {
        if ((Axes <= MAX_CONTROLLER_AXES) && (Axes >= 1)) {
            success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context), "BASE", Axes, Order, &fReturnValue));
        }
        else
            success = FALSE;
    }
    else
        success = FALSE;

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_MoveAbs(void *Context, INT16 Axes, const DOUBLE* Position, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    double  fReturnValue;

    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // if there are too many axes then fail
    if (Axes > MAX_CONTROLLER_AXES) goto fail;

    // move
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "MOVEABS",Axes,Position,Axis, &fReturnValue));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_MoveCirc(void *Context, double FinishBase, double FinishNext, double CentreBase, double CentreNext, INT16 Direction, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fAxis[5], fReturnValue;
    INT32    lAxis = -1; // has to be INT32 for DELPHI

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the parameters
    fAxis[0] = FinishBase;
    fAxis[1] = FinishNext;
    fAxis[2] = CentreBase;
    fAxis[3] = CentreNext;
    fAxis[4] = Direction;

    // move
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "MOVECIRC",5,fAxis,Axis, &fReturnValue));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_AddAxis(void *Context, INT16 LinkAxis, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fAxis[1], fReturnValue;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the parameters
    fAxis[0] = LinkAxis;

    // move
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "ADDAX",1,fAxis,Axis, &fReturnValue));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Cambox(void *Context, INT16 TableStart, INT16 TableStop, double TableMultiplier, double LinkDistance, INT16 LinkAxis, INT16 LinkOptions, double LinkPos, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    INT32    lAxis = -1;
    double  fAxis[7], fReturnValue;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the parameters
    fAxis[0] = TableStart;
    fAxis[1] = TableStop;
    fAxis[2] = TableMultiplier;
    fAxis[3] = LinkDistance;
    fAxis[4] = LinkAxis;
    fAxis[5] = LinkOptions;
    fAxis[6] = LinkPos;

    // move
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "CAMBOX",7,fAxis,Axis, &fReturnValue));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Cam(void *Context, INT16 TableStart, INT16 TableStop, double TableMultiplier, double LinkDistance, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fAxis[4], fReturnValue;
    INT32    lAxis = -1;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the parameters
    fAxis[0] = TableStart;
    fAxis[1] = TableStop;
    fAxis[2] = TableMultiplier;
    fAxis[3] = LinkDistance;

    // move
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "CAM",4,fAxis,Axis, &fReturnValue));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Cancel(void *Context, INT16 Mode, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fAxis[1], fReturnValue;
    INT32    lAxis = -1;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the parameters
    fAxis[0] = Mode;

    // move
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "CANCEL",1,fAxis,Axis, &fReturnValue));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Connect(void *Context, double Ratio, INT16 LinkAxis, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fAxis[2];
    INT32    lAxis = -1;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the parameters
    fAxis[0] = Ratio;
    fAxis[1] = LinkAxis;

    // move
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "CONNECT",2,fAxis,Axis, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Datum(void *Context, INT16 Sequence, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fAxis[1];
    INT32    lAxis = -1;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the parameters
    fAxis[0] = Sequence;

    // move
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "DATUM",1,fAxis,Axis, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Forward(void *Context, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    INT32    lAxis = -1;

    // run the command
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "FORWARD",0,NULL,Axis, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_MoveHelical(void *Context, double FinishBase, double FinishNext, double CentreBase, double CentreNext, INT16 Direction, double LinearDistance, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fAxis[6];
    INT32    lAxis = -1; // has to be INT32 for DELPHI

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the parameters
    fAxis[0] = FinishBase;
    fAxis[1] = FinishNext;
    fAxis[2] = CentreBase;
    fAxis[3] = CentreNext;
    fAxis[4] = Direction;
    fAxis[5] = LinearDistance;

    // move
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "MHELICAL",6,fAxis,Axis, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_MoveLink(void *Context, double Distance, double LinkDistance, double LinkAcceleration, double LinkDeceleration, INT16 LinkAxis, INT16 LinkOptions, double LinkPosition, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fAxis[7];
    INT32    lAxis = -1; // has to be INT32 for DELPHI

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the parameters
    fAxis[0] = Distance;
    fAxis[1] = LinkDistance;
    fAxis[2] = LinkAcceleration;
    fAxis[3] = LinkDeceleration;
    fAxis[4] = LinkAxis;
    fAxis[5] = LinkOptions;
    fAxis[6] = LinkPosition;

    // move
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "MOVELINK",7,fAxis,Axis, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_MoveModify(void *Context, double Position, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fAxis[1];
    INT32    lAxis = -1; // has to be INT32 for DELPHI

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the parameters
    fAxis[0] = Position;

    // move
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "MOVEMODIFY",1,fAxis,Axis, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_RapidStop(void *Context)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // run the command
    success = Check(Context, TokenCommand(TOKEN_CLASS(Context), "RAPIDSTOP"));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Reverse(void *Context, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    INT32    lAxis = -1;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // run the command
    success = Check(Context, TokenMotionCommand(TOKEN_CLASS(Context), "REVERSE",0,NULL,Axis, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Run(void *Context, LPCTSTR Program, const INT Process)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // run the command
    success = Check(Context, TokenProcessCommand(TOKEN_CLASS(Context), "RUN",Program,Process, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Stop(void *Context, LPCTSTR Program, const INT Process)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // run the command
    success = Check(Context, TokenProcessCommand(TOKEN_CLASS(Context), "STOP",Program,Process, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Op(void *Context, INT16 StartChannel, INT16 StopChannel, INT32 Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fAxis[] = { StartChannel, StopChannel, Value };

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // move
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context), "OP", 3, fAxis, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_GetVariable(void *Context, LPCTSTR Variable, double FAR* Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenGetVariable(TOKEN_CLASS(Context), Variable, Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_SetVariable(void *Context, LPCTSTR Variable, double Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenSetVariable(TOKEN_CLASS(Context), Variable,Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Ain(void *Context, INT16 Channel, double *Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fAxis[1];

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the channel
    else fAxis[0] = Channel;

    // move
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context), "AIN",1,fAxis,Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Get(void *Context, INT16 Channel, INT16 FAR* Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return success status of the command
    success = Check(Context, TokenGetCommand(TOKEN_CLASS(Context), Channel,Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Input(void *Context, INT16 Channel, double FAR* Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenInputCommand(TOKEN_CLASS(Context), Channel,Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Key(void *Context, INT16 Channel, INT16 *Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenKeyCommand(TOKEN_CLASS(Context), Channel,Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Linput(void *Context, INT16 Channel, INT16 StartVr)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenLinputCommand(TOKEN_CLASS(Context), Channel,StartVr));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_PswitchOff(void *Context, INT16 Switch, INT16 Hold)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the fixed parameters
	double  afArray[] = { Switch, 0, Hold };

    // return the success status of the command
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context), "PSWITCH", SIZEOF_ARRAY(afArray), afArray, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Pswitch(void *Context, INT16 Switch, INT16 Enable, const INT Axis, const INT OutputNumber, const INT OutputStatus, const double SetPosition, const double ResetPosition)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  afArray[7];

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the fixed parameters
    afArray[0] = Switch;
    afArray[1] = Enable;
    afArray[2] = Axis;
    afArray[3] = OutputNumber;
    afArray[4] = OutputStatus;
    afArray[5] = SetPosition;
    afArray[6] = ResetPosition;

    // return the success status of the command
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context), "PSWITCH",7,afArray, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_ReadPacket(void *Context, INT16 PortNumber, INT16 StartVr, INT16 NumberVr, INT16 Format)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  afArray[4];

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the fixed parameters
    afArray[0] = PortNumber;
    afArray[1] = StartVr;
    afArray[2] = NumberVr;
    afArray[3] = Format;

    // return the success status of the command
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context), "READPACKET",4,afArray, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Regist1(void *Context, INT16 Mode)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  afArray[1];

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the fixed parameters
    afArray[0] = Mode;

    // return the success status of the command
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context),"REGIST",1,afArray, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Regist2(void *Context, INT16 Mode, const double Distance)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  afArray[2];

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the fixed parameters
    afArray[0] = Mode;
    afArray[0] = Distance;

    // return the success status of the command
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context), "REGIST",2,afArray, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Send3(void *Context, INT16 Destination, INT16 Type, INT16 Data1)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  afArray[3];

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the fixed parameters
    afArray[0] = Destination;
    afArray[1] = Type;
    afArray[2] = Data1;

    // return the success status of the command
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context), "SEND",3,afArray, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Send4(void *Context, INT16 Destination, INT16 Type, INT16 Data1, const INT16 Data2)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  afArray[4];

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the fixed parameters
    afArray[0] = Destination;
    afArray[1] = Type;
    afArray[2] = Data1;
    afArray[3] = Data2;

    // return the success status of the command
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context),"SEND",4,afArray, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Setcom(void *Context, INT32 BaudRate, INT16 DataBits, INT16 StopBits, INT16 Parity, INT16 Port, INT32 Control)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  afArray[6];

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // store the fixed parameters
    afArray[0] = BaudRate;
    afArray[1] = DataBits;
    afArray[2] = StopBits;
    afArray[3] = Parity;
    afArray[4] = Port;
    afArray[5] = Control;

    // return the success status of the command
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context), "SETCOM",6,afArray, NULL));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_In(void *Context, INT16 StartChannel, INT16 StopChannel, INT32 FAR* Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    int     nParameters = 0;
    double  afArray[2],fValue;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // if start channel is not specified then this must be a no parameter version
    if (-1 == StartChannel) {
        // if the stop channel is specified then fail
        if (-1 != StopChannel)
            goto fail;
    }
    // there is at lease one parameter
    else {
        // store the start channel
        afArray[nParameters++] = StartChannel;

        // if the stop channel is out of range then fail
        if ((StopChannel < StartChannel) || (StopChannel - StartChannel > 30))
            goto fail;
        // set the second parameter
        else afArray[nParameters++] = StopChannel;
    }

    // get the value
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context), "IN",nParameters,afArray,&fValue));

    // store the return value
    if (success)
        *Value = (INT32)fValue;

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Execute(void *Context, LPCTSTR Command)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // run the command
    success = Check(Context, TokenExecuteCommand(TOKEN_CLASS(Context), Command));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_GetVr(void *Context, INT16 Variable, double FAR* Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // run the command
    success = Check(Context, TokenGetVr(TOKEN_CLASS(Context), Variable, Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_SetVr(void *Context, INT16 Variable, double Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // run the command
    success = Check(Context, TokenSetVr(TOKEN_CLASS(Context), Variable,Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_GetProcessVariable(void *Context, INT16 Variable, INT16 Process, double FAR* Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // local data
    double  fValue;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // if the command fails then fail
    success = !Check(Context, TokenGetNamedVariable(TOKEN_CLASS(Context), Variable,Process,&fValue));

    // set the return value
    if (success)
        *Value = fValue;

    // return ok
    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_SetTable(void *Context, INT32 StartPosition, INT32 NumberOfValues, const DOUBLE Values[])
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    if (ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE) && NumberOfValues <= TPC_STATIC_TABLE_SIZE && NumberOfValues > 0) {
        // Read data via token command
        success = Check(Context,  TokenSetTable(TOKEN_CLASS(Context), StartPosition, NumberOfValues, Values ) );
    } // if ( ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)  && ...
    else
        success = FALSE;

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_New(void *Context, LPCTSTR Program)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // delete the program
    success = Check(Context, TokenNew(TOKEN_CLASS(Context), Program));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Select(void *Context, LPCTSTR Program)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // select the program
    success = Check(Context, TokenSelect(TOKEN_CLASS(Context), Program));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Dir(void *Context, LPTSTR Directory, DWORD DirectorySize, const LPCTSTR Option)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE))
        success = FALSE;
    else {
        // get the directory
	    CString csDir;
        if (Check(Context, TokenDir(TOKEN_CLASS(Context), csDir, Option))) {
            // Correct end of line if <CR>
            if (csDir.Right(1).Compare("\r") == 0)
                csDir += '\n';
            strncpy(Directory, csDir, DirectorySize);
            success = TRUE;
        }
        else
            success = FALSE;
    }

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_InsertLine(void *Context, LPCTSTR Program, INT16 LineNumber, LPCTSTR Line)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // insert the line into the program
    success = Check(Context, TokenInsertLine(TOKEN_CLASS(Context), Program, LineNumber, Line));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_GetTable(void *Context, INT32 StartPosition, INT32 NumberOfValues, DOUBLE Values[])
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    if (ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE) && NumberOfValues <= TPC_STATIC_TABLE_SIZE && NumberOfValues >= 0) {
        // Read data via token command
        success = Check(Context, TokenGetTable(TOKEN_CLASS(Context), StartPosition, NumberOfValues, Values));
    } // if ( ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)  && ...
    else
        success = FALSE;

    fail: UnlockMethod();
    return success;
}

/////////////////////////////////////////////////////////////////////////////
//
void TRIOPC_DECL __stdcall TrioPC_SetHost(void *Context, LPCTSTR strHost)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    if (!TrioPC_IsOpen(Context, -1 )) {
		((TrioPC_Context_t *)Context)->m_strHost = strHost;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
BOOL TRIOPC_DECL __stdcall TrioPC_SendData(void *Context, INT16 Channel, LPCTSTR Data)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    size_t nLen = strlen(Data);

    // if the communications port is closed then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_MPE))
        success = FALSE;
    else
        success = ControllerWrite(CONTROLLER_CLASS(Context), (char)Channel, Data, nLen );

    fail: UnlockMethod();
    return success;
}


/////////////////////////////////////////////////////////////////////////////
//
BOOL TRIOPC_DECL __stdcall TrioPC_GetData(void *Context, INT16 Channel, LPTSTR Data, size_t DataSize)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_MPE))
        success = FALSE;
    else {
        // get the data
        success = ControllerRead(CONTROLLER_CLASS(Context), (char)Channel, Data, &DataSize);

        // Terminate the string, prior to conversion to BSTR.
        Data[DataSize - 1]='\0';
    }

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_IsInvokeAllowed (void *Context, DISPID)
{
    // You can check to see if COleControl::m_bInitialized is FALSE
    // in your automation functions to limit access.
    return TRUE;
}

INT32 TRIOPC_DECL __stdcall TrioPC_GetBoard(void *Context)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    return ((TrioPC_Context_t *)Context)->m_lBoard;
}

void TRIOPC_DECL __stdcall TrioPC_SetBoard(void *Context, INT32 Board)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    if (!TrioPC_IsOpen(Context, -1))
        ((TrioPC_Context_t *)Context)->m_lBoard = Board;
}

INT32 TRIOPC_DECL __stdcall TrioPC_GetCmdProtocol(void *Context)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    return ((TrioPC_Context_t *)Context)->m_lCmdProtocol;
}

void TRIOPC_DECL __stdcall TrioPC_SetCmdProtocol(void *Context, INT32 CmdProtocol)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    ((TrioPC_Context_t *)Context)->m_lCmdProtocol = CmdProtocol;
}

INT32 TRIOPC_DECL __stdcall TrioPC_GetFlushBeforeWrite(void *Context)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    return g_lFlushBeforeWrite;
}

void TRIOPC_DECL __stdcall TrioPC_SetFlushBeforeWrite(void *Context, INT32 FlushBeforeWrite)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    g_lFlushBeforeWrite = FlushBeforeWrite;
}

BOOL TRIOPC_DECL __stdcall TrioPC_GetFastSerialMode(void *Context)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    return ((TrioPC_Context_t *)Context)->m_bFastSerialMode ? VARIANT_TRUE : VARIANT_FALSE;
}

void TRIOPC_DECL __stdcall TrioPC_SetFastSerialMode(void *Context, BOOL FastSerialMode)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    ((TrioPC_Context_t *)Context)->m_bFastSerialMode = !FastSerialMode;
}

BSTR TRIOPC_DECL __stdcall TrioPC_GetHostAddress(void *Context)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    return ((TrioPC_Context_t *)Context)->m_strHost.AllocSysString();
}

void TRIOPC_DECL __stdcall TrioPC_SetHostAddress(void *Context, LPCTSTR Host)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

	TrioPC_SetHost(Context, Host);
}

BOOL TRIOPC_DECL __stdcall TrioPC_IsOpen(void *Context, INT32 lMode)
{
    BOOL    bOpen = FALSE;

    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    switch (lMode) {
    case -1:    // Check all modes for one open
        bOpen = ControllerIsOpen(CONTROLLER_CLASS(Context), -1);
        break;
    case 0:
    case 3240:  // Special case for Ethernet comms
        bOpen = ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE);
        break;
    default:
        bOpen = ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_MPE);
        break;
    }

    return bOpen;
}

INT32 TRIOPC_DECL __stdcall TrioPC_GetConnectionType(void *Context)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    return ((TrioPC_Context_t *)Context)->m_PortType;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Mark(void *Context, INT16 Axis, INT16 *Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenMarkCommand(TOKEN_CLASS(Context), Axis,Value,0));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_MarkB(void *Context, INT16 Axis, INT16 *Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenMarkCommand(TOKEN_CLASS(Context), Axis,Value,1));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_ScopeOnOff(void *Context, BOOL OnOff)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // Do command
    success = Check(Context, TokenScope1(TOKEN_CLASS(Context), (BOOL)OnOff));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Scope(void *Context, BOOL OnOff, const INT32 SamplePeriod, const INT32 TableStart, const INT32 TableEnd, LPCTSTR Params)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    const static int    knMaxParams = 4;
    CStringList         lstParams;
    CString             strDebug;
    CString             csParams;

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // Check parameters
    success = TRUE;
    if ((SamplePeriod > 0) && (TableStart >= 0) && (TableEnd > TableStart)) {
        // Get list of system parameters
        CString strParams = csParams;

        if (strParams.IsEmpty())
            success = FALSE;
        else {
            int     nStartPos = 0,
            nCommaPos = -1,
            nParam = 0;
            CString strAdd;

            do {
                nCommaPos = strParams.Find(',', nStartPos);
                if (nCommaPos == -1) {
#if _MFC_VER >= 0x0700
                    strAdd = strParams.Mid(nStartPos).Trim();
#elif _MFC_VER >= 0x0600
                    strAdd = strParams.Mid(nStartPos);
#else
#error Unsupported MFC version
#endif
                    lstParams.AddTail(strAdd);
                }
                else {
#if _MFC_VER >= 0x0700
                    strAdd = strParams.Mid(nStartPos, nCommaPos - nStartPos).Trim();
#elif _MFC_VER >= 0x0600
                    strAdd = strParams.Mid(nStartPos, nCommaPos - nStartPos);
#else
#error Unsupported MFC version
#endif
                    lstParams.AddTail(strAdd);
                    nStartPos = nCommaPos + 1;
                }
            }
            while (nParam < knMaxParams && nCommaPos > 0);
        } // if (strParams.IsEmpty()) ... else
    } // if (SamplePeriod > 0 && TableStart >= 0 && TableEnd > TableStart)
    else
        success = FALSE;

    if (success) {
        success = Check(Context, TokenScope5(TOKEN_CLASS(Context), OnOff, SamplePeriod, TableStart, TableEnd, lstParams));
    }

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_Trigger(void *Context)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    success = Check(Context, TokenTrigger(TOKEN_CLASS(Context)));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_MechatroLink(void *Context, INT16 Module, INT16 Function, INT16 NumberOfParameters, const DOUBLE * MLParameters, double FAR* pdResult)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    success = Check(Context, TokenMechatroLink(TOKEN_CLASS(Context), Module, Function, NumberOfParameters, MLParameters, pdResult));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_LoadProject(void *Context, LPCTSTR ProjectFile)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

#if 0
    BOOL success = FALSE;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // open this port
    if (ControllerOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_MPE)) {
        // load project
        success = ControllerProjectLoad(CONTROLLER_CLASS(Context), ProjectFile, TRUE, (fireEventClass_t)this, m_FireCommunicationsEvent);

        // close port
        ControllerClose(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_MPE);
    }
#endif

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_LoadSystem(void *Context, LPCTSTR SystemFile)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

#if 0
    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // open this port
    if (ControllerOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_MPE)) {
        // erase all programs
        if (!TokenNew(TOKEN_CLASS(Context), "ALL")) goto fail;

        // load project
        success = ControllerSystemSoftwareLoad(CONTROLLER_CLASS(Context), SystemFile, (fireEventClass_t)this, m_FireCommunicationsEvent);

        // close the async port
        ControllerClose(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_MPE);
    }
#endif
    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_LoadProgram(void *Context, LPCTSTR ProgramFile, BOOL SlowLoad)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // open this port
    if (ControllerOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_MPE)) {
        // fast load requires MPE mode
        if (!SlowLoad)
            ControllerSetChannelMode(CONTROLLER_CLASS(Context), channel_mode_on, FALSE);

        // load program
        success = ControllerProgramLoad(CONTROLLER_CLASS(Context), ProgramFile, SlowLoad, ((TrioPC_Context_t *)Context)->m_FireCommunicationsEventContext, ((TrioPC_Context_t *)Context)->m_FireCommunicationsEvent);

        // close the async port
        ControllerClose(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_MPE);
    }

fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_GetAxisVariable(void *Context, LPCTSTR Variable, INT16 Axis, DOUBLE* Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenGetAxisVariable(TOKEN_CLASS(Context), Variable, Axis, Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_SetAxisVariable(void *Context, LPCTSTR Variable, INT16 Axis, DOUBLE Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenSetAxisVariable(TOKEN_CLASS(Context), Variable, Axis, Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_GetProcVariable(void *Context, LPCTSTR Variable, INT16 Proc, DOUBLE* Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenGetProcessVariable(TOKEN_CLASS(Context), Variable, Proc, Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_SetProcVariable(void *Context, LPCTSTR Variable, INT16 Proc, DOUBLE Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenSetProcessVariable(TOKEN_CLASS(Context), Variable, Proc, Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_GetSlotVariable(void *Context, LPCTSTR Variable, INT16 Slot, DOUBLE* Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenGetSlotVariable(TOKEN_CLASS(Context), Variable, Slot, Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_SetSlotVariable(void *Context, LPCTSTR Variable, INT16 Slot, DOUBLE Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenSetSlotVariable(TOKEN_CLASS(Context), Variable, Slot, Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_GetPortVariable(void *Context, LPCTSTR Variable, INT16 Port, DOUBLE* Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenGetPortVariable(TOKEN_CLASS(Context), Variable, Port, Value));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_SetPortVariable(void *Context, LPCTSTR Variable, INT16 Port, DOUBLE Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // return the success status of the command
    success = Check(Context, TokenSetPortVariable(TOKEN_CLASS(Context), Variable, Port, Value));

    fail: UnlockMethod();
    return success;
}

static CString GetVersionInfo(const char *info)
{
    CString rc;

    // get the file path
    CString path = AfxGetApp()->m_pszHelpFilePath;
    if (path.GetLength() > 3) path = path.Left(path.GetLength() - 3) + "dll";

    // get the size of the version information
    DWORD handle, size = GetFileVersionInfoSize(path, &handle);
    if (size)
    {
        // create the buffer to receive the version information
        char *versionBuffer = new char[size];
        if (versionBuffer != NULL)
        {
            // read the version data
            if(GetFileVersionInfo(path, handle, size, versionBuffer))
            {
                void *buffer;
                UINT length;

                // get the language information
                if (VerQueryValue(versionBuffer, "\\VarFileInfo\\Translation", &buffer, &length) && (length >= 4))
                {
                    struct LANGANDCODEPAGE {
                            WORD wLanguage;
                            WORD wCodePage;
                    };
                    rc.Format("\\StringFileInfo\\%04X%04X\\%s",
                            ((LANGANDCODEPAGE *)buffer)->wLanguage,
                            ((LANGANDCODEPAGE *)buffer)->wCodePage,
                            info
                    );
                }
                else
                    rc.Format("\\StringFileInfo\\%04X04B0\\%s", GetUserDefaultLangID(), info);

                // get the value
                if (VerQueryValue(versionBuffer, rc, &buffer, &length))
                        rc = (char *)buffer;
            }

            // free the version buffer
            delete [] versionBuffer;
        }
    }

    return rc;
}

BOOL TRIOPC_DECL __stdcall TrioPC_ProductVersion(void *Context, LPTSTR Version, size_t VersionSize)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CString info = GetVersionInfo("ProductVersion");
    strncpy(Version, info, VersionSize);
    return TRUE;
}

BOOL TRIOPC_DECL __stdcall TrioPC_ProductName(void *Context, LPTSTR Name, size_t NameSize)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CString info = GetVersionInfo("ProductName");
    strncpy(Name, info, NameSize);
    return TRUE;
}

BOOL TRIOPC_DECL __stdcall TrioPC_StepRatio(void *Context, INT Numerator, INT Denominator, const INT Axis)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // set the step ration
    success = Check(Context, TokenStepRatio(TOKEN_CLASS(Context), Numerator, Denominator, Axis));

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_TextFileLoader(void *Context, const char *p_source_file, int p_destination, const char *p_destination_file, int p_protocol, BOOL p_compression, int p_compression_level, BOOL p_timeout, int p_timeout_seconds, int p_direction) {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    // open the TFL port
    if ((success = ControllerOpen(CONTROLLER_CLASS(Context), (p_protocol == 2) ? COMMUNICATIONS_CAPABILITY_TFL : COMMUNICATIONS_CAPABILITY_TFLNP)) != FALSE)
    {
        // send the file
        success = ControllerTextFileLoader(CONTROLLER_CLASS(Context), ((TrioPC_Context_t *)Context)->m_FireCommunicationsEventContext, ((TrioPC_Context_t *)Context)->m_FireCommunicationsEvent, p_source_file, p_destination, p_destination_file, p_protocol, p_compression, p_compression_level, p_timeout, p_timeout_seconds, p_direction);

        // close the TFL port
        ControllerClose(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_TFL);
    }

    Check(Context, success);

    fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_ReadOp(void *Context, INT16 StartChannel, INT16 StopChannel, INT32 FAR* Value)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    int     nParameters = 0;
    double  afArray[2],fValue;

    // if the communications port is closed or there are no tokens then fail
    if (!ControllerIsOpen(CONTROLLER_CLASS(Context), COMMUNICATIONS_CAPABILITY_REMOTE)) goto fail;

    // if start channel is not specified then this must be a no parameter version
    if (-1 == StartChannel) {
        // if the stop channel is specified then fail
        if (-1 != StopChannel)
            goto fail;
    }
    // there is at lease one parameter
    else {
        // store the start channel
        afArray[nParameters++] = StartChannel;

        // if the stop channel is out of range then fail
        if ((StopChannel < StartChannel) || (StopChannel - StartChannel > 30))
            goto fail;
        // set the second parameter
        else afArray[nParameters++] = StopChannel;
    }

    // get the value
    success = Check(Context, TokenArrayCommand(TOKEN_CLASS(Context), "READ_OP",nParameters,afArray,&fValue));

    // store the return value
    if (success)
        *Value = (INT32)fValue;

fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_PRMBLK_DefineAxis(void *Context, INT32 BlockNumber, LPCTSTR Variable)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    success = Check(Context, TokenPRMBLK_DefineVariable(TOKEN_CLASS(Context), BlockNumber, PRMBLK_Axis, Variable));

fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_PRMBLK_DefineSystem(void *Context, INT32 BlockNumber, LPCTSTR Variable)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    success = Check(Context, TokenPRMBLK_DefineVariable(TOKEN_CLASS(Context), BlockNumber, PRMBLK_System, Variable));

fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_PRMBLK_DefineVr(void *Context, INT32 BlockNumber, INT32 Variable)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    success = Check(Context, TokenPRMBLK_DefineIndex(TOKEN_CLASS(Context), BlockNumber, PRMBLK_Vr, Variable));

fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_PRMBLK_DefineTable(void *Context, INT32 BlockNumber, INT32 Variable)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    success = Check(Context, TokenPRMBLK_DefineIndex(TOKEN_CLASS(Context), BlockNumber, PRMBLK_Table, Variable));

fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_PRMBLK_DefineProgram(void *Context, INT32 BlockNumber, LPCTSTR ProgramName, INT32 ProcessNumber, LPCTSTR Variable)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    success = Check(Context, TokenPRMBLK_DefineProgram(TOKEN_CLASS(Context), BlockNumber, PRMBLK_Program, ProgramName, ProcessNumber, Variable));

fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_PRMBLK_Append(void *Context, INT32 BlockNumber, VARIANT &Variable)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    success = Check(Context, TokenPRMBLK_Append(TOKEN_CLASS(Context), BlockNumber, Variable));

fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_PRMBLK_Get(void *Context, INT32 BlockNumber, BOOL All, VARIANT* Values)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    success = Check(Context, TokenPRMBLK_Get(TOKEN_CLASS(Context), BlockNumber, -1, All, Values));

fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_PRMBLK_GetAxis(void *Context, INT32 BlockNumber, INT32 Axis, BOOL All, VARIANT* Values)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    success = Check(Context, TokenPRMBLK_Get(TOKEN_CLASS(Context), BlockNumber, Axis, All, Values));

fail: UnlockMethod();
    return success;
}

BOOL TRIOPC_DECL __stdcall TrioPC_PRMBLK_Delete(void *Context, INT32 BlockNumber)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BOOL success = FALSE;
    if (!LockMethod()) goto fail;

    success = Check(Context, TokenPRMBLK_Delete(TOKEN_CLASS(Context), BlockNumber));

fail: UnlockMethod();
    return success;
}

extern "C" long g_lLastError;

INT32 TRIOPC_DECL __stdcall TrioPC_GetLastError(void *Context)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    return g_lLastError;
}
