#pragma once

#include "Listener.h"

// ConsoleSocketThread : socket client thread for the Console

class ConsoleSocketThread : public SocketThread {
public:
    ConsoleSocketThread(SOCKET s, SocketListener* l) : SocketThread(s, l) { }
    virtual ~ConsoleSocketThread() {}

private:
    virtual void ClientActivity();

};

// ConsoleListener : derived Listener for the Console.

class ConsoleListener : public TcpListener {
public:
    ConsoleListener(std::string service = "") : TcpListener(service) {}

    // TODO: all operations that interact with the Console

private:
    virtual SocketThread* createSocketThread(SOCKET s, SocketListener* l) { return new ConsoleSocketThread(s, l); }

};