/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

#include "StdAfx.h"

#if !defined(_WIN32_WCE)

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#define SEND_FILE_BUFFER_SIZE 32768
#define MINIZ_HEADER_FILE_ONLY

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "../Controller/ControllerLib.h"
#include "Controller.h"
#include "miniz.c"

#include "TextFileLoader.h"

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

CTextFileLoader::CTextFileLoader(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction, void *p_controllerClass)
    : m_controller((CController *)p_controllerClass)
    , m_fireEventClass(p_fireEventClass)
    , m_fireEventFunction(p_fireEventFunction)
{
}

BOOL CTextFileLoader::Send(
    const CString &p_source_file,
    int p_destination, const CString &p_destination_file,
    int p_protocol,
    BOOL p_compression, int p_compression_level, BOOL p_decompression,
    BOOL p_timeout, int p_timeout_seconds,
    int p_direction
    )
{
    // store parameters
    m_source_file = p_source_file;
    m_destination = p_destination;
    m_destination_file = p_destination_file; m_destination_file.MakeUpper();
    m_protocol = p_protocol;
    m_compression = p_compression;
    m_compression_level = p_compression_level;
    m_decompression = p_decompression;
    m_timeout = p_timeout;
    m_timeout_seconds = p_timeout_seconds;
    m_direction = p_direction;

    // create the progress window
    m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_CREATE, 0, 0);

    // clear current status
    m_receiveFlags = 0;
    m_xoffFlag = 0;

    // thread data
    DWORD threadId[2] = { 0, 0 };
    HANDLE threadHandle[2] = { 0, 0 };

    // start the send thread
    threadHandle[0] = CreateThread(NULL, 0, SendThread, this, 0, &threadId[0]);
    if (!threadHandle)
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Error creating send thread");
        goto fail;
    }

    // start the receive thread
    if (m_protocol)
    {
        threadHandle[1] = CreateThread(NULL, 0, RecvThread, this, 0, &threadId[1]);
        if (!threadHandle[1])
        {
            m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Error creating receive thread");
            goto fail;
        }
    }

    // wait for send to finish
    while (WAIT_OBJECT_0 != WaitForMultipleObjects(m_protocol ? 2 : 1, threadHandle, TRUE, 100))
    {
        MSG msg;

        while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            AfxGetApp()->PumpMessage();
        }
    }

    // Destroy the progress window
    m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_DESTROY, 0, 0);

fail:
    // close the thread
    if (threadHandle[0])
        CloseHandle(threadHandle[0]);
    if (threadHandle[1])
        CloseHandle(threadHandle[1]);

    return m_transfer_successfull;
}

void CTextFileLoader::RecvFile()
{
    enum receive_statii { receive_status_idle, receive_status_start_tag, receive_status_end_tag, receive_status_file } status = receive_status_idle;
    CString string;
    size_t bufferLength = 0, bufferPosition = 0;
    char buffer[256];

    // receive the response
    do
    {
        // if we have processed the current buffer then read a new buffer
        if (bufferPosition >= bufferLength)
        {
            // read a character
            bufferPosition = 0;
            bufferLength = sizeof(buffer);
            m_controller->Read(CHANNEL_TFL, buffer, &bufferLength);
            if (!bufferLength)
            {
                Sleep(1);
                continue;
            }
        }

        // if the next character is STX then reset the state machine
        char chr = buffer[bufferPosition];
        if (2 == chr)
            status = receive_status_start_tag;

        // process character
        switch (status)
        {
        case receive_status_idle:
            bufferPosition++;
            break;
        case receive_status_start_tag:
            if (2 == chr)
            {
                string.Empty();
                status = receive_status_end_tag;
            }
            bufferPosition++;
            break;
        case receive_status_end_tag:
            if (3 == chr)
            {
                // analyze contents
                if (!string.Left(4).CompareNoCase("CRC="))
                {
                    m_receivedCrc = strtoul(string.Mid(4), NULL, 16);
                    m_receiveFlags |= RECEIVE_FLAG_CRC;
                    status = receive_status_idle;
                }
                else if (!string.Left(5).CompareNoCase("FILE"))
                {
                    status = receive_status_file;
                }
                else if (!string.CompareNoCase("ABORT"))
                {
                    m_receiveFlags |= RECEIVE_FLAG_CANCEL;
                    m_controller->SetCancel(TRUE);
                    status = receive_status_idle;
                }
            }
            else
                string += chr;
            bufferPosition++;
            break;
        case receive_status_file:
            {
                // count the characters we can process
                size_t length;
                for (length = 0; (bufferPosition < bufferLength) && (buffer[bufferPosition] != 2); bufferPosition++, length++);

                // store characters
                m_transfer_file.Write(&buffer[bufferPosition - length], (UINT)length);
                m_crc.CRCBuffer(&buffer[bufferPosition - length], (int)(length));
            }
            break;
        }
    } while (!m_controller->GetCancel() && !(m_receiveFlags & RECEIVE_FLAG_DONE));
}

