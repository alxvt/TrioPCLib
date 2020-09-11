#include "StdAfx.h"
#include "fins.h"

CFins::CFins(void)
	: m_destinationNodeAddress(0)
	, m_destinationUnitAddress(0)
	, m_sourceNodeAddress(0)
	, m_mpeSocket(INVALID_SOCKET)
	, m_mpePort(0)
	, m_openCommand(0)
    , m_receive_bufferHead(0)
    , m_receive_bufferTail(0)
    , m_transmit_bufferHead(0)
    , m_transmit_bufferTail(0)
    , write_mutex(NULL)
{
	// initialise structures
	m_lastWrite.QuadPart = 0;
	m_lastPoll.QuadPart = 0;

    // Initialise sockets.
    WSADATA wsaData;
    WORD wdVersion = MAKEWORD(1,1);
    WSAStartup( wdVersion, &wsaData );

	// source node address 1 is the last digit of my IP address
	char hostname[100];
	if (!gethostname(hostname, sizeof(hostname))) {
		HOSTENT *h = gethostbyname(hostname);

		if (NULL != h) {
			m_sourceNodeAddress = (char)(ntohl(*(long *)h->h_addr) & 0xff);
		}
	}
}

CFins::~CFins(void)
{
    // Close the port
    Close(-1);

    // Don't need this with AfxSockInit() ...
    WSACleanup();
}

void CFins::SetInterfaceFins(const char *p_hostname, short p_port, char p_unitAddress)
{
	HOSTENT *h;
	DWORD ipAddress;

	// convert host name to IP
	h = gethostbyname(p_hostname);
	if (!h) {
		ipAddress = ntohl(inet_addr(p_hostname));
	}
	else {
		ipAddress = ntohl(*(long *)h->h_addr);
	}

	SetInterfaceFins(ipAddress, p_port, p_unitAddress);
}

void CFins::SetInterfaceFins(DWORD p_ipAddress, short p_port, char p_unitAddress)
{
	m_destinationNodeAddress = (char)(p_ipAddress & 0xff);
	m_destinationUnitAddress = p_unitAddress;
	m_ipAddress = p_ipAddress;
	m_mpePort = p_port;
}

BOOL CFins::OpenMPE()
{
    // local data
    PROTOENT* p;

    // Get the protocol information.
    p = getprotobyname("udp");
	if (NULL == p) goto fail;

    // Create a socket
    m_mpeSocket = socket(AF_INET, SOCK_DGRAM, p->p_proto);
	if (m_mpeSocket == INVALID_SOCKET) goto fail;

	// bind it to port 9600. I don't know whether this is necessary but Unit++ does
    SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_mpePort);
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (0 != bind(m_mpeSocket, (sockaddr *)&addr, sizeof(addr))) goto fail;

	// set non blocking
	DWORD fionbio = 1;
    if (ioctlsocket(m_mpeSocket, FIONBIO, &fionbio)) goto fail;

    // empty the fifo
    m_receive_bufferHead = m_receive_bufferTail = 0;
    m_transmit_bufferHead = m_transmit_bufferTail = 0;

    // prepare the mutex
    write_mutex = CreateMutex(NULL, FALSE, NULL);
    if (!write_mutex)
        goto fail;

	// set the open command
	m_openCommand = 1;

	// return success
	return TRUE;
fail:
	SetLastErrorMessage(WSAGetLastError());
	return FALSE;
} // end function OpenSocket

void CFins::CloseMPE()
{
	// set close command
	m_openCommand = 0;
	WriteMPE("", 0);
	Sleep(100); // give the TCP stack time to send the 

    // destroy the mutex
    if (write_mutex)
    {
        CloseHandle(write_mutex);
        write_mutex = NULL;
    }

	// if the socket is open then close it
    if (m_mpeSocket != INVALID_SOCKET) {
        // close the socket
        closesocket(m_mpeSocket);

        // invalidate port
        m_mpeSocket = INVALID_SOCKET;
    }
}

size_t CFins::WriteHeader(char *p_packet, size_t p_packetSize) const {
	// if packet is too small then fail
	if (p_packetSize < 12) return -1;

	// store packet data
	p_packet[ 0] = (char)0x80; //ICF - request : 0x80, response : 0x00 
	p_packet[ 1] = (char)0x00; //RSVS - n/a 
	p_packet[ 2] = (char)0x02; //GCNT - Gateway count 
	p_packet[ 3] = (char)0x00; //DNA - destination address : network, ... 
	p_packet[ 4] = (char)m_destinationNodeAddress; //DA1 - node, ... 
	p_packet[ 4] = 0;
	p_packet[ 5] = (char)m_destinationUnitAddress; //DA2 - unit (based on switch setting on unit.) 
	p_packet[ 6] = (char)0x00; //SNA - Source address, network, ... 
	p_packet[ 7] = (char)m_sourceNodeAddress; //SA1 - node (last digit of my IP address), ... 
	p_packet[ 8] = (char)0x00; //SA2 - unit 
	p_packet[ 9] = (char)0x55; //SID - my selected transaction ID value.

	// return length
	return 10;
}

