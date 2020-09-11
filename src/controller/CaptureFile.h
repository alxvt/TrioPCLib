/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

#if !defined(__CAPTURE_FILE__) && !defined(_WIN32_WCE) && !defined(__GNUC__)
#define __CAPTURE_FILE__

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
class CCaptureFile
{
protected:
    CString m_lastFifoId;
    FILE *m_captureFile;
    BOOL m_captureLineEmpty;

protected:
    void CharToString(char chr, char *string, int nMaxLength);

public:
    CCaptureFile(void);
    ~CCaptureFile(void);

    // open/close
    BOOL Open(const CString &p_captureFile);
    void Close();

    //-- capture
    void Capture(const CString &p_fifoId, const char *p_buffer, size_t p_bufferSize, BOOL p_ascii = FALSE, BOOL p_forceNewLine = FALSE); // WE ASSUME ONLY ONE THREAD HERE AT A TIME
};

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

#endif