void CTextFileLoader::SendFile()
{
    CString string, transfer_message;
    char soh = 1, stx = 2, etx = 3;
    CTime start = CTime::GetCurrentTime();
    tdefl_compressor d;
    ULONGLONG file_position = 0, send_position = 0;

    // open the file
    if (!m_transfer_file.Open(m_source_file, (m_direction ? (CFile::modeCreate | CFile::modeWrite) : CFile::modeRead) | CFile::shareExclusive | CFile::typeBinary))
    {
        char errstring[256];

        strerror_s(errstring, sizeof(errstring), errno);
        string.Format("Error opening %s [%i] %s", m_source_file, errno, errstring);
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, string);
        goto cleanup;
    }

    // we have not been successful
    m_transfer_successfull = 0;

    // initialise compression
    if (m_compression)
        tdefl_init(&d, NULL, NULL, tdefl_create_comp_flags_from_zip_params(m_compression_level, 0, 0));

    // build the message header
    switch (m_protocol)
    {
    case 0:
        string.Empty();
        break;
    case 1:
        // <soh>SD:<filename><extension><stx>
        string.Format("%cSD:%s%c", soh, m_destination_file, stx);
        break;
    case 2:
        {
            int flags;
            flags = (1 && (m_compression && m_decompression)) | ((1 && m_direction) << 1);
            // version 00: <soh>00<flags><destination><filename><extension><stx>
            string.Format("%c%02i%c%c%s%c", soh, 0, '0' + flags, '0' + m_destination, m_destination_file, stx);
        }
        break;
    }

    // send the message header
    if (!string.IsEmpty())
        m_controller->Write(CHANNEL_TFL, string, string.GetLength());

    // initialise the CRC
    m_crc.SelectCRCType("CRC-16");
    m_crc.InitCRC();

    // if we are sending then send
    if (!m_direction)
    {
        char buffers[2][SEND_FILE_BUFFER_SIZE], *send_buffer = buffers[0], *compress_buffer = buffers[1];

        // send the file
        do
        {
            // read the file
            ULONGLONG file_length = m_transfer_file.Read(send_buffer, SEND_FILE_BUFFER_SIZE);
            size_t send_length = (size_t)file_length;

            // compress the file
            if (m_compression)
            {
                size_t compress_length = SEND_FILE_BUFFER_SIZE;
                tdefl_compress(&d, send_buffer, &send_length, compress_buffer, &compress_length, (file_length + file_position) >= m_transfer_file.GetLength() ? TDEFL_FINISH : TDEFL_FULL_FLUSH);

                swap(send_length, compress_length);
                swap(send_buffer, compress_buffer);
            }

            if (send_length > 0)
            {
                if (m_protocol)
                {
                    UINT blockOffset = 0;

                    // we cannot send SOH, STX or ETX characters in the buffer so we must escape them
                    // we use the \ to escape so we must escape this as well
                    //     \\ = \
                    //     \s = stx
                    //     \e = etx
                    //     \h = soh
                    do
                    {
                        UINT blockLength;

                        // find the longest possible transparent block
                        for (blockLength = blockOffset; blockLength < send_length; blockLength++)
                        {
                            char chr = send_buffer[blockLength];

                            if (('\\' == chr) || (1 == chr) || (2 == chr) || (3 == chr))
                                break;
                        }

                        // wait until we can transmit
                        while (m_xoffFlag)
                            Sleep(1);

                        // if we have been canceled then skip
                        if (m_receiveFlags & RECEIVE_FLAG_CANCEL)
                            continue;

                        // if we have a block then send it
                        if (blockLength > blockOffset)
                            m_controller->Write(CHANNEL_TFL, &send_buffer[blockOffset], blockLength - blockOffset);

                        // if we have an escaped character then send it
                        if (blockLength < send_length)
                        {
                            char escapeBuffer[2] = "\\";
                            char chr = send_buffer[blockLength];

                            switch (chr)
                            {
                            case '\\': escapeBuffer[1] = '\\'; break;
                            case 1:    escapeBuffer[1] = 'h'; break;
                            case 2:    escapeBuffer[1] = 's'; break;
                            case 3:    escapeBuffer[1] = 'e'; break;
                            }

                            // send escaped character
                            m_controller->Write(CHANNEL_TFL, escapeBuffer, 2);

                            // we have sent a character
                            blockLength++;
                        }

                        // update indices
                        blockOffset = blockLength;
                    } while (blockOffset < send_length);

                    // update CRC
                    m_crc.CRCBuffer(send_buffer, (int)send_length);
                }
                else
                    m_controller->Write(CHANNEL_TFL, send_buffer, send_length);

                // update file positions
                file_position += file_length;
                send_position += send_length;
                m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_SET_POS, (long)(100 * file_position / m_transfer_file.GetLength()), NULL);
            }
        } while (!m_controller->GetCancel() && !m_receiveFlags && (file_position < m_transfer_file.GetLength()));

        // clear cancel status (just in case)
        m_controller->SetCancel(FALSE);
        m_controller->Write(CHANNEL_TFL, &etx, 1);
    }
    // we are receiving so wait for receive to finish
    else
    {
        while (!m_controller->GetCancel() && !m_receiveFlags)
            Sleep(100);

        m_controller->SetCancel(FALSE);
        send_position = file_position = m_transfer_file.GetPosition();
    }

    // end of transmission
    if (m_protocol)
    {
        if (!(m_receiveFlags & RECEIVE_FLAG_CANCEL))
        {
            // get the calculated CRC
            m_calculatedCrc = m_crc.GetCRC();

            // wait for CRC from controller
            CTime timeout = CTime::GetCurrentTime();
            while (!(m_receiveFlags & RECEIVE_FLAG_CRC) && !m_controller->GetCancel() && (!m_timeout || ((CTime::GetCurrentTime() - timeout).GetTotalSeconds() < m_timeout_seconds)))
                Sleep(100);

            if (!(m_receiveFlags & RECEIVE_FLAG_CRC) && !m_controller->GetCancel())
                m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Timeout waiting for CRC.");
        }
    }
    else
        Sleep(1500);

