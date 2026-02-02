// Console Application for VECS using Dear ImGui with SDL3 / Vulkan

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
#include "imgui.h"
#include "implot.h"
#include <stdio.h>          // printf, fprintf
#include <stdlib.h>         // abort
#include <fstream>
#include <filesystem>

// Console Listener
#include "ConsoleListener.h"

#ifndef _countof
// Linux doesn't have _countof()
template <typename _CountofType, size_t _SizeOfArray>
char (*__countof_helper(_CountofType(&_Array)[_SizeOfArray]))[_SizeOfArray];
#define _countof(_Array) (sizeof(*__countof_helper(_Array)) + 0)
#endif

ConsoleListener listening;

static bool connectionWindow = true;
static bool viewSnapshotWindow = false;
static bool liveView = false;
static bool showWatchlist = false;
static bool showSnapshotFileList = false;
std::string selectedSnapshotFile;

float GetContentScale();

/// @brief set up the listening thread
/// @return true if listener thread was created
bool SetupListener(std::string cmdService) {
    return listening.Create(cmdService);
}

/// @brief terminate the listening thread
void TerminateListener() {
    listening.Terminate();
}

//cache for snapshot selections
static class SnapshotDisplayCache
{
private:
    int64_t tstamp;
    size_t tableLines{ 0 };
    std::string filterArchetype;
    std::string filterEntity;
    std::string filterCompType;
    std::string filterTag;
    using cacheTuple = std::tuple<Console::Archetype*, Console::Entity*, Console::Component*>;
    std::vector<cacheTuple> compCache;
    std::vector<std::string> archetypeCache;
    std::vector<std::string> entityCache;
    std::vector<std::string> componentCache;
    std::vector<std::string> tagCache;

public:
    /// @brief Cache the filterlists for the snapshot window filters
    /// @param snap Saved snapshot of current thread.
    /// @return true when filterlists are changed, false if nothing changed.
    bool CacheFilters(Console::Registry& snap) {
        auto newStamp = snap.GetJsonTS();
        if (tstamp != newStamp) {
            tstamp = newStamp;
            filterArchetype = "?"; //make sure to rebuild the cache
            filterEntity = "-";
            filterCompType = "-";
            filterTag = "-";
            compCache.clear();
            archetypeCache.clear();
            entityCache.clear();
            componentCache.clear();
            tagCache.clear();
            tableLines = snap.GetComponentCount();

            archetypeCache.push_back("-");
            entityCache.push_back("-");
            componentCache.push_back("-");
            tagCache.push_back("-");

            std::set<std::string>tagNames;
            for (auto& arch : snap.GetArchetypes()) {
                archetypeCache.push_back(arch.second.ToString());
                for (auto& tag : arch.second.GetTags())
                    tagNames.insert(snap.GetTagName(tag));
                for (auto& ent : arch.second.GetEntities())
                    entityCache.push_back(ent.second.ToString());
            }

            for (auto& curtype : snap.GetTypes()) {
                componentCache.push_back(curtype.second);
            }

            for (auto& curTag : tagNames) {
                tagCache.push_back(curTag);
            }

            struct {
                bool operator()(std::string& a, std::string& b) {
                    size_t sa = 0, sb = 0;
                    sscanf(a.c_str(), "%zu", &sa);
                    sscanf(b.c_str(), "%zu", &sb);
                    return sa < sb;
                }
            } size_tComp;

            std::sort(archetypeCache.begin() + 1, archetypeCache.end(), size_tComp);

            std::sort(entityCache.begin() + 1, entityCache.end(), size_tComp);

            std::sort(componentCache.begin() + 1, componentCache.end());

            std::sort(tagCache.begin() + 1, tagCache.end(), size_tComp);
            return true;
        }
        return false;
    }


