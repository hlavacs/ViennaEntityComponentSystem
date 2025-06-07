
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
typedef struct protoent PROTOENT;
typedef struct servent SERVENT;
typedef struct hostent HOSTENT;
// milliseconds are good enough, so resort to these where necessary
#define Sleep(a) usleep((a) * 1000)
#endif

#include <iostream>
#include <algorithm>

#include <errno.h>

#include "ConsoleListener.h"

// if USING_SELECT is defined, all incoming socket events are handled through select()
// if undefined, timeout loops are used (can be quite inefficient at times)
// TODO: remove the possibility once the select() method works reliably on all target systems
#define USING_SELECT


// ConsoleSocketThread class members

// ClientActivity : the handler for all incoming socket activity

void ConsoleSocketThread::ClientActivity() {

    int waitrc;

    auto& s = getSocket();

    // TEST - start by sending a welcome string to the other side
    std::string welcome("{\"cmd\":\"handshake\",\"pid\":" + std::to_string(getpid()) + ",\"compiled\":\"" __DATE__ " " __TIME__ "\"}");
    s.sendData(welcome);

    std::string json;       // current json string
    int brcs{ 0 };          // currently open braces
    int brks{ 0 };          // currently open brackets
    bool instr{ false };    // currently in string
    bool inesc{ false };   // currently in escape sequence (\" , to be exact)
    char lchr{ 0 };         // last character

    // wait for incoming data with a timeout of 500 ms
    while ((waitrc = s.wait(500)) != SOCKET_ERROR) {
        // check for timeout ...
        if (waitrc == 0) {
            // TODO : if there's an external influence governing this thread,
            //        make it happen here
        }
        else if (waitrc > 0) {
            // something HAPPENED!

            // if no data there, the socket connection has presumably been closed from this or the other side
            if (!s.dataThere())
                break;

            int nLen;
            while ((nLen = s.bytesBuffered()) > 0) {
                char sbuf[4096];
                int maxRd = min(sizeof(sbuf), nLen);
                int rlen = s.receiveData(sbuf, maxRd);
                nLen -= rlen;
#if 0
                // FIRST TEST - simply read in as many data as available and echo them back
                s.sendData(sbuf, rlen);
#else
                // process incoming buffers, which are supposed to be in JSON format
                // refer to https://www.json.org/json-en.html for complete JSON specification

                // quick and dirty parser - just good enough to capture complete json buffers
                for (int i = 0; i < rlen; i++) {
                    bool complete{ false };
                    switch (sbuf[i]) {
                    case '{':
                        if (inesc) {
                            inesc = false;
                            break;
                        }
                        if (instr) break;
                        brcs++;
                        break;
                    case '}':
                        if (inesc) {
                            inesc = false;
                            break;
                        }
                        if (instr) break;
                        if (brcs)  // ignore badly formed buffers starting with a }
                            brcs--;
                        if (!brcs && !brks)  // if outside any braces or brackets,
                            complete = true;   // done with this one
                        break;
                    case '[':
                        if (inesc) {
                            inesc = false;
                            break;
                        }
                        if (instr) break;
                        brks++;
                        break;
                    case ']':
                        if (inesc) {
                            inesc = false;
                            break;
                        }
                        if (instr) break;
                        if (brks)  // ignore badly formed buffers starting with a ]
                            brks--;
                        if (!brcs && !brks)  // if outside any braces or brackets,
                            complete = true; // done with this one
                        break;
                    case '\\':
                        inesc = !inesc;  // assure that \\ is NOT starting an escape sequence
                        break;
                    case '\"':
                        if (inesc) {
                            inesc = false;
                            break;
                        }
                        instr = !instr;
                        if (!instr && !brcs && !brks) // string closed and no open brackets or braces ?
                            complete = true;
                        break;
                    default:
                        if (inesc)  // anything else ends an escape sequence (good enough)
                            inesc = false;
                        // for a really complete JSON parser, we would need to deal with the fact that
                        // a well-formed json buffer can also consist of ...
                        // - a single number
                        // - the strings true, false, null
                        // - or even a single whitespace character (bonkers, but correct)
                        // for now, however simply assume incoming objects and/or arrays.
                        // after all, we KNOW the sender and his ways.
                        break;
                    }
                    lchr = sbuf[i];
                    json += sbuf[i];
                    if (complete) {
                        ProcessJSON(json);  // process completed json string
                        json.clear();
                    }
                }

#endif
            }

        }
    }
}

bool ConsoleSocketThread::ProcessJSON(std::string sjson) {

    //Process JSon
    nlohmann::json msgjson;
    try {
        msgjson = nlohmann::json::parse(sjson);
    }

    catch (nlohmann::json::exception &e) {
       auto x =  e.what();
       std::cout << x << "\n";
    }
    // could catch specific JSON error here, but for now, don't.
    catch (...) {
        return false;
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
        return false;
    }

    switch (cmd) {
    case cmdHandshake:
        // VECS information coming in
        // This is where interaction with the main program has to happen.
        // We got a new client! :-)
        onHandshake(msgjson);

        // for now, however ...
        // TEST TEST TEST TEST TEST - simply request a snapshot
        //requestSnapshot();
        break;
    case cmdSnapshot:
        onSnapshot(msgjson);
        break;
    case cmdLiveView:
        // this is SURELY going to change ... a LOT
        onLiveView(msgjson);
        break;
    default:
        // keep the compiler happy
        break;
    }
    return true;
}

bool ConsoleSocketThread::onHandshake(nlohmann::json const& json) {
    // TODO: parse necessary values, return false if the connection does not match our criteria
    try {
        pid = json["pid"];
    }
    catch (...) {
        return false;
    }
    handShook = true;  // :-)
    return true;
}

bool ConsoleSocketThread::requestSnapshot() {
    return sendData("{\"cmd\":\"snapshot\"}") > 0;
}

bool ConsoleSocketThread::onSnapshot(nlohmann::json const& json) {
    // parse incoming snapshot, create internal structure for it that can be handled from GUI
    try {
        entitycount = json["entities"];
    }
    catch (...) {
        return false;
    }
    return true;
}

bool ConsoleSocketThread::requestLiveView() {  // presumably expanded on in later versions
    return sendData("{\"cmd\":\"liveview\"}") > 0;
}

bool ConsoleSocketThread::onLiveView(nlohmann::json const& json) {
    // parse incoming live view data, create internal structure for it that can be handled from GUI
    return true;
}