cleanup:
    // close file
    if (m_transfer_file.m_hFile != CFile::hFileNull)
        m_transfer_file.Close();

    // why did we stop
    CString message;
    if (m_controller->GetCancel())
        message = "File transfer cancelled by user.";
    else if (!m_protocol)
    {
        message = "File transfer successful.";
        m_transfer_successfull = 1;
    }
    else if (m_receiveFlags & RECEIVE_FLAG_CANCEL)
        message = "File transfer cancelled by controller.";
    else if (m_receiveFlags & RECEIVE_FLAG_TIMEOUT)
        message = "Timeout waiting for CRC.";
    else if (m_receiveFlags & RECEIVE_FLAG_CRC)
    {
        if (m_receivedCrc != m_calculatedCrc)
            message.Format("File transfer failed. CRC wrong (Calculated=$%x, Received=$%x)", m_calculatedCrc, m_receivedCrc);
        else
        {
            message = "File transfer successful.";
            m_transfer_successfull = 1;
        }
    }
    else
        message = "Error in file transfer.";
    CTimeSpan duration = CTime::GetCurrentTime() - start;
    LONGLONG seconds = duration.GetTotalSeconds();
    message += "\nDuration=" + duration.Format("%H:%M:%S");
    if (send_position == file_position)
    {
        string.Format("\nBytes transferred=%lli\nBytes per second=%.2f",
                      file_position,
                      !seconds ? 0.0 : (double)file_position / (double)duration.GetTotalSeconds()
                     );
    }
    else
    {
        string.Format(
            "\nBytes read=%lli Bytes sent=%lli"
            "\nEffective bytes per second=%.2f Real bytes per second=%.2f"
            "\nCompression ratio=%.2f%%",
            file_position, send_position,
            !seconds ? 0.0 : (double)file_position / (double)duration.GetTotalSeconds(),
            !seconds ? 0.0 : (double)send_position / (double)duration.GetTotalSeconds(),
            100.0 * (double)(file_position - send_position) / (double)file_position
            );
    }
    message += string;
    m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, message);
}

#endif
