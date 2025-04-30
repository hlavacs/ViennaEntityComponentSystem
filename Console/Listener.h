#pragma once

#include <vector>
#include <thread>
#include <string>

#ifdef WIN32
#include <winsock2.h>
#else
typedef int SOCKET;
#define INVALID_SOCKET  (SOCKET)(~0)    // copied from winsock.h
#define SOCKET_ERROR            (-1)
#endif

// SocketThread : thread class for a TCP based connection

class SocketListener;
class SocketThread {
public:
    SocketThread(SOCKET s, SocketListener *l) : sockClient(s), listener(l) { }
    virtual ~SocketThread() {}

    void SetThread(std::thread &allocThd) { thd.swap(allocThd); }
    std::thread &GetThread() { return thd; }

    void Run();
    void Terminate() { terminateThread = true; }

private:
    virtual void ClientActivity() { } // has to be overridden for any meaningful activity!

private:
    volatile bool terminateThread{ false };
    SOCKET sockClient;
    SocketListener *listener;
    std::thread thd;
};

// SocketListener : base class for a socked based (TCP or UDP) listener

class SocketListener {
public:
    SocketListener(std::string service = "", int sockType = SOCK_STREAM) { if (service.size()) Create(service, sockType); }
    virtual ~SocketListener();

public:
    bool Create(std::string service, int sockType = SOCK_STREAM);

    bool AddClient(SocketThread *thd);
    bool RemoveClient(SocketThread *thd);

    void Terminate();

private :
    void thdFuncListener();
    void removeEndedClients();
    virtual void onDatagram(const char *data, int len) { }
    virtual SocketThread *createSocketThread(SOCKET s, SocketListener *l) { return new SocketThread(s, l); }

private:
    SOCKET sockListener{ INVALID_SOCKET };
    int portListener{ 0 };
    std::string serviceListener { "" };
    int typeListener { SOCK_STREAM };
    std::thread thdListener;
    volatile bool terminateListener { false };
    std::vector<char> dgramBuf;
    std::vector<SocketThread *> streamClient;
    std::vector<SocketThread*> streamClientGone;
};

// TcpListener: base class for a socket-based TCP listener

class TcpListener : public SocketListener {
public:
    TcpListener(std::string service) { if (service.size()) Create(service); }
    virtual ~TcpListener() { }

public:
    bool Create(std::string service) { return SocketListener::Create(service, SOCK_STREAM); }

private:
};

