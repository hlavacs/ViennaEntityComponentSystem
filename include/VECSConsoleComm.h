#pragma once

#ifdef WIN32
extern "C" {
    //F'in Windows.h includes minwindef.h which overrides min, which collides with C++ std::min
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

// Still warnings here
#define getpid() _getpid()

}

#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int SOCKET;
#define INVALID_SOCKET  (SOCKET)(~0)    // copied from winsock.h
#define SOCKET_ERROR            (-1)
#endif

#include <thread>
#include <nlohmann/json.hpp> 

namespace vecs {

    class VECSConsoleComm {

    private:
        std::jthread commThread;
        std::jthread initThread;
        std::mutex socketMutex;
        bool running = false;

        // LiveView : encapsulation for all LiveView-related data
        class LiveView {

        private:
            Registry* registry{ nullptr };
            std::map<Handle, std::string> watched;
            bool active{ false };
            size_t handles{ 0 };
            float avgComp{ 0.f };
            size_t estSize{ 0 };

        public:
            LiveView() {}
            ~LiveView() {}

            void setRegistry(Registry* reg = nullptr) { registry = reg; }

            bool SetActive(bool onoff = true) { bool old = active; active = onoff; return old; }
            bool IsActive() const { return active; }
            bool Watch(std::set<Handle>& newSet) {
                std::set<Handle> toRemove;
                for (auto& el : watched)
                    if (!newSet.contains(el.first))
                        toRemove.insert(el.first);
                for (auto& el : newSet)
                    if (!watched.contains(el))
                        watched[el] = "";
                for (auto& el : toRemove)
                    watched.erase(el);
                return true;
            }

            std::tuple<bool, std::string> getChangesJSON() {
                if (!active || !registry)
                    return std::tuple<bool, std::string>(false, "");

                registry->GetMutex().lock();

                size_t oldHandles = handles; handles = registry->Size();
                bool changes = oldHandles != handles;
                std::string json = "{\"cmd\":\"liveview\"";
                if (changes) {
                    json += std::string(",\"entities\":") + std::to_string(handles);
                }

                float oldAvgComp = avgComp;
                avgComp = registry->GetAvgComp();
                bool compChanges = oldAvgComp != avgComp;
                if (compChanges) {
                    json += std::string(",\"avgComp\":") + std::to_string(avgComp);
                    changes = true;
                }

                size_t oldEstSize = estSize;
                estSize = registry->GetEstSize();
                compChanges = oldEstSize != estSize;
                if (compChanges) {
                    json += std::string(",\"estSize\":") + std::to_string(estSize);
                    changes = true;
                }


                size_t changedWatch = 0;
                for (auto& entity : watched) {
                    std::string entityJSON = registry->ToJSON(entity.first);
                    if (entityJSON != entity.second) {

                        entity.second = entityJSON;

                        if (!changedWatch++)
                            json += ",\"watched\":[{";
                        else
                            json += ",{";
                        changes = true;
                        json += std::string("\"entity\":") +
                            std::to_string(entity.first.GetIndex()) +
                            ",\"values\":" +
                            entityJSON +
                            "}";
                    }
                }
                if (changedWatch)
                    json += "]";
                json += "}";
                registry->GetMutex().unlock();
                return std::tuple<bool, std::string>(changes, json);
            }

        } liveView; //declare member variable

        Registry* registry{ nullptr };

    public:
        ~VECSConsoleComm() {
            DisconnectFromServer();
            Cleanup();
        }

        //handle one registry
        void SetRegistry(Registry* reg = nullptr) { registry = reg; liveView.setRegistry(reg); }

        SOCKET ConnectToServer(Registry* reg, std::string host = "127.0.0.1", int port = 2000) {
            if (!Startup())
                return INVALID_SOCKET;

            SetRegistry(reg);

            if (ConnectSocket != INVALID_SOCKET)
                return ConnectSocket;

            ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (ConnectSocket == INVALID_SOCKET)
                return INVALID_SOCKET;

            sockaddr_in clientService;
            clientService.sin_family = AF_INET;
            clientService.sin_addr.s_addr = inet_addr(host.c_str());
            clientService.sin_port = htons(port);

            //----------------------
            // Connect to server.

            int returnval = connect(ConnectSocket, (const struct sockaddr*)&clientService, sizeof(clientService));
            if (returnval == 0) {
                running = true;
                commThread = std::jthread(&VECSConsoleComm::HandleConnection, this);
            }
            else if (returnval < 0) {
                DisconnectFromServer();
            }
            return ConnectSocket;
        }
        bool IsConnected() {
            return ConnectSocket != INVALID_SOCKET && running;
        }

