/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __CONTROLLERTYPES_H__
#define __CONTROLLERTYPES_H__
#define COMMUNICATIONS_CAPABILITY_MPE               1
#define COMMUNICATIONS_CAPABILITY_REMOTE            2
#define COMMUNICATIONS_CAPABILITY_MEMORY            4
#define COMMUNICATIONS_CAPABILITY_TFL               8
#define COMMUNICATIONS_CAPABILITY_TFLNP            16

#define CHANNEL_INVALID    -99
#define CHANNEL_REMOTE      98
#define CHANNEL_TFL         97

#define EVENT_WRITE_FAIL                    0
#define EVENT_READ_FAIL                     1
#define EVENT_MESSAGE                       2
#define EVENT_RECEIVE                       3
#define EVENT_BUFFER_OVERRUN                4
#define EVENT_PROGRESS_CREATE               5
#define EVENT_PROGRESS_DESTROY              6
#define EVENT_PROGRESS_SET_POS              7
#define EVENT_GET_CONTROLLER_KEY            8

#define TRIO_TCP_TOKEN_SOCKET               3240
#define TRIO_TCP_TFL_SOCKET                 10001
#define TRIO_TCP_TFLNP_SOCKET               3241
#define TRIO_TCP_DEFAULT_HOST_ADDRESS       "192.168.0.250"

#define TRUE 1
#define FALSE 0

#ifdef _WIN32
#define INTERFACE_SERIAL
#define INTERFACE_ETHERNET
#define INTERFACE_PCI
#define INTERFACE_FINS
#define INTERFACE_PATH
#define INTERFACE_USB
#else
#define INTERFACE_ETHERNET
#define INTERFACE_PCI
#endif

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
#ifndef _WIN32
//#define BOOL unsigned int
//#define INT int
//#define LPCTSTR const char *
//typedef unsigned int BOOL;
//typedef const char * LPCTSTR;
//typedef int INT;
#endif

#define __FIREEVENT__
typedef void * fireEventContext_t;
typedef BOOL (__stdcall *fireEventFunction_t)(fireEventContext_t fireEventClass, const int event_type, const long integerData, LPCTSTR message);
enum PRMBLK_Types { PRMBLK_Axis, PRMBLK_System, PRMBLK_Vr, PRMBLK_Table, PRMBLK_Program };

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
enum channel_direction_t { pc_to_controller, controller_to_pc, channel_directions };
enum channel_mode_t { channel_mode_off, channel_mode_on, channel_mode_download };
enum port_type_t { port_type_none, port_type_rs232, port_type_usb, port_type_pci, port_type_ethernet, port_type_fins, port_type_path };
enum port_mode_t { port_mode_normal, port_mode_slow, port_mode_fast, port_mode_download};

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/
#endif
