#pragma once

#include "Listener.h"
#include "ConsoleRegistry.h"
#include <nlohmann/json.hpp> 
#include <set>


namespace Console {
    //Entity specialization for the watchlist.
    class WatchEntity : public Entity {
    private:
        Archetype arch;
        std::map<size_t, std::string> typeNames;
        std::map<size_t, std::string> tags;

    public:
        WatchEntity() {}
        //if a new WatchEntity is added to the watchlist the constructor collects all needed data from the registry
        WatchEntity(Entity const& org) : Entity(org) {
            if (org.GetArchetype()) {
                arch.copyArchetype(*org.GetArchetype());
                auto registry = org.GetArchetype()->GetRegistry();
                if (registry) {
                    for (auto& component : getComponents()) {
                        typeNames[component.getType()] = registry->GetTypeName(component.getType()).c_str();
                    }
                    for (auto& tag : org.GetArchetype()->getTags()) {
                        tags[tag] = registry->GetTagName(tag);
                    }
                }
            }
            SetArchetype(&arch);
        }

        WatchEntity& operator=(Entity const& org) {
            Entity::operator=(org);
            if (org.GetArchetype()) {
                arch.copyArchetype(*org.GetArchetype());
                auto registry = org.GetArchetype()->GetRegistry();
                if (registry) {
                    for (auto& component : getComponents()) {
                        typeNames[component.getType()] = registry->GetTypeName(component.getType()).c_str();
                    }
                    for (auto& tag : org.GetArchetype()->getTags()) {
                        tags[tag] = registry->GetTagName(tag);
                    }
                }
            }
            SetArchetype(&arch);
            return *this;
        }

        std::string GetTypeName(size_t t) {
            auto it = typeNames.find(t);
            assert(it != typeNames.end());
            return it->second;
        }

        std::string GetTagName(size_t t) {
            auto it = tags.find(t);
            assert(it != tags.end());
            return it->second;
        }


    };


}

// ConsoleSocketThread : socket client thread for the Console

class ConsoleSocketThread : public SocketThread {
private:
    bool handShook{ false };
    int pid{ 0 };
    int entitycount{ 0 };
    Console::Registry snapshot[2]; //we use only one snapshot, but alternating between two prevents conflicts wfile creating new snaphots
    int snapidx{ 0 };
    std::map<size_t, Console::WatchEntity> watchlist;
    bool isLive{ false };
    float avgComp{ 0.f };
    size_t estSize{ 0 };

    virtual void ClientActivity();
    bool ProcessJSON(std::string sjson);
    bool onHandshake(nlohmann::json const& json);
    bool onSnapshot(nlohmann::json const& json) { return parseSnapshot(json); }
    bool onLiveView(nlohmann::json const& json);

public:
    ConsoleSocketThread(SOCKET s, SocketListener* l) : SocketThread(s, l) {}
    virtual ~ConsoleSocketThread() {}

    bool isConnected() { return handShook; }
    int getPid() { return pid; }
    void setPid(int newPid) { pid = newPid; handShook = pid != 0; }
    int getEntitycount() { return entitycount; }
    Console::Registry& getSnapshot() { return snapshot[snapidx]; }

    int lvEntityCount[200]{ 0 };
    int lvEntityMax = 1;

    float getAvgComp() { return avgComp; }
    size_t getEstSize() { return estSize; }

    bool requestSnapshot();
    bool requestLiveView(bool active = true);
    bool sendWatchlist(std::map<size_t, Console::WatchEntity>& watchlist);
    bool getIsLive() { return isLive; }

    bool selected{ false };
    bool parseSnapshot(nlohmann::json const& json);

    void addWatch(size_t handle) { watchlist[handle] = *snapshot[snapidx].findEntity(handle); sendWatchlist(watchlist); }
    void deleteWatch(size_t handle) { watchlist.erase(handle); sendWatchlist(watchlist); }
    bool isWatched(size_t handle) { return watchlist.contains(handle); }
    std::map<size_t, Console::WatchEntity>& getWatchlist() { return watchlist; }


};

// ConsoleListener : derived Listener for the Console.
class ConsoleListener : public TcpListener {
public:
    ConsoleListener(std::string service = "") : TcpListener(service) {
        AddClient(createSocketThread(INVALID_SOCKET, this));  //create an empty Listener for "Load from File"
        getVecs(0)->setPid(1); // force it to PID 1 (which will never come in from any socket)
    }

    int cursel{ -1 };
    virtual bool RemoveClient(SocketThread* thd) {
        if (static_cast<ConsoleSocketThread*>(thd)->selected)
            cursel = -1; // reset selection
        return TcpListener::RemoveClient(thd);
    }

    //there's a 1:1 relation between VECS and client connections
    size_t vecsCount() { return streamClientSize(); }
    ConsoleSocketThread* getVecs(size_t i) { return static_cast<ConsoleSocketThread*>(streamClientAt(i)); }

private:
    virtual SocketThread* createSocketThread(SOCKET s, SocketListener* l) { return new ConsoleSocketThread(s, l); }
};

