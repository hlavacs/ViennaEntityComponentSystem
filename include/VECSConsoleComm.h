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
}
#include <nlohmann/json.hpp> 

#else
typedef int SOCKET;
#define INVALID_SOCKET  (SOCKET)(~0)    // copied from winsock.h
#define SOCKET_ERROR            (-1)
#endif

namespace vecs {

class VECSConsoleComm {

private: 
    std::jthread commThread;
    std::mutex socketMutex;
    bool running = false;

public:
    ~VECSConsoleComm() {
        disconnectFromServer();
        cleanup();
    }

    int connectToServer(std::string host = "127.0.0.1", int port = 2000) {
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

        // VECSConsoleComm::connectToServer() so erweitern, dass es, wenn connect() eine Verbindung herstellen konnte (also 0 zurückgibt), einen eigenen Thread startet
        int returnval = connect(ConnectSocket, (SOCKADDR*)&clientService, sizeof(clientService));
        if (returnval == 0) {
            running = true;
            commThread = std::jthread(&VECSConsoleComm::handleConnection, this);
        }
        return returnval;
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
        nlohmann::json msgjson = nlohmann::json::parse(msg);
       

        if (msgjson["cmd"] == "Hello") {
            // TESt TEST TEST
            std::cout << "Hello from Console " << msgjson["version"] << "!\r\n";
        }
        // Live View 

        // snapshot
        // first: all Entity name strings 
    }

    int sendMessage(std::string sendmessage){
       const char* csendmessage = sendmessage.c_str();
       return send(ConnectSocket, csendmessage, sendmessage.length(), 0);
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
                return INVALID_SOCKET;
            started = true;
        }
#endif
        return true;
    }
    void cleanup() {
#ifdef _WINSOCKAPI_
        if (started) {
            WSACleanup();
            started = false;
        }
#endif
    }
};

}
