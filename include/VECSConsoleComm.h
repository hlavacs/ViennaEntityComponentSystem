#pragma once

#ifdef WIN32
extern "C" {
    //F'in Windows.h includes minwindef.h which overrides min, which collides with C++ std::min
    // ... so suppress that.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <thread>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

// VS and its posix warnings ... <sigh>
#define getpid() _getpid()

}

#else
typedef int SOCKET;
#define INVALID_SOCKET  (SOCKET)(~0)    // copied from winsock.h
#define SOCKET_ERROR            (-1)
#endif

#include <nlohmann/json.hpp> 

namespace vecs {

    // the age-old chicken-and-egg problem in its header-only variant ...
    // we need to declare two classes : registry and console communication classes.
    // Each one should know the other, and each one should be able  to use the other's methods.
    // How can we declare that?
    // Here's one way:
    // If the VECS program should use console communication, define an interface class and derive
    // the VECS registry class from that. The console class can then use the interface methods without
    // having to have full knowledge of the registry.
    // The registry, however, can see the full method set of the console communication class.
    // To do so, declare VECSConsoleCommInterface (i.e., include VECSConsoleComm.h in VECSRegistry.h)
    // before declaring the registry class and derive Registry from VECSConsoleInterface.

    class VECSConsoleInterface {
    public:
        // Snapshot functionality
        virtual std::string getSnapshot() = 0;
        // LiveView functionality
        virtual std::string getLiveView() = 0;
        virtual size_t Size() = 0;
        virtual std::string toJSON(Handle h) = 0;
    };

    class VECSConsoleComm {

    private:
        std::jthread commThread;
        std::mutex socketMutex;
        bool running = false;

        // LiveView : encapsulation for all LiveView-related data
        class LiveView {

        public:
            LiveView() {}  // don't need anything yet
            ~LiveView() {}  // don't need anything yet

            void setRegistry(VECSConsoleInterface* reg = nullptr) { registry = reg; }

            bool SetActive(bool onoff = true) { bool old = active; active = onoff; return old; }
            bool IsActive() const { return active; }
            bool Watch(std::set<Handle>& newSet) {
                // determine handles to remove
                std::set<Handle> toRemove;
                for (auto& el : watched)
                    if (!newSet.contains(el.first))
                        toRemove.insert(el.first);
                // add new handles
                for (auto& el : newSet)
                    if (!watched.contains(el))
                        watched[el] = "";
                // remove unwatched handles
                for (auto& el : toRemove)
                    watched.erase(el);
                return true; // needed?
            }

            // TODO: make these thread-safe. The listener thread might come in at any moment and fetch the changes.
            std::tuple<bool, std::string> getChangesJSON() {
                if (!active || !registry)
                    return std::tuple<bool, std::string>(false, "");
                size_t oldHandles = handles; handles = registry->Size();
                bool changes = oldHandles != handles;
                std::string json = "{\"cmd\":\"liveview\"";
                if (changes) {
                    json += std::string(",\"entities\":") + std::to_string(handles);
                }
                size_t changedWatch = 0;
                for (auto& entity : watched) {
                    std::string entityJSON = registry->toJSON(entity.first);
                    if (entityJSON != entity.second) {
                        if (!changedWatch++)
                            json += ",\"watched\":[{";
                        else
                            json += ",{";
                        entity.second = entityJSON;
                        changes = true;
                        json += std::string("\"entity\":") +
                            std::to_string(entity.first.GetIndex()) +
                            ",\"data\":" +
                            entityJSON +
                            "}";
                    }
                }
                if (changedWatch)
                    json += "]";
                json += "}";
                return std::tuple<bool, std::string>(changes, json);
            }

        private:
            VECSConsoleInterface* registry{ nullptr };
            std::map<Handle, std::string> watched;
            bool active{ false };
            size_t handles{ 0 };

        } liveView; //declare member variable

        // design question ... do we want one comm object for each VECS registry, or one for all registries in the program?
        // for now, let's assume a simple 1:1 relation - each registry gets its own connection object
        VECSConsoleInterface* registry{ nullptr };

    public:
        ~VECSConsoleComm() {
            disconnectFromServer();
            cleanup();
        }

