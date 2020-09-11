/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

#include "StdAfx.h"

#if !defined(_WIN32_WCE) && !defined(__GNUC__)

#include "Shlwapi.h"

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#define MAX_PROGRAM_NAME_LENGTH     15
#define MAX_PROGRAMS                14
#define MAX_PROGRAM_ENTRY_LENGTH    100

#define PROJECT_EXIT_FLAG_DONE 1
#define PROJECT_EXIT_FLAG_USER_ABORT 2
#define PROJECT_EXIT_FLAG_CRC_ERROR 4
#define PROJECT_EXIT_FLAG_FILE_ERROR 8
#define PROJECT_EXIT_FLAG_COMMAND_ERROR 16

#define PROJECT_RUN_FLAG_START 1
#define PROJECT_RUN_FLAG_ACKNOWLEDGE 2
#define PROJECT_RUN_FLAG_DELETE_ALL 4

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "Project.h"
#include "Ascii.h"
#include "Crc.h"

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

CProject::CProject(CController *p_controller, BOOL p_slowLoad, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
: m_exitFlags(0)
, m_runFlags(0)
, m_controller(p_controller)
, m_fireEventFunction(p_fireEventFunction)
, m_fireEventClass(p_fireEventClass)
, m_slowLoad(p_slowLoad)
{
}

CProject::~CProject(void)
{
}

BOOL CProject::loadProject(const char *p_fullProjectPath)
{
    CString string;
    int program;
    BOOL encrypted = FALSE, retval = FALSE;
#ifdef _WIN32
    HANDLE thread[2] = { NULL, NULL};
#endif
    CTime start;

    // set the project path
    strncpy_s(m_projectPath, sizeof(m_projectPath), p_fullProjectPath, sizeof(m_projectPath));
    PathRemoveFileSpec(m_projectPath);

    // create the project load list
    m_programPending.RemoveAll();
    for (program = 0;program < MAX_PROGRAMS;program++)
    {
        // local data
        int programType;
        char programName[MAX_PROGRAM_ENTRY_LENGTH + 1];

        // get this program entry
        string.Format("Program%02d",program);
        GetPrivateProfileString("Programs",string,"",programName,sizeof(programName),p_fullProjectPath);

        // if this entry is not present then skip
        if (!programName[0]) continue;

        // get this program type
        switch (programType = GetPrivateProfileInt("ProgramType",programName,0,p_fullProjectPath))
        {
        case 1:
            encrypted = TRUE;
            break;
        }

        // create the program id
        string.Format("%s%c%i",programName,ASCII_FS,programType);
        string.MakeUpper();

        // store this program entry
        m_programPending.AddTail(string);
    }

    // flush the command line
    write(0, "\r", 1, NULL, 0);
    Sleep(500);

    // if the file has been encrypted then set the project key
    if (encrypted)
    {
        const char *controllerKey = "";

        // get the key
        if (m_fireEventFunction(m_fireEventClass, EVENT_GET_CONTROLLER_KEY, 0, (LPCTSTR)&controllerKey))
        {
            // send project_key
            if (!write(0, "PROJECT_KEY\r","PROJECT_KEY\r\nEnter project key: "))
            {
                m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Error setting project key");
                goto fail;
            }

            // write the key
            CString controllerKeyCommand(controllerKey);
            controllerKeyCommand += '\r';
            if (!write(0, controllerKeyCommand, controllerKeyCommand + "\n>>"))
            {
                m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Error setting project key");
                goto fail;
            }
        }
        else
        {
            goto fail;
        }
    }

    // create the progress window
    m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_CREATE, 0, 0);

    // initialise the flags
    start = CTime::GetCurrentTime();
    m_runFlags = PROJECT_RUN_FLAG_DELETE_ALL;
    m_exitFlags = 0;
    m_controller->SetCancel(FALSE);

    // send the command
    if (!m_slowLoad)
    {
        if (m_controller->GetChannelMode() == channel_mode_on)
        {
            if (!write(8, "LOAD_PROJECT(1)\r", "LOAD_PROJECT(1)\n\x14"))
            {
                m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Error executing command LOAD_PROJECT");
                m_exitFlags = PROJECT_EXIT_FLAG_COMMAND_ERROR;
                goto fail;
            }
        }
        else
        {
            if (!write(0, "LOAD_PROJECT(0)\r", "LOAD_PROJECT(0)\r\n"))
            {
                m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Error executing command LOAD_PROJECT");
                m_exitFlags = PROJECT_EXIT_FLAG_COMMAND_ERROR;
                goto fail;
            }
        }
    }

#ifdef _WIN32
    // start the threads
    DWORD threadId;
    int threadCount = sizeof(thread) / sizeof(*thread);
    thread[0] = CreateThread(NULL,0,transmitThread,this,0,&threadId);
    if (!thread[0])
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Could not start the transmit thread");
        goto fail;
    }
    thread[1] = CreateThread(NULL,0,receiveThread,this,0,&threadId);
    if (!thread[1])
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Could not start the receive thread");
        goto fail;
    }

    // wait for the process to stop
    DWORD waitObject;
    do
    {
        MSG msg;

        if (PeekMessage(&msg,NULL,0,0,PM_NOREMOVE))
        {
            AfxGetApp()->PumpMessage();
        }
        waitObject = WaitForMultipleObjects(threadCount, thread, TRUE, 10);
    } while (waitObject == WAIT_TIMEOUT);
