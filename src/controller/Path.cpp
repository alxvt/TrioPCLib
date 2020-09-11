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
#include "Path.h"
#include "winioctl.h"
#include "TrioIOCTL.h"

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
CPath::CPath() 
	: m_mpePort(INVALID_HANDLE_VALUE)
	, m_remotePort(INVALID_HANDLE_VALUE)
	, m_memoryPort(INVALID_HANDLE_VALUE)
{
}

CPath::~CPath(void)
{
	Close(-1);
}

void CPath::SetInterfacePath(const char *p_path) {
	m_path = p_path;
}

BOOL CPath::OpenPath(const char *p_portName, HANDLE &p_port) {
	CStringA driverInstanceName;

	// if we don't have an instance then fail
	if (m_path.IsEmpty()) return FALSE;

	// select the driver for this mode
	driverInstanceName.Format("%s_%s", m_path, p_portName);

	// open the file
	p_port = CreateFile(m_path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == p_port) {
		SetLastErrorMessage(GetLastError());
		return FALSE;
	}

	return TRUE;
}

void CPath::ClosePath(HANDLE &p_port)
{
	// if the port is open then close it
	if (p_port != INVALID_HANDLE_VALUE)
	{
		CloseHandle(p_port);
		p_port = INVALID_HANDLE_VALUE;
	}
}

BOOL CPath::WritePath(HANDLE p_port, const char *p_buffer, size_t p_bufferSize)
{
	BOOL	bOK = FALSE;
	DWORD	dwBytesWritten;

	bOK = WriteFile(p_port, p_buffer, (DWORD)p_bufferSize, &dwBytesWritten, NULL);
	if ( bOK )
	{
		bOK = (dwBytesWritten == p_bufferSize);
	}

	return bOK;
}

BOOL CPath::ReadPath(HANDLE p_port, char *p_buffer, size_t *p_bufferSize)
{
	DWORD bufferLength = 0;
	BOOL bRead;

	// read the data
	bRead = ReadFile(p_port, p_buffer, (DWORD)*p_bufferSize, &bufferLength, NULL);

	*p_bufferSize = bufferLength;
	return bRead;
}

BOOL CPath::GetTablePoints(long p_start, long p_points, float p_values[]) {
    TrioGetValueRange_t ValueRange[1];
    DWORD dwBytesReturned;
    BOOL retval = FALSE;

	// build the transfer structure
    ValueRange[0].start = p_start;
    ValueRange[0].stop = p_start + p_points - 1;

	// dispatch
    retval = DeviceIoControl(m_memoryPort, IOCTL_TRIO_GET_TABLE, ValueRange, sizeof(ValueRange), p_values, sizeof(*p_values) * p_points, &dwBytesReturned, NULL);
    if (!retval) {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    retval = TRUE;

fail:
    return retval;
}

BOOL CPath::SetTablePoints(long p_start, long p_points, const double p_values[]) {
    TrioSetValue_t *ValueRange = NULL;
    DWORD dwBytesReturned;
    BOOL retval = FALSE;
    long index;

    // assign memory
    ValueRange = new TrioSetValue_t[p_points];
    if (!ValueRange) {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    // build output block
    for (index = 0;index < p_points;index++) {
        ValueRange[index].offset = p_start + index;
        ValueRange[index].value.f = (float)p_values[index];
    }

	// transfer
    retval = DeviceIoControl(m_memoryPort, IOCTL_TRIO_SET_TABLE, ValueRange, sizeof(*ValueRange) * p_points, 0, 0, &dwBytesReturned, NULL);
    if (!retval) {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    retval = TRUE;

fail:
    if (ValueRange) delete[] ValueRange;
    return retval;
}

BOOL CPath::SetTableBase(long base) {
    double fBaseOut = 0;
    float fBaseIn = 1;
	DWORD retryCount;

	// at most we can have a base of table size - 1024
	fBaseOut = min((double)base, 32000 - 1024);

	// set base
	if (!SetTablePoints(1024, 1, &fBaseOut)) {
		fBaseIn = (float)(fBaseOut - 1.0);
		goto fail;
	}

    // wait for controller to adjust base
	retryCount = 0;
	do {
		// get the controllers table base
		if(!GetTablePoints(1025, 1, &fBaseIn)) {
			fBaseIn = (float)(fBaseOut - 1.0);
			goto fail;
		}

		// if the controller has not updated yet then spin for 1 microsecond
		if (fBaseIn != fBaseOut) {
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

BOOL CPath::SetTable(long p_start, long p_points, const double p_values[])
{
    long blockSize, currentOffset, currentPosition = 0;
    BOOL bOk = FALSE, updateBase;
    float fBase;

	// if we have no memory interface then fail
	if (!IsOpen(COMMUNICATIONS_CAPABILITY_MEMORY)) return FALSE;

	// get the current table base register
    if(!GetTablePoints(1025, 1, &fBase)) goto fail;
    updateBase = (p_start < (long)fBase) || (p_start >= ((long)fBase + 1024));
    blockSize = min(p_points, updateBase ? 1024 : 1024 - (p_start - (long)fBase));

    // write table
    while(p_points > 0) {
        // set the base
        if(updateBase) {
            if(!SetTableBase(p_start)) {
                goto fail;
            }
			if(!GetTablePoints(1025, 1, &fBase)) {
				goto fail;
			}
        }
        else {
            updateBase = TRUE;
        }
        currentOffset = p_start - (long)fBase;

        // set this block
        if(!SetTablePoints(currentOffset, blockSize, &p_values[currentPosition])) {
            goto fail;
        }

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

BOOL CPath::GetTable(long p_start, long lValues, double p_values[]) {
    long blockSize, currentOffset, currentPosition = 0;
    BOOL bOk = FALSE, updateBase;
    float fBase, *floatValues = NULL;

	// if we have no memory interface then fail
	if (!IsOpen(COMMUNICATIONS_CAPABILITY_MEMORY)) return FALSE;

	// assign buffer
	floatValues = new float[1024];

    // get the current table base register
    if(!GetTablePoints(1025, 1, &fBase)) goto fail;
    updateBase = (p_start < (long)fBase) || (p_start >= ((long)fBase + 1024));
    blockSize = min(lValues, updateBase ? 1024 : 1024 - (p_start - (long)fBase));

    // write table
    while(lValues > 0) {
		long index;

        // set the base
        if(updateBase) {
            if(!SetTableBase(p_start)) {
                goto fail;
            }
			if(!GetTablePoints(1025, 1, &fBase)) {
				goto fail;
			}
        }
        else {
            updateBase = TRUE;
        }
        currentOffset = p_start - (long)fBase;

        // set this block
        if(!GetTablePoints(currentOffset, blockSize, floatValues)) {
            goto fail;
        }

		// store this block
		for(index = 0;index < blockSize;index++) {
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
	if(floatValues) delete[] floatValues;
    return bOk;
}

BOOL CPath::GetVr(long p_vr, double *p_value) {
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
    if (!retval) {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

	*p_value = fValue;
    retval = TRUE;

fail:
    return retval;
}

BOOL CPath::SetVr(long p_vr, double p_value) {
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
    if (!retval) {
        SetLastErrorMessage(GetLastError());
        goto fail;
    }

    retval = TRUE;

fail:
    return retval;
}
