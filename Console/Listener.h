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
    ~Socket() { destroy(); }
    operator SOCKET() { return s; } // if the need arises to use the underlying SOCKET

    // TODO: make this more intelligent. Copying transfers ownership!
    Socket& operator=(SOCKET const other) { s = other; return *this; }
    Socket& operator=(Socket& other) { s = other.s; other.detach(); return *this; }

    static int getError();  // retrieve last (socket) error
    static int getProtoNumber(std::string proto = "");  // get number for a protocol name
    static int getServicePort(std::string service, std::string proto = "tcp");
    static unsigned long getHostAddress(std::string hostName = "");  // TODO: this is IPv4-specific ATM!

    SOCKET create(int sType = SOCK_STREAM, std::string proto = "");
    bool isCreated();
    bool isSocket();
    int bind(std::string server = "", std::string service = "", int bReuse = 1);
    int connect(std::string server, std::string service);
    int disconnect();
    int destroy();
    int getPort();
    int getOpt(int level, int optname, char* optval, size_t* optlen);
    int setOpt(int level, int optname, const char* optval, int optlen);

    int setSendTimeout(int msecs = 0);
    int setReceiveTimeout(int msecs = 0);
    int listen(int backlog);
    int ioctl(long cmd, unsigned long* argp);
    SOCKET accept(sockaddr* addr, int* addrlen);
    int wait(int timeout = 0);
    int bytesBuffered();
    bool dataThere();

    int sendData(const char* data, int datalen);
    int sendData(std::string const s) { return sendData(s.c_str(), static_cast<int>(s.size())); }
    int receiveData(char* lpData, int cbData, bool UntilFull = false);


private:
    void detach() { s = INVALID_SOCKET; }

};

// SocketThread : thread class for a TCP based connection

class SocketListener;
class SocketThread {
public:
    SocketThread(SOCKET s, SocketListener* l) : sockClient(s), listener(l) {}
    virtual ~SocketThread() {}

    void SetThread(std::thread& allocThd) { thd.swap(allocThd); }
    std::thread& GetThread() { return thd; }

    Socket& getSocket() { return sockClient; }

    void Run();
    void Terminate() { terminateThread = true; }

    int sendData(const char* data, int datalen) { return sockClient.sendData(data, datalen); }
    int sendData(const std::string s) { return sockClient.sendData(s); }
private:
    virtual void ClientActivity() {} // has to be overridden for any meaningful activity!

private:
    volatile bool terminateThread{ false };
    Socket sockClient;
    SocketListener* listener;
    std::thread thd;
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

    size_t streamClientSize() { return streamClient.size(); }
    SocketThread* streamClientAt(size_t i) { return streamClient[i]; }

    void Terminate();

private:
    void thdFuncListener();
    void removeEndedClients();
    virtual void onDatagram(const char* data, int len) {}
    virtual SocketThread* createSocketThread(SOCKET s, SocketListener* l) { return new SocketThread(s, l); }


};

// UdpListener: base class for a socket-based UDP datagram listener

class UdpListener : public SocketListener {
public:
    UdpListener(std::string service) { if (service.size()) Create(service); }
    virtual ~UdpListener() {}

public:
    bool Create(std::string service) { return SocketListener::Create(service, SOCK_DGRAM); }

private:
};

// TcpListener: base class for a socket-based TCP listener

class TcpListener : public SocketListener {
public:
    TcpListener(std::string service) { if (service.size()) Create(service); }
    virtual ~TcpListener() {}

public:
    bool Create(std::string service) { return SocketListener::Create(service, SOCK_STREAM); }

private:
};