        // simple variant: handle exactly one registry
        void setRegistry(VECSConsoleInterface* reg = nullptr) { registry = reg; liveView.setRegistry(reg); }

        SOCKET connectToServer(std::string host = "127.0.0.1", int port = 2000) {
            if (!startup())
                return INVALID_SOCKET;

            if (ConnectSocket != INVALID_SOCKET)
                return ConnectSocket;
            // Alternate posibility: disconnectFromServer();

            ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (ConnectSocket == INVALID_SOCKET)
                return INVALID_SOCKET;

            sockaddr_in clientService;
            clientService.sin_family = AF_INET;
            clientService.sin_addr.s_addr = inet_addr(host.c_str());
            clientService.sin_port = htons(port);

            //----------------------
            // Connect to server.

            // VECSConsoleComm::connectToServer() so erweitern, dass es, wenn connect() eine Verbindung herstellen konnte (also 0 zurueckgibt), einen eigenen Thread startet
            int returnval = connect(ConnectSocket, (SOCKADDR*)&clientService, sizeof(clientService));
            if (returnval == 0) {
                running = true;
                commThread = std::jthread(&VECSConsoleComm::handleConnection, this);
            }
            else if (returnval < 0) {
                disconnectFromServer();
            }
            return ConnectSocket;
        }
        bool isConnected() {
            return ConnectSocket != INVALID_SOCKET;
        }

        int disconnectFromServer() {
            if (ConnectSocket != INVALID_SOCKET) {
#ifdef _WINSOCKAPI_
                int irc = closesocket(ConnectSocket);
#else
                int irc = close(ConnectSocket);
#endif
                ConnectSocket = INVALID_SOCKET;
            }
            return 0;
        }

        void handleConnection() {
            while (running) {
                std::string msg = receiveMessage();

                // If message is empty, it could mean a disconnect or error
                if (msg.empty()) {
                    std::cout << "Disconnected from server or error occurred.\n";
                    running = false;
                    break;
                }

                // Handle the received message (you can expand this!)
                processMessage(msg);
            }

            //TODO: close connection here? 

        }


    private:
        void processMessage(std::string msg) {

            //Process JSon
            nlohmann::json msgjson;
            try {
                msgjson = nlohmann::json::parse(msg);
            }
            // could catch specific JSON error here, but for now, don't.
            catch (...) {
                return;
            }

            // convert {"cmd":"..."} to enumeration
            enum {
                cmdUnknown,
                cmdHandshake,
                cmdSnapshot,
                cmdLiveView,
            };
            static std::map<std::string, int> cmds;
            if (!cmds.size()) {
                cmds["handshake"] = cmdHandshake;
                cmds["snapshot"] = cmdSnapshot;
                cmds["liveview"] = cmdLiveView;
            }
            int cmd{ cmdUnknown };
            try {
                auto cmdit = cmds.find(msgjson["cmd"]);
                if (cmdit != cmds.end())
                    cmd = cmdit->second;
            }
            // could catch specific JSON error here, but for now, don't.
            catch (...) {
                // ignore, following switch simply won't do anything
            }

            switch (cmd) {
            case cmdHandshake:
                // Here, the following could / should be done:
                // .) check whether compatible Console version - close connection if not (keep error description somewhere, maybe?)
                // ... but for now, simply move on.

                // TEST TEST TEST
                std::cout << "Hello from Console pid " << msgjson["pid"] << " built " << msgjson["compiled"] << "!\n";

                // prepare a nice "Hello yourself!" to tell Console about us
                sendMessage("{\"cmd\":\"handshake\", \"pid\":" + std::to_string(getpid()) + " }");
                break;
            case cmdSnapshot:
                // snapshot
                if (!registry) {       // if no registry there,
                    sendMessage("{}"); // send empty object (maybe some kind of error object would be better?)
                }
                // first variant: simply get full snapshot in JSON form from the registry
#if 1 // TEST TEST TEST TEST TEST TEST
                {
                    std::string josnap = registry->getSnapshot();
                    sendMessage(josnap);
                    std::cout << "Sending snapshot: " << josnap << "\n";

                }
#else
                sendMessage(registry->getSnapshot());
#endif
                break;
            case cmdLiveView:
                // Live View - can have following attributes:
                // "active":true|false  - sets live view active or inactive
                if (msgjson.contains("active")) {
                    auto& act = msgjson["active"];
                    if (act.is_boolean()) {
                        liveView.SetActive(act);
                        // TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST
                        std::string jsonlive = registry->getLiveView();
                        sendMessage(jsonlive);
                        std::cout << "Sending liveview: " << jsonlive << "\n";

                    } // else ignore
                }
                // "watch":id or [id,id,...]  - adds a (set of) id(s) to the watched set
                if (msgjson.contains("watchlist")) {
                    auto& watch = msgjson["watchlist"];
                    std::set<Handle> newWatchlist;
                    if (watch.is_array()) {
                        for (auto& el : watch) {
                            if (el.is_number_unsigned()) {
                                size_t wh = el;
                                newWatchlist.insert(static_cast<Handle>(wh));
                            }
                        }
                    }
                    else if (watch.is_number_unsigned()) {
                        size_t wh = watch;
                        newWatchlist.insert(static_cast<Handle>(wh));
                    } // else ignore.
                    liveView.Watch(newWatchlist);
                }

                break;
            default:
                // keep compiler happy

                // TEST TEST TEST TEST
                std::cout << "Incoming command \"" << msgjson["cmd"] << "\" ignored\n";
                break;
            }

        }

