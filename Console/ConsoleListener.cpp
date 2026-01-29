
#ifdef WIN32
//F'in Windows.h includes minwindef.h which overrides min, which collides with C++ std::min
#ifndef NOMINMAX
#define NOMINMAX
#endif
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
#include <fstream>
#include <algorithm>

#include <errno.h>

#ifndef _countof
// Linux doesn't have _countof()
template <typename _CountofType, size_t _SizeOfArray>
char (*__countof_helper(_CountofType(&_Array)[_SizeOfArray]))[_SizeOfArray];
#define _countof(_Array) (sizeof(*__countof_helper(_Array)) + 0)
#endif

#include "ConsoleListener.h"

SyncClock syncClock;  // we only need one.

//all incoming socket events are handled through select()
#define USING_SELECT

/// @brief handles all incoming client activity
void ConsoleSocketThread::ClientActivity() {

    int waitrc;

    auto& s = GetSocket();

    std::string welcome("{\"cmd\":\"handshake\",\"pid\":" + std::to_string(getpid()) + ",\"compiled\":\"" __DATE__ " " __TIME__ "\"}");
    s.SendData(welcome);

    std::string json;
    int brcs{ 0 };          // currently open braces
    int brks{ 0 };          // currently open brackets
    bool instr{ false };    // currently in string
    bool inesc{ false };   // currently in escape sequence
    char lchr{ 0 };         // last character
    int64_t tsStart;

    // wait for incoming data with a timeout of 500 ms
    while ((waitrc = s.Wait(500)) != SOCKET_ERROR) {
        if (waitrc == 0) {
            // if there's an external influence governing this thread
        }
        else if (waitrc > 0) {
            if (!s.DataThere())
                break;

            int nLen;
            while ((nLen = s.BytesBuffered()) > 0) {
                char sbuf[4096];
                int maxRd = std::min((int)sizeof(sbuf), nLen);
                int rlen = s.ReceiveData(sbuf, maxRd);
                nLen -= rlen;

                // process incoming buffers, which are supposed to be in JSON format
                // capture complete json buffers
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
                        if (!brcs && !brks)
                            complete = true;
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
                        if (!brcs && !brks)
                            complete = true;
                        break;
                    case '\\':
                        inesc = !inesc;
                        break;
                    case '\"':
                        if (inesc) {
                            inesc = false;
                            break;
                        }
                        instr = !instr;
                        if (!instr && !brcs && !brks)
                            complete = true;
                        break;
                    default:
                        if (inesc)  // anything else ends an escape sequence
                            inesc = false;
                        break;
                    }
                    lchr = sbuf[i];
                    if (json.empty()) {
                        json.reserve(rlen);
                        tsStart = syncClock.NowMicro();
                    }
                    json += sbuf[i];
                    if (complete) {
#if CONSOLE_XF_METRICS  // metrics - remove when not necessary
                        auto tsProcessed = syncClock.NowMicro();
                        auto msecs = (tsProcessed - tsStart) / 1000;
                        if (msecs > 10)
                            std::cout << "JSON receive time: " << msecs << " msecs\n";
                        std::string ststmps = ",\"gst3\":" + std::to_string(tsStart) +
                            ",\"gst4\":" + std::to_string(tsProcessed);
                        json.insert(json.size() - 1, ststmps);
#endif
                        ProcessJSON(json);  // process completed json string
                        json.clear();
                    }
                }
            }
        }
    }
}
using namespace nlohmann;

/// @brief processes incoming json strings
/// @param sjson String representing json data
/// @return true if valid json
bool ConsoleSocketThread::ProcessJSON(std::string sjson) {

    //Process JSON
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
        OnHandshake(sjson, msgjson);
        break;
    case cmdSnapshot:
        OnSnapshot(sjson, msgjson);
        break;
    case cmdLiveView:
        OnLiveView(sjson, msgjson);
        break;
    default:
        // keep the compiler happy
        break;
    }
    return true;
}

