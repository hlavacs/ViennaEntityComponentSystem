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
                arch.CopyArchetype(*org.GetArchetype());
                auto registry = org.GetArchetype()->GetRegistry();
                if (registry) {
                    for (auto& component : GetComponents()) {
                        typeNames[component.GetType()] = registry->GetTypeName(component.GetType()).c_str();
                    }
                    for (auto& tag : org.GetArchetype()->GetTags()) {
                        tags[tag] = registry->GetTagName(tag);
                    }
                }
            }
            SetArchetype(&arch);
        }

        WatchEntity& operator=(Entity const& org) {
            Entity::operator=(org);
            if (org.GetArchetype()) {
                arch.CopyArchetype(*org.GetArchetype());
                auto registry = org.GetArchetype()->GetRegistry();
                if (registry) {
                    for (auto& component : GetComponents()) {
                        typeNames[component.GetType()] = registry->GetTypeName(component.GetType()).c_str();
                    }
                    for (auto& tag : org.GetArchetype()->GetTags()) {
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
    bool OnHandshake(nlohmann::json const& json);
    bool OnSnapshot(nlohmann::json const& json) { return ParseSnapshot(json); }
    bool OnLiveView(nlohmann::json const& json);

public:
    ConsoleSocketThread(SOCKET s, SocketListener* l) : SocketThread(s, l) {}
    virtual ~ConsoleSocketThread() {}

   /* bool IsConnected() { return handShook; }*/
    int GetPid() { return pid; }
    void SetPid(int newPid) { pid = newPid; handShook = pid != 0; }
    /*int GetEntitycount() { return entitycount; }*/
    Console::Registry& GetSnapshot() { return snapshot[snapidx]; }

    int lvEntityCount[200]{ 0 };
    int lvEntityMax = 1;

    float GetAvgComp() { return avgComp; }
    size_t GetEstSize() { return estSize; }

    bool RequestSnapshot();
    bool RequestLiveView(bool active = true);
    bool SendWatchlist(std::map<size_t, Console::WatchEntity>& watchlist);
    bool GetIsLive() { return isLive; }

    bool selected{ false };
    bool ParseSnapshot(nlohmann::json const& json);

    void AddWatch(size_t handle) { watchlist[handle] = *snapshot[snapidx].FindEntity(handle); SendWatchlist(watchlist); }
    void DeleteWatch(size_t handle) { watchlist.erase(handle); SendWatchlist(watchlist); }
    bool IsWatched(size_t handle) { return watchlist.contains(handle); }
    std::map<size_t, Console::WatchEntity>& GetWatchlist() { return watchlist; }


};

// ConsoleListener : derived Listener for the Console.
class ConsoleListener : public TcpListener {
public:
    ConsoleListener(std::string service = "") : TcpListener(service) {
        AddClient(CreateSocketThread(INVALID_SOCKET, this));  //create an empty Listener for "Load from File"
        GetVecs(0)->SetPid(1); // force it to PID 1 (which will never come in from any socket)
    }

    int cursel{ -1 };
    virtual bool RemoveClient(SocketThread* thd) {
        if (static_cast<ConsoleSocketThread*>(thd)->selected)
            cursel = -1; // reset selection
        return TcpListener::RemoveClient(thd);
    }

    //there's a 1:1 relation between VECS and client connections
    size_t VecsCount() { return StreamClientSize(); }
    ConsoleSocketThread* GetVecs(size_t i) { return static_cast<ConsoleSocketThread*>(StreamClientAt(i)); }

private:
    virtual SocketThread* CreateSocketThread(SOCKET s, SocketListener* l) { return new ConsoleSocketThread(s, l); }
};

