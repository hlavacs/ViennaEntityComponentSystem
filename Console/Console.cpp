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

ConsoleListener listening;
std::string service = "2000";

static bool connectionWindow = true;
static bool viewSnapshotWindow = false;
static bool liveView = false;
static bool showWatchlist = false;
static bool showSnapshotFileList = false;
std::string selectedSnapshotFile;

float GetContentScale();

bool SetupListener() {
    return listening.Create(service);
}

void TerminateListener() {
    listening.Terminate();
}

#define WITH_CLIPPER 1

#if WITH_CLIPPER
//cache for snapshot selections
static class SnapshotDisplayCache
{
private:
    std::chrono::steady_clock::time_point tstamp;
    size_t tableLines{ 0 };
    std::string filter_archetype;
    std::string filter_entity;
    std::string filter_comptype;
    std::string filter_tag;
    using cacheTuple = std::tuple<Console::Archetype*, Console::Entity*, Console::Component*>;
    std::vector<cacheTuple> comp_cache; 
    std::vector<std::string> archetype_cache; 
    std::vector<std::string> entity_cache; 
    std::vector<std::string> component_cache; 
    std::vector<std::string> tag_cache; 

public:
    
   bool cache_filters(Console::Registry& snap) {
       auto newStamp = snap.getJsonTS();
       if (tstamp != newStamp) {
           tstamp = newStamp;
           filter_archetype = "?"; //make sure to rebuild the cache
           filter_entity = "-";
           filter_comptype = "-";
           filter_tag = "-";
           comp_cache.clear();
           archetype_cache.clear();
           entity_cache.clear();
           component_cache.clear();
           tag_cache.clear();
           tableLines = snap.GetComponentcount();

           archetype_cache.push_back("-");
           entity_cache.push_back("-");
           component_cache.push_back("-");
           tag_cache.push_back("-");

           std::set<std::string>tagNames;
           for (auto& arch : snap.getArchetypes()) {
               archetype_cache.push_back(arch.second.toString());
               for (auto& tag : arch.second.getTags())
                   tagNames.insert(snap.GetTagName(tag));
               for (auto& ent : arch.second.getEntities())
                   entity_cache.push_back(ent.second.toString());
           }

           for (auto& curtype : snap.GetTypes()) {
               component_cache.push_back(curtype.second);
           }

           for (auto& curTag : tagNames) {
               tag_cache.push_back(curTag);
           }
           return true; 
       }
       return false;
   }