#else
    pthread_t thread[2];
    if (pthread_create(&thread[0], NULL, transmitThread, this))
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Could not start the transmit thread");
        goto fail;
    }
    if (pthread_create(&thread[1], NULL, receiveThread, this))
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Could not start the receive thread");
        goto fail;
    }

    // wait for the dust to settle
    pthread_join(thread[0], NULL);
    pthread_join(thread[1], NULL);
#endif

    retval = (m_exitFlags & PROJECT_EXIT_FLAG_DONE);

    fail:
    // report exist status
    AnalyzeExitStatus();

    // cleanup
#ifdef _WIN32
    if (thread[0]) CloseHandle(thread[0]);
    if (thread[1]) CloseHandle(thread[1]);
#endif

    // inform how long we took
    string = (CTime::GetCurrentTime() - start).Format("Load took %M:%S");
    m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_SET_POS, 100, (const char *)string);

    // destroy the progress window
    m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_DESTROY, retval, "Done");

    // clear cancel status
    m_controller->SetCancel(FALSE);

    return retval;
}

BOOL CProject::write(const char p_channel, const char *p_send, const char *p_recv)
{
    return p_recv ? write(p_channel, p_send, strlen(p_send), p_recv, strlen(p_recv)) : write(p_channel, p_send, strlen(p_send), NULL, 0);
}

BOOL CProject::write(const char p_channel, const char *p_send, size_t p_send_len, const char *p_recv, size_t p_recv_len)
{
    size_t compare;

    // send
    m_controller->Write(p_channel, p_send, p_send_len);
    if (!p_recv) return TRUE;

    // check
    for (compare = 0;(compare < p_recv_len) && !m_controller->GetCancel();)
    {
        char chr;
        size_t length = 1;

        m_controller->Read(p_channel, &chr, &length);
#ifdef _WIN32
        if (!length)
        {
            MSG msg;

            while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
            {
                AfxGetApp()->PumpMessage();
            }

            Sleep(0);
            continue;
        }
#else
        usleep(0);
#endif
        if (chr != p_recv[compare])
        {
            compare = 0;
            continue;
        }
        compare++;
    }
    if (compare < p_recv_len)
    {
        return FALSE;
    }

    return TRUE;
}

