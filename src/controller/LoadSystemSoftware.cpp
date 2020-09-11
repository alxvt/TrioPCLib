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

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "LoadSystemSoftware.h"
#include "coff.h"

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
CLoadSystemSoftware::CLoadSystemSoftware(CController *p_controller, fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction)
    : m_run(FALSE)
    , m_controller(p_controller)
    , m_fireEventFunction(p_fireEventFunction)
    , m_fireEventClass(p_fireEventClass)
{
}

CLoadSystemSoftware::~CLoadSystemSoftware()
{
}

BOOL CLoadSystemSoftware::Load(const char *p_coffFile)
{
    // initialise
    m_coff_file = p_coffFile;
    m_run = TRUE;
    m_controller->SetCancel(FALSE);

    // create the progress bar
    m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_CREATE, 0, 0);

    DWORD loadThreadExitCode;
#ifdef _WIN32
    // create worker thread
    DWORD threadId;
    HANDLE loadThread = CreateThread(NULL, 0, LoadThread, this, 0, &threadId);
    if (!loadThread)
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Error creating the load thread");
        return FALSE;
    }

    // wait for the dust to settle
    while (WAIT_OBJECT_0 != WaitForSingleObject(loadThread, 100))
    {
        MSG msg;

        while(PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {
            AfxGetApp()->PumpMessage();
        }
    }

    // get the exit status
    GetExitCodeThread(loadThread, &loadThreadExitCode);

    // close the thread handle
    CloseHandle(loadThread);
#else
    pthread_t loadThread;
    if (pthread_create(&loadThread, NULL, LoadThread, this))
    {
        m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, "Error creating the load thread");
        return FALSE;
    }

    // wait for the dust to settle
    pthread_join(loadThread, (void **)&loadThreadExitCode);
#endif

    // destroy the progress window
    m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_DESTROY, !loadThreadExitCode, "Done");

    // reset the cancel status
    m_controller->SetCancel(FALSE);

    return TRUE;
}


