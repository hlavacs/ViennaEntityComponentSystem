#pragma once

#include <vector>
#include <thread>
#include <string>

#ifdef WIN32
#define NOMINMAX
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int SOCKET;
#define INVALID_SOCKET  (SOCKET)(~0)    // copied from winsock.h
#define SOCKET_ERROR            (-1)
#endif

/// @brief Class to encapsulate the basic socket functionality
class Socket {
private:
    SOCKET s{ INVALID_SOCKET };

public:
    Socket(SOCKET s = INVALID_SOCKET) : s(s) {}
    ~Socket() { Destroy(); }
    operator SOCKET() { return s; } // if the need arises to use the underlying SOCKET

    Socket& operator=(SOCKET const other) { s = other; return *this; }
    Socket& operator=(Socket& other) { s = other.s; other.Detach(); return *this; }

    /// @brief Return error of last socket operation.
    /// @return Last error code.
    static int GetError();

    /// @brief Retrieve the number for a protocol name.
    /// @param proto Protocol name ("ip", "tcp", ...).
    /// @return Protocol number or 0.
    static int GetProtoNumber(std::string proto = "");

    /// @brief Retrieve the port number for a service name.
    /// @param service Service name ("http", "pop3", ...). "#nnn" is accepted, too, and just converts nnn to a port number.
    /// @param proto Protocol name ("ip", "tcp", ...). Defaults to "tcp".
    /// @return Port number or 0.
    static int GetServicePort(std::string service, std::string proto = "tcp");

    /// @brief Returns the IPv4 address for a host name.
    /// @param hostName Host name (either DNS or numeric "xxx.xxx.xxx.xxx"). "" implicitly uses the local host name.
    /// @return Host address as 32bit value in host format.
    static unsigned long GetHostAddress(std::string hostName = "");

    /// @brief Creates a socket.
    /// @param sType Socket Type to use (SOCK_STREAM, SOCK_DGRAM)
    /// @param proto Protocol number ("ip", "tcp", "udp", ...).
    /// @return Socket number or INVALID_SOCKET
    SOCKET Create(int sType = SOCK_STREAM, std::string proto = "");

    /// @brief Returns whether an underlying socket has been created for this object.
    /// @return true if underlying system socket has already been created.
    bool IsCreated();

    /// @brief Returns whether an underlying socket has been created and initialized for this object.
    ///        Initialization is done with Socket::Bind() or Socket::Connect().
    /// @return true if underlying system socket has already been created and initialized.
    bool IsSocket();

    /// @brief Binds the socket to a listener port.
    /// @param server Server name (either DNS or numeric "xxx.xxx.xxx.xxx"). "" implies INADDR_ANY, used to bind to all interfaces.
    /// @param service Service name. "#nnn" is accepted, too, and just converts nnn to a port number.
    /// @param bReuse Socket reuse flag. Should always be set for listeners.
    ///        Search for SO_REUSEADDR on the Internet for details.
    /// @return 0 if successful, error otherwise. If an error occurs, the exact cause can be retrieved by Socket::GetError().
    int Bind(std::string server = "", std::string service = "", int bReuse = 1);

    /// @brief Connects the socket to a port (potentially on another machine).
    /// @param server Server name (either DNS or numeric "xxx.xxx.xxx.xxx").
    /// @param service Service name. "#nnn" is accepted, too, and just converts nnn to a port number.
    /// @return 0 if successful, error otherwise.
    int Connect(std::string server, std::string service);

    /// @brief Disconnects the socket from a port.
    /// @return 0 for successful disconnection or SOCKET_ERROR.
    int Disconnect();

    /// @brief Destroy the underlying socket.
    /// @return 0 if successful or SOCKET_ERROR.
    ///         If an error occurs, the cause can be retrieved by Socket::GetError().
    int Destroy();

    /// @brief Returns the port number that the underlying socket uses.
    /// @return Port number or SOCKET_ERROR (in which case the underlying socket has not been bound or connected yet).
    int GetPort();