/// @brief handle incoming handshake commands
/// @param json Json containing handshake data
/// @return true if valid json
bool ConsoleSocketThread::OnHandshake(std::string& sjson, json& json) {
    try {
        pid = json["pid"];
    }
    catch (...) {
        return false;
    }
    handShook = true;
    return true;
}

/// @brief request a snapshot from the connected vecs
/// @return true if request was sent
bool ConsoleSocketThread::RequestSnapshot() {
    return SendData("{\"cmd\":\"snapshot\"}") > 0;
}

/// @brief parse incoming snapshots
/// @param json Json containing snapshot data
/// @return true if valid json
bool ConsoleSocketThread::ParseSnapshot(nlohmann::json& json, std::string* psjson) {
    int newSnapIdx = snapidx ^ 1;  // switch to other snapshot to prevent conflicts
    auto& snap = snapshot[newSnapIdx];

    // clear potentially pre-existing snapshot
    snap.Clear();
    // parse incoming snapshot, create internal structure for it that can be handled from GUI
    try {
        entitycount = json["entities"];
        auto& archs = json["archetypes"];
        for (auto& a : archs) {
            auto& a2 = a["archetype"];
            auto& maps = a2["maps"];
            for (auto& m : maps) {
                snap.AddTypeName(m["id"], m["name"]);
            }
            auto& types = a2["types"];
            std::vector<size_t> tags;
            for (auto& t : types) {
                // this is a bit unsafe, as there are no constraints defined for tags.
                if (!snap.HasTypeName(t)) {  // if it s not a type, it is a tag.
                    snap.AddTag(t, std::to_string(t.get<size_t>()));
                    tags.push_back(t); // remember as one of the tags to add to the entities in this archetype
                }
            }

            Console::Archetype ca(a2["hash"]);
            for (auto& tag : tags) ca.AddTag(tag);

            auto& entities = a2["entities"];
            for (auto& e : entities) {
                Console::Entity ce(e["index"], e["version"], e["stgindex"], e["value"]);
                auto& values = e["values"];
                int i = 0;
                for (auto& v : values) {
                    std::string sv;
                    // build string for expected primitive JSON types
                    if (v.is_number_integer())
                        sv = std::to_string(v.get<long long>());
                    else if (v.is_number())
                        sv = std::to_string(v.get<long double>());
                    else
                        sv = v;

                    Console::Component cc;
                    cc.AddData(std::tuple<size_t, std::string>(maps[i]["id"], sv));
                    ce.AddComponent(cc);

                    i++;
                }
                ca.AddEntity(ce);
            }
            snap.AddArchetype(ca);
        }
#if CONSOLE_XF_METRICS
        snap.SetParsed();
        auto gst = json.find("gst1"); // look for snapshot fetch start timestamp
        if (gst != json.end() && gst->is_number())
            snap.SetRequested(*gst);
        gst = json.find("gst2"); // look for snapshot sent timestamp
        if (gst != json.end() && gst->is_number())
            snap.SetSent(*gst);
        gst = json.find("gst3"); // look for snapshot received start timestamp
        if (gst != json.end() && gst->is_number())
            snap.SetReceivedStart(*gst);
        gst = json.find("gst4"); // look for snapshot received end timestamp
        if (gst != json.end() && gst->is_number())
            snap.SetReceivedEnd(*gst);
        gst = json.find("gst5"); // look for JSON fetched timestamp
        if (gst != json.end() && gst->is_number())
            snap.SetFetched(*gst);
        else {
            // if no parsed timestamp available, be sure to add one!
            if (psjson) {// either to string, if one has been passed in
                std::string ststmp = ",\"gst5\":" + std::to_string(snap.GetJsonTS());
                psjson->insert(psjson->size() - 1, ststmp);
            }
            else  // or to the json object otherwise
                json += { "gst5", snap.GetJsonTS() };
        }
        gst = json.find("gst6"); // look for snapshot parsed timestamp
        if (gst != json.end() && gst->is_number())
            snap.SetParsed(*gst);
        else {
            // if no parsed timestamp available, be sure to add one!
            if (psjson) {// either to string, if one has been passed in
                std::string ststmp = ",\"gst6\":" + std::to_string(snap.GetParsedTS());
                psjson->insert(psjson->size() - 1, ststmp);
            }
            else  // or to the json object otherwise
                json += { "gst6", snap.GetParsedTS() };
        }
#if 0  // ONLY ACTIVATE IF YOU WANT TO CREATE STATISTICS FILE!
        try {
            auto milsGather = (snap.GetSentTS() - snap.GetRequestedTS()) / 1000;
            auto milsSend = (snap.GetReceivedEndTS() - snap.GetSentTS()) / 1000;
            auto milsJson = (snap.GetJsonTS() - snap.GetReceivedEndTS()) / 1000;
            auto milsParse = (snap.GetParsedTS() - snap.GetJsonTS()) / 1000;
            auto milsTotal = (snap.GetParsedTS() - snap.GetRequestedTS()) / 1000;
            // this creates a .csv file. Each line contains:
            // Entities,Components,Gather(ms),Send(ms),JSON(ms),Parse(ms),Total(ms)
            std::string initText = std::to_string(snap.GetEntityCount()) +
                "," + std::to_string(snap.GetComponentCount()) +
                "," + std::to_string(milsGather) +
                "," + std::to_string(milsSend) +
                "," + std::to_string(milsJson) +
                "," + std::to_string(milsParse) +
                "," + std::to_string(milsTotal) +
                "\n";
            std::ofstream("snapshotmetrics.csv", std::ios::app) << initText;
        }
        catch (...) {
            // ignore. If the disk is full or defective, that's the smallest of our problems.
        }
#endif
#endif
        snap.SetJsonsnap(psjson ? *psjson : json.dump());
        snapidx = newSnapIdx;
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

/// @brief request live view communication from the connected vecs
/// @param active Bool to activate or deactivate the live view
/// @return true if request was sent
bool ConsoleSocketThread::RequestLiveView(bool active) {
    isLive = active;
    return SendData(std::string("{\"cmd\":\"liveview\",\"active\":") + (active ? "true" : "false") + "}") > 0;
}


/// @brief send watchlist to the connected vecs
/// @param watchlist map of watched entities with their handles
/// @return true if request was sent
bool ConsoleSocketThread::SendWatchlist(std::map<size_t, Console::WatchEntity>& watchlist) {
    std::string watchlistString;
    int count = 0;
    for (auto& id : watchlist) {
        if (count++) watchlistString += ",";
        watchlistString += std::to_string(id.first);
    }
    return SendData(std::string("{\"cmd\":\"liveview\",\"watchlist\":[") + watchlistString + "]}") > 0;
}


/// @brief parse incoming live view data, create internal structure for it that can be handled from GUI
/// @param json Json containing live view data
/// @return true if valid json
bool ConsoleSocketThread::OnLiveView(std::string& sjson, json& json) {
    try {
        if (json.contains("entities")) {
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
                    entity.SetDeleted();
                }
                else {
                    auto& values = entityObject["values"];
                    int i = 0;
                    bool changes = false;
                    auto coit = entity.GetComponents().begin();
                    // walk through all components and look for changes
                    for (auto& v : values) {
                        std::string sv;
                        // build string for expected primitive JSON types
                        if (v.is_number_integer())
                            sv = std::to_string(v.get<long long>());
                        else if (v.is_number())
                            sv = std::to_string(v.get<long double>());
                        else
                            sv = v;
                        if (sv != coit->ToString()) {
                            coit->SetString(sv);
                            changes = true;
                        }
                        i++;
                        coit++;
                    }

                    if (changes)
                        entity.SetModified();
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

