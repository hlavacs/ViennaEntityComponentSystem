
#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/select.h>
#include <netdb.h>
typedef struct protoent PROTOENT;
typedef struct servent SERVENT;
typedef struct hostent HOSTENT;
// milliseconds are good enough, so resort to these where necessary
#define Sleep(a) usleep((a) * 1000)
#endif

#include <algorithm>

#include <errno.h>

#include "Listener.h"

#ifndef TRUE
#define TRUE (1==1)
#endif
#ifndef FALSE
#define FALSE (1==0)
#endif

//all incoming socket events are handled through select()
#define USING_SELECT


static bool sockStarted = false;
static bool sockStart(void) {
#ifdef _WINSOCKAPI_ 

    WSADATA wsaData;

    if (sockStarted)
        return true;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return false;

    sockStarted = true;
    return true;

#else

    return true;

#endif
}

// sockEnd : terminates WinSock
static void sockEnd(void) {
#ifdef _WINSOCKAPI_ 
    if (!sockStarted)
        return;

    WSACleanup();
    sockStarted = false;
#endif
}

inline int getSocketError(void)
{
#ifdef _WINSOCKAPI_
    return WSAGetLastError();
#else
    return errno;
#endif
}

// setSockAddr : setup a sockaddr_in structure from the passed server/port
void setSockAddr(struct sockaddr_in* pSA, std::string server, std::string service = "") {
    pSA->sin_family = AF_INET;
#ifdef _AIX
    pSA->sin_len = sizeof(saiS);
#endif
    pSA->sin_addr.s_addr = server.size() ? Socket::GetHostAddress(server) : INADDR_ANY;
    pSA->sin_port = Socket::GetServicePort(service);
}


// ===============================================================================
// Socket class members

int Socket::GetError() {
    return getSocketError();
}

int Socket::GetProtoNumber(std::string proto) {
    PROTOENT* lpPE;

    if (!sockStart() || proto.empty())
        return 0;

    lpPE = getprotobyname(proto.c_str());
    if (!lpPE)
        return 0;

#ifndef _WINSOCKAPI_
    /* under AIX, close connection to /etc/protocols file */
    endprotoent();
#endif

    return lpPE->p_proto;
}


SOCKET Socket::Create(int sType, std::string proto) {
    if (!sockStart() || s != INVALID_SOCKET)
        return INVALID_SOCKET;
    s = socket(AF_INET, sType, GetProtoNumber(proto));
    return s;
}

// servicePort : determine port number to use instead of a passed in service name
int Socket::GetServicePort(std::string service, std::string proto) {
    if (service.empty() || (!sockStart()))
        return 0;
    if (service[0] == '#')  // accept number with leading # to allow clear numeric indication
        return htons(stoi(service.substr(1)));
    else {
        size_t i;
        for (i = 0; i < service.length(); i++)
            if (!isdigit(service[i]))
                break;
        if (i == service.length())
            return htons(stoi(service));
    }

    SERVENT* lpS = getservbyname(service.c_str(), proto.empty() ? NULL : proto.c_str());
    if (!lpS)
        return 0;
#ifndef _WINSOCKAPI_
    /* under *N[I|U]X, close connection to /etc/services file */
    endservent();
#endif
    return lpS->s_port;
}

// getHostAddress : returns the host address for a given host name
unsigned long Socket::GetHostAddress(std::string hostName)
{
    unsigned long* lpAdr;
    char szLocal[128];

    if (hostName.empty())
    {
        gethostname(szLocal, sizeof(szLocal));
        hostName = szLocal;
    }

    if (!sockStart())
        return INADDR_NONE;
#if 1
    HOSTENT* lpH = gethostbyname(hostName.c_str());
#ifndef _WINSOCKAPI_
    /* under *N[I|U]X, close connection to /etc/hosts file */
    endhostent();
#endif
    if (!lpH)
        return INADDR_NONE;
    lpAdr = (unsigned long*)lpH->h_addr;
#else
    ADDRINFO hints{ .ai_family = AF_INET };
    ADDRINFO* pai{ NULL };
    if (getaddrinfo(hostName.c_str(), nullptr, &hints, &pai))
        return INADDR_NONE;
    // TODO: parse resulting structure for the best matching result

    freeaddrinfo(pai);
#endif

    return *lpAdr;
}