    //return number of lines that are displayed based on current filter criteria
    size_t TableLines(Console::Registry& snap, std::string sel_archetype, std::string sel_entity, std::string sel_comptype, std::string sel_tag) {
        auto newStamp = snap.getJsonTS();
        if (tstamp != newStamp) {  // if snapshot changed, reset
            tstamp = newStamp;
            filter_archetype = "?"; //make sure to rebuild the cache
            filter_entity = "-";
            filter_comptype = "-";
            filter_tag = "-";
            comp_cache.clear();
            tableLines = snap.GetComponentcount();
        }
        enum {
            fcArchetype = 1,
            fcEntity = 2,
            fcComptype = 4,
            fcTag = 8,
        };
        int fc = 0;
        if (filter_archetype != sel_archetype) {
            filter_archetype = sel_archetype;
            fc += fcArchetype;
        }
        if (filter_entity != sel_entity) {
            filter_entity = sel_entity;
            fc += fcEntity;
        }
        if (filter_comptype != sel_comptype) {
            filter_comptype = sel_comptype;
            fc += fcComptype;
        }
        if (filter_tag != sel_tag) {
            filter_tag = sel_tag;
            fc += fcTag;
        }
        if (fc) {  // if there are any filter changes, recalculation is necessary
            // same logic as in showViewSnapshotWindow() below

            bool selectedArchetype = (filter_archetype != "-");
            bool selectedEntity = (filter_entity != "-");
            bool selectedComptype = (filter_comptype != "-");
            bool selectedTag = (filter_tag != "-");

            comp_cache.clear();

            if (!(selectedArchetype || selectedEntity || selectedComptype || selectedTag)) {
                tableLines = 0;

                for (auto& archetype : snap.getArchetypes()) {
                    std::string aHash = archetype.second.toString();
                    bool abortTag = (selectedTag && !archetype.second.getTags().size());
                    for (auto& tag : archetype.second.getTags()) {
                        std::string tagName = snap.GetTagName(tag);
                        if (selectedTag && tagName != filter_tag)
                            abortTag = true;
                    }
                    
                    if (!archetype.second.getEntities().size()) {
                        tableLines++;
                        comp_cache.push_back(cacheTuple(&archetype.second, nullptr, nullptr));

                    }
                    else {
                        for (auto& x : archetype.second.getEntities()) {
                            auto& entity = x.second;
                            for (auto& component : entity.getComponents()) {
                                comp_cache.push_back(cacheTuple(&archetype.second, &entity, &component));
                                tableLines++;
                            }
                        }
                    }
                }

            }
            else {
                tableLines = 0;
                for (auto& archetype : snap.getArchetypes()) {
                    std::string aHash = archetype.second.toString();
                    if (selectedArchetype && aHash != filter_archetype)
                        continue;
                    bool abortTag = (selectedTag && !archetype.second.getTags().size());
                    for (auto& tag : archetype.second.getTags()) {
                        std::string tagName = snap.GetTagName(tag);
                        if (selectedTag && tagName != filter_tag)
                            abortTag = true;
                    }
                    if (abortTag) continue;
                    if (!archetype.second.getEntities().size()) {
                        if (selectedEntity) continue;
                        tableLines++;
                        comp_cache.push_back(cacheTuple(&archetype.second, nullptr, nullptr));
                    }
                    else {
                        for (auto& x : archetype.second.getEntities()) {
                            auto& entity = x.second;
                            std::string eIndex = entity.toString();
                            if (selectedEntity && eIndex != filter_entity) {
                                continue;
                            }
                            for (auto& component : entity.getComponents()) {
                                std::string actCompType = snap.GetTypeName(component.getType());
                                if (selectedComptype && actCompType != filter_comptype) {
                                    continue;
                                }
                                comp_cache.push_back(cacheTuple(&archetype.second, &entity,&component));
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
        return comp_cache[index];
    }

    std::vector<std::string>& getArchetype_cache() {
        return archetype_cache;
    }

    std::vector<std::string>& getEntity_cache() {
        return entity_cache;
    }

    std::vector<std::string>& getComponent_cache() {
        return component_cache;
    }

    std::vector<std::string>& getTag_cache() {
        return tag_cache;
    }

} snapshotDisplayCache;

#endif


void static showViewSnapshotWindow(ConsoleListener& listening, bool* p_open)
{
    float scale = GetContentScale();
    //for over a hundred thousand entities, this way of displaying the data and filtering it is too slow.
    //rework is needed
    //e.g only load data of the first 100 displayed entites and only load more when scrolling down
    //maybe initialize filters when snapshot is coming in or methods for the filters 


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

            // used in child window area calculation below
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
                auto timestamp = std::localtime(&currTime);
                auto savetime = std::put_time(timestamp, "%Y%m%d%H%M%S");
                std::ostringstream oss;
                oss << savetime;
                std::string saveName = std::string("snapshot") + oss.str() + ".json";
                savefile.open(saveName);
                savefile << snap.getJsonsnap();
                savefile.close();
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

#if WITH_CLIPPER
            //cache filterlist
            snapshotDisplayCache.cache_filters(snap);

            //--------------Archetype Filter--------------
            static std::string current_archetype = "-";
            ImGui::Text("Archetype");
            if (ImGui::BeginCombo("A", current_archetype.c_str())) {
                ImGuiListClipper clipper;
                auto& cache = snapshotDisplayCache.getArchetype_cache();
                auto archetypelines = cache.size();
                clipper.Begin(archetypelines);
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd && row < archetypelines; row++) {

                        bool selected = (current_archetype == cache[row]);
                        if (ImGui::Selectable(cache[row].c_str(), selected))
                            current_archetype = cache[row];

                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }


            //-------------Entity Filter-------------------
            ImGui::Text("Entity");
            static std::string current_entity = "-";

            if (ImGui::BeginCombo("E", current_entity.c_str())) {

                ImGuiListClipper clipper;
                auto& cache = snapshotDisplayCache.getEntity_cache(); 
                auto entitylines = cache.size();
                clipper.Begin(entitylines);
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd && row < entitylines; row++) {

                            bool selected = (current_entity == cache[row]);
                            if (ImGui::Selectable(cache[row].c_str(), selected))
                                current_entity = cache[row];

                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        
                    }
                }

                ImGui::EndCombo();
            }


            //-------------Component Filter-------------------
            ImGui::Text("Component Type");
            static std::string current_comptype = "-";

            if (ImGui::BeginCombo("C", current_comptype.c_str())) {

                ImGuiListClipper clipper;
                auto& cache = snapshotDisplayCache.getComponent_cache();
                auto componentlines = cache.size();
                clipper.Begin(componentlines);
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd && row < componentlines; row++) {

                        bool selected = (current_comptype == cache[row]);
                        if (ImGui::Selectable(cache[row].c_str(), selected))
                            current_comptype = cache[row];

                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }


            //---------------Tag Filter--------------------


            ImGui::Text("Tag");
            static std::string current_tag = "-";

            if (ImGui::BeginCombo("T", current_tag.c_str())) {

                ImGuiListClipper clipper;
                auto& cache = snapshotDisplayCache.getTag_cache();
                auto taglines = cache.size();
                clipper.Begin(taglines);
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd && row < taglines; row++) {

                        bool selected = (current_tag == cache[row]);
                        if (ImGui::Selectable(cache[row].c_str(), selected))
                            current_tag = cache[row];

                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }

#else
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
#endif
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("Snapshot", childSnapshotTableSz);

#if WITH_CLIPPER
            size_t tableLines = snapshotDisplayCache.TableLines(snap, current_archetype, current_entity, current_comptype, current_tag);
#endif

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

#if WITH_CLIPPER
                ImGuiListClipper clipper;
                clipper.Begin(tableLines);
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd && row < tableLines; row++) {
                        auto cacherow = snapshotDisplayCache[row];
                        auto& archetype = *std::get<0>(cacherow);
                        auto entity = std::get<1>(cacherow);
                        auto component = std::get<2>(cacherow);
                        // schnippeln wirs mal zusammen ...
                        std::string aHash = archetype.toString();
                        int archtagcount = 0;
                        std::string tagstr;
                        for (auto& tag : archetype.getTags()) {
                            if (archtagcount++) tagstr += ",";
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
                            std::string eIndex = entity->toString();
                            std::string actCompType = snap.GetTypeName(component->getType());

                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);

                            std::string cvalue = component->toString();
                            // generate nice label for this table row; the "##" part is to guarantee unique labels while only displaying the hash
                            std::string componentLabel = aHash + "##" + eIndex + "." + std::to_string(row);
                            // make table row selectable
                            const ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
                            const bool itemIsSelected = (component == selTableComponent);
                            if (itemIsSelected) componentSelected = true;
                            if (ImGui::Selectable(componentLabel.c_str(), itemIsSelected, selectable_flags)) {
                                selTableComponent = component;
                                selTableEntity = entity;
                                componentSelected = true;
                            }
                            if (ImGui::BeginPopupContextItem()) {
                                selTableComponent = component;
                                selTableEntity = entity;
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

                        }
                    }
                }
#else

#if WITH_CLIPPER
                ImGuiListClipper clipper;
                clipper.Begin(tableLines);
                while (clipper.Step()) {
#endif
                    bool selectedArchetype = (current_archetype != "-");
                    bool selectedEntity = (current_entity != "-");
                    bool selectedComptype = (current_comptype != "-");
                    bool selectedTag = (current_tag != "-");
                    int curLine = 0;

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
                        if (!archetype.second.getEntities().size()) {
                            if (selectedEntity) continue;
#if WITH_CLIPPER
                            if (curLine >= clipper.DisplayStart && curLine < clipper.DisplayEnd) {
#endif
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
#if WITH_CLIPPER
                            }
#endif
                            curLine++;
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
#if WITH_CLIPPER
                                    if (curLine >= clipper.DisplayStart && curLine < clipper.DisplayEnd) {
#endif
                                        ImGui::TableNextRow();
                                        ImGui::TableSetColumnIndex(0);

                                        std::string cvalue = component.toString();
                                        // generate nice label for this table row; the "##" part is to guarantee unique labels while only displaying the hash
                                        std::string componentLabel = aHash + "##" + std::to_string(entityIndex) + "." + std::to_string(componentIndex);
                                        // make table row selectable
                                        const ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
                                        const bool itemIsSelected = (&component == selTableComponent);
                                        if (itemIsSelected) componentSelected = true;
                                        if (ImGui::Selectable(componentLabel.c_str(), itemIsSelected, selectable_flags)) {
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
#if WITH_CLIPPER
                                    }
#endif

                                    curLine++;
#if WITH_CLIPPER
                                    if (curLine >= clipper.DisplayEnd)
                                        break;
#endif
                                    componentIndex++;
                                }
                                entityIndex++;
#if WITH_CLIPPER
                                if (curLine >= clipper.DisplayEnd)
                                    break;
#endif
                            }
                        }

#if WITH_CLIPPER
                        if (curLine >= clipper.DisplayEnd)
                            break;
#endif
                    }

#if WITH_CLIPPER
                }   // while (clipper.Step())
#endif

#endif
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
            // this is for debugging purposes only and can be removed once the caching works reliably!
            else {
                auto mics = std::chrono::duration_cast<std::chrono::milliseconds>(snap.getParsedTS() - snap.getJsonTS()).count();
                std::string initText = std::string("Snapshot Entities: ") + std::to_string(snap.GetEntitycount()) +
                    ", Components: " + std::to_string(snap.GetComponentcount()) +
                    ", Parse time: " + std::to_string(mics) + " msecs";
                ImGui::TextUnformatted(initText.c_str());
#if WITH_CLIPPER
                initText = std::string("Table lines: ") + std::to_string(tableLines);
                ImGui::TextUnformatted(initText.c_str());
#endif
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
            if (entry.substr(entry.size() - 5) == ".json")
                if (ImGui::Selectable(entry.c_str())) {
                    selectedSnapshotFile = entry;
                }
        }
        if (ImGui::Button(selectedSnapshotFile.size() ? "Select File: " : "Cancel")) {
            showSnapshotFileList = false;
        }
        ImGui::SameLine();
        ImGui::Text(selectedSnapshotFile.c_str());
        ImGui::End();
    }
}