void CProject::transmitThreadFast()
{
    CString string, messages;
    int pos;
    char buffer[120];
    size_t bytesWritten;
    UINT calculatedCrc;
    CCRC crc;

    // set the crc type
    crc.SelectCRCType("CRC-16");

    // wait for the start signal
    while (!(m_runFlags & PROJECT_RUN_FLAG_START) && !m_exitFlags) Sleep(1);
    if (m_exitFlags)
    {
        return;
    }

    // send delete all
    if (m_runFlags & PROJECT_RUN_FLAG_DELETE_ALL)
    {
        char bs = 8;

        write(m_controller->GetChannelMode() == channel_mode_on ? -1 : 0, &bs, 1, 0, 0);
    }

    // process until stopped
    while (!m_exitFlags && m_programPending.GetCount())
    {
        // retrieve this string
        m_currentProgramName = m_programPending.RemoveHead();

        // create the filename
        pos = m_currentProgramName.Find(ASCII_FS);
        m_currentFileName = m_currentProgramName.Left(pos);
        switch (atoi(m_currentProgramName.Mid(pos + 1)))
        {
        case 0: m_currentFileName += ".bas"; break;
        case 1: m_currentFileName += ".enc"; break;
        case 2: m_currentFileName += ".gco"; break;
        case 3: m_currentFileName += ".txt"; break;
        }
        messages += m_currentFileName + "\n";
        m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_SET_POS, 0, (const char *)messages);

        // open the file
        CFile file;
        CString fileName;
        fileName.Format("%s\\%s", m_projectPath, m_currentFileName);
        if (!file.Open(fileName,CFile::modeRead))
        {
            m_exitFlags = PROJECT_EXIT_FLAG_FILE_ERROR;
            continue;
        }

        // send the header
        crc.InitCRC();
        crc.CRCString(m_currentProgramName);
        calculatedCrc = crc.GetCRC();
        bytesWritten = sprintf_s(buffer, sizeof(buffer), "%c%s%c", ASCII_SOH, (LPCTSTR)m_currentProgramName, ASCII_ETX);
        bytesWritten += base64(&buffer[bytesWritten], calculatedCrc);
        bytesWritten += sprintf_s(buffer + bytesWritten, sizeof(buffer) - bytesWritten, "%c", ASCII_STX);
        m_runFlags &= ~PROJECT_RUN_FLAG_ACKNOWLEDGE;
        m_controller->Write(m_controller->GetChannelMode() == channel_mode_on ? -1 : 0, buffer, bytesWritten);
        while (!(m_runFlags & PROJECT_RUN_FLAG_ACKNOWLEDGE) && !m_exitFlags)
            Sleep(0);
        if (m_exitFlags) continue;

        // process the file
        crc.InitCRC();
        char last_chr = 0;
        do
        {
            // local data
            UINT i,j;

            // read this file
            bytesWritten = file.Read(buffer,(UINT)min(m_controller->GetFifoLength(),sizeof(buffer)));

            // if no data then done
            if (!bytesWritten) continue;

            // prepare this buffer
            for (i = j = 0;i < bytesWritten;i++)
            {
                char chr = buffer[i];
                if (chr == '\n')
                {
                    if (last_chr != '\r')
                        buffer[j++] = '\r';
                }
                else
                    buffer[j++] = chr;
                last_chr = chr;
            }
            m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_SET_POS, (long)(100 * file.GetPosition() / file.GetLength()), (const char *)messages);

            // send this buffer
            crc.CRCBuffer(buffer,j);
            m_controller->Write(m_controller->GetChannelMode() == channel_mode_on ? -1 : 0, buffer, j);
        } while (bytesWritten && !m_exitFlags);

        // close the file
        file.Close();

        calculatedCrc = crc.GetCRC();
        buffer[0] = ASCII_ETX;
        bytesWritten = 1 + base64(&buffer[1],calculatedCrc);
        m_runFlags &= ~PROJECT_RUN_FLAG_ACKNOWLEDGE;
        m_controller->Write(m_controller->GetChannelMode() == channel_mode_on ? -1 : 0, buffer,bytesWritten);
        while (!(m_runFlags & PROJECT_RUN_FLAG_ACKNOWLEDGE) && !m_exitFlags)
            Sleep(0);
    }

    // terminate
    buffer[0] = ASCII_EOT;
    m_controller->Write(m_controller->GetChannelMode() == channel_mode_on ? -1 : 0, buffer,1);

    // done
    if (!m_exitFlags) m_exitFlags |= PROJECT_EXIT_FLAG_DONE;
}

BOOL CProject::SendAndReceiveCommand(const int channel)
{
    // send command
    m_commandResponseIndex = 0;
    m_runFlags &= ~PROJECT_RUN_FLAG_ACKNOWLEDGE;
    write(channel, m_commandRequest, NULL);

    // wait for response
    while (!(m_runFlags & PROJECT_RUN_FLAG_ACKNOWLEDGE) && !m_controller->GetCancel())
    {
        Sleep(10);
    }

    // return success status
    return m_runFlags & PROJECT_RUN_FLAG_ACKNOWLEDGE;
}

