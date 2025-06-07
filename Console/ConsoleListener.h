#pragma once

#include "Listener.h"

#include <nlohmann/json.hpp> 


// ConsoleSocketThread : socket client thread for the Console

class ConsoleSocketThread : public SocketThread {
public:
    ConsoleSocketThread(SOCKET s, SocketListener* l) : SocketThread(s, l) {}
    virtual ~ConsoleSocketThread() {}

    bool isConnected() { return handShook; }  // that can surely be enhanced later on
    int getPid() { return pid; }
    int getEntitycount() { return entitycount; }

    bool requestSnapshot();
    bool requestLiveView();  // presumably expanded on in later versions
    bool selected{ false };

private:
    virtual void ClientActivity();
    bool ProcessJSON(std::string sjson);
    bool onHandshake(nlohmann::json const& json);
    bool onSnapshot(nlohmann::json const& json);
    bool onLiveView(nlohmann::json const& json);
private:
    bool handShook{ false };
    int pid{ 0 };
    int entitycount{ 0 };
};

// ConsoleListener : derived Listener for the Console.

class ConsoleListener : public TcpListener {
public:
    ConsoleListener(std::string service = "") : TcpListener(service) {}

    //TODO: if selected Thread is gone, toss selection
    int cursel{ -1 };
    virtual bool RemoveClient(SocketThread* thd) {
        if (static_cast<ConsoleSocketThread*>(thd)->selected)
            cursel = -1; // selektion zurücksetzen
        return TcpListener::RemoveClient(thd);
    }
    // TODO: all operations that interact with the Console

    // for now, there's a 1:1 relation between VECS and client connections
    size_t vecsCount() { return streamClientSize(); }
    ConsoleSocketThread* getVecs(size_t i) { return static_cast<ConsoleSocketThread*>(streamClientAt(i)); }

private:
    virtual SocketThread* createSocketThread(SOCKET s, SocketListener* l) { return new ConsoleSocketThread(s, l); }
};

