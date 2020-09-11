/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __FIFO_H__
#define __FIFO_H__
#define CHANNEL_ID_SIZE 32
#define MAX_FIFO_BUFFER_SIZE 0x1000

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
class CCaptureFile;

class CFIFO
{
protected:
	char m_buffer[MAX_FIFO_BUFFER_SIZE];
	size_t m_head, m_tail, m_fifoSize, m_fifoMask;
	CCaptureFile *m_putCaptureFile, *m_getCaptureFile;
	char m_channelId[CHANNEL_ID_SIZE];
	CRITICAL_SECTION m_criticalSection;

public:
	CFIFO(size_t p_fifoSize = MAX_FIFO_BUFFER_SIZE);
	~CFIFO(void);
	void SetChannelId(const char *p_channelId) { 
		strncpy_s(m_channelId, sizeof(m_channelId), p_channelId, CHANNEL_ID_SIZE);
	}

	//-- storage
	void Flush() { m_head = m_tail = 0; }
	size_t Put(const char *buffer, size_t bufferSize);
	BOOL Get(char *buffer, size_t *bufferSize);

	//-- status
	BOOL IsEmpty() const;
	BOOL IsFull() const;
	size_t FreeSpace() const;

	// capture
	void SetCaptureFile(CCaptureFile *p_putCaptureFile, CCaptureFile *p_getCaptureFile) { m_putCaptureFile = p_putCaptureFile; m_getCaptureFile = p_getCaptureFile; }
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
