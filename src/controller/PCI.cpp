/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

#include "StdAfx.h"

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "PCI.h"
#ifdef _WIN32
#include "winioctl.h"
#include "TrioIOCTL.h"
#else
#include "triopci_ioctl.h"
#endif

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
CPCI::CPCI()
: m_mpePort(INVALID_HANDLE_VALUE)
, m_remotePort(INVALID_HANDLE_VALUE)
, m_memoryPort(INVALID_HANDLE_VALUE)
, m_driverInstance(-1)
{
}

CPCI::~CPCI(void)
{
    Close(-1);
}

void CPCI::SetInterfacePCI(int p_driverInstance)
{
    m_driverInstance = p_driverInstance;
}

BOOL CPCI::OpenPCI(const char *p_portName, HANDLE &p_port)
{
    // if we don't have an instance then fail
    if (-1 == m_driverInstance) return FALSE;

#ifdef _WIN32
    // select the driver for this mode
    CString driverInstanceName;
    driverInstanceName.Format(_T("\\\\.\\TrioPCI-%03i\\%s"), m_driverInstance, p_portName);

    // open the file
    p_port = CreateFile(driverInstanceName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == p_port)
    {
        SetLastErrorMessage(GetLastError());
        return FALSE;
    }
#else
    // select the driver for this mode
    CString driverInstanceName;
    driverInstanceName.Format("/dev/triopci_%s%i", p_portName, m_driverInstance);

    // open the file
    p_port = (HANDLE)open(driverInstanceName, O_RDWR);
    if (INVALID_HANDLE_VALUE == p_port)
    {
        SetLastErrorMessage(GetLastError());
        return FALSE;
    }
#endif

    // if we get here we have been successful
    return TRUE;
}

void CPCI::ClosePCI(HANDLE &p_port)
{
    // if the port is open then close it
    if (p_port != INVALID_HANDLE_VALUE)
    {
        CloseHandle(p_port);
        p_port = INVALID_HANDLE_VALUE;
    }
}

BOOL CPCI::WritePCI(HANDLE p_port, const char *p_buffer, size_t p_bufferSize)
{
    BOOL    bWrite;
    DWORD   dwBytesWritten;

    // write data
    bWrite = WriteFile(p_port, p_buffer, (DWORD)p_bufferSize, &dwBytesWritten, NULL);
    if (!bWrite)
    {
        SetLastErrorMessage(GetLastError());
    }

    return bWrite && (dwBytesWritten == p_bufferSize);
}

BOOL CPCI::ReadPCI(HANDLE p_port, char *p_buffer, size_t *p_bufferSize)
{
    DWORD bufferLength = 0;
    BOOL bRead;

    // read the data
    bRead = ReadFile(p_port, p_buffer, (DWORD)*p_bufferSize, &bufferLength, NULL);
    if (!bRead)
    {
        SetLastErrorMessage(GetLastError());
        *p_bufferSize = 0;
    }
    else
    {
        *p_bufferSize = bufferLength;
    }

    return bRead;
}