BOOL CFins::WriteMPE(const char *p_buffer,size_t p_bufferSize) {
    BOOL success = FALSE;

    // exclusive entry
    while (WaitForSingleObject(write_mutex, 1000) != WAIT_OBJECT_0)
        TRACE(L"Huh???\n");
    //WaitForSingleObject(write_mutex, INFINITE);

    // select this buffer
    size_t transmit_buffer_size = sizeof(m_transmit_buffer[m_transmit_bufferTail]);
    char *transmit_buffer = m_transmit_buffer[m_transmit_bufferTail];
    size_t *transmit_buffer_length = &m_transmit_bufferLength[m_transmit_bufferTail];

    // point to the next buffer
    m_transmit_bufferTail = (m_transmit_bufferTail + 1)  % WINDOW_SIZE;

	// build the output buffer
	if ((*transmit_buffer_length = WriteHeader(transmit_buffer, transmit_buffer_size)) < 0) 
        goto fail;
	transmit_buffer[(*transmit_buffer_length)++] = (char)0x04;
	transmit_buffer[(*transmit_buffer_length)++] = (char)0x01;
	transmit_buffer[(*transmit_buffer_length)++] = (char)'T';
	transmit_buffer[(*transmit_buffer_length)++] = (char)'0';
	transmit_buffer[(*transmit_buffer_length)++] = (char)m_openCommand; // 01 => Open, 00 => close
	memcpy(&transmit_buffer[*transmit_buffer_length], p_buffer, min(p_bufferSize, transmit_buffer_size - *transmit_buffer_length));
	*transmit_buffer_length += min(p_bufferSize, transmit_buffer_size - *transmit_buffer_length);

	// create the destination address
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_mpePort);
	addr.sin_addr.S_un.S_addr = htonl(m_ipAddress);

	// send packet
resend:
	if (*transmit_buffer_length != sendto(m_mpeSocket, transmit_buffer, (int)*transmit_buffer_length, 0, (sockaddr *)&addr, (int)sizeof(addr))) {
		SetLastErrorMessage(WSAGetLastError());
		goto fail;
	}

    // wait for response
    BOOL done = FALSE;
	do {
        // read the data
        int reread_count = 5;
        char buffer[1024];
reread:
        size_t bufferLength = recv(m_mpeSocket, buffer, sizeof(buffer), 0);
	    if (SOCKET_ERROR == bufferLength) 
        {
            if (WSAEWOULDBLOCK == WSAGetLastError())
		        bufferLength = 0;
            else
                goto fail;
	    }

        // if there is no response then check if we can continue
        if (!bufferLength)
        {
            // if we have space in the buffer then we can finish
            if (m_transmit_bufferTail != m_transmit_bufferHead)
                done = true;
            // if we can reread then do it
            else if (--reread_count)
            {
                // give some time for the remote to send
                Sleep(10);

                // reread
                goto reread;
            }
            // we timed out the last packet
            else
            {
                // step over the packet
                m_transmit_bufferHead = (m_transmit_bufferHead + 1) % WINDOW_SIZE;

                // set the message
	            SetLastErrorMessage("Timeout waiting for response");
	            goto fail;
            }
        }
        // process response
        else if (bufferLength >= 14) {
		    CStringA message;

		    // node busy then resent the oldest packet
            if ((p_buffer[12] == 0x02) && (p_buffer[13] == 0x04))
            {
                // give the remote time to process
                Sleep(10);

                // pick up the oldest packet
                transmit_buffer = m_transmit_buffer[m_transmit_bufferHead];
                transmit_buffer_length = &m_transmit_bufferLength[m_transmit_bufferHead];

                // retransmit
                goto resend;
            }
            // not busy so for better or worst this packet has been processed
		    else
            {
                // the oldest message is now done
                m_transmit_bufferHead = (m_transmit_bufferHead + 1) % WINDOW_SIZE;

                // if this was not an error then get the response data
                if (!IsResponseError(buffer, bufferLength, message)) {
                    for (size_t byte = 14; byte < bufferLength; byte++)
                    {
                        // wait for space available
                        int next_buffer_tail = (m_receive_bufferTail + 1) % sizeof(m_receive_buffer);
                        while (next_buffer_tail == m_receive_bufferHead)
                            Sleep(0);

                        // store data
                        m_receive_buffer[m_receive_bufferTail] = buffer[byte];
                        m_receive_bufferTail = next_buffer_tail;
                    }
                }
                // this was an error so flag it
	            else {
		            SetLastErrorMessage(message);
		            goto fail;
	            }
                done = TRUE;
		    }
        }
	}
	while (!m_cancel && !done);

    // if we get here we have been successful
    success = TRUE;