    /// @brief Retrieves a socket option (search for getsockopt() on the Internet for details).
    /// @param level Level for this option (SOL_SOCKET, normally).
    /// @param optname Name of the option to get (SO_ACCEPTCONN, SO_RCVBUF, ...).
    /// @param optval Pointer to buffer for the option's value.
    /// @param optlen Pointer to length of the buffer in optval.
    ///        Must be set to the buffer size before the call, contains the actual size on return.
    /// @return 0 if successful or SOCKET_ERROR.
    ///         If an error occurs, the cause can be retrieved by Socket::GetError().
    int GetOpt(int level, int optname, char* optval, size_t* optlen);

    /// @brief Sets a socket option (search for setsockopt() on the Internet for details).
    /// @param level Level for this option (SOL_SOCKET, normally)
    /// @param optname Name of the option to set (SO_ACCEPTCONN, SO_RCVBUF, ...).
    /// @param optval Pointer to buffer for the option's value.
    /// @param optlen Length of the buffer in optval.
    /// @return 0 if successful or SOCKET_ERROR.
    ///         If an error occurs, the cause can be retrieved by Socket::GetError().
    int SetOpt(int level, int optname, const char* optval, int optlen);

    /// @brief Sets the send timeout for the socket in msecs.
    /// @param msecs Timeout value in milliseconds
    /// @return 0 if successful or SOCKET_ERROR.
    ///         If an error occurs, the cause can be retrieved by Socket::GetError().
    int SetSendTimeout(int msecs = 0);

    /// @brief Sets the receive timeout for the socket in msecs.
    /// @param msecs Timeout value in milliseconds
    /// @return 0 if successful or SOCKET_ERROR.
    ///         If an error occurs, the cause can be retrieved by Socket::GetError().
    int SetReceiveTimeout(int msecs = 0);

    /// @brief Makes socket a listener.
    /// @param backlog Maximum number of connections in the wait queue.
    ///        SOMAXCONN specifies the maximum applicable for the underlying operating system.
    /// @return 0 if successful or SOCKET_ERROR.
    ///         If an error occurs, the cause can be retrieved by Socket::GetError().
    int Listen(int backlog = SOMAXCONN);

    /// @brief Perform an ioctl operation on the socket.
    ///        Search for ioctlsocket() on the Internet for details.
    /// @param cmd IOCTL command.
    /// @param argp Value for the IOCTL command.
    /// @return 0 if successful or SOCKET_ERROR.
    ///         If an error occurs, the cause can be retrieved by Socket::GetError().
    int Ioctl(long cmd, unsigned long* argp);

    /// @brief Accepts a client connection and returns the resulting basic socket.
    /// @param addr Optional pointer to a buffer for the address of the peer.
    /// @param addrlen optional pointer to the length of the buffer address.
    ///        If not used, both addr and addrlen should be set to nullptr.
    /// @return The resulting basic socket or INVALID_SOCKET.
    SOCKET Accept(sockaddr* addr = nullptr, int* addrlen = nullptr);

    /// @brief Waits for something to come in on the socket.
    /// @param timeout Timeout in milliseconds.
    ///        0 means no timeout, i.e., wait indefinitely.
    /// @return 0 if successful or SOCKET_ERROR.
    ///         If an error occurs, the cause can be retrieved by Socket::GetError().
    int Wait(int timeout = 0);

    // @brief Return number of buffered bytes available for reading.
    /// @return >=0 if successful or -1 in case of an error.
    ///         If an error occurs, the cause can be retrieved by Socket::GetError().
    int BytesBuffered();

    /// @brief Returns whether data can be received from the socket.
    /// @return true if data are waiting, false if not (or an error occured).
    bool DataThere();

    /// @brief Send a block of data.
    /// @param data Buffer containing the data to be sent.
    /// @param Length of the data to be sent.
    /// @return 0 if successful or an error code.
    int SendData(const char* data, int datalen);