#ifdef _WIN32
BOOL CPCI::GetTablePoints(long p_start, long p_points, float p_values[])
{
    TrioGetValueRange_t ValueRange[1];
    DWORD dwBytesReturned;
    BOOL retval = FALSE;

    // build the transfer structure
    ValueRange[0].start = p_start;
    ValueRange[0].stop = p_start + p_points - 1;

    // dispatch
    retval = DeviceIoControl(m_memoryPort, IOCTL_TRIO_GET_TABLE, ValueRange, sizeof(ValueRange), p_values, sizeof(*p_values) * p_points, &dwBytesReturned, NULL);
    if (!retval)
    {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    retval = TRUE;

    fail:
    return retval;
}

BOOL CPCI::SetTablePoints(long p_start, long p_points, const double p_values[])
{
    TrioSetValue_t *ValueRange = NULL;
    DWORD dwBytesReturned;
    BOOL retval = FALSE;
    long index;

    // assign memory
    ValueRange = new TrioSetValue_t[p_points];
    if (!ValueRange)
    {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    // build output block
    for (index = 0;index < p_points;index++)
    {
        ValueRange[index].offset = p_start + index;
        ValueRange[index].value.f = (float)p_values[index];
    }

    // transfer
    retval = DeviceIoControl(m_memoryPort, IOCTL_TRIO_SET_TABLE, ValueRange, sizeof(*ValueRange) * p_points, 0, 0, &dwBytesReturned, NULL);
    if (!retval)
    {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    retval = TRUE;

    fail:
    if (ValueRange) delete[] ValueRange;
    return retval;
}

BOOL CPCI::SetTableBase(long base)
{
    double fBaseOut = 0;
    float fBaseIn = 1;
    DWORD retryCount;

    // at most we can have a base of table size - 1024
    fBaseOut = min((double)base, 32000 - 1024);

    // set base
    if (!SetTablePoints(1024, 1, &fBaseOut))
    {
        fBaseIn = (float)(fBaseOut - 1.0);
        goto fail;
    }

    // wait for controller to adjust base
    retryCount = 0;
    do
    {
        // get the controllers table base
        if (!GetTablePoints(1025, 1, &fBaseIn))
        {
            fBaseIn = (float)(fBaseOut - 1.0);
            goto fail;
        }

        // if the controller has not updated yet then spin for 1 microsecond
        if (fBaseIn != fBaseOut)
        {
            // increment retry count
            retryCount++;

            // wait
            SpinWait(1);
        }
    }
    while (fBaseIn != fBaseOut);

    fail:
    return fBaseOut == fBaseIn;
}

BOOL CPCI::GetVr(long p_vr, double *p_value)
{
    BOOL retval = FALSE;
    TrioGetValueRange_t ValueRange[1];
    DWORD dwBytesReturned;
    float fValue;

    // if we have no memory interface then fail
    if (!IsOpen(COMMUNICATIONS_CAPABILITY_MEMORY)) return FALSE;

    // initialise access data
    ValueRange[0].start = p_vr;
    ValueRange[0].stop = p_vr;

    // dispatch
    retval = DeviceIoControl(m_memoryPort, IOCTL_TRIO_GET_VR, ValueRange, sizeof(ValueRange), &fValue, sizeof(fValue), &dwBytesReturned, NULL);
    if (!retval)
    {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    *p_value = fValue;
    retval = TRUE;

    fail:
    return retval;
}

BOOL CPCI::SetVr(long p_vr, double p_value)
{
    BOOL retval = FALSE;
    CString csString;
    TrioSetValue_t Value[1];
    DWORD dwBytesReturned;

    // if we have no memory interface then fail
    if (!IsOpen(COMMUNICATIONS_CAPABILITY_MEMORY)) return FALSE;

    // initialise access data
    Value[0].offset = p_vr;
    Value[0].value.f = (float)p_value;

    /* write data */
    retval = DeviceIoControl(m_memoryPort, IOCTL_TRIO_SET_VR, Value, sizeof(Value), NULL, 0, &dwBytesReturned, NULL);
    if (!retval)
    {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    retval = TRUE;

    fail:
    return retval;
}
#else
BOOL CPCI::SetVr(long p_vr, double p_value)
{
    int retval = 0;
    CString csString;
    TrioValue_t Value;
    float f = (float)p_value;

    // if we have no memory interface then fail
    if (!IsOpen(COMMUNICATIONS_CAPABILITY_MEMORY)) return FALSE;

    // initialise access data
    Value.start = p_vr;
    Value.stop = p_vr;
    Value.data = &f;

    /* write data */
    retval = ioctl((int)m_memoryPort, IOCTL_TRIO_SET_VR, &Value);
    if (retval < 0)
    {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

fail:
    return retval < 0 ? FALSE : TRUE;
}

BOOL CPCI::GetVr(long p_vr, double *p_value)
{
    int retval = 0;
    CString csString;
    TrioValue_t Value;
    float f;

    // if we have no memory interface then fail
    if (!IsOpen(COMMUNICATIONS_CAPABILITY_MEMORY)) return FALSE;

    // initialise access data
    Value.start = p_vr;
    Value.stop = p_vr;
    Value.data = &f;

    /* write data */
    retval = ioctl((int)m_memoryPort, IOCTL_TRIO_GET_VR, &Value);
    if (retval < 0)
    {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    *p_value = f;

fail:
    return retval < 0 ? FALSE : TRUE;
}

BOOL CPCI::GetTablePoints(long p_start, long p_points, float p_values[])
{
    TrioValue_t Value;
    int retval = 0;

    // build the transfer structure
    Value.start = p_start;
    Value.stop = p_start + p_points - 1;
    Value.data = p_values;

    // dispatch
    retval = ioctl((int)m_memoryPort, IOCTL_TRIO_GET_TABLE, &Value);
    if (retval < 0)
    {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

fail:
    return retval < 0 ? FALSE : TRUE;
}

BOOL CPCI::SetTablePoints(long p_start, long p_points, double p_values[])
{
    TrioValue_t Value;
    int retval = 0, index;

    // assign memory
    Value.start = p_start;
    Value.stop = p_start + p_points - 1;
    Value.data = new float[p_points];
    if (!Value.data)
    {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    // store data
    for (index = 0;index < p_points;index++)
        Value.data[index] = (float)p_values[index];

    // transfer
    retval = ioctl((int)m_memoryPort, IOCTL_TRIO_SET_TABLE, &Value);
    if (retval < 0)
    {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    retval = TRUE;

fail:
    if (Value.data)
        delete[] Value.data;
    return retval < 0 ? FALSE : TRUE;
}

BOOL CPCI::SetTableBase(long base)
{
    double fBaseOut = 0;
    float fBaseIn = 1;
    DWORD retryCount;

    // at most we can have a base of table size - 1024
    fBaseOut = min((double)base, 32000 - 1024);

    // set base
    if (!SetTablePoints(1024, 1, &fBaseOut))
    {
        fBaseIn = (float)(fBaseOut - 1.0);
        goto fail;
    }

    // wait for controller to adjust base
    retryCount = 0;
    do
    {
        // get the controllers table base
        if (!GetTablePoints(1025, 1, &fBaseIn))
        {
            fBaseIn = (float)(fBaseOut - 1.0);
            goto fail;
        }

        // if the controller has not updated yet then spin for 1 microsecond
        if (fBaseIn != fBaseOut)
        {
            // increment retry count
            retryCount++;

            // wait
            SpinWait(1);
        }
    }
    while (fBaseIn != fBaseOut);

    fail:
    return fBaseOut == fBaseIn;
}
#endif

BOOL CPCI::SetTable(long p_start, long p_points, const double p_values[])
{
    long blockSize, currentOffset, currentPosition = 0;
    BOOL bOk = FALSE, updateBase;
    float fBase;

    // if we have no memory interface then fail
    if (!IsOpen(COMMUNICATIONS_CAPABILITY_MEMORY)) return FALSE;

    // get the current table base register
    if (!GetTablePoints(1025, 1, &fBase))
        goto fail;
    updateBase = (p_start < (long)fBase) || (p_start >= ((long)fBase + 1024));
    blockSize = min(p_points, updateBase ? 1024 : 1024 - (p_start - (long)fBase));

    // write table
    while (p_points > 0)
    {
        // set the base
        if (updateBase)
        {
            if (!SetTableBase(p_start))
                goto fail;
            if (!GetTablePoints(1025, 1, &fBase))
                goto fail;
        }
        else
            updateBase = TRUE;
        currentOffset = p_start - (long)fBase;

        // set this block
        if (!SetTablePoints(currentOffset, blockSize, &p_values[currentPosition]))
            goto fail;

        // increment indices
        p_points -= blockSize;
        currentPosition += blockSize;
        p_start += blockSize;
        blockSize = min(p_points, 1024);
    }

    // if we get here then we have been successful
    bOk = TRUE;

    fail:
    return bOk;
}

BOOL CPCI::GetTable(long p_start, long lValues, double p_values[])
{
    long blockSize, currentOffset, currentPosition = 0;
    BOOL bOk = FALSE, updateBase;
    float fBase, *floatValues = NULL;

    // if we have no memory interface then fail
    if (!IsOpen(COMMUNICATIONS_CAPABILITY_MEMORY)) return FALSE;

    // assign buffer
    floatValues = new float[1024];

    // get the current table base register
    if (!GetTablePoints(1025, 1, &fBase)) goto fail;
    updateBase = (p_start < (long)fBase) || (p_start >= ((long)fBase + 1024));
    blockSize = min(lValues, updateBase ? 1024 : 1024 - (p_start - (long)fBase));

    // write table
    while (lValues > 0)
    {
        long index;

        // set the base
        if (updateBase)
        {
            if (!SetTableBase(p_start))
            {
                goto fail;
            }
            if (!GetTablePoints(1025, 1, &fBase))
            {
                goto fail;
            }
        }
        else
        {
            updateBase = TRUE;
        }
        currentOffset = p_start - (long)fBase;

        // set this block
        if (!GetTablePoints(currentOffset, blockSize, floatValues))
        {
            goto fail;
        }

        // store this block
        for (index = 0;index < blockSize;index++)
        {
            p_values[currentPosition + index] = floatValues[index];
        }

        // increment indices
        lValues -= blockSize;
        currentPosition += blockSize;
        p_start += blockSize;
        blockSize = min(lValues, 1024);
    }

    // if we get here then we have been successful
    bOk = TRUE;

    fail:
    if (floatValues) delete[] floatValues;
    return bOk;
}

