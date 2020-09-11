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
#include "Ethernet.h"

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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// CEthernet.cpp: implementation of the CEthernet class.
//
//

#include "Ascii.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//#define _DEBUG_TIMINGS

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
CEthernet::CEthernet()
    : m_mpeSocket(INVALID_SOCKET)
    , m_remoteSocket(INVALID_SOCKET)
    , m_tflSocket(INVALID_SOCKET)
    , m_ipAddress(0)
    , m_mpePort(0)
    , m_remotePort(0)
    , m_sndbufSize(0)
{
    // Initialise sockets.
#ifdef _WIN32
    WSADATA wsaData;
    WORD wdVersion = MAKEWORD(1,1);
    WSAStartup( wdVersion, &wsaData );
#endif
    // Only possible with MFC.
    //VERIFY(AfxSocketInit(&wsaData));
}


//////////////////////////////////////////////////////////////////////
//
CEthernet::~CEthernet()
{
    // Close the port
    Close(-1);

    // Don't need this with AfxSockInit() ...
#ifdef _WIN32
    WSACleanup();
#endif
}


//////////////////////////////////////////////////////////////////////
//

void CEthernet::SetInterfaceTCP(DWORD p_ipAddress, short p_mpePort, short p_remotePort, short p_tflPort, short p_tflnpPort) {
    m_ipAddress = p_ipAddress;
    m_mpePort = p_mpePort;
    m_remotePort = p_remotePort;
    m_tflPort = p_tflPort;
    m_tflnpPort = p_tflnpPort;
}

void CEthernet::SetInterfaceTCP(const char *p_hostname, short p_mpePort, short p_remotePort, short p_tflPort, short p_tflnpPort) {
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

    SetInterfaceTCP(ipAddress, p_mpePort, p_remotePort, p_tflPort, p_tflnpPort);
}

BOOL CEthernet::OpenTCP(SOCKET &p_socket, short p_port)
{
    // local data
    SOCKADDR_IN addr_in;
    DWORD fionbio;

    // not connected
    ASSERT(INVALID_SOCKET == p_socket);
    p_socket = INVALID_SOCKET;

    // Create a socket
    p_socket = socket( AF_INET, SOCK_STREAM, 0);
    if (p_socket == INVALID_SOCKET) goto fail;

    // set non-blocking
    fionbio = 1;
    if (ioctlsocket(p_socket, FIONBIO, &fionbio)) goto fail;

    // Open a connection.
    addr_in.sin_port = htons(p_port);
    addr_in.sin_family = AF_INET;
#ifdef _WIN32
    addr_in.sin_addr.S_un.S_addr = htonl(m_ipAddress);
#else
    addr_in.sin_addr.s_addr = htonl(m_ipAddress);
#endif
    connect(p_socket, (struct sockaddr*)&addr_in, sizeof(addr_in));
    fd_set writefds; FD_ZERO(&writefds); FD_SET(p_socket,&writefds);
    timeval tv; tv.tv_sec = 5; tv.tv_usec = 0;
    switch (select(FD_SETSIZE, NULL, &writefds, NULL, &tv)) {
    case 0:
        SetLastErrorMessage("Timeout connecting to remote");
        goto cleanup;
    case SOCKET_ERROR:
        goto fail;
    }

    // set blocking
    //fionbio = 0;
    //if (ioctlsocket(p_socket, FIONBIO, &fionbio)) goto fail;

    // set no delay for the REMOTE connection
    if (p_port == TRIO_TCP_TOKEN_SOCKET)
    {
        BOOL nodelay = 1;
        setsockopt(p_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(nodelay));
        if (m_sndbufSize != -1)
            setsockopt(p_socket, SOL_SOCKET, SO_SNDBUF, (char *)&m_sndbufSize, sizeof(m_sndbufSize));
    }

    // return success
    return TRUE;

fail:
    SetLastErrorMessage(WSAGetLastError());

cleanup:
    if(INVALID_SOCKET != p_socket) {
        closesocket(p_socket);
        p_socket = INVALID_SOCKET;
    }
    return FALSE;
} // end function OpenSocket

void CEthernet::CloseTCP(SOCKET &p_socket)
{
    // if the socket is open then close it
    if (p_socket != INVALID_SOCKET)
    {
        // close the socket
        closesocket(p_socket);

        // invalidate port
        p_socket = INVALID_SOCKET;
    }
}

//////////////////////////////////////////////////////////////////////
//
BOOL CEthernet::WriteTCP(SOCKET p_socket, const char *p_buffer, size_t p_bufferSize)
{
    // local data
    BOOL    bWrite = FALSE;
    int rc, retry = 100;

    // write the data out
    do
    {
        rc = send(p_socket, p_buffer, (int)p_bufferSize, 0);
        if (rc == SOCKET_ERROR)
        {
            rc = WSAGetLastError();
            if (rc == WSAEWOULDBLOCK)
                Sleep(10);
        }
        else
            bWrite = TRUE;
    }
    while (!bWrite && (rc == WSAEWOULDBLOCK) && !m_cancel && retry--);

    // log error
    if (!bWrite) {
        SetLastErrorMessage(rc);
    }

    // return result.
    return bWrite;
} // end function Write

//////////////////////////////////////////////////////////////////////
// Transfer from socket to internal buffers.
//
BOOL CEthernet::ReadTCP(SOCKET p_socket, char *p_buffer, size_t *p_bufferSize)
{
    // local data
    fd_set read_set;
    TIMEVAL tv;
    BOOL retval = TRUE;
    int bufferLength = 0;

    //ASSERT(64 == *p_bufferSize);

    // data to read?
    FD_ZERO(&read_set);
    FD_SET(p_socket, &read_set);
    tv.tv_sec = 0; tv.tv_usec = 0;
    switch(select(FD_SETSIZE, &read_set, NULL, NULL, &tv) ) {
    case 0:
        SetLastErrorMessage("No data");
        break;
    case SOCKET_ERROR:
        SetLastErrorMessage(WSAGetLastError());
        break;
    default:
        // read the data
        bufferLength = recv( p_socket, p_buffer, (int)*p_bufferSize - 1, 0 );

        // Check whether ok ... (read length of 0 means remote socket closed)
        if (!bufferLength || (bufferLength == SOCKET_ERROR)) {
            SetLastErrorMessage(WSAGetLastError());
            retval = FALSE;
        }
    }

    // return success
    *p_bufferSize = bufferLength;
    return retval;

}