    /// @brief Cache the displayed table lines for the snapshot window 
    /// @param snap Saved snapshot of current thread.
    /// @param sel_archetype selected archetype from filtering.
    /// @param sel_entity selected entity from filtering.
    /// @param sel_comptype selected component type from filtering.
    /// @param sel_tag selected tag from filtering.
    /// @return number of lines that are displayed based on current filter criteria
    size_t TableLines(Console::Registry& snap, std::string sel_archetype, std::string sel_entity, std::string sel_comptype, std::string sel_tag) {
        auto newStamp = snap.GetJsonTS();
        if (tstamp != newStamp) {  // if snapshot changed, reset
            tstamp = newStamp;
            filterArchetype = "?"; //make sure to rebuild the cache
            filterEntity = "-";
            filterCompType = "-";
            filterTag = "-";
            compCache.clear();
            tableLines = snap.GetComponentCount();
        }
        enum {
            fcArchetype = 1,
            fcEntity = 2,
            fcComptype = 4,
            fcTag = 8,
        };
        int fc = 0;
        if (filterArchetype != sel_archetype) {
            filterArchetype = sel_archetype;
            fc += fcArchetype;
        }
        if (filterEntity != sel_entity) {
            filterEntity = sel_entity;
            fc += fcEntity;
        }
        if (filterCompType != sel_comptype) {
            filterCompType = sel_comptype;
            fc += fcComptype;
        }
        if (filterTag != sel_tag) {
            filterTag = sel_tag;
            fc += fcTag;
        }
        if (fc) {  // if there are any filter changes, recalculation is necessary
            // same logic as in showViewSnapshotWindow() below

            bool selectedArchetype = (filterArchetype != "-");
            bool selectedEntity = (filterEntity != "-");
            bool selectedComptype = (filterCompType != "-");
            bool selectedTag = (filterTag != "-");

            compCache.clear();

            if (!(selectedArchetype || selectedEntity || selectedComptype || selectedTag)) {
                tableLines = 0;

                for (auto& archetype : snap.GetArchetypes()) {
                    std::string aHash = archetype.second.ToString();
                    bool abortTag = (selectedTag && !archetype.second.GetTags().size());
                    for (auto& tag : archetype.second.GetTags()) {
                        std::string tagName = snap.GetTagName(tag);
                        if (selectedTag && tagName != filterTag)
                            abortTag = true;
                    }

                    if (!archetype.second.GetEntities().size()) {
                        tableLines++;
                        compCache.push_back(cacheTuple(&archetype.second, nullptr, nullptr));

                    }
                    else {
                        for (auto& x : archetype.second.GetEntities()) {
                            auto& entity = x.second;
                            for (auto& component : entity.GetComponents()) {
                                compCache.push_back(cacheTuple(&archetype.second, &entity, &component));
                                tableLines++;
                            }
                        }
                    }
                }

            }
            else {
                tableLines = 0;
                for (auto& archetype : snap.GetArchetypes()) {
                    std::string aHash = archetype.second.ToString();
                    if (selectedArchetype && aHash != filterArchetype)
                        continue;
                    bool abortTag = (selectedTag && !archetype.second.GetTags().size());
                    for (auto& tag : archetype.second.GetTags()) {
                        std::string tagName = snap.GetTagName(tag);
                        if (selectedTag && tagName != filterTag)
                            abortTag = true;
                    }
                    if (abortTag) continue;
                    if (!archetype.second.GetEntities().size()) {
                        if (selectedEntity) continue;
                        tableLines++;
                        compCache.push_back(cacheTuple(&archetype.second, nullptr, nullptr));
                    }
                    else {
                        for (auto& x : archetype.second.GetEntities()) {
                            auto& entity = x.second;
                            std::string eIndex = entity.ToString();
                            if (selectedEntity && eIndex != filterEntity) {
                                continue;
                            }
                            for (auto& component : entity.GetComponents()) {
                                std::string actCompType = snap.GetTypeName(component.GetType());
                                if (selectedComptype && actCompType != filterCompType) {
                                    continue;
                                }
                                compCache.push_back(cacheTuple(&archetype.second, &entity, &component));
                                tableLines++;
                            }
                        }
                    }
                }
            }
        }
        return tableLines;
    }
    cacheTuple operator[](size_t index) {
        return compCache[index];
    }

    /// @brief return cached archetypes
    /// @return string vector of cached archetypes
    std::vector<std::string>& GetArchetypeCache() {
        return archetypeCache;
    }

    /// @brief return cached entities
    /// @return string vector of cached entities
    std::vector<std::string>& GetEntityCache() {
        return entityCache;
    }
    /// @brief return cached components
    /// @return string vector of cached components
    std::vector<std::string>& GetComponentCache() {
        return componentCache;
    }