    /// @brief Send a string.
    /// @param s String to be sent (without terminator).
    /// @return 0 if successful or an error code.
    int SendData(std::string const s) { return SendData(s.c_str(), static_cast<int>(s.size())); }

    /// @brief Receives a block of data from the other side of the connection.
    /// @param lpData Address of buffer to receive into.
    /// @param cbData Size of the buffer.
    /// @param untilFull Flag whether to receive the maximum amount given by cbData.
    ///            If false, the read operation stops after the first read, regardless of the received amount of data.
    /// @return Length of received data block; >0 if successful.
    int ReceiveData(char* lpData, int cbData, bool UntilFull = false);


private:
    void Detach() { s = INVALID_SOCKET; }

};

class SocketListener;
// SocketThread : thread class for a TCP based connection
class SocketThread {
private:
    /// @brief Flag for thread termination.
    volatile bool terminateThread{ false };
    /// @brief Client socket.
    Socket sockClient;
    /// @brief Listener waiting for connections on the Client socket.
    SocketListener* listener;
    std::thread thd;

public:
    SocketThread(SOCKET s, SocketListener* l) : sockClient(s), listener(l) {}
    virtual ~SocketThread() {}

    /// @brief Remembers the passed thread as current.
    /// @param allocThd Freshly deployed thread.
    ///        Gets SWAPPED with the currently remembered thread.
    void SetThread(std::thread& allocThd) { thd.swap(allocThd); }

    /// @brief Returns the current thread object.
    /// @return The current thread object.
    std::thread& GetThread() { return thd; }

    /// @brief Returns the client socket object.
    /// @return The client socket object.
    Socket& GetSocket() { return sockClient; }

    void Run();
    void Terminate() { terminateThread = true; }

    int SendData(const char* data, int datalen) { return sockClient.SendData(data, datalen); }
    int SendData(const std::string s) { return sockClient.SendData(s); }
private:
    virtual void ClientActivity() {}
};

// SocketListener : base class for a socked based (TCP or UDP) listener
class SocketListener {
private:
    Socket sockListener;
    int portListener{ 0 };
    std::string serviceListener{ "" };
    int typeListener{ SOCK_STREAM };
    std::thread thdListener;
    volatile bool terminateListener{ false };
    std::vector<char> dgramBuf;
    std::vector<SocketThread*> streamClient;
    std::vector<SocketThread*> streamClientGone;

public:
    SocketListener(std::string service = "", int sockType = SOCK_STREAM) { if (service.size()) Create(service, sockType); }
    virtual ~SocketListener();

public:
    bool Create(std::string service, int sockType = SOCK_STREAM);

    bool AddClient(SocketThread* thd);
    virtual bool RemoveClient(SocketThread* thd);

    size_t StreamClientSize() { return streamClient.size(); }
    SocketThread* StreamClientAt(size_t i) { return streamClient[i]; }

    void Terminate();

private:
    void ThdFuncListener();
    void RemoveEndedClients();
    virtual void OnDatagram(const char* data, int len) {}
    virtual SocketThread* CreateSocketThread(SOCKET s, SocketListener* l) { return new SocketThread(s, l); }


};

// TcpListener: base class for a socket-based TCP listener
class TcpListener : public SocketListener {
public:
    TcpListener(std::string service) { if (service.size()) Create(service); }
    virtual ~TcpListener() {}

public:
    bool Create(std::string service) { return SocketListener::Create(service, SOCK_STREAM); }

};

#if 0
// Optional: UdpListener base class for a socket-based UDP datagram listener
// For now, only TCP is used
class UdpListener : public SocketListener {
public:
    UdpListener(std::string service) { if (service.size()) Create(service); }
    virtual ~UdpListener() {}

public:
    bool Create(std::string service) { return SocketListener::Create(service, SOCK_DGRAM); }

private:
};
#endif

