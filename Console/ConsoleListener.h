#pragma once

#include "Listener.h"
#include "ConsoleRegistry.h"
#if 1
// Include NLohmann JSON directly instead of using a submodule
#include "json.hpp"
#else
#include <nlohmann/json.hpp> 
#endif
#include <set>


namespace Console {
    /// @brief Entity specialization for the watchlist.
    class WatchEntity : public Entity {
    private:
        Archetype arch;
        std::map<size_t, std::string> typeNames;
        std::map<size_t, std::string> tags;

    public:
        WatchEntity() {}
        /// @brief if a new WatchEntity is added to the watchlist the constructor collects all needed data from the registry
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

        /// @brief get name for a specific type
        /// @param t type as hash
        /// @return typename as string
        std::string GetTypeName(size_t t) {
            auto it = typeNames.find(t);
            assert(it != typeNames.end());
            return it->second;
        }

        /// @brief get name for a specific tag
        /// @param t tag as number
        /// @return tagname as string
        std::string GetTagName(size_t t) {
            auto it = tags.find(t);
            assert(it != tags.end());
            return it->second;
        }


    };


}

// ConsoleSocketThread : socket client thread for the Console

class ConsoleSocketThread : public SocketThread {
public:
    int lvEntityCount[200]{ 0 };
    int lvEntityMax = 1;
    bool selected{ false };

private:
    bool handShook{ false };
    int pid{ 0 };
    int entitycount{ 0 };
    Console::Registry snapshot[2]; //we use only one snapshot, but alternating between two prevents conflicts while creating new snaphots
    int snapidx{ 0 };
    std::map<size_t, Console::WatchEntity> watchlist;
    bool isLive{ false };
    float avgComp{ 0.f };
    size_t estSize{ 0 };

    virtual void ClientActivity();

    /// @brief processes incoming json strings
    /// @param sjson String representing json data
    /// @return true if valid json
    bool ProcessJSON(std::string sjson);

    /// @brief handle incoming handshake commands
    /// @param sjson string containing the Json object
    /// @param json Json containing handshake data
    /// @return true if valid json
    bool OnHandshake(std::string& sjson, nlohmann::json& json);

    /// @brief parse incoming snapshots
    /// @param sjson string containing the Json object
    /// @param json Json containing snapshot data
    /// @return true if valid json
    bool OnSnapshot(std::string& sjson, nlohmann::json& json) { return ParseSnapshot(json, &sjson); }

    /// @brief parse incoming live view data, create internal structure for it that can be handled from GUI
    /// @param sjson string containing the Json object
    /// @param json Json containing live view data
    /// @return true if valid json
    bool OnLiveView(std::string& sjson, nlohmann::json& json);

public:
    ConsoleSocketThread(SOCKET s, SocketListener* l) : SocketThread(s, l) {}
    virtual ~ConsoleSocketThread() {}

    int GetPid() { return pid; }
    void SetPid(int newPid) { pid = newPid; handShook = pid != 0; }
    Console::Registry& GetSnapshot() { return snapshot[snapidx]; }

    float GetAvgComp() { return avgComp; }
    size_t GetEstSize() { return estSize; }

    /// @brief request a snapshot from the connected vecs
    /// @return true if request was sent
    bool RequestSnapshot();

    /// @brief request live view communication from the connected vecs
    /// @param active Bool to activate or deactivate the live view
    /// @return true if request was sent
    bool RequestLiveView(bool active = true);

    /// @brief send watchlist to the connected vecs
    /// @param watchlist map of watched entities with their handles
    /// @return true if request was sent
    bool SendWatchlist(std::map<size_t, Console::WatchEntity>& watchlist);

    /// @brief returns true if live view is live
    bool GetIsLive() { return isLive; }

    /// @brief parse incoming snapshots
    /// @param json Json containing snapshot data
    /// @param psjson (optional) pointer to string containing the Json object
    /// @return true if valid json
    bool ParseSnapshot(nlohmann::json& json, std::string* psjson = nullptr);

    /// @brief add a entity to be watched and send updated watchlist to vecs
    void AddWatch(size_t handle) { watchlist[handle] = *snapshot[snapidx].FindEntity(handle); SendWatchlist(watchlist); }

    /// @brief delete a entity to be watched and send updated watchlist to vecs
    void DeleteWatch(size_t handle) { watchlist.erase(handle); SendWatchlist(watchlist); }

    /// @brief returns whether an entity is in the watchlist
    /// @return true if entity is in watchlist
    bool IsWatched(size_t handle) { return watchlist.contains(handle); }

    /// @brief get the watchlist
    /// @return watchlist as map consisting of WatchEntity with their handles
    std::map<size_t, Console::WatchEntity>& GetWatchlist() { return watchlist; }


};

/// @brief derived Listener for the Console.
class ConsoleListener : public TcpListener {
public:
    int cursel{ -1 };

    /// @brief ConsoleListener contructor
    ConsoleListener(std::string service = "") : TcpListener(service) {
        AddClient(CreateSocketThread(INVALID_SOCKET, this));  //create an empty Listener for "Load from File"
        GetVecs(0)->SetPid(1); // force it to PID 1 (which will never come in from any socket)
    }

    /// @brief remove a Client
    /// @param thd SocketThread to be removed
    /// @return true if successfull
    virtual bool RemoveClient(SocketThread* thd) {
        if (static_cast<ConsoleSocketThread*>(thd)->selected)
            cursel = -1; // reset selection
        return TcpListener::RemoveClient(thd);
    }

    /// @brief get number of connected vecs (plus empty thread for loading from file)
    /// @return number of connected vecs
    size_t VecsCount() { return StreamClientSize(); }

    /// @brief get a specific vecs
    /// @param i index 
    /// @return vecs connection handling thread
    ConsoleSocketThread* GetVecs(size_t i) { return static_cast<ConsoleSocketThread*>(StreamClientAt(i)); }

private:
    /// @brief create a Console Socket thread
    /// @param s Socket
    /// @param l SocketListener
    /// @return SocketThread
    virtual SocketThread* CreateSocketThread(SOCKET s, SocketListener* l) { return new ConsoleSocketThread(s, l); }
};