    /// @brief return cached tags
    /// @return string vector of cached tags
    std::vector<std::string>& GetTagCache() {
        return tagCache;
    }

} snapshotDisplayCache;



/// @brief Display the snapshot window
/// @param listening Listening thread containing data.
/// @param p_open bool for opening and closing the window
void static ShowViewSnapshotWindow(ConsoleListener& listening, bool* p_open)
{
    float scale = GetContentScale();

    /* Initial window layout:  1135x700 at offset 150,20 - potentially scaled to main screen scale
                                            x (init 1135, min. 900)
    +-------------------------------------------------------------------------------------------------+
    | Button area                                                                                     |
    +-------------------------+-----------------------------------------------------------------------+
    |          160            |                           Rest (x-160, min.740)                       |
    |                         |                                                                       |
    |     Selections          |               Table                                                   |
    |     Child Window        |               Child Window                                            |
    |                         |                                                                       | y (init 700)
    |     y-b.area-60, min.300|                                                   y-b.area-60, min.300|
    |                         |                                                                       |
    |                         |                                                                       |
    |                         |                                                                       |
    +-------------------------+-----------------------------------------------------------------------+
    |  Details                                                                                        |
    |  Child Window                                                                                 60|
    +-------------------------------------------------------------------------------------------------+
    */
    ImVec2 windowSize = ImVec2(1135 * scale, 700 * scale);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Once);
    // ImGui::SetNextWindowCollapsed(false);
    ImVec2 wpos(150 * scale, 20 * scale);
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        wpos.x += ImGui::GetMainViewport()->Pos.x;
        wpos.y += ImGui::GetMainViewport()->Pos.y;
    }
    ImGui::SetNextWindowPos(wpos, ImGuiCond_Once);

    if (!ImGui::Begin("View Snapshot", p_open, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::End();
    }
    else
    {
        if (listening.cursel >= 0) {

            // used in child window area calculation below
            ImVec2 initCursorPos = ImGui::GetCursorPos();
            auto vecs = listening.GetVecs(listening.cursel);
            if (ImGui::Button("Get new Snapshot"))
            {
                vecs->RequestSnapshot();
            }
            Console::Registry& snap = vecs->GetSnapshot();

            ImGui::SameLine();
            if (ImGui::Button("Save snapshot to file"))
            {
                std::ofstream saveFile;
                std::time_t currTime = std::time(nullptr);
                auto timestamp = std::localtime(&currTime);
                auto saveTime = std::put_time(timestamp, "%Y%m%d%H%M%S");
                std::ostringstream oss;
                oss << saveTime;
                std::string saveName = std::string("snapshot") + oss.str() + ".json";
                saveFile.open(saveName);
                saveFile << snap.GetJsonsnap();
                saveFile.close();
            }

            ImVec2 windowSize = ImGui::GetWindowSize();
            ImVec2 cursorPos = ImGui::GetCursorPos();
            // calculate area for our 3 child windows - minimum is 600,300 (scaled)
            float detailsY = 50.f * scale;
            ImVec2 childArea(std::max(900.f * scale, windowSize.x - 2 * cursorPos.x),
                std::max(300.f * scale - detailsY, windowSize.y - cursorPos.y - initCursorPos.y - 5.f));
            ImVec2 childFilterSz(160.f * scale, childArea.y - detailsY);
            ImVec2 childSnapshotTableSz(childArea.x - (160.f * scale) - (10.f * scale), childArea.y - detailsY);
            ImVec2 childDetailsSz(childArea.x, detailsY);

            // Filter child window

            ImGui::BeginChild("Filter", childFilterSz);

            //cache filterlist
            snapshotDisplayCache.CacheFilters(snap);

            //--------------Archetype Filter--------------
            static std::string currentArchetype = "-";
            ImGui::Text("Archetype");
            if (ImGui::BeginCombo("A", currentArchetype.c_str())) {
                ImGuiListClipper clipper;
                auto& cache = snapshotDisplayCache.GetArchetypeCache();
                auto archetypeLines = cache.size();
                clipper.Begin(static_cast<int>(archetypeLines));
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd && row < archetypeLines; row++) {

                        bool selected = (currentArchetype == cache[row]);
                        if (ImGui::Selectable(cache[row].c_str(), selected))
                            currentArchetype = cache[row];

                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }


            //-------------Entity Filter-------------------
            ImGui::Text("Entity");
            static std::string currentEntity = "-";

            if (ImGui::BeginCombo("E", currentEntity.c_str())) {

                ImGuiListClipper clipper;
                auto& cache = snapshotDisplayCache.GetEntityCache();
                auto entityLines = cache.size();
                clipper.Begin(static_cast<int>(entityLines));
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd && row < entityLines; row++) {

                        bool selected = (currentEntity == cache[row]);
                        if (ImGui::Selectable(cache[row].c_str(), selected))
                            currentEntity = cache[row];

                        if (selected)
                            ImGui::SetItemDefaultFocus();

                    }
                }

                ImGui::EndCombo();
            }


            //-------------Component Filter-------------------
            ImGui::Text("Component Type");
            static std::string currentCompType = "-";

            if (ImGui::BeginCombo("C", currentCompType.c_str())) {

                ImGuiListClipper clipper;
                auto& cache = snapshotDisplayCache.GetComponentCache();
                auto componentLines = cache.size();
                clipper.Begin(static_cast<int>(componentLines));
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd && row < componentLines; row++) {

                        bool selected = (currentCompType == cache[row]);
                        if (ImGui::Selectable(cache[row].c_str(), selected))
                            currentCompType = cache[row];

                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }


            //---------------Tag Filter--------------------


            ImGui::Text("Tag");
            static std::string currentTag = "-";

            if (ImGui::BeginCombo("T", currentTag.c_str())) {

                ImGuiListClipper clipper;
                auto& cache = snapshotDisplayCache.GetTagCache();
                auto tagLines = cache.size();
                clipper.Begin(static_cast<int>(tagLines));
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd && row < tagLines; row++) {

                        bool selected = (currentTag == cache[row]);
                        if (ImGui::Selectable(cache[row].c_str(), selected))
                            currentTag = cache[row];

                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("Snapshot", childSnapshotTableSz);

            size_t tableLines = snapshotDisplayCache.TableLines(snap, currentArchetype, currentEntity, currentCompType, currentTag);

            // allow to single select a component
            static Console::Component* selTableComponent{ nullptr };
            static Console::Entity* selTableEntity{ nullptr };
            bool componentSelected{ false };

            if (ImGui::BeginTable("Snapshot", 5, ImGuiTableFlags_RowBg)) {

                ImGui::TableSetupColumn("Archetype");
                ImGui::TableSetupColumn("Index");
                ImGui::TableSetupColumn("Typename");
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Tag");
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin((int)tableLines);
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd && row < tableLines; row++) {
                        auto cacheRow = snapshotDisplayCache[row];
                        auto& archetype = *std::get<0>(cacheRow);
                        auto entity = std::get<1>(cacheRow);
                        auto component = std::get<2>(cacheRow);
                        // schnippeln wirs mal zusammen ...
                        std::string aHash = archetype.ToString();
                        int archtagCount = 0;
                        std::string tagstr;
                        for (auto& tag : archetype.GetTags()) {
                            if (archtagCount++) tagstr += ",";
                            std::string tagName = snap.GetTagName(tag);
                            tagstr += tagName;
                        }
                        if (entity == nullptr) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(aHash.c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted("-");
                            ImGui::TableSetColumnIndex(2);
                            ImGui::TextUnformatted("-");
                            ImGui::TableSetColumnIndex(3);
                            ImGui::TextUnformatted("-");
                            ImGui::TableSetColumnIndex(4);
                            ImGui::TextUnformatted("-");
                        }
                        else {
                            std::string eIndex = entity->ToString();
                            std::string actCompType = snap.GetTypeName(component->GetType());

                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);

                            std::string cValue = component->ToString();
                            // generate nice label for this table row; the "##" part is to guarantee unique labels while only displaying the hash
                            std::string componentLabel = aHash + "##" + eIndex + "." + std::to_string(row);
                            // make table row selectable
                            const ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
                            const bool itemIsSelected = (component == selTableComponent);
                            if (itemIsSelected) componentSelected = true;
                            if (ImGui::Selectable(componentLabel.c_str(), itemIsSelected, selectableFlags)) {
                                selTableComponent = component;
                                selTableEntity = entity;
                                componentSelected = true;
                            }
                            if (ImGui::BeginPopupContextItem()) {
                                selTableComponent = component;
                                selTableEntity = entity;
                                componentSelected = true;
                                bool watched = listening.GetVecs(listening.cursel)->IsWatched(selTableEntity->GetValue());
                                if (!watched && ImGui::Button("Add to watchlist")) {
                                    vecs->AddWatch(selTableEntity->GetValue());
                                    ImGui::CloseCurrentPopup();
                                }
                                if (watched && ImGui::Button("Remove from watchlist")) {
                                    vecs->DeleteWatch(selTableEntity->GetValue());
                                    ImGui::CloseCurrentPopup();
                                }
                                ImGui::EndPopup();
                            }

                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(eIndex.c_str());
                            ImGui::TableSetColumnIndex(2);
                            ImGui::TextUnformatted(actCompType.c_str());
                            ImGui::TableSetColumnIndex(3);
                            ImGui::TextUnformatted(cValue.c_str());
                            ImGui::TableSetColumnIndex(4);
                            ImGui::TextUnformatted(tagstr.c_str());

                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();

            // details area
            ImGui::NewLine();
            ImGui::BeginChild("Details", childDetailsSz);
            if (componentSelected && selTableComponent) {
                std::string entityText = std::string("Entity Index: ") + selTableEntity->ToString() +
                    ", Version " + std::to_string(selTableEntity->GetVersion()) +
                    ", Storage Index " + std::to_string(selTableEntity->GetStorageIndex());
                ImGui::TextUnformatted(entityText.c_str());

                std::string componentText = std::string("Component Type: ") + snap.GetTypeName(selTableComponent->GetType());
                ImGui::TextUnformatted(componentText.c_str());
                ImGui::TextUnformatted(selTableComponent->ToString().c_str());


            }
            // this is for debugging purposes only and can be removed once the caching works reliably!
            else {
                std::string initText = std::string("Snapshot Entities: ") + std::to_string(snap.GetEntityCount()) +
                    ", Components: " + std::to_string(snap.GetComponentCount());
#if CONSOLE_XF_METRICS
                auto milsGather = (snap.GetSentTS() - snap.GetRequestedTS()) / 1000;
                auto milsSend = (snap.GetReceivedEndTS() - snap.GetSentTS()) / 1000;
                auto milsJson = (snap.GetJsonTS() - snap.GetReceivedEndTS()) / 1000;
                auto milsParse = (snap.GetParsedTS() - snap.GetJsonTS()) / 1000;
                auto milsTotal = (snap.GetParsedTS() - snap.GetRequestedTS()) / 1000;
                initText += ", Gather time: " + std::to_string(milsGather) + " msecs" +
                    ", Send Time: " + std::to_string(milsSend) + " msecs" +
                    ", JSON time: " + std::to_string(milsJson) + " msecs" +
                    ", Parse time: " + std::to_string(milsParse) + " msecs" +
                    ", Total time: " + std::to_string(milsTotal) + " msecs";
#endif
                ImGui::TextUnformatted(initText.c_str());
                initText = std::string("Table lines: ") + std::to_string(tableLines);
                ImGui::TextUnformatted(initText.c_str());
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

/// @brief Display the snapshot file selection window
/// @param p_open bool for opening and closing the window
void static ShowSnapshotFileListWindow(bool* p_open) {
    float scale = GetContentScale();
    ImGui::SetNextWindowSize(ImVec2(350 * scale, 150 * scale), ImGuiCond_Once);
    ImVec2 wpos(150 * scale, 20 * scale);
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        wpos.x += ImGui::GetMainViewport()->Pos.x;
        wpos.y += ImGui::GetMainViewport()->Pos.y;
    }
    ImGui::SetNextWindowPos(wpos, ImGuiCond_Once);

    if (!ImGui::Begin("Load Snapshot from File", p_open))
    {
        ImGui::End();
    }
    else
    {
        ImGui::Text("Choose a Snapshot: ");
        std::filesystem::path curDir{ "." };
        for (auto const& dirEntry : std::filesystem::directory_iterator{ curDir }) {
            auto entry = dirEntry.path().string();
            if (entry.size() < 5)continue;
            if (entry.substr(entry.size() - 5) == ".json")
                if (ImGui::Selectable(entry.c_str())) {
                    selectedSnapshotFile = entry;
                }
        }
        if (ImGui::Button(selectedSnapshotFile.size() ? "Select File: " : "Cancel")) {
            showSnapshotFileList = false;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(selectedSnapshotFile.c_str());
        ImGui::End();
    }
}

/// @brief Display the connection window
/// @param listening Listening thread containing data.
/// @param p_open bool for opening and closing the window
void static ShowConnectionWindow(ConsoleListener& listening, bool* p_open)
{
    float scale = GetContentScale();
    ImGui::SetNextWindowSize(ImVec2(150 * scale, 100 * scale), ImGuiCond_Once);
    ImVec2 wpos(0 * scale, 20 * scale);
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        wpos.x += ImGui::GetMainViewport()->Pos.x;
        wpos.y += ImGui::GetMainViewport()->Pos.y;
    }
    ImGui::SetNextWindowPos(wpos, ImGuiCond_Once);
    if (!ImGui::Begin("Connections", p_open))
    {
        ImGui::End();
    }
    else
    {
        int cursel = -1;
        // special case : read from file
        ConsoleSocketThread* thd = listening.GetVecs(0);
        auto wasSelected = thd->selected;
        ImGui::Selectable("Load from File", &thd->selected);
        //If loadFromfile is selected set showSnapshotFileList to true
        if (thd->selected != wasSelected) {
            if (thd->selected) {
                selectedSnapshotFile.clear();
                showSnapshotFileList = true;
            }
        }
        //if Load from file was selected: open selecction window: 

        if (showSnapshotFileList) {
            ShowSnapshotFileListWindow(&showSnapshotFileList);
            if (!showSnapshotFileList) {
                thd->selected = (selectedSnapshotFile.size() != 0);
            }
        }

        if (thd->selected != wasSelected) {
            if (thd->selected) {
                try {
                    std::ifstream loadFile(selectedSnapshotFile);
                    nlohmann::json inputJson = nlohmann::json::parse(loadFile);
                    loadFile.close();
                    thd->ParseSnapshot(inputJson);
                    cursel = 0;
                }
                catch (...) {
                    thd->selected = false;
                }
            }
            else if (wasSelected) {
                listening.cursel = -1;
            }
        }

        for (size_t i = 1; i < listening.VecsCount(); i++) {
            ConsoleSocketThread* thd = listening.GetVecs(i);
            if (thd) {
                int pid = thd->GetPid();
                if (pid > 0) {
                    std::string spid = "VECS PID " + std::to_string(pid);
                    auto wasSelected = thd->selected;
                    ImGui::Selectable(spid.c_str(), &thd->selected);
                    if (thd->selected != wasSelected) {
                        if (thd->selected)
                            cursel = static_cast<int>(i);
                        else
                            listening.cursel = -1;
                    }
                }
            }
        }
        if (cursel >= 0) {
            listening.cursel = cursel;
            for (size_t i = 0; i < listening.VecsCount(); i++) {
                ConsoleSocketThread* thd = listening.GetVecs(i);
                if (thd) {
                    thd->selected = (i == cursel);
                }
            }
        }
        ImGui::End();
    }
}

/// @brief Display the live view window
/// @param listening Listening thread containing data.
/// @param p_open bool for opening and closing the window
void static ShowLiveView(ConsoleListener& listening, bool* p_open)
{
    float scale = GetContentScale();

    ImVec2 windowSize = ImVec2(1135 * scale, 700 * scale);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Once);
    ImVec2 wpos(150 * scale, 20 * scale);
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        wpos.x += ImGui::GetMainViewport()->Pos.x;
        wpos.y += ImGui::GetMainViewport()->Pos.y;
    }
    ImGui::SetNextWindowPos(wpos, ImGuiCond_Once);

    ImGui::SetNextWindowCollapsed(false);

    if (!ImGui::Begin("Live View", p_open, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::End();
    }
    else
    {
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 cursorPos = ImGui::GetCursorPos();
        ImVec2 childArea(std::max(900.f * scale, windowSize.x - 2 * cursorPos.x - 7.f),
            std::max(300.f * scale, windowSize.y - cursorPos.y - 12.f));

        float lowerY = childArea.y * 0.5f;

        ImVec2 childLiveViewGraphSz(childArea.x - (10.f * scale), childArea.y - lowerY);
        ImVec2 childStatsSz(childArea.x / 3.f, lowerY);
        ImVec2 childWatchlistSz(childArea.x * (2.f / 3.f), lowerY);

        // Filter child window

        ImGui::BeginChild("LiveView", childLiveViewGraphSz);

        if (listening.cursel >= 0) {
            auto vecs = listening.GetVecs(listening.cursel);
            bool isLive = vecs->GetIsLive();
            if (!isLive && ImGui::Button("Start LiveView"))
            {
                vecs->RequestLiveView();
            }
            if (isLive && ImGui::Button("Stop LiveView"))
            {
                vecs->RequestLiveView(false);
            }

            if (ImPlot::BeginPlot("Live View", ImVec2(-1, -1))) {

                ImPlot::SetupAxisLimits(ImAxis_X1, 0, _countof(vecs->lvEntityCount));
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0, vecs->lvEntityMax, ImPlotCond_Always);
                ImPlot::PlotBars("Entities", vecs->lvEntityCount, _countof(vecs->lvEntityCount));
                ImPlot::EndPlot();
            }

        }
        ImGui::EndChild();

        ImGui::BeginChild("Statistics", childStatsSz);

        if (listening.cursel >= 0) {
            auto vecs = listening.GetVecs(listening.cursel);
            ImGui::Text("Number of Entities: %d", vecs->lvEntityCount[_countof(vecs->lvEntityCount) - 1]);
            ImGui::Text("Average Number of Components: %.2f", vecs->GetAvgComp());
            char s[128];
            size_t estSize = vecs->GetEstSize();
            // primitive human-readable value
            if (estSize >= 1000000000L) // over a gigabyte
                sprintf(s, "%.2lf GB", static_cast<double>(estSize) / 1000000000.0);
            else if (estSize >= 1000000L) // over a megabyte
                sprintf(s, "%.2lf MB", static_cast<double>(estSize) / 1000000.0);
            else if (estSize >= 1000L) // over a kilobyte
                sprintf(s, "%.2lf KB", static_cast<double>(estSize) / 1000.0);
            else  // below a kilobyte
                sprintf(s, "%ld B", (long)estSize);
            ImGui::Text("Estimated Memory usage: %s", s);
        }
        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("Watchlist", childWatchlistSz);
        if (listening.cursel >= 0) {
            auto vecs = listening.GetVecs(listening.cursel);

            // allow to single select a component
            static Console::Component* selTableComponent{ nullptr };
            static Console::Entity* selTableEntity{ nullptr };
            bool componentSelected{ false };

            if (ImGui::BeginTable("Watchlist", 5, ImGuiTableFlags_RowBg)) {

                ImGui::TableSetupColumn("Archetype");
                ImGui::TableSetupColumn("Index");
                ImGui::TableSetupColumn("Typename");
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Tag");
                ImGui::TableHeadersRow();

                auto& watchlist = vecs->GetWatchlist();

                size_t entityIndex = 0;
                for (auto& entityHandle : watchlist) {
                    auto& entity = entityHandle.second;

                    auto archetype = entity.GetArchetype();
                    std::string aHash = archetype->ToString();
                    std::string eIndex = entity.ToString();
                    int archTagCount = 0;
                    std::string tagstr;
                    for (auto& tag : archetype->GetTags()) {
                        if (archTagCount++) tagstr += ",";
                        tagstr += entity.GetTagName(tag);
                    }
                    ImVec4 color = entity.IsModified() ? ImVec4(255, 255, 0, 255) :
                        entity.IsDeleted() ? ImVec4(255, 0, 0, 255) :
                        ImVec4(255, 255, 255, 255);
                    size_t componentIndex = 0;
                    for (auto& component : entity.GetComponents()) {

                        std::string cValue = component.ToString();

                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextColored(color, "%s", aHash.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextColored(color, "%s", eIndex.c_str());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextColored(color, "%s", entity.GetTypeName(component.GetType()).c_str());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextColored(color, "%s", cValue.c_str());
                        ImGui::TableSetColumnIndex(4);
                        ImGui::TextColored(color, "%s", tagstr.c_str());

                        componentIndex++;
                    }
                    entityIndex++;
                }

                ImGui::EndTable();
            }

        }
        ImGui::EndChild();
        ImGui::End();
    }
}

/// @brief Display the watchlist window
/// @param listening Listening thread containing data.
/// @param p_open bool for opening and closing the window
void static ShowWatchlistWindow(ConsoleListener& listening, bool* p_open) {
    float scale = GetContentScale();

    ImVec2 windowSize = ImVec2(1135 * scale, 700 * scale);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Once);
    ImVec2 wpos(150 * scale, 20 * scale);
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        wpos.x += ImGui::GetMainViewport()->Pos.x;
        wpos.y += ImGui::GetMainViewport()->Pos.y;
    }
    ImGui::SetNextWindowPos(wpos, ImGuiCond_Once);

    ImGui::SetNextWindowCollapsed(false);

    if (!ImGui::Begin("Watchlist", p_open))
    {
        ImGui::End();
    }
    else
    {
        if (listening.cursel >= 0) {

            // allow to single select a component
            static Console::Component* selTableComponent{ nullptr };
            static Console::Entity* selTableEntity{ nullptr };
            bool componentSelected{ false };

            auto vecs = listening.GetVecs(listening.cursel);
            if (ImGui::BeginTable("Watchlist", 5, ImGuiTableFlags_RowBg)) {

                ImGui::TableSetupColumn("Archetype");
                ImGui::TableSetupColumn("Index");
                ImGui::TableSetupColumn("Typename");
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Tag");
                ImGui::TableHeadersRow();

                auto& watchlist = vecs->GetWatchlist();

                size_t entityIndex = 0;
                size_t entidel = (size_t)-1;
                for (auto& entityHandle : watchlist) {
                    auto& entity = entityHandle.second;
                    auto archetype = entity.GetArchetype();
                    std::string aHash = archetype->ToString();
                    std::string eIndex = entity.ToString();
                    int archTagCount = 0;
                    std::string tagstr;
                    for (auto& tag : archetype->GetTags()) {
                        if (archTagCount++) tagstr += ",";
                        tagstr += entity.GetTagName(tag);
                    }
                    size_t componentIndex = 0;
                    for (auto& component : entity.GetComponents()) {

                        std::string cValue = component.ToString();

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);

                        // generate nice label for this table row; the "##" part is to guarantee unique labels while only displaying the hash
                        std::string componentLabel = aHash + "##wl" + std::to_string(entityIndex) + "." + std::to_string(componentIndex);
                        // make table row selectable
                        const ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
                        const bool itemIsSelected = (&component == selTableComponent);
                        if (itemIsSelected) componentSelected = true;
                        if (ImGui::Selectable(componentLabel.c_str(), itemIsSelected, selectableFlags)) {
                            selTableComponent = &component;
                            selTableEntity = &entity;
                            componentSelected = true;
                        }
                        if (ImGui::BeginPopupContextItem()) {
                            selTableComponent = &component;
                            selTableEntity = &entity;
                            componentSelected = true;

                            if (ImGui::Button("Remove from watchlist")) {
                                entidel = entityHandle.first;
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(eIndex.c_str());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(entity.GetTypeName(component.GetType()).c_str());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextUnformatted(cValue.c_str());
                        ImGui::TableSetColumnIndex(4);
                        ImGui::TextUnformatted(tagstr.c_str());

                        componentIndex++;
                    }
                    entityIndex++;
                }
                if (entidel != (size_t)-1)
                    vecs->DeleteWatch(entidel);

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

}

/// @brief Main Loop continually called by ImGui to display the selected windows of the console
void MainLoop() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Connections")) {
            if (ImGui::MenuItem("Manage Connections")) {
                connectionWindow = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Snapshot")) {
                viewSnapshotWindow = true;
            }

            if (ImGui::MenuItem("Live View")) {
                liveView = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Watchlist")) {

            if (ImGui::MenuItem("Edit/View")) {
                showWatchlist = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (connectionWindow) {
        ShowConnectionWindow(listening, &connectionWindow);
    }

    if (viewSnapshotWindow) {
        ShowViewSnapshotWindow(listening, &viewSnapshotWindow);
    }

    if (liveView)
    {
        ShowLiveView(listening, &liveView);
    }

    if (showWatchlist) {
        ShowWatchlistWindow(listening, &showWatchlist);
    }

}