bool Socket::IsCreated() {
    return (s != INVALID_SOCKET);
}

bool Socket::IsSocket() {

    if (s == INVALID_SOCKET)
        return false;

    struct sockaddr_in sain = { 0 };

#ifdef _WINSOCKAPI_
    int len = sizeof(sain);
#else
    socklen_t len = sizeof(sain);
#endif
    // return whether NO error getting it
    return !getsockname(s, (struct sockaddr*)&sain, &len);
}

// bind : binds socket to a server/port
int Socket::Bind(std::string server, std::string service, int bReuse) {
    struct sockaddr_in saiS;

    if (!sockStart() || s == INVALID_SOCKET)
        return SOCKET_ERROR;

    setSockAddr(&saiS, server, service);

    int rc = ::bind(s, (const struct sockaddr*)&saiS, sizeof(saiS));
    if (!rc) {
        if (service.size()) {
            // if binding to specific port, assure socket gets reused
            SetOpt(SOL_SOCKET, SO_REUSEADDR, (char*)&bReuse, sizeof(bReuse));
        }
    }
    return rc;

}

int Socket::Connect(std::string server, std::string service) {
    struct sockaddr_in saiS;
    int rc;

    if (!sockStart() || s == INVALID_SOCKET)
        return SOCKET_ERROR;

    setSockAddr(&saiS, server, service);
    rc = ::connect(s, (const struct sockaddr*)&saiS, sizeof(saiS));
    if (rc != 0)
        rc = getSocketError();
    return rc;
}

int Socket::Disconnect() {
    if (!sockStart() || s == INVALID_SOCKET)
        return SOCKET_ERROR;

    unsigned long ulNBIO{ 0 };
    Ioctl(FIONBIO, &ulNBIO);

    linger li{ TRUE, 20 };
    SetOpt(SOL_SOCKET, SO_LINGER, (char*)&li, sizeof(li));

    shutdown(s, 2);
    return 0;
}

int Socket::Destroy() {
    int rc;

    if (!sockStart() || s == INVALID_SOCKET)
        return SOCKET_ERROR;

    Disconnect();

#ifdef _WINSOCKAPI_
    rc = closesocket(s);
#else
    rc = close(s);
#endif

    s = INVALID_SOCKET;

    return rc;
}

int Socket::GetPort() {
    if (!sockStart() || s == INVALID_SOCKET)
        return SOCKET_ERROR;

    sockaddr_in sain{ 0 };
#ifdef _WINSOCKAPI_
    int len = sizeof(sain);
#else
    socklen_t len = sizeof(sain);
#endif
    if (getsockname(s, (struct sockaddr*)&sain, &len))
        return SOCKET_ERROR;
    return ntohs(sain.sin_port);
}

int Socket::GetOpt(int level, int optname, char* optval, size_t* optlen) {
    int rc;
#ifdef _WINSOCKAPI_
    int iopt = optlen ? (int)*optlen : 0;
    rc = getsockopt(s, level, optname, optval, &iopt);
    if (optlen)
        *optlen = (size_t)iopt;
#else
    rc = getsockopt(s, level, optname, optval, (socklen_t*)optlen);
#endif
    return rc;
}

// setOpt : sets a socket option
int Socket::SetOpt(int level, int optname, const char* optval, int optlen) {
    return ::setsockopt(s, level, optname, optval, optlen);
}

// setSendTimeout : sets the send timeout on a socket in msecs
int Socket::SetSendTimeout(int msecs) {
    return SetOpt(IPPROTO_TCP, SO_SNDTIMEO, (char*)&msecs, sizeof(msecs));
}

// setReceiveTimeout : sets the receive timeout on a socket in msecs
int Socket::SetReceiveTimeout(int msecs) {
    return SetOpt(IPPROTO_TCP, SO_RCVTIMEO, (char*)&msecs, sizeof(msecs));
}

