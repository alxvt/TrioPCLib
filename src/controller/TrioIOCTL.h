
#define TRIO_IOCTL_INDEX  0x0000


#define IOCTL_TRIO_GET_CONFIG_DESCRIPTOR     CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_GET_DEVICE_DESCRIPTOR     CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+1,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_GET_STRING_DESCRIPTOR     CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+2,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_GET_INTERFACE_DESCRIPTOR  CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+3,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_GET_ENDPOINT_DESCRIPTOR   CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+4,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_SELECT_CONFIGURATION      CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+5,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_GET_SELECT_CONFIGURATION  CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+6,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_RESET_PORT                CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+7,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_DEBUG                        CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+8,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_GET_VR                       CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+9,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_SET_VR                       CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+10,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_GET_TABLE                    CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+11,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_SET_TABLE                    CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+12,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_GET_INPUT                    CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+13,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_SET_OUTPUT                   CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+14,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_GET_OUTPUT                   CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+15,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

#define IOCTL_TRIO_GET_ANALOGUE_INPUT           CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                      TRIO_IOCTL_INDEX+16,\
                                                      METHOD_BUFFERED,\
                                                      FILE_ANY_ACCESS)

typedef struct tag_TrioGetValueRange {
    ULONG start, stop;
} TrioGetValueRange_t;

typedef struct tag_TrioSetValue {
    ULONG offset;
    union {
        float f;
        long l;
    } value;
} TrioSetValue_t;