DWORD CLoadSystemSoftware::LoadThread() {
    enum {
        error, open_file,
        send_command, send_confirmation, wait_second_1, wait_second_2, wait_second_3, wait_second_4, wait_second_5, wait_second_6,
        set_communications_parameters, load_section_header, prepare_section, load_section_size, load_section_address, load_section_body,
        reset_communications_parameters, done
    } status = open_file;
    coff coff;
    CString message, messages;
    int section;
    long address, checksum = 0;
    size_t size, send_offset, recv_offset, length;
    CTime start = CTime::GetCurrentTime();
    size_t totalSize = 0, totalSizeSent = 0;

    // run until stopped
    while(m_run && !m_controller->GetCancel()) {
        switch(status) {
        case open_file:
            if(coff.read(m_coff_file)) {
                message.Format("Error loading COFF file %s\r\n", (LPCTSTR)m_coff_file);
                status = error;
                break;
            }
            message.Format("COFF file %s opened\r\n", (LPCTSTR)m_coff_file);
            for (section = 0, totalSize = 0; section < coff.get_sections();section++)
            {
                if(coff.get_section_has_data(section) && coff.get_section_size(section))
                {
                    totalSize += coff.get_section_size(section) * coff.get_size_of_char();
                }
            }
            section = 0;
            status = send_command;
            break;
        case send_command:
            m_controller->SetChannelMode(channel_mode_off);
            message = "LOADSYSTEM\r\nWARNING: This will stop the controller\r\nContinue [Y/N] ? ";
            if(!write("LOADSYSTEM\r", message)) {
                write("N\r", NULL);
                message.Format("Error sending LOAD_SYSTEM command\r\n");
                status = error;
            }
            else {
                status = send_confirmation;
            }
            break;
        case send_confirmation:
            message = "Y\x8\r\nSystem will shut down in 5 seconds.";
            if(!write("Y\r", message)) {
            //if(!write("Y\r", 2, NULL, 0)) {
                message.Format("Error confirming LOAD_SYSTEM command\r\n");
                status = error;
            }
            else {
                message += "\r\nWait ";
                status = wait_second_1;
            }
            break;
        case wait_second_1:
            message = ".";
            Sleep(1000);
            status = wait_second_2;
            break;
        case wait_second_2:
            message = ".";
            Sleep(1000);
            status = wait_second_3;
            break;
        case wait_second_3:
            message = ".";
            Sleep(1000);
            status = wait_second_4;
            break;
        case wait_second_4:
            message = ".";
            Sleep(1000);
            status = wait_second_5;
            break;
        case wait_second_5:
            message = ".";
            Sleep(1000);
            status = wait_second_6;
            break;
        case wait_second_6:
            message = ".\r\n";
            Sleep(1000);
            status = set_communications_parameters;
            break;
        case set_communications_parameters:
            m_controller->SetChannelMode(channel_mode_download);
            status = load_section_header;
            break;
        case load_section_header:
            if (section >= coff.get_sections()) {
                if(!write(0L, 0L)) {
                    message = "Error sending end of load\r\n";
                    status = error;
                }
                status = reset_communications_parameters;
            }
            else if(coff.get_section_has_data(section) && coff.get_section_size(section)) {
                char sectionName[100];
                CString string = coff.get_section_name(section, sectionName);

                // we send bytes, so set size to the number of bytes
                size = coff.get_section_size(section) * coff.get_size_of_char();
                address = coff.get_section_address(section);
                message.Format("load section %s [0x%lx:0x%lx] ", (LPCTSTR)string.Left(8), address, size);
                status = prepare_section;
            }
            else {
                section++;
            }
            break;
        case prepare_section:
            // TI controllers download bytes in different order
            if (4 == coff.get_size_of_char()) {
                char *section_data = coff.get_section_data_pointer(section);
                char reorder[4];
                size_t offset;

                // process all data
                for (offset = 0;offset < size;offset += 4, section_data += 4)
                {
                    reorder[0] = section_data[3];
                    reorder[1] = section_data[2];
                    reorder[2] = section_data[1];
                    reorder[3] = section_data[0];
                    memcpy(section_data, reorder, 4);
                }
                message.Format("reordered ");
            }
            status = load_section_size;
            break;
        case load_section_size:
            // MIPS controllers download bytes, MIPS coff informs bytes
            // TI controllers download longs, TI coff informs longs
            if(!write((long)coff.get_section_size(section), (long)coff.get_section_size(section))) {
                message = "Error sending section size\r\n";
                status = error;
            }
            else {
                status = load_section_address;
            }
            break;
        case load_section_address:
            if(!write(address, address)) {
                message = "Error sending section address\r\n";
                status = error;
            }
            else {
                status = load_section_body;
                send_offset = recv_offset = 0;
            }
            break;
        case load_section_body:
            // accumulate checksum
            if (section || (send_offset >= (size - 8))) {
                checksum += coff.get_section_data_pointer(section)[send_offset];
            }

            // calculate the length to send
            length = min((size_t)(size - send_offset), m_controller->GetSystemSoftwareDownloadFifoLength() - 4);

            // if we are loading a TI controller then we can only send multiples of 4 bytes (& ~3)
            // if we are loading a MIPS controll then we can load any number of bytes (& ~0)
            length &= ~(coff.get_size_of_char() - 1);

            // send buffer
            if(!write((char *)&coff.get_section_data_pointer(section)[send_offset], length, NULL, 0)) {
                message.Format("Error sending data address %lx\r\n", send_offset);
                status = error;
            }
            else {
                char chr;

                // another byte sent
                send_offset += length;
                totalSizeSent += length;

                // check data received
                do {
                    length = 1;
                    m_controller->Read(0, &chr, &length);
                    if(!length) Sleep(2);
                    else if(chr != coff.get_section_data_pointer(section)[recv_offset]) {
                        message.Format("Error sending data address %lx\r\n", recv_offset);
                        status = error;
                    }
                    else {
                        // accept this byte
                        recv_offset++;

                        // tell the user we are doing something
                        if(1 == recv_offset) message += "|";
                        else if(recv_offset == ( 1 * (size >> 4))) message += ".";
                        else if(recv_offset == ( 2 * (size >> 4))) message += ".";
                        else if(recv_offset == ( 3 * (size >> 4))) message += ".";
                        else if(recv_offset == ( 4 * (size >> 4))) message += "|";
                        else if(recv_offset == ( 5 * (size >> 4))) message += ".";
                        else if(recv_offset == ( 6 * (size >> 4))) message += ".";
                        else if(recv_offset == ( 7 * (size >> 4))) message += ".";
                        else if(recv_offset == ( 8 * (size >> 4))) message += "|";
                        else if(recv_offset == ( 9 * (size >> 4))) message += ".";
                        else if(recv_offset == (10 * (size >> 4))) message += ".";
                        else if(recv_offset == (11 * (size >> 4))) message += ".";
                        else if(recv_offset == (12 * (size >> 4))) message += "|";
                        else if(recv_offset == (13 * (size >> 4))) message += ".";
                        else if(recv_offset == (14 * (size >> 4))) message += ".";
                        else if(recv_offset == (15 * (size >> 4))) message += ".";
                        else if(recv_offset >= size) {
                            do {
                                length = 1;
                                m_controller->Read(0, &chr, &length);
                                if(!length) {
                                    Sleep(0);
                                }
                                else if(chr != coff.get_section_data_pointer(section)[recv_offset]) {
                                    message.Format("Error sending data address %lx\r\n", recv_offset);
                                    status = error;
                                }
                                else recv_offset++;
                            } while(m_run && !m_controller->GetCancel() && (load_section_body == status) && (recv_offset < size));
                            message = "|\r\n";
                            section++;
                            status = load_section_header;
                        }
                    }
                } while(
                    m_run && !m_controller->GetCancel() && (load_section_body == status) &&
                    ((send_offset - recv_offset) > (size_t)(send_offset >= size ? 0 : 4))
                );
            }
            break;
        case reset_communications_parameters:
            m_controller->SetChannelMode(channel_mode_off);
            status = done;
            break;
        default:
            message.Format("Done [duration = %s;checksum = 0x%08x]\r\n", (LPCTSTR)(CTime::GetCurrentTime() - start).Format("%D:%H:%M:%S"), checksum);
            m_run = FALSE;
        }

        // append message
        if(!message.IsEmpty()) {
            messages += message; message.Empty();
            m_fireEventFunction(m_fireEventClass, EVENT_PROGRESS_SET_POS, (long)(totalSize ? (100 * totalSizeSent / totalSize) : 0), (const char *)messages);
        }
    }

    return status == error;
}