        int DisconnectFromServer() {
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

        void HandleConnection() {
            while (running) {
                std::string msg = ReceiveMessage();

                // If message is empty, it could mean a disconnect or error
                if (msg.empty()) {
                    std::cout << "Disconnected from server or error occurred.\n";
                    running = false;
                    break;
                }

                ProcessMessage(msg);
            }
        }

        void HandleInitConnection(Registry* reg) {

            while (!IsConnected())
            {
                auto res = ConnectToServer(reg); // either socket or invalid socket
#ifdef WIN32
                Sleep(2000);
#else
                usleep(2000 * 1000);
#endif
            }
        }

        void StartConnection(Registry* reg) {
            initThread = std::jthread{ [=, this]() { HandleInitConnection(reg); } };
        }


    private:
        void ProcessMessage(std::string msg) {

            //Process JSon
            nlohmann::json msgjson;
            try {
                msgjson = nlohmann::json::parse(msg);
            }
            // Console sent wrong JSON, ignore
            catch (...) {
                return;
            }

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
            // Console sent wrong or unknown command, ignore
            catch (...) {
                // ignore
            }

            switch (cmd) {
            case cmdHandshake:
                //print for testing
                std::cout << "Hello from Console pid " << msgjson["pid"] << " built " << msgjson["compiled"] << "!\n";
                // prepare a nice "Hello yourself!" to tell Console about us
                SendMessage("{\"cmd\":\"handshake\", \"pid\":" + std::to_string(getpid()) + " }");
                break;
            case cmdSnapshot:
                // snapshot
                if (!registry) {       // if no registry there,
                    SendMessage("{}"); // send empty object (maybe some kind of error object would be better?)
                }
#if 1 // Debugging:
                {
                    auto t1 = std::chrono::high_resolution_clock::now();
                    std::string josnap = registry->GetSnapshot();
                    auto t2 = std::chrono::high_resolution_clock::now();
                    SendMessage(josnap);
                    auto t3 = std::chrono::high_resolution_clock::now();
                    auto cmics = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
                    auto tmics = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
                    auto orgSize = josnap.size();
                    if (orgSize > 40)
                        josnap = josnap.substr(0, 37) + "...}";
                    std::cout << "Sending snapshot: " << josnap << " len " << orgSize << ", creation time: " << cmics << " msecs, "
                        "send time: " << tmics << " msecs\n";

                }
#else // release
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

                        std::string jsonlive = registry->GetLiveView();
                        SendMessage(jsonlive);
                        //Debugging: 
                        if (jsonlive.size() > 80) // most likely ...
                            jsonlive = jsonlive.substr(0, 77) + "...}";
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

                // Debugging:
                std::cout << "Incoming command \"" << msgjson["cmd"] << "\" ignored\n";
                break;
            }

        }

        int SendMessage(std::string sendmessage) const {
            const char* csendmessage = sendmessage.c_str();
            return send(ConnectSocket, csendmessage, static_cast<int>(sendmessage.length()), 0);
        }

        std::string ReceiveMessage() {
            char buffer[1024];
            int received = 0;

            while (running) {
                if (ConnectSocket == INVALID_SOCKET)
                    return "";

                int selectrc;
                timeval tmou{ .tv_usec = 200L * 1000L };  // that's 200 ms, i.e., 5 times per second
                timeval* ptmou = (tmou.tv_sec || tmou.tv_usec) ? &tmou : NULL;
                fd_set fds;
                do {
                    FD_ZERO(&fds);
                    FD_SET(ConnectSocket, &fds);
                    selectrc = select(static_cast<int>(ConnectSocket + 1), &fds, NULL, NULL, ptmou);
                    // then wait for activity 
                    if (selectrc == 0) {
                        // no data read from the other side within timeout period
                        auto lv = liveView.getChangesJSON();
                        if (std::get<0>(lv)) {
                            std::string lvchg = std::get<1>(lv);
                            SendMessage(lvchg);
                            // Debugging:
                            if (lvchg.size() > 80)
                                lvchg = lvchg.substr(0, 77) + "...}";
                            std::cout << "Liveview changes: " << lvchg << "\n";
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
                    DisconnectFromServer();
                }
                else {
#ifdef _WINSOCKAPI_
                    if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
#endif
                        DisconnectFromServer();
                }
            }
            return "";
        }

    private:
        bool started{ false };
        SOCKET ConnectSocket{ INVALID_SOCKET };

        bool Startup() {
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
        void Cleanup() {
#ifdef _WINSOCKAPI_
            if (started) {
                WSACleanup();
            }
#endif
            started = false;
        }
    };

    inline VECSConsoleComm* GetConsoleComm(Registry* reg) {
        static VECSConsoleComm* comm = nullptr;
        if (comm == nullptr) comm = new VECSConsoleComm;
        if (comm && reg) {
            comm->StartConnection(reg);
        }
        return comm;
    }


}