BOOL CProject::New(const int channel, const CString &program)
{
    // create request
    m_commandRequest.Format("NEW %s\r", (LPCTSTR)program);

    // create response
    if (8 == channel)
    {
        m_commandResponse.Format("OK\r\x14\n\x14");
    }
    else
    {
        m_commandResponse.Format("OK\r\n>>");
    }

    // send request
    return SendAndReceiveCommand(channel);
}

BOOL CProject::Select(const int channel, const CString &program, const CString &programType)
{
    // create request
    m_commandRequest.Format("SELECT %s,%s\r", (LPCTSTR)program, (LPCTSTR)programType);

    // create response
    if (8 == channel)
    {
        m_commandResponse.Format("%s selected\r\x14\n\x14", (LPCTSTR)program);
    }
    else
    {
        m_commandResponse.Format("%s selected\r\n>>", (LPCTSTR)program);
    }

    // send request
    return SendAndReceiveCommand(channel);
}

void CProject::transmitThreadSlow()
{
    int channel = (m_controller->GetChannelMode() == channel_mode_on) ? 8 : 0;
    CString messages;

    // send delete all
    if ((m_runFlags & PROJECT_RUN_FLAG_DELETE_ALL) && !New(channel, "ALL")) goto fail;

    // send files
    while (!m_exitFlags && m_programPending.GetCount())
    {
        // retrieve this string
        m_currentProgramName = m_programPending.RemoveHead();

        // seperate out program name and type
        int pos = m_currentProgramName.Find(ASCII_FS);
        CString currentProgramName = m_currentProgramName.Left(pos);
        CString currentProgramType = m_currentProgramName.Mid(pos + 1);

        // create the filename
        m_currentFileName = currentProgramName;
        switch (atoi(currentProgramType))
        {
        case 0: m_currentFileName += ".bas"; break;
        case 1: m_currentFileName += ".enc"; break;
        case 2: m_currentFileName += ".gco"; break;
        case 3: m_currentFileName += ".txt"; break;
        }
        messages += m_currentFileName + "\n";
        m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_SET_POS, 0, (const char *)messages);

        // open the file
        CFile file;
        CString fileName;
        fileName.Format("%s\\%s", m_projectPath, m_currentFileName);
        if (!file.Open(fileName,CFile::modeRead))
        {
            m_exitFlags = PROJECT_EXIT_FLAG_FILE_ERROR;
            continue;
        }

        // erase this program
        if (!(m_runFlags & PROJECT_RUN_FLAG_DELETE_ALL) && !New(channel, currentProgramName)) goto fail;

        // select this program
        if (!Select(channel, currentProgramName, currentProgramType)) goto fail;

        // load this program
        int line = 0;
        size_t bytesRead;
        char buffer[256];
        m_commandRequest = "&0r,";
        m_commandResponse.Format("%s", (channel == 8) ? "\r\x14\n\x14" : "\n>>");
        BOOL lineHasData = FALSE;
        char last_chr = 0;
        int status = 0;
        do
        {
            // local data
            UINT i;

            // read this file
            bytesRead = file.Read(buffer,(UINT)min(m_controller->GetFifoLength(),min(sizeof(buffer), m_controller->GetFifoLength())));

            // if no data then done
            if (!bytesRead) continue;

            // prepare this buffer
            for (i = 0;i < bytesRead;i++)
            {
                // store this character
                char chr = buffer[i];

                // if this is the end of line send it
                if ('\n' == chr)
                {
                    // make sure that the command is terminated
                    if (last_chr != '\r')
                        m_commandRequest += '\r';

                    // send request
                    if (!SendAndReceiveCommand(channel)) goto fail;

                    // initialise next line
                    line++;
                    lineHasData = FALSE;
                    m_commandRequest.Format("&%ii,", line);
                }
                else if ('\n' != chr)
                {
                    lineHasData = TRUE;
                    m_commandRequest += chr;
                }

                last_chr = chr;
            }
            m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_SET_POS, (long)(100 * file.GetPosition() / file.GetLength()), (const char *)messages);
        } while (bytesRead && !m_exitFlags);

        // send last line
        if (lineHasData)
        {
            // create response
            m_commandResponse.Format("%s%s", (LPCTSTR)m_commandResponse, (channel == 8) ? "\n\x14\r\x14\n\x14" : "\r\n>>");

            // terminate request
            m_commandResponse += '\r';

            // send request
            if (!SendAndReceiveCommand(channel)) goto fail;
        }

        // close the file
        file.Close();
    }

    fail:
    // done
    if (!m_exitFlags) m_exitFlags |= PROJECT_EXIT_FLAG_DONE;
}