int Socket::SendData(const char* data, int datalen) {
    int iBSent;
    int nTimeout = 0;
    int nError;
    do
    {
        iBSent = send(s, data, datalen, 0);
        if (iBSent == SOCKET_ERROR) {
#ifdef _WINSOCKAPI_
            nError = WSAGetLastError();
            if (nError == WSAEWOULDBLOCK)
#else
            nError = errno;
            if (nError == EWOULDBLOCK)
#endif
                iBSent = 0;
            else
                return nError;
        }
        else if (iBSent == 0) {
            nTimeout++;
            // if 10 seconds passed without send possibility, give up
            if (nTimeout > 100)
                return SOCKET_ERROR;
            Sleep(100);
        }
        data += iBSent;
        datalen -= iBSent;
    } while (datalen > 0);

    return 0;
}

// receiveData: receives data from other side of the connection
int Socket::ReceiveData(char* lpData, int cbData, bool UntilFull)
{
    int iBRecv;
    int iBTotal = 0;

    do {
        iBRecv = recv(s, lpData, cbData, 0);
        if (iBRecv == SOCKET_ERROR) {
#ifdef _WINSOCKAPI_
            if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
            if (errno != EWOULDBLOCK && errno != EAGAIN)
#endif
                return iBTotal;
            else
                iBRecv = 0;
        }
        else if (!iBRecv) {
            break;
        }

        iBTotal += iBRecv;
        cbData -= iBRecv;
        lpData += iBRecv;
    } while (UntilFull && cbData > 0);

    return iBTotal;
}

// listen : make socket a listener
int Socket::Listen(int backlog) {
    return ::listen(s, backlog);
}

// ioctl : perform an ioctl operation on the socket
int Socket::Ioctl(long cmd, unsigned long* argp) {
#ifdef _WINSOCKAPI_
    return ioctlsocket(s, cmd, argp);
#else
    return ::ioctl(s, cmd, argp);
#endif
}

// accept : accepts a client connection and returns the resulting basic socket
SOCKET Socket::Accept(sockaddr* addr, int* addrlen) {
    SOCKET sClient = ::accept(s, addr, (socklen_t*)addrlen);
    return sClient;
}

// wait : waits for something to come in on socket
int Socket::Wait(int timeout) {
    if (s == INVALID_SOCKET)
        return SOCKET_ERROR;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s, &fds);

    timeval to{ .tv_usec = timeout * 1000L };
    return select(static_cast<int>(s + 1), &fds, NULL, NULL, (to.tv_sec || to.tv_usec) ? &to : NULL);
}

// bytesBuffered : return number of buffered bytes available for reading
int Socket::BytesBuffered() {
    unsigned long nLen{ 0 };
    if (Ioctl(FIONREAD, &nLen) == SOCKET_ERROR)
        return -1;
    return static_cast<int>(nLen);  // could potentially overflow, but the probability is negligible
}

// dataThere : returns whether data can be received from the socket
bool Socket::DataThere() {
    char dummy;
    return recv(s, &dummy, 1, MSG_PEEK) > 0;
}


// ===============================================================================
// SocketListener class members

SocketListener::~SocketListener() {
    Terminate();
}

bool SocketListener::Create(std::string service, int sockType) {
    if (service.empty())
        return false;

    if (sockListener.IsCreated())
        return false;

    // create a listener socket
    sockListener.Create(sockType);
    if (!sockListener.IsCreated()) return false;
    // assure socket gets reused
    linger li{ TRUE, 2 };
    sockListener.SetOpt(SOL_SOCKET, SO_LINGER, (char*)&li, sizeof(li));

    // try to bind it to the specified port
    int reuse{ TRUE };

    if (sockListener.Bind("", service)) {
        sockListener.Destroy();
        return false;
    }

    // activate listener if stream
    switch (sockType)
    {
    case SOCK_STREAM:
        // set socket to listening state
        if (sockListener.Listen(SOMAXCONN)) {
            sockListener.Destroy();
            return false;
        }
        break;
    case SOCK_DGRAM:
        // not necessary for datagram handler
        break;
    }

    typeListener = sockType;
    serviceListener = service;
    terminateListener = false;

#ifndef USING_SELECT
    unsigned long ulNBIO = TRUE;
    int rc = sockListener.ioctl(FIONBIO, &ulNBIO);
#endif

    // now create and run the listener thread
    try {
        thdListener = std::thread(&SocketListener::ThdFuncListener, this);
    }
    catch (...) {
        sockListener.Destroy();
        return false;
    }
    return true;
}

