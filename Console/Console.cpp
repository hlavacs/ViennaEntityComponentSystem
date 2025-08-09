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

// use scaling (resize window based on main screen scale) - still very experimental!
#define USE_SCALING 0

ConsoleListener listening;
std::string service = "2000";

static bool connectionWindow = true;
static bool viewSnapshotWindow = false;
static bool liveView = false;
static bool showWatchlist = false;
static bool showSnapshotFileList = false;
std::string selectedSnapshotFile; 

bool SetupListener() {
    return listening.Create(service);
}

void TerminateListener() {
    listening.Terminate();
}

void static showViewSnapshotWindow(ConsoleListener& listening, bool* p_open)
{
#if USE_SCALING
    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
#else
    const float scale = 1.f;
#endif
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
    ImVec2 windowsize = ImVec2(1135 * scale, 700 * scale);
    ImGui::SetNextWindowSize(windowsize, ImGuiCond_Once);
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

            // used in child window area calculation below - presumably there's a better way in Imgui
            ImVec2 initCursorPos = ImGui::GetCursorPos();
            auto vecs = listening.getVecs(listening.cursel);
            if (ImGui::Button("Get new Snapshot"))
            {
                vecs->requestSnapshot();
            }
            Console::Registry& snap = vecs->getSnapshot();

            ImGui::SameLine();
            if (ImGui::Button("Save snapshot to file"))
            {
                std::ofstream savefile;
                std::time_t currTime = std::time(nullptr);
                auto timestamp =  std::localtime(&currTime);
                auto savetime = std::put_time(timestamp, "%Y%m%d%H%M%S");
                std::ostringstream oss;
                oss << savetime;
                std::string saveName = std::string("snapshot") + oss.str()+ ".json";
                savefile.open(saveName); 
                savefile << snap.getJsonsnap();
                savefile.close();
            }

            ImVec2 windowSize = ImGui::GetWindowSize();
            ImVec2 cursorPos = ImGui::GetCursorPos();
            // calculate area for our 3 child windows - minimum is 600,300 (scaled)
            // the y size calculation is a bit meh ... need to find out more about child window padding, the effects of ImGui::NewLine etc.
            float detailsY = 50.f * scale;
            ImVec2 childArea(std::max(900.f * scale, windowSize.x - 2 * cursorPos.x),
                std::max(300.f * scale - detailsY, windowSize.y - cursorPos.y - initCursorPos.y - 5.f));
            ImVec2 childFilterSz(160.f * scale, childArea.y - detailsY);
            ImVec2 childSnapshotTableSz(childArea.x - (160.f * scale) - (10.f * scale /*for scrollbar*/), childArea.y - detailsY);
            ImVec2 childDetailsSz(childArea.x, detailsY);

            // Filter child window

            ImGui::BeginChild("Filter", childFilterSz);

            std::vector<std::string> archetypeNames;
            std::vector<std::string> entityNames;
            std::vector<std::string> componentTypes;
            std::set<std::string> tagNames;
            entityNames.push_back("-");
            componentTypes.push_back("-");
            archetypeNames.push_back("-");
            tagNames.insert("-");

            //--------------Archetype Filter--------------
            ImGui::Text("Archetype");
            for (auto& arch : snap.getArchetypes()) {
                archetypeNames.push_back(arch.second.toString());
                for (auto& tag : arch.second.getTags())
                    tagNames.insert(snap.GetTagName(tag));
                for (auto& ent : arch.second.getEntities())
                    entityNames.push_back(ent.second.toString());
            }
            static std::string current_archetype = "-";

            if (ImGui::BeginCombo("A", current_archetype.c_str())) {
                for (auto& archetype : archetypeNames) {
                    bool selected = (current_archetype == archetype);
                    if (ImGui::Selectable(archetype.c_str(), selected))
                        current_archetype = archetype;

                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            };
            // --------------------------------------------

            //-------------Entity Filter-------------------
            ImGui::Text("Entity");
            static std::string current_entity = "-";

            if (ImGui::BeginCombo("E", current_entity.c_str())) {
                for (auto& entity : entityNames) {
                    bool selected = (current_entity == entity);
                    if (ImGui::Selectable(entity.c_str(), selected))
                        current_entity = entity;

                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            };

            //---------------------------------------------


            //------------Component Filter-----------------

            ImGui::Text("Component Type");
            static std::string current_comptype = "-";

            if (ImGui::BeginCombo("C", current_comptype.c_str())) {
                // since this is an ad hoc list, "-" needs to be prepended
                std::string comptype = "-";
                bool selected = (current_comptype == comptype);
                if (ImGui::Selectable(comptype.c_str(), selected))
                    current_comptype = comptype;
                if (selected)
                    ImGui::SetItemDefaultFocus();

                for (auto& curtype : snap.GetTypes()) {
                    comptype = curtype.second;
                    selected = (current_comptype == comptype);
                    if (ImGui::Selectable(comptype.c_str(), selected))
                        current_comptype = comptype;

                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            };

            //---------------------------------------------

            //---------------Tag Filter--------------------


            ImGui::Text("Tag");
            static std::string current_tag = "-";

            if (ImGui::BeginCombo("T", current_tag.c_str())) {
                for (auto& tag : tagNames) {
                    bool selected = (current_tag == tag);
                    if (ImGui::Selectable(tag.c_str(), selected))
                        current_tag = tag;

                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            };

            //---------------------------------------------

            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("Snapshot", childSnapshotTableSz);

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

                bool selectedArchetype = (current_archetype != "-");
                bool selectedEntity = (current_entity != "-");
                bool selectedComptype = (current_comptype != "-");
                bool selectedTag = (current_tag != "-");

                for (auto& archetype : snap.getArchetypes()) {
                    std::string aHash = archetype.second.toString();
                    if (selectedArchetype && aHash != current_archetype)
                        continue;
                    int archtagcount = 0;
                    std::string tagstr;
                    bool abortTag = (selectedTag && !archetype.second.getTags().size());
                    for (auto& tag : archetype.second.getTags()) {
                        if (archtagcount++) tagstr += ",";
                        std::string tagName = snap.GetTagName(tag);
                        if (selectedTag && tagName != current_tag)
                            abortTag = true;
                        tagstr += tagName;
                    }
                    if (abortTag) continue;
                    //ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(ImVec4(0,188,0,50)));
                    if (!archetype.second.getEntities().size()) {
                        if (selectedEntity) continue;
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
                        size_t entityIndex = 0;

                        for (auto& x : archetype.second.getEntities()) {
                            auto& entity = x.second;
                            std::string eIndex = entity.toString();
                            if (selectedEntity && eIndex != current_entity) {
                                entityIndex++;
                                continue;
                            }
                            size_t componentIndex = 0;
                            for (auto& component : entity.getComponents()) {
                                std::string actCompType = snap.GetTypeName(component.getType());
                                if (selectedComptype && actCompType != current_comptype) {
                                    componentIndex++;
                                    continue;
                                }
                                std::string cvalue = component.toString();
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);

                                // generate nice label for this table row; the "##" part is to guarantee unique labels while only displaying the hash
                                std::string componentLabel = aHash + "##" + std::to_string(entityIndex) + "." + std::to_string(componentIndex);
                                // make table row selectable
                                const ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
                                const bool itemIsSelected = (&component == selTableComponent);
                                if (itemIsSelected) componentSelected = true;
                                if (ImGui::Selectable(componentLabel.c_str(), itemIsSelected, selectable_flags /*, ImVec2(0, 10)*/)) {
                                    selTableComponent = &component;
                                    selTableEntity = &entity;
                                    componentSelected = true;
                                }
                                if (ImGui::BeginPopupContextItem()) {
                                    selTableComponent = &component;
                                    selTableEntity = &entity;
                                    componentSelected = true;
                                    bool watched = listening.getVecs(listening.cursel)->isWatched(selTableEntity->GetValue());
                                    if (!watched && ImGui::Button("Add to watchlist")) {
                                        vecs->addWatch(selTableEntity->GetValue());
                                        ImGui::CloseCurrentPopup();
                                    }
                                    if (watched && ImGui::Button("Remove from watchlist")) {
                                        vecs->deleteWatch(selTableEntity->GetValue());
                                        ImGui::CloseCurrentPopup();
                                    }
                                    ImGui::EndPopup();
                                }

                                ImGui::TableSetColumnIndex(1);
                                ImGui::TextUnformatted(eIndex.c_str());
                                ImGui::TableSetColumnIndex(2);
                                ImGui::TextUnformatted(actCompType.c_str());
                                ImGui::TableSetColumnIndex(3);
                                ImGui::TextUnformatted(cvalue.c_str());
                                ImGui::TableSetColumnIndex(4);
                                ImGui::TextUnformatted(tagstr.c_str());

                                componentIndex++;
                            }
                           entityIndex++;
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
                std::string entityText = std::string("Entity Index: ") + selTableEntity->toString() +
                    ", Version " + std::to_string(selTableEntity->GetVersion()) +
                    ", Storage Index " + std::to_string(selTableEntity->GetStorageIndex());
                ImGui::TextUnformatted(entityText.c_str());

                std::string componentText = std::string("Component Type: ") + snap.GetTypeName(selTableComponent->getType());
                ImGui::TextUnformatted(componentText.c_str());
                ImGui::TextUnformatted(selTableComponent->toString().c_str());


            }
         ImGui::EndChild();
        }
     ImGui::End();
    }
}

void static showSnapshotFileListWindow(bool* p_open) {
        if (!ImGui::Begin("SnapshotFileListWindow", p_open))
        {
            ImGui::End();
        }
        else
        {
            ImGui::Text("Choose a Snapshot: ");
            std::filesystem::path curDir{ "." };
            for (auto const& dir_entry : std::filesystem::directory_iterator{ curDir }) {
                auto entry = dir_entry.path().string();
                if (entry.size() < 5)continue; 
                if(entry.substr(entry.size()-5)==".json")
                    if (ImGui::Selectable(entry.c_str())) {
                        selectedSnapshotFile = entry;
                    }
            }

            //TODO: if not selected, display cancel button
            if (ImGui::Button(selectedSnapshotFile.size() ? "Select File: " : "Cancel")) {
                showSnapshotFileList = false;
            }
            ImGui::SameLine();
            ImGui::Text(selectedSnapshotFile.c_str());
            ImGui::End();
        }
    }

void static showConnectionWindow(ConsoleListener & listening, bool* p_open)
    {
#if USE_SCALING
        float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
#else
        const float scale = 1.f;
#endif
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
          ConsoleSocketThread* thd = listening.getVecs(0);
          auto wasselected = thd->selected;
          ImGui::Selectable("Load from File", &thd->selected);
          //If loadFromfile is selected set showSnapshotFileList to true
          if (thd->selected != wasselected) {
              if (thd->selected) {
                  selectedSnapshotFile.clear();
                  showSnapshotFileList = true;
              }
          }
          //if Load from file was selected: open selecction window: 

          if (showSnapshotFileList) {
              showSnapshotFileListWindow(&showSnapshotFileList);
              if (!showSnapshotFileList) {
                  thd->selected = (selectedSnapshotFile.size() != 0);
              }
          }

          if (thd->selected != wasselected) {
              if (thd->selected) {
                  try {
                      std::ifstream loadfile(selectedSnapshotFile);
                      nlohmann::json inputjson = nlohmann::json::parse(loadfile);
                      loadfile.close();
                      thd->parseSnapshot(inputjson);
                      cursel = 0;
                  }
                  catch (...) {
                      thd->selected = false;
                  }
              }
              else if (wasselected) {
                  listening.cursel = -1;
              }
          }

          for (size_t i = 1; i < listening.vecsCount(); i++) {
              ConsoleSocketThread* thd = listening.getVecs(i);
              if (thd) {
                  int pid = thd->getPid();
                  if (pid > 0) {
                      std::string spid = "VECS PID " + std::to_string(pid);
                      auto wasselected = thd->selected;
                      ImGui::Selectable(spid.c_str(), &thd->selected);
                      if (thd->selected != wasselected) {
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
              for (size_t i = 0; i < listening.vecsCount(); i++) {
                  ConsoleSocketThread* thd = listening.getVecs(i);
                  if (thd) {
                      thd->selected = (i == cursel);
                  }
              }
          }
          ImGui::End();
        }
    }

void static showLiveView(ConsoleListener & listening, bool* p_open)
    {
#if USE_SCALING
        float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
#else
        const float scale = 1.f;
#endif

        ImVec2 windowsize = ImVec2(1135 * scale, 700 * scale);
        ImGui::SetNextWindowSize(windowsize, ImGuiCond_Once);
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
            // calculate area for our 3 child windows - minimum is 600,300 (scaled)
            // the y size calculation is a bit meh ... need to find out more about child window padding, the effects of ImGui::NewLine etc.

            ImVec2 childArea(std::max(900.f * scale, windowSize.x - 2 * cursorPos.x - 7.f),
                std::max(300.f * scale, windowSize.y - cursorPos.y - 12.f));

            float lowerY = childArea.y * 0.5f;

            ImVec2 childLiveViewGraphSz(childArea.x - (10.f * scale /*for scrollbar*/), childArea.y - lowerY);
            ImVec2 childStatsSz(childArea.x / 3.f, lowerY);
            ImVec2 childWatchlistSz(childArea.x * (2.f / 3.f), lowerY);

            // Filter child window

            ImGui::BeginChild("LiveView", childLiveViewGraphSz);

            if (listening.cursel >= 0) {
                auto vecs = listening.getVecs(listening.cursel);
                bool isLive = vecs->getIsLive();
                if (!isLive && ImGui::Button("Start LiveView"))
                {
                    vecs->requestLiveView();
                }
                if (isLive && ImGui::Button("Stop LiveView"))
                {
                    vecs->requestLiveView(false);
                }

                if (ImPlot::BeginPlot("TestPlot", ImVec2(-1, -1))) {

                    ImPlot::SetupAxisLimits(ImAxis_X1, 0, _countof(vecs->lvEntityCount));
                    ImPlot::SetupAxisLimits(ImAxis_Y1, 0, vecs->lvEntityMax, ImPlotCond_Always);
                    //ImPlot::SetupAxesLimits(0,50,0,100);
                    ImPlot::PlotBars("Entities", vecs->lvEntityCount, _countof(vecs->lvEntityCount));
                    ImPlot::EndPlot();
                }

            }
            ImGui::EndChild();

            ImGui::BeginChild("Statistics", childStatsSz);

            if (listening.cursel >= 0) {
                auto vecs = listening.getVecs(listening.cursel);
                ImGui::Text("Number of Entities: %d", vecs->lvEntityCount[_countof(vecs->lvEntityCount) - 1]);
                ImGui::Text("Average Number of Components: %.2f", vecs->getAvgComp());
                char s[128];
                size_t estSize = vecs->getEstSize();
                // primitive human-readable value ...
                if (estSize >= 1000000000L) // over a gigabyte?
                    sprintf(s, "%.2lf GB", static_cast<double>(estSize) / 1000000000.0);
                else if (estSize >= 1000000L) // over a megabyte?
                    sprintf(s, "%.2lf MB", static_cast<double>(estSize) / 1000000.0);
                else if (estSize >= 1000L) // over a killabyte?
                    sprintf(s, "%.2lf KB", static_cast<double>(estSize) / 1000.0);
                else  // below a killahbyte
                    sprintf(s, "%ld B", (int)estSize);
                ImGui::Text("Estimated Memory usage: %s", s);
            } 
            ImGui::EndChild();
            ImGui::SameLine();

            ImGui::BeginChild("Watchlist", childWatchlistSz);
            if (listening.cursel >= 0) {
                auto vecs = listening.getVecs(listening.cursel);

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

                    auto& watchlist = vecs->getWatchlist();

                    size_t entityIndex = 0;
                    for (auto& entityhandle : watchlist) {
                        auto& entity = entityhandle.second;

                        auto archetype = entity.GetArchetype();
                        std::string aHash = archetype->toString();
                        std::string eIndex = entity.toString();
                        int archtagcount = 0;
                        std::string tagstr;
                        for (auto& tag : archetype->getTags()) {
                            if (archtagcount++) tagstr += ",";
                            tagstr += std::to_string(tag);
                        }
                        ImVec4 color = entity.isModified() ? ImVec4(255, 255, 0, 255) :
                            entity.isDeleted() ? ImVec4(255, 0, 0, 255) :
                            ImVec4(255, 255, 255, 255);
                        size_t componentIndex = 0;
                        for (auto& component : entity.getComponents()) {

                            std::string cvalue = component.toString();

                            ImGui::TableNextRow();

                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextColored(color, aHash.c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextColored(color, eIndex.c_str());
                            ImGui::TableSetColumnIndex(2);
                            ImGui::TextColored(color, entity.GetTypeName(component.getType()).c_str());
                            ImGui::TableSetColumnIndex(3);
                            ImGui::TextColored(color, cvalue.c_str());
                            ImGui::TableSetColumnIndex(4);
                            ImGui::TextColored(color, tagstr.c_str());

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

void static showWatchlistWindow(ConsoleListener & listening, bool* p_open) {
#if USE_SCALING
        float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
#else
        const float scale = 1.f;
#endif

        ImVec2 windowsize = ImVec2(1135 * scale, 700 * scale);
        ImGui::SetNextWindowSize(windowsize, ImGuiCond_Once);
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

                auto vecs = listening.getVecs(listening.cursel);
                if (ImGui::BeginTable("Watchlist", 5, ImGuiTableFlags_RowBg)) {

                    ImGui::TableSetupColumn("Archetype");
                    ImGui::TableSetupColumn("Index");
                    ImGui::TableSetupColumn("Typename");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableSetupColumn("Tag");
                    ImGui::TableHeadersRow();

                    auto& watchlist = vecs->getWatchlist();

                    size_t entityIndex = 0;
                    size_t entidel = (size_t)-1;
                    for (auto& entityhandle : watchlist) {
                        auto& entity = entityhandle.second;
                        auto archetype = entity.GetArchetype();
                        std::string aHash = archetype->toString();
                        std::string eIndex = entity.toString();
                        int archtagcount = 0;
                        std::string tagstr;
                        for (auto& tag : archetype->getTags()) {
                            if (archtagcount++) tagstr += ",";
                            tagstr += /*snap.GetTagName(tag)*/std::to_string(tag);
                        }
                        size_t componentIndex = 0;
                        for (auto& component : entity.getComponents()) {

                            std::string cvalue = component.toString();

                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);

                            // generate nice label for this table row; the "##" part is to guarantee unique labels while only displaying the hash
                            std::string componentLabel = aHash + "##wl" + std::to_string(entityIndex) + "." + std::to_string(componentIndex);
                            // make table row selectable
                            const ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
                            const bool itemIsSelected = (&component == selTableComponent);
                            if (itemIsSelected) componentSelected = true;
                            if (ImGui::Selectable(componentLabel.c_str(), itemIsSelected, selectable_flags /*, ImVec2(0, 10)*/)) {
                                selTableComponent = &component;
                                selTableEntity = &entity;
                                componentSelected = true;
                            }
                            if (ImGui::BeginPopupContextItem()) {
                                selTableComponent = &component;
                                selTableEntity = &entity;
                                componentSelected = true;

                                if (ImGui::Button("Remove from watchlist")) {
                                    //TODO 
                                    entidel = entityhandle.first;
                                    ImGui::CloseCurrentPopup();
                                }
                                ImGui::EndPopup();
                            }
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(eIndex.c_str());
                            ImGui::TableSetColumnIndex(2);
                            ImGui::TextUnformatted(entity.GetTypeName(component.getType()).c_str());
                            ImGui::TableSetColumnIndex(3);
                            ImGui::TextUnformatted(cvalue.c_str());
                            ImGui::TableSetColumnIndex(4);
                            ImGui::TextUnformatted(tagstr.c_str());

                            componentIndex++;
                        }
                        entityIndex++;
                    }
                    if (entidel != (size_t)-1)
                        vecs->deleteWatch(entidel);

                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }

    }

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
            showConnectionWindow(listening, &connectionWindow);
        }

        if (viewSnapshotWindow) {
            showViewSnapshotWindow(listening, &viewSnapshotWindow);
        }

        if (liveView)
        {
            showLiveView(listening, &liveView);
        }

        if (showWatchlist) {
            showWatchlistWindow(listening, &showWatchlist);
        }

    }
