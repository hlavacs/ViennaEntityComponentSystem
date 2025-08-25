#pragma once

#include <vector>
#include <thread>
#include <string>

#ifdef WIN32
#define NOMINMAX
#include <winsock2.h>
#else
typedef int SOCKET;
#define INVALID_SOCKET  (SOCKET)(~0)    // copied from winsock.h
#define SOCKET_ERROR            (-1)
#endif

// Socket : class to encapsulate the basic socket functionality

class Socket {
private:
    SOCKET s{ INVALID_SOCKET };

public:
    Socket(SOCKET s = INVALID_SOCKET) : s(s) {}
    ~Socket() { Destroy(); }
    operator SOCKET() { return s; } // if the need arises to use the underlying SOCKET

    Socket& operator=(SOCKET const other) { s = other; return *this; }
    Socket& operator=(Socket& other) { s = other.s; other.Detach(); return *this; }

    static int GetError(); 
    static int GetProtoNumber(std::string proto = "");  // get number for a protocol name
    static int GetServicePort(std::string service, std::string proto = "tcp");
    static unsigned long GetHostAddress(std::string hostName = ""); 

    SOCKET Create(int sType = SOCK_STREAM, std::string proto = "");
    bool IsCreated();
    bool IsSocket();
    int Bind(std::string server = "", std::string service = "", int bReuse = 1);
    int Connect(std::string server, std::string service);
    int Disconnect();
    int Destroy();
    int GetPort();
    int GetOpt(int level, int optname, char* optval, size_t* optlen);
    int SetOpt(int level, int optname, const char* optval, int optlen);

    int SetSendTimeout(int msecs = 0);
    int SetReceiveTimeout(int msecs = 0);
    int Listen(int backlog);
    int Ioctl(long cmd, unsigned long* argp);
    SOCKET Accept(sockaddr* addr, int* addrlen);
    int Wait(int timeout = 0);
    int BytesBuffered();
    bool DataThere();

    int SendData(const char* data, int datalen);
    int SendData(std::string const s) { return SendData(s.c_str(), static_cast<int>(s.size())); }
    int ReceiveData(char* lpData, int cbData, bool UntilFull = false);


private:
    void Detach() { s = INVALID_SOCKET; }

};

class SocketListener;
// SocketThread : thread class for a TCP based connection
class SocketThread {
private:
    volatile bool terminateThread{ false };
    Socket sockClient;
    SocketListener* listener;
    std::thread thd;

public:
    SocketThread(SOCKET s, SocketListener* l) : sockClient(s), listener(l) {}
    virtual ~SocketThread() {}

    void SetThread(std::thread& allocThd) { thd.swap(allocThd); }
    std::thread& GetThread() { return thd; }

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

