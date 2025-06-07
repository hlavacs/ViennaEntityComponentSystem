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
        virtual std::string getSnapshot() = 0;
    };

    class VECSConsoleComm {

    private:
        std::jthread commThread;
        std::mutex socketMutex;
        bool running = false;

        // design question ... do we want one comm object for each VECS registry, or one for all registries in the program?
        // for now, let's assume a simple 1:1 relation - each registry gets its own connection object
        VECSConsoleInterface* registry{ nullptr };

    public:
        ~VECSConsoleComm() {
            disconnectFromServer();
            cleanup();
        }

        // simple variant: handle exactly one registry
        void setRegistry(VECSConsoleInterface* reg = nullptr) { registry = reg; }

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

            // VECSConsoleComm::connectToServer() so erweitern, dass es, wenn connect() eine Verbindung herstellen konnte (also 0 zur?ckgibt), einen eigenen Thread startet
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
                // Live View 
                break;
            default:
                // keep compiler happy

                // TEST TEST TEST TEST
                std::cout << "Incoming command \"" << msgjson["cmd"] << "\" ignored\n";
                break;
            }

        }

        int sendMessage(std::string sendmessage) {
            const char* csendmessage = sendmessage.c_str();
            return send(ConnectSocket, csendmessage, static_cast<int>(sendmessage.length()), 0);
        }

        std::string receiveMessage() {
            char buffer[1024];
            int received = 0;

            while (running) {
                if (ConnectSocket == INVALID_SOCKET)
                    return "";

                received = recv(ConnectSocket, buffer, sizeof(buffer) - 1, 0);

                if (received > 0) {
                    buffer[received] = '\0';
                    return std::string(buffer);
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