void SocketListener::Terminate() {

    // terminate all client connections
    for (auto thd : streamClient) {
        thd->Terminate();
    }

    terminateListener = true;
    if (thdListener.joinable())
        thdListener.join();
}

// thdFuncListener : listener thread function
void SocketListener::ThdFuncListener() {

    if (sockListener == INVALID_SOCKET)
        return;

    int dgrambufLen{ 0 };

#ifdef USING_SELECT
    int nfds = 0;
    fd_set acceptfds, fds;
    SOCKET sfds[2];
    FD_ZERO(&fds); // set up accept selector set
    int maxfd = 0;
    if (sockListener != INVALID_SOCKET) {
        FD_SET(sockListener, &fds);
        sfds[nfds++] = sockListener;
        if ((int)sockListener > maxfd)
            maxfd = (int)sockListener;
    }
#endif

    while (!terminateListener) {

        RemoveEndedClients();

#ifdef USING_SELECT
        acceptfds = fds;
        static timeval to{ .tv_sec = 0, .tv_usec = 200000 };

        int rc = select(maxfd + 1, &acceptfds, NULL, NULL, &to);
        if (rc == SOCKET_ERROR)
            break;
        else if (rc == 0)
            continue;
#endif

        if (typeListener == SOCK_STREAM) {
            sockaddr_in saiClient{ 0 };
            //#ifdef _WINSOCKAPI_
#if 1
            int lsaiClient = sizeof(saiClient);
#else
            size_t lsaiClient = sizeof(saiClient);
#endif
            SOCKET sClient = sockListener.Accept((sockaddr*)&saiClient, &lsaiClient);
            if (sClient == INVALID_SOCKET) {
                continue;
            }
            SocketThread* thdData{ nullptr };
            try {
                // allocate new thread for this client
                thdData = CreateSocketThread(sClient, this);
                std::thread newThd(&SocketThread::Run, thdData);
                thdData->SetThread(newThd);

            }
            catch (...) {
                if (thdData) delete thdData;
            }
        }
#if 0
        else if (typeListener == SOCK_DGRAM) {
            // assure datagram buffer is available
            if (!dgramBuf.size()) {
                bool allocFailed{ false };
                size_t iRBufLenLen{ sizeof(dgrambufLen) };
                sockListener.getOpt(SOL_SOCKET, SO_RCVBUF, (char*)&dgrambufLen, &iRBufLenLen);
                try {
                    dgramBuf.resize(dgrambufLen);
                }
                catch (...) {
                    terminateListener = true;
                }
            }

            if (!terminateListener) {
                // fetch datagram from other side
                int rcvd = sockListener.receiveData(dgramBuf.data(), dgrambufLen, FALSE);
                if (rcvd) {
                    // datagram clients don't require a separate thread
                    if (rcvd < dgrambufLen)
                        dgramBuf[rcvd] = 0;
                    onDatagram(dgramBuf.data(), rcvd);
                }
            }

        }
#endif
        Sleep(10);
    }

    sockListener.Destroy();

    // wait for all connections to go away
    int timeouts{ 0 };
    while (streamClient.size()) {
        RemoveEndedClients();
        if (++timeouts > 100)
            break;
        Sleep(10);
    }

}

// AddClient : add running client thread to list
bool SocketListener::AddClient(SocketThread* thd) {
    bool ok{ false };
    try {
        streamClient.push_back(thd);
        ok = true;
    }
    catch (...) {
    }
    return ok;
}

// RemoveClient : remove client thread from list
bool SocketListener::RemoveClient(SocketThread* thd) {
    bool ok{ false };
    try {
        streamClientGone.push_back(thd);
        ok = true;
    }
    catch (...) {
    }
    return ok;

}

// removeEndedClients : remove all ended clients from the client vector
void SocketListener::RemoveEndedClients() {

    for (auto thd : streamClientGone) {
        if (thd->GetThread().joinable())
            thd->GetThread().join();

        streamClient.erase(std::remove(streamClient.begin(), streamClient.end(), thd), streamClient.end());
    }

}



// ===============================================================================
// SocketThread class members

void SocketThread::Run() {
    listener->AddClient(this);

    ClientActivity();

    sockClient.Destroy();

    listener->RemoveClient(this);
}



