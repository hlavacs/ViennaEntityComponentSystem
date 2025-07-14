#pragma once

#include "Listener.h"

#include "ConsoleRegistry.h"

#include <nlohmann/json.hpp> 
#include <set>


// ConsoleSocketThread : socket client thread for the Console

class ConsoleSocketThread : public SocketThread {
private:
    bool handShook{ false };
    int pid{ 0 };
    int entitycount{ 0 };
    Console::Registry snapshot;  // we deal with exactly ONE snapshot at the moment
    std::set<size_t> watchlist;
    bool isLive{ false }; 

    virtual void ClientActivity();
    bool ProcessJSON(std::string sjson);
    bool onHandshake(nlohmann::json const& json);
    bool onSnapshot(nlohmann::json const& json) { return parseSnapshot(json); }
    bool onLiveView(nlohmann::json const& json);

public:
    ConsoleSocketThread(SOCKET s, SocketListener* l) : SocketThread(s, l) {}
    virtual ~ConsoleSocketThread() {}

    bool isConnected() { return handShook; }  // that can surely be enhanced later on
    int getPid() { return pid; }
    void setPid(int newPid) { pid = newPid; handShook = pid != 0; }
    int getEntitycount() { return entitycount; }
    Console::Registry& getSnapshot() { return snapshot; }

    int lvEntityCount[200]{ 0 };
    int lvEntityMax = 1;

    bool requestSnapshot();
    bool requestLiveView(bool active = true);  // presumably expanded on in later versions
    bool sendWatchlist(std::set<size_t>& watchlist);
    bool getIsLive() { return isLive; } // nanananana

    bool selected{ false };
    bool parseSnapshot(nlohmann::json const& json);

    void addWatch(size_t handle) { watchlist.insert(handle); sendWatchlist(watchlist); }
    void deleteWatch(size_t handle) { watchlist.erase(handle); sendWatchlist(watchlist); }
    bool isWatched(size_t handle) { return watchlist.contains(handle); }
    std::set<size_t>& getWatchlist() { return watchlist; }


};

// ConsoleListener : derived Listener for the Console.

class ConsoleListener : public TcpListener {
public:
    ConsoleListener(std::string service = "") : TcpListener(service) {
        // add an empty "thread" at the start for snapshot file
        AddClient(createSocketThread(INVALID_SOCKET, this));
        // force it to PID 1 (which will never come in from any socket)
        getVecs(0)->setPid(1);
    }

    //TODO: if selected Thread is gone, toss selection
    int cursel{ -1 };
    virtual bool RemoveClient(SocketThread* thd) {
        if (static_cast<ConsoleSocketThread*>(thd)->selected)
            cursel = -1; // reset selection
        return TcpListener::RemoveClient(thd);
    }
    // TODO: all operations that interact with the Console

    // for now, there's a 1:1 relation between VECS and client connections
    size_t vecsCount() { return streamClientSize(); }
    ConsoleSocketThread* getVecs(size_t i) { return static_cast<ConsoleSocketThread*>(streamClientAt(i)); }

private:
    virtual SocketThread* createSocketThread(SOCKET s, SocketListener* l) { return new ConsoleSocketThread(s, l); }
};

