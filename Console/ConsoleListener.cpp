
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

// Liberally sprinkling the source code with "nlohmann::" is a bit beside the point, so ...
using namespace nlohmann;

bool ConsoleSocketThread::ProcessJSON(std::string sjson) {

    //Process JSon
    json msgjson;
    try {
        msgjson = json::parse(sjson);
    }

    catch (json::exception& e) {
        auto x = e.what();
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
        // We got a new client! :)
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

bool ConsoleSocketThread::onHandshake(json const& json) {
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

bool ConsoleSocketThread::parseSnapshot(nlohmann::json const& json) {
    // clear potentially pre-existing snapshot
    int newSnapIdx = snapidx ^ 1;  // switch to other snap
    snapshot[newSnapIdx].clear();
    snapshot[newSnapIdx].setJsonsnap(json.dump());
    // parse incoming snapshot, create internal structure for it that can be handled from GUI
    try {
        entitycount = json["entities"];
        // get array of archetypes from JSON
        // we need that first to determine what we got here ...
        auto& archs = json["archetypes"];
        for (auto& a : archs) {
            auto& a2 = a["archetype"];   // TODO: is this sub-object really necessary?
            auto& maps = a2["maps"];
            for (auto& m : maps) {
                snapshot[newSnapIdx].AddTypeName(m["id"], m["name"]);
            }
            // get set of types from JSON
            // this also contains the tags assigned to this archetype
            auto& types = a2["types"];
            std::vector<size_t> tags;
            for (auto& t : types) {
                // this is a bit unsafe, as there are no constraints defined for tags.
                if (!snapshot[newSnapIdx].HasTypeName(t)) {  // if it s not a type, it is a tag.
                    snapshot[newSnapIdx].AddTag(t, std::to_string(t.get<size_t>()));      // TODO: once there are tag names, set them correctly
                    tags.push_back(t);           // remember as one of the tags to add to the entities in this archetype
                }
            }

            Console::Archetype ca(a2["hash"]);
            for (auto& tag : tags) ca.addTag(tag);

            // now that we got all necessary details, fetch the entities 
            auto& entities = a2["entities"];
            for (auto& e : entities) {
                Console::Entity ce(e["index"], e["version"], e["stgindex"], e["value"]);
                auto& values = e["values"];
                int i = 0;
                for (auto& v : values) {
                    std::string sv;  // in Console, we're content with strings for the moment
                    // build string for expected primitive JSON types
                    if (v.is_number_integer())
                        sv = std::to_string(v.get<long long>());
                    else if (v.is_number())
                        sv = std::to_string(v.get<long double>());
                    else
                        sv = v;

                    Console::Component cc;
                    cc.addData(std::tuple<size_t, std::string>(maps[i]["id"], sv));
                    ce.addComponent(cc);

                    i++;
                }
                ca.addEntity(ce);
            }

            snapshot[newSnapIdx].addArchetype(ca);  // TODO: this is just a first draft - it COPIES the complete thing!

            snapidx = newSnapIdx;  // the ONLY moment for a race condition ...
        }
    }
    catch (json::exception& e) {
        // for now, simply dump JSON errors
        std::cout << e.what() << "\n";
        return false;
    }
    catch (...) {
        return false;
    }
    return true;

}

bool ConsoleSocketThread::requestLiveView(bool active) {  // presumably expanded on in later versions
    isLive = active;
    return sendData(std::string("{\"cmd\":\"liveview\",\"active\":") + (active ? "true" : "false") + "}") > 0;
}

bool ConsoleSocketThread::sendWatchlist(std::map<size_t, Console::WatchEntity>& watchlist) {
    std::string watchlistString;
    int count = 0;
    for (auto& id : watchlist) {
        if (count++) watchlistString += ",";
        watchlistString += std::to_string(id.first);
    }
    return sendData(std::string("{\"cmd\":\"liveview\",\"watchlist\":[") + watchlistString + "]}") > 0;
}

bool ConsoleSocketThread::onLiveView(json const& json) {
    // parse incoming live view data, create internal structure for it that can be handled from GUI
    try {
        if (json.contains("entities")) {
            // TEST TEST TEST TEST TEST TEST TEST TEST
            size_t entities = json["entities"];
            std::cout << "LiveView entities: " << entities << "\n";
            int newMax = (int)entities;
            for (int i = 1; i < _countof(lvEntityCount); i++) {
                if (lvEntityCount[i] > newMax)
                    newMax = lvEntityCount[i];
                lvEntityCount[i - 1] = lvEntityCount[i];
            }
            lvEntityCount[_countof(lvEntityCount) - 1] = (int)entities;

            // scale max up and down - up at once, down only if difference is too big
            if (newMax > lvEntityMax)
                lvEntityMax = newMax;
            else if (!newMax)
                lvEntityMax = 1;
            else if (newMax < lvEntityMax)
                lvEntityMax = newMax + ((lvEntityMax - newMax) / 2);
        }
        if (json.contains("avgComp")) {
             avgComp = json["avgComp"]; 
        }
        if (json.contains("estSize")) {
            estSize = json["estSize"];
        }
        
        if (json.contains("watched")) {
            for (auto& entityObject : json["watched"]) {
                if (!entityObject.contains("entity") || !entityObject.contains("values"))
                    continue;
                auto& entity = watchlist[entityObject["entity"]];
                if (entityObject["values"].is_null()) {
                    entity.setDeleted();
                }
                else {
                    auto& values = entityObject["values"];
                    int i = 0;
                    bool changes = false;
                    auto coit = entity.getComponents().begin();
                    // walk through all components and look for changes
                    for (auto& v : values) {
                        std::string sv;  // in Console, we're content with strings for the moment
                        // build string for expected primitive JSON types
                        if (v.is_number_integer())
                            sv = std::to_string(v.get<long long>());
                        else if (v.is_number())
                            sv = std::to_string(v.get<long double>());
                        else
                            sv = v;
                        if (sv != coit->toString()) {
                            // for now, assume that any type change automatically means a new archetype and thus
                            // a deletion from the old archetype, so don't examine archetype type changes
                            coit->setString(sv);
                            changes = true;
                        }
                        i++;
                        coit++;
                    }

                    if (changes)
                        entity.setModified();
                }
            }
        }
    }
    catch (json::exception& e) {
        // for now, simply dump JSON errors
        std::cout << e.what() << "\n";
        return false;
    }
    catch (...) {
        return false;
    }
    return true;
}