fail:
    // release access
    ReleaseMutex(write_mutex);

	return success;
}

BOOL CFins::ReadMPE(char *p_buffer, size_t *p_bufferSize) {
    // an empty write forces a read
    if (!WriteMPE("", 0))
        return FALSE;

    // get the received data
    size_t byte;
    for (byte = 0; (byte < *p_bufferSize) && (m_receive_bufferHead != m_receive_bufferTail); byte++)
    {
        // store data
        p_buffer[byte] = m_receive_buffer[m_receive_bufferHead];
        m_receive_bufferHead = (m_receive_bufferHead + 1) % sizeof(m_receive_buffer);
    }
    *p_bufferSize = byte;

	// we have been successful
	return TRUE;
}

BOOL CFins::WritePacket(const char *p_packet, size_t p_packetLength) 
{
	char packet[1024];
	size_t packetLength=0;
	SOCKADDR_IN addr;

	// build the output buffer
	if ((packetLength = WriteHeader(packet, sizeof(packet))) < 0) return FALSE;
	memcpy(&packet[packetLength], p_packet, min(p_packetLength, sizeof(packet) - packetLength));
	packetLength += min(p_packetLength, sizeof(packet) - packetLength);

	// create the destination address
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_mpePort);
	addr.sin_addr.S_un.S_addr = htonl(m_ipAddress);

    // send packet
	if (packetLength != sendto(m_mpeSocket, packet, (int)packetLength, 0, (sockaddr *)&addr, (int)sizeof(addr))) {
		SetLastErrorMessage(WSAGetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL CFins::ReadPacket(char *p_packet, size_t *p_packetSize) 
{
    fd_set read_set;
    TIMEVAL tv;
	int packetSize = (int)*p_packetSize, packetLength = 0;

	// no data yet
	*p_packetSize = 0;

	// data to read?
    FD_ZERO(&read_set);
    FD_SET(m_mpeSocket, &read_set);
    tv.tv_sec = 0; tv.tv_usec = 0;
	switch (select(FD_SETSIZE, &read_set, NULL, NULL, &tv)) {
	case 0: // no data
		SetLastErrorMessage("No data");
		return FALSE;
	case SOCKET_ERROR: // error
		goto fail;
	}

    // read the data
    packetLength = recv(m_mpeSocket, p_packet, packetSize, 0);

	// if there is an error then fail
	if (SOCKET_ERROR == packetLength) goto fail;
	// if there is no data then fail
	else if (0 == packetLength) {
		SetLastErrorMessage("No data");
		return FALSE;
	}
	// there is data so process it
	else *p_packetSize = packetLength;

	// we have been successful
	return TRUE;

fail:
	SetLastErrorMessage(WSAGetLastError());
	return FALSE;
}

BOOL CFins::IsResponseError(const char *p_packet, size_t p_packetLength, CStringA &message)
{
	BOOL isError = TRUE;

	// if packet is too short then fail
	if (p_packetLength < 14) {
		message = "Response too short";
	}
	else {
		CString major("Unknown"), minor("Unknown");

		// process error code
		switch (p_packet[12]) {
		case 0x00:
			// major status
			major = "Normal completion";

			// minor status
			switch(p_packet[13]) {
			case 0x00: minor = "Normal completion"; isError = FALSE; break;
			case 0x01: minor = "Service canceled"; break;
			}
			break;
		case 0x01:
			// major status
			major = "Local node error";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "Local node not in network"; break;
			case 0x02: minor = "Token timeout"; break;
			case 0x03: minor = "Retries failed"; break;
			case 0x04: minor = "Too many send frames"; break;
			case 0x05: minor = "Node address range error"; break;
			case 0x06: minor = "Node address duplication"; break;
			}
			break;
		case 0x02:
			// major status
			major = "Destination node error";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "Destination node not in network"; break;
			case 0x02: minor = "Unit missing"; break;
			case 0x03: minor = "Third node missing"; break;
			case 0x04: minor = "Destination node busy"; break;
			case 0x05: minor = "Response timeout"; break;
			}
			break;
		case 0x03:
			// major status
			major = "Controller error";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "Communications controller error"; break;
			case 0x02: minor = "CPU unit error"; break;
			case 0x03: minor = "Controller"; break;
			case 0x04: minor = "Unit number error"; break;
			}
			break;
		case 0x04:
			// major status
			major = "Service unsupported";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "Undefined command"; break;
			case 0x02: minor = "Not supported by model/version"; break;
			}
			break;
		case 0x05:
			// major status
			major = "Routing table error";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "Destination address setting error"; break;
			case 0x02: minor = "No routing tables"; break;
			case 0x03: minor = "Routing table error"; break;
			case 0x04: minor = "Too many relays"; break;
			}
			break;
		case 0x10:
			// major status
			major = "Command format error";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "Command too long"; break;
			case 0x02: minor = "Command too short"; break;
			case 0x03: minor = "Elements/data do not match"; break;
			case 0x04: minor = "Command format error"; break;
			case 0x05: minor = "Header error"; break;
			}
			break;
		case 0x11:
			// major status
			major = "Parameter error";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "Area clasification missing"; break;
			case 0x02: minor = "Access size error"; break;
			case 0x03: minor = "Address range error"; break;
			case 0x04: minor = "Address range exceeded"; break;
			case 0x06: minor = "Program missing"; break;
			case 0x09: minor = "Relational error"; break;
			case 0x0a: minor = "Duplicate data access"; break;
			case 0x0b: minor = "Response too long"; break;
			case 0x0c: minor = "Parameter error"; break;
			}
			break;
		case 0x20:
			// major status
			major = "Read not possible";

			// minor status
			switch(p_packet[13]) {
			case 0x02: minor = "Protected"; break;
			case 0x03: minor = "Table missing"; break;
			case 0x04: minor = "Data missing"; break;
			case 0x05: minor = "Program missing"; break;
			case 0x06: minor = "File missing"; break;
			case 0x07: minor = "Data mismatch"; break;
			}
			break;
		case 0x21:
			// major status
			major = "Write not possible";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "Read only"; break;
			case 0x02: minor = "Protected"; break;
			case 0x03: minor = "Cannot register"; break;
			case 0x05: minor = "Program missing"; break;
			case 0x06: minor = "File missing"; break;
			case 0x07: minor = "File name already exists"; break;
			case 0x08: minor = "Cannot change"; break;
			}
			break;
		case 0x22:
			// major status
			major = "Not executable in current mode";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "Not possible during execution"; break;
			case 0x02: minor = "Not possible while running"; break;
			case 0x03: // same as next so just drop through
			case 0x04: // same as next so just drop through
			case 0x05: // same as next so just drop through
			case 0x06: minor = "Wrong PLC mode"; break;
			case 0x07: minor = "Specified node not polling node"; break;
			case 0x08: minor = "Step cannot be executed"; break;
			case 0x0f: minor = "Connection already open"; break;
			}
			break;
		case 0x23:
			// major status
			major = "No such device";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "File device missing"; break;
			case 0x02: minor = "Memory missing"; break;
			case 0x03: minor = "Clock missing"; break;
			case 0x04: minor = "Unit missing"; break;
			}
			break;
		case 0x24:
			// major status
			major = "Cannot start/stop";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "Table missing"; break;
			case 0x02: minor = "File missing"; break;
			}
			break;
		case 0x25:
			// major status
			major = "Unit error";

			// minor status
			switch(p_packet[13]) {
			case 0x02: minor = "Memory contents error"; break;
			case 0x03: minor = "I/O setting error"; break;
			case 0x04: minor = "Too many I/O points"; break;
			case 0x05: minor = "CPU bus error"; break;
			case 0x06: minor = "I/O duplication"; break;
			case 0x07: minor = "I/O bus error"; break;
			case 0x09: minor = "SYSMAC BUS error"; break;
			case 0x0a: minor = "CPU BUS unit error"; break;
			case 0x0d: minor = "SYSMAC BUS number duplication"; break;
			case 0x0f: minor = "Memory status error"; break;
			case 0x10: minor = "SYSMAC BUS terminator error"; break;
			}
			break;
		case 0x26:
			// major status
			major = "Command error";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "No protection"; break;
			case 0x02: minor = "Incorrect password"; break;
			case 0x04: minor = "Protected"; break;
			case 0x05: minor = "Service already executing"; break;
			case 0x06: minor = "Service stopped"; break;
			case 0x07: minor = "No execution right"; break;
			case 0x08: minor = "Settings not complete"; break;
			case 0x09: minor = "Necessary items not set"; break;
			case 0x0a: minor = "Number already defined"; break;
			case 0x0b: minor = "Error will not clear"; break;
			}
			break;
		case 0x30:
			// major status
			major = "Access right error";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "No access right"; break;
			}
			break;
		case 0x40:
			// major status
			major = "Service aborted";

			// minor status
			switch(p_packet[13]) {
			case 0x01: minor = "Service aborted"; break;
			}
			break;
		}

		// create message
		message.Format("%02x:%s-%02x:%s", (unsigned char)p_packet[12], major, (unsigned char)p_packet[13], minor);
	}

	return isError;
}
