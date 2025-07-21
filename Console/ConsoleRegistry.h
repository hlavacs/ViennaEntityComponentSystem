#pragma once

#include <cassert>
#include <unordered_map>
#include <map>
#include <string>

#include "ConsoleArchetype.h"

namespace Console {

    /// @brief  internal representation of a Registry from a VECS
    class Registry {

    private:
        // map of all types and their names 
        std::map<size_t, std::string> types;
        std::map<size_t, std::string> tags;
        std::map<size_t, Archetype> archetypes;
        std::map<size_t, size_t> entities;
        std::string jsonsnap;


    public:
        Registry() {}
        Registry(Registry const& org) {
            types = org.types;
            tags = org.tags;
            archetypes = org.archetypes;
            entities = org.entities;
            for (auto& a : archetypes) a.second.SetRegistry(this);
        }
        Registry& operator=(Registry const& org) {
            clear();
            types = org.types;
            tags = org.tags;
            archetypes = org.archetypes;
            entities = org.entities;
            for (auto& a : archetypes) a.second.SetRegistry(this);
            return *this;
        }

        void clear() {
            types.clear();
            tags.clear();
            archetypes.clear();
            entities.clear();
        }

        void setJsonsnap(std::string json) {
            jsonsnap = json;
        }
        std::string getJsonsnap() {
            return jsonsnap;
        }

        // archetype handling
        std::map<size_t, Archetype>& getArchetypes() {
            return archetypes;
        }

        int addArchetype(Archetype& a) {
            archetypes[a.getHash()] = a;
            archetypes[a.getHash()].SetRegistry(this);
            return 0;
        }

        void addEntity(size_t entityhandle, size_t archetype) {
            entities[entityhandle] = archetype;
        }

        Entity* findEntity(size_t handle) {
            auto hashit = entities.find(handle);
            if (hashit != entities.end()) {
                auto arch = findArchetype(hashit->second);
                if (arch != nullptr)
                    return arch->findEntity(handle);
            }
            return nullptr;
        }

        Archetype* findArchetype(size_t hash) {
            auto i = archetypes.find(hash);
            if (i != archetypes.end()) {
                return &i->second;
            }
            else return nullptr;
        }

        // type name handling stuff
        std::map<size_t, std::string>& GetTypes() { return types; }
        bool AddTypeName(size_t t, std::string name) {
            if (HasTypeName(t))                 // if already there ...
                return name != GetTypeName(t);  // report whether same (if not: two names for the same type are BAD!)
            types[t] = name;                    // otherwise peacefully insert new type in map
            return true;
        }
        bool HasTypeName(size_t t) {
            return types.find(t) != types.end();
        }
        std::string GetTypeName(size_t t) {
            auto it = types.find(t);
            assert(it != types.end());
            return it->second;
        }

        // tag name handling stuff
        bool AddTag(size_t t, std::string name) {
            if (HasTag(t))                     // if already there ...
                return name != GetTagName(t);  // report whether same (if not: two names for the same tag are BAD!)
            tags[t] = name;                    // otherwise peacefully insert new tag in map
            return true;
        }
        bool HasTag(size_t t) {
            return tags.find(t) != tags.end();
        }
        std::string GetTagName(size_t t) {
            auto it = tags.find(t);
            assert(it != tags.end());
            return it->second;
        }

    }; // Snapshot
}