DWORD CProject::transmitThread()
{
    if (m_slowLoad) transmitThreadSlow();
    else transmitThreadFast();
    return 0;
}

void CProject::receiveThreadFast()
{
    enum statii
    {
        idle, active
    } status = idle;
    char chr;
    size_t bytesRead;

    // process until stopped
    while (!m_exitFlags)
    {
        // wait for a character
        do
        {
            // read a character
            bytesRead = 1;
            m_controller->Read(m_controller->GetChannelMode() == channel_mode_on ? -1 : 0, &chr,&bytesRead);
            if (!bytesRead) Sleep(0);

            // if we have been canceled then abort
            if (m_controller->GetCancel())
            {
                m_exitFlags = PROJECT_EXIT_FLAG_USER_ABORT;
            }
        } while (!m_exitFlags && !bytesRead);

        // if we have been stopped then skip
        if (!bytesRead) continue;

        // process this status
        if (m_runFlags & PROJECT_RUN_FLAG_START)
        {
            // CRC error
            if (ASCII_NAK == chr)
            {
                m_exitFlags |= PROJECT_EXIT_FLAG_CRC_ERROR;
            }
            // if this is an ack then ack
            else if (ASCII_ACK == chr)
            {
                // start
                m_runFlags |= PROJECT_RUN_FLAG_ACKNOWLEDGE;
            }
        }
        else
        {
            // if this is the start character then start the transmission
            if (ASCII_ENQ == chr)
            {
                // start
                m_runFlags |= PROJECT_RUN_FLAG_START;

                // set the next status
                status = active;
            }
        }
    }
}

void CProject::receiveThreadSlow()
{
    char chr;
    size_t bytesRead;
    int channel;

    // select the channel
    if (m_controller->GetChannelMode() != channel_mode_on) channel = 0;
    else if (m_slowLoad) channel = 8;
    else channel = -1;

    // process until stopped
    while (!m_exitFlags)
    {
        // wait for a character
        do
        {
            // read a character
            bytesRead = 1;
            m_controller->Read(channel, &chr,&bytesRead);
            if (!bytesRead)
                Sleep(1);

            // if we have been canceled then abort
            if (m_controller->GetCancel())
            {
                m_exitFlags = PROJECT_EXIT_FLAG_USER_ABORT;
            }
        } while (!m_exitFlags && !bytesRead);

        // if we have been stopped then skip
        if (!bytesRead) continue;

        // process this status
        if (m_commandResponse.GetAt((int)m_commandResponseIndex) == chr)
        {
            if (++m_commandResponseIndex == m_commandResponse.GetLength())
            {
                m_runFlags |= PROJECT_RUN_FLAG_ACKNOWLEDGE;
            }
        }
        else
        {
            m_commandResponseIndex = 0;
        }
    }
}

DWORD CProject::receiveThread()
{
    if (m_slowLoad) receiveThreadSlow();
    else receiveThreadFast();

    return 0;
}

int CProject::base64(char *p_buffer, int p_value)
{
    // store this value
    p_buffer[2] = (char)(p_value & 0x3f) + 0x20; p_value >>= 6;
    p_buffer[1] = (char)(p_value & 0x3f) + 0x20; p_value >>= 6;
    p_buffer[0] = (char)(p_value & 0x0f) + 0x20;

    // return the number of bytes used
    return 3;
}