BOOL CLoadSystemSoftware::write(long p_send, long p_recv) {
    char send[4], recv[4];
    int i;

    // prepare data
    for (i = 3;i >= 0;i--) {
        send[i] = (char)(p_send & 0xff); p_send >>= 8;
        recv[i] = (char)(p_recv & 0xff); p_recv >>= 8;
    }

    // do
    return write(send, 4, recv, 4);
}

BOOL CLoadSystemSoftware::write(const char *p_send, const char *p_recv) {
    return write(p_send, strlen(p_send), p_recv, p_recv ? strlen(p_recv) : 0);
}

BOOL CLoadSystemSoftware::write(const char *p_send, size_t p_send_len, const char *p_recv, size_t p_reclen) {
    size_t compare;
    CTime start;

    // send
    m_controller->Write(0, p_send, p_send_len);
    if(!p_recv) return TRUE;

    // check
    start = CTime::GetCurrentTime();
    for (compare = 0;m_run && (compare < p_reclen) && !m_controller->GetCancel() && ((CTime::GetCurrentTime() - start).GetTotalSeconds() < 600);) {
        char chr;
        size_t length = 1;

        m_controller->Read(0, &chr, &length);
        if (!length) {
            Sleep(1);
            continue;
        }
        if(chr != p_recv[compare]) {
            return FALSE;
        }
        compare++;
    }
    if(compare < p_reclen) {
        return FALSE;
    }

    return TRUE;
}

#endif