        int sendMessage(std::string sendmessage) const {
            const char* csendmessage = sendmessage.c_str();
            return send(ConnectSocket, csendmessage, static_cast<int>(sendmessage.length()), 0);
        }

        std::string receiveMessage() {
            char buffer[1024];
            int received = 0;

            while (running) {
                if (ConnectSocket == INVALID_SOCKET)
                    return "";

                int selectrc;
                timeval tmou{ .tv_usec = 200L * 1000L };  // that's 200 ms, i.e., 5 times per second
                const timeval* ptmou = (tmou.tv_sec || tmou.tv_usec) ? &tmou : NULL;
                fd_set fds;
                do {
                    // setup selector set (... of one ...)
                    FD_ZERO(&fds);
                    FD_SET(ConnectSocket, &fds);
                    selectrc = select(static_cast<int>(ConnectSocket + 1), &fds, NULL, NULL, ptmou);
                    // then wait for activity (or lack thereof)
                    if (selectrc == 0) {
                        // no data read from the other side within timeout period

                        // TODO: in liveview, send assembled changes
                        auto lv = liveView.getChangesJSON();
                        if (std::get<0>(lv)) {
                            // TEST TEST TEST TEST TEST TEST TEST TEST TEST
                            std::cout << "Liveview changes: " << std::get<1>(lv) << "\n";
                            sendMessage(std::get<1>(lv));
                        }
                    }
                } while (selectrc == 0);
                if (selectrc != INVALID_SOCKET)
                    received = recv(ConnectSocket, buffer, sizeof(buffer) - 1, 0);
                else
                    received = -1;

                if (received > 0) {
                    buffer[received] = '\0';
                    return std::string(buffer);
                }
                else if (received == 0) {
                    // in all likelyhood, the connection was dropped from the other side, so ...
                    // close from our side, too
                    disconnectFromServer();
                }
                else {
                    // some network error or blocking would occur for non-blocking sockets
                    // we don't do non-blocking socket opreations here, but why not capture it
#ifdef _WINSOCKAPI_
                    if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
#endif
                        disconnectFromServer();
                }
            }
            return "";
        }

    private:
        bool started{ false };
        SOCKET ConnectSocket{ INVALID_SOCKET };

        bool startup() {
#ifdef _WINSOCKAPI_
            if (!started) {
                WSADATA wsaData;
                int iResult = WSAStartup(MAKEWORD(1, 1), &wsaData);
                if (iResult != NO_ERROR)
                    return false;
            }
#endif
            started = true;
            return true;
        }
        void cleanup() {
#ifdef _WINSOCKAPI_
            if (started) {
                WSACleanup();
            }
#endif
            started = false;
        }
    };

}