void static showConnectionWindow(ConsoleListener& listening, bool* p_open)
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

void static showLiveView(ConsoleListener& listening, bool* p_open)
{
    float scale = GetContentScale();

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
        ImVec2 childArea(std::max(900.f * scale, windowSize.x - 2 * cursorPos.x - 7.f),
            std::max(300.f * scale, windowSize.y - cursorPos.y - 12.f));

        float lowerY = childArea.y * 0.5f;

        ImVec2 childLiveViewGraphSz(childArea.x - (10.f * scale), childArea.y - lowerY);
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
            // primitive human-readable value
            if (estSize >= 1000000000L) // over a gigabyte
                sprintf(s, "%.2lf GB", static_cast<double>(estSize) / 1000000000.0);
            else if (estSize >= 1000000L) // over a megabyte
                sprintf(s, "%.2lf MB", static_cast<double>(estSize) / 1000000.0);
            else if (estSize >= 1000L) // over a kilobyte
                sprintf(s, "%.2lf KB", static_cast<double>(estSize) / 1000.0);
            else  // below a kilobyte
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
                        tagstr += entity.GetTagName(tag);
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

void static showWatchlistWindow(ConsoleListener& listening, bool* p_open) {
    float scale = GetContentScale();

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
                        tagstr += entity.GetTagName(tag);
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
                        if (ImGui::Selectable(componentLabel.c_str(), itemIsSelected, selectable_flags)) {
                            selTableComponent = &component;
                            selTableEntity = &entity;
                            componentSelected = true;
                        }
                        if (ImGui::BeginPopupContextItem()) {
                            selTableComponent = &component;
                            selTableEntity = &entity;
                            componentSelected = true;

                            if (ImGui::Button("Remove from watchlist")) {
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