BOOL CProject::loadProgram(const char *p_fullProgramPath)
{
    BOOL retval = FALSE, programType;
    char programName[MAX_PROGRAM_ENTRY_LENGTH + 1];
    const char *pos;
    CString string;

    // set the project path
    strncpy_s(m_projectPath, sizeof(m_projectPath), p_fullProgramPath, sizeof(m_projectPath));
    PathRemoveFileSpec(m_projectPath);

    // get this program name
    const char *fileName = PathFindFileName(p_fullProgramPath);
    pos = strrchr(fileName, '.');
    if (NULL == pos)
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Invalid file type");
        goto fail;
    }
    memset(programName, 0, sizeof(programName));
    memcpy(programName, fileName, min(MAX_PROGRAM_ENTRY_LENGTH, pos - fileName));

    // get this program type
    if (!_stricmp(".gco", pos))
    {
        programType = 2;
    }
    else if (!_stricmp(".bas", pos))
    {
        programType = 0;
    }
    else if (!_stricmp(".txt", pos))
    {
        programType = 3;
    }
    else
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Invalid file type");
        goto fail;
    }

    // create the program id
    string.Format("%s%c%i", programName, ASCII_FS, programType);
    string.MakeUpper();

    // store this program entry
    m_programPending.AddTail(string);

    // create the progress window
    m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_CREATE, 0, 0);

    // initialise data
    m_runFlags = 0;
    m_exitFlags = 0;
    m_controller->SetCancel(FALSE);

    // flush any pending data
    m_controller->Flush(m_controller->GetChannelMode() == channel_mode_on ? 8 : 0);

    // send the command
    if (!m_slowLoad)
    {
        if (m_controller->GetChannelMode() == channel_mode_on)
        {
            if (!write(8, "LOAD_PROJECT(1)\r", "LOAD_PROJECT(1)\n\x14"))
            {
                m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Error executing command LOAD_PROJECT");
                m_exitFlags = PROJECT_EXIT_FLAG_COMMAND_ERROR;
                goto fail;
            }
        }
        else
        {
            m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Must be in MPE mode to load program");
            m_exitFlags = PROJECT_EXIT_FLAG_COMMAND_ERROR;
            goto fail;
        }
    }

#ifdef _WIN32
    // start threads
    HANDLE thread[2] = {NULL, NULL};
    DWORD threadId;
    thread[0] = CreateThread(NULL,0,transmitThread,this,0,&threadId);
    if (!thread[0])
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Could not start the transmit thread");
        goto fail;
    }
    thread[1] = CreateThread(NULL,0,receiveThread,this,0,&threadId);
    if (!thread[1])
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Could not start the receive thread");
        goto fail;
    }

    // count the number of threads we have to wait for
    int threadCount;
    for (threadCount = 0;(threadCount < (sizeof(thread) / sizeof(*thread))) && (thread[threadCount] != NULL);threadCount++);

    // wait for all threads to stop
    if (threadCount)
    {
        DWORD waitObject;
        do
        {
            MSG msg;

            if (PeekMessage(&msg,NULL,0,0,PM_NOREMOVE))
            {
                AfxGetApp()->PumpMessage();
            }
            waitObject = WaitForMultipleObjects(threadCount, thread, TRUE, 10);
        } while ((waitObject < WAIT_OBJECT_0) || (waitObject >= (WAIT_OBJECT_0 + threadCount)));
    }
#else
    pthread_t thread[2];
    if (pthread_create(&thread[0], NULL, transmitThread, this))
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Could not start the transmit thread");
        goto fail;
    }
    if (pthread_create(&thread[1], NULL, receiveThread, this))
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Could not start the receive thread");
        goto fail;
    }

    // wait for the dust to settle
    pthread_join(thread[0], NULL);
    pthread_join(thread[1], NULL);
#endif

    retval = (m_exitFlags & PROJECT_EXIT_FLAG_DONE);

    fail:
    // report exist status
    AnalyzeExitStatus();

#ifdef _WIN32
    // free the thread handles
    for (threadCount = 0;threadCount < (sizeof(thread) / sizeof(*thread));threadCount++)
    {
        if (thread[threadCount])
        {
            CloseHandle(thread[threadCount]);
            thread[threadCount] = NULL;
        }
    }
#endif

    // destroy the progress window
    m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_DESTROY, retval, "Done");

    // clear the cancel status
    m_controller->SetCancel(FALSE);

    return retval;
}

void CProject::AnalyzeExitStatus()
{
    if (m_exitFlags & PROJECT_EXIT_FLAG_USER_ABORT)
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Operation aborted by user");
    }
    else if (m_exitFlags & PROJECT_EXIT_FLAG_CRC_ERROR)
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Operation aborted by motion coordinator");
    }
    else if (m_exitFlags & PROJECT_EXIT_FLAG_FILE_ERROR)
    {
        CString message;

        message.Format("Error accessing file %s", (LPCTSTR)m_currentFileName);
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, (LPCTSTR)message);
    }
    else if (m_exitFlags & PROJECT_EXIT_FLAG_COMMAND_ERROR)
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Error running load command on the motion coordinator");
    }
}

#endif