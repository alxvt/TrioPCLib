/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

#include "StdAfx.h"

#if !defined(_WIN32_WCE) && !defined(__GNUC__)

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "CaptureFile.h"
#include "Ascii.h"

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
CCaptureFile::CCaptureFile(void)
    : m_captureFile(NULL)
    , m_captureLineEmpty(FALSE)
{
}

CCaptureFile::~CCaptureFile(void)
{
}

BOOL CCaptureFile::Open(const CString &p_captureFile) {
    // already open?
    if (NULL != m_captureFile) return TRUE;

    // if we cannot open then fail
#ifdef _WIN32
    if (fopen_s(&m_captureFile, p_captureFile, "wt") != 0)
#else
    if ((m_captureFile = fopen(p_captureFile, "wt")) == NULL)
#endif
    {
        return FALSE;
    }

    // initialise capture data
    m_lastFifoId.Empty();

    // success
    return TRUE;
}

void CCaptureFile::Close() {
    // already closed?
    if (NULL == m_captureFile) return;

    // terminate
    if (!m_captureLineEmpty) {
        fprintf(m_captureFile, "]\n");
    }

    // close
    fclose(m_captureFile);
    m_captureFile = NULL;
}

void CCaptureFile::Capture(const CString &p_fifoId, const char *p_buffer, size_t p_bufferSize, BOOL b_raw, BOOL p_forceNewLine) {
    // if we are capturing then process it
    if (m_captureFile) {
        // if this is the first time through then initialise the last capture id
        if (m_lastFifoId.IsEmpty()) {
            m_lastFifoId = p_fifoId;
        }

        // if the fifo id has changed then terminate the line
        if (p_forceNewLine || ((p_fifoId != m_lastFifoId) && !m_captureLineEmpty)) {
            m_lastFifoId = p_fifoId;
            fprintf(m_captureFile, "]\n%s", (LPCTSTR)p_fifoId);
        }

        // capture data
        if (b_raw) {
            char string[6];
            size_t byte;

            for (byte = 0;byte < p_bufferSize;byte++) {
                CharToString(p_buffer[byte], string, sizeof(string));
                fprintf(m_captureFile, "%s", string);
            }
        }
        else {
            fwrite(p_buffer, p_bufferSize, 1, m_captureFile);
        }

        // flush file to disk
        fflush(m_captureFile);
    }
}

void CCaptureFile::CharToString(char chr, char *string, int nMaxLength)
{
    switch (chr)
    {
    case ASCII_NUL: strcpy_s(string, nMaxLength, "<NUL>"); break;
    case ASCII_SOH: strcpy_s(string, nMaxLength,"<SOH>"); break;
    case ASCII_STX: strcpy_s(string, nMaxLength,"<STX>"); break;
    case ASCII_ETX: strcpy_s(string, nMaxLength,"<ETX>"); break;
    case ASCII_EOT: strcpy_s(string, nMaxLength,"<EOT>"); break;
    case ASCII_ENQ: strcpy_s(string, nMaxLength,"<ENQ>"); break;
    case ASCII_ACK: strcpy_s(string, nMaxLength,"<ACK>"); break;
    case ASCII_BEL: strcpy_s(string, nMaxLength,"<BEL>"); break;
    case ASCII_BS: strcpy_s(string, nMaxLength,"<BS>"); break;
    case ASCII_HT: strcpy_s(string, nMaxLength,"<HT>"); break;
    case ASCII_LF: strcpy_s(string, nMaxLength,"<LF>"); break;
    case ASCII_VT: strcpy_s(string, nMaxLength,"<VT>"); break;
    case ASCII_FF: strcpy_s(string, nMaxLength,"<FF>"); break;
    case ASCII_CR: strcpy_s(string, nMaxLength,"<CR>"); break;
    case ASCII_SO: strcpy_s(string, nMaxLength,"<SO>"); break;
    case ASCII_SI: strcpy_s(string, nMaxLength,"<SI>"); break;
    case ASCII_DC1: strcpy_s(string, nMaxLength,"<DC1>"); break;
    case ASCII_DC2: strcpy_s(string, nMaxLength,"<DC2>"); break;
    case ASCII_DC3: strcpy_s(string, nMaxLength,"<DC3>"); break;
    case ASCII_DC4: strcpy_s(string, nMaxLength,"<DC4>"); break;
    case ASCII_NAK: strcpy_s(string, nMaxLength,"<NAK>"); break;
    case ASCII_SYN: strcpy_s(string, nMaxLength,"<SYN>"); break;
    case ASCII_ETB: strcpy_s(string, nMaxLength,"<ETB>"); break;
    case ASCII_CAN: strcpy_s(string, nMaxLength,"<CAN>"); break;
    case ASCII_EM: strcpy_s(string, nMaxLength,"<EM>"); break;
    case ASCII_SUB: strcpy_s(string, nMaxLength,"<SIB>"); break;
    case ASCII_ESC: strcpy_s(string, nMaxLength,"<ESC>"); break;
    case ASCII_FS: strcpy_s(string, nMaxLength,"<FS>"); break;
    case ASCII_GS: strcpy_s(string, nMaxLength,"<GS>"); break;
    case ASCII_RS: strcpy_s(string, nMaxLength,"<RS>"); break;
    case ASCII_US: strcpy_s(string, nMaxLength,"<US>"); break;
    case ASCII_DEL: strcpy_s(string, nMaxLength,"<DEL>"); break;
    default:
        if (nMaxLength > 1)
            string[0] = chr; string[1] = '\0';
    }
}

#endif
