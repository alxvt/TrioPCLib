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
#include "FIFO.h"
#include "CaptureFile.h"

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

CFIFO::CFIFO(size_t p_fifoSize)
    : m_head(0)
    , m_tail(0)
    , m_putCaptureFile(NULL)
    , m_getCaptureFile(NULL)
{
    // initialise the critical section
    InitializeCriticalSection(&m_criticalSection);

    // fifo size in range
    if (p_fifoSize > MAX_FIFO_BUFFER_SIZE)
        p_fifoSize = MAX_FIFO_BUFFER_SIZE;

    // fifo size as a power of 2
    size_t bit;
    for (bit = 1; bit <= p_fifoSize; bit <<= 1);
    p_fifoSize = bit >> 1;

    // store fifo dimensions
    m_fifoSize = p_fifoSize;
    m_fifoMask = p_fifoSize - 1;
}

CFIFO::~CFIFO(void)
{
    DeleteCriticalSection(&m_criticalSection);
}

size_t CFIFO::Put(const char *buffer, size_t bufferSize) {
    size_t sent = 0;

    // gain exclusive access to the buffer. We cannot assure that assign is atomic for all sizes of head/tail data so
    // we make the buffer access atomic
    EnterCriticalSection(&m_criticalSection);

    // if we are capturing then store data
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
    if (m_putCaptureFile) {
        CString header;

        header.Format("Put %s: ", m_channelId);
        m_putCaptureFile->Capture(header, buffer, bufferSize);
    }
#endif

    // store buffer
    for (sent = 0; (sent < bufferSize) && (((m_tail + 1) & m_fifoMask) != m_head); sent++) {
        m_buffer[m_tail] = buffer[sent];
        m_tail = (m_tail + 1) & m_fifoMask;
    }

    // release the buffer
    LeaveCriticalSection(&m_criticalSection);

    return sent;
}

BOOL CFIFO::Get(char *buffer, size_t *bufferSize) {
    size_t bytes = 0;

    // get exclusive access. see Put for a description of why
    EnterCriticalSection(&m_criticalSection);

    // get data
    for (bytes = 0; (bytes < *bufferSize) && (m_head != m_tail); bytes++) {
        buffer[bytes] = m_buffer[m_head];
        m_head = (m_head + 1) & m_fifoMask;
    }

    // if we are capturing then store data
#if !defined(_WIN32_WCE) && !defined(__GNUC__)
    if (m_getCaptureFile) {
        CString header;

        header.Format("Get %s: ", m_channelId);
        m_getCaptureFile->Capture(header, buffer, bytes);
    }
#endif

    // release the buffer
    LeaveCriticalSection(&m_criticalSection);

    *bufferSize = bytes;
    return TRUE;
}

BOOL CFIFO::IsEmpty() const {
    BOOL empty = FALSE;

    empty = (m_head == m_tail);

    return empty;
}

BOOL CFIFO::IsFull() const {
    BOOL full = TRUE;

    full = (m_head == ((m_tail + 1) & m_fifoMask));

    return full;
}

size_t CFIFO::FreeSpace() const {
    size_t freeSpace = 0;

    freeSpace = (m_fifoSize + m_head - m_tail - 1) & m_fifoMask;

    return freeSpace;
}