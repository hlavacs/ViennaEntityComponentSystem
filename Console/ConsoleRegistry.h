#pragma once

#include <cassert>
#include <unordered_map>
#include <map>
#include <string>
#include <chrono>

#include "ConsoleArchetype.h"

namespace Console {

    // Representation of a Registry for the Console
    class Registry {

    private:
        // map of all types and their names 
        std::map<size_t, std::string> types;
        std::map<size_t, std::string> tags;
        std::map<size_t, Archetype> archetypes;
        std::map<size_t, size_t> entities;
        std::string jsonsnap;
        size_t entitycount{ 0 }, componentcount{ 0 };
        std::chrono::high_resolution_clock::time_point tstampFetched, tstampParsed;

    public:
        Registry() {}
        Registry(Registry const& org) {
            types = org.types;
            tags = org.tags;
            archetypes = org.archetypes;
            entities = org.entities;
            entitycount = org.entitycount;
            componentcount = org.componentcount;
            tstampFetched = org.tstampFetched;
            tstampParsed = org.tstampParsed;
            for (auto& a : archetypes) a.second.SetRegistry(this);
        }
        Registry& operator=(Registry const& org) {
            clear();
            types = org.types;
            tags = org.tags;
            archetypes = org.archetypes;
            entities = org.entities;
            entitycount = org.entitycount;
            componentcount = org.componentcount;
            tstampFetched = org.tstampFetched;
            tstampParsed = org.tstampParsed;
            for (auto& a : archetypes) a.second.SetRegistry(this);
            return *this;
        }

        void clear() {
            types.clear();
            tags.clear();
            archetypes.clear();
            entities.clear();
            entitycount = componentcount = 0;
            tstampFetched = {};
            tstampParsed = {};
        }

        void setJsonsnap(std::string json) {
            tstampFetched = std::chrono::high_resolution_clock::now();
            jsonsnap = json;
        }
        std::string getJsonsnap() {
            return jsonsnap;
        }
        void setParsed() {
            tstampParsed = std::chrono::high_resolution_clock::now();
        }
        std::chrono::steady_clock::time_point getJsonTS() const { return tstampFetched; }
        std::chrono::steady_clock::time_point getParsedTS() const { return tstampParsed; }

        // archetype handling
        std::map<size_t, Archetype>& getArchetypes() {
            return archetypes;
        }

        int addArchetype(Archetype& a) {
            archetypes[a.getHash()] = a;
            archetypes[a.getHash()].SetRegistry(this);
            return 0;
        }

        void addEntity(Entity& e, size_t archetype) {
            entitycount++;
            componentcount += e.getComponents().size();
            entities[e.GetValue()] = archetype;
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

        // type name handling 
        std::map<size_t, std::string>& GetTypes() { return types; }
        bool AddTypeName(size_t t, std::string name) {
            if (HasTypeName(t))
                return name != GetTypeName(t);  // report whether same
            types[t] = name;                    // otherwise insert new type in map
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

        // tag name handling 
        bool AddTag(size_t t, std::string name) {
            if (HasTag(t))
                return name != GetTagName(t);  // report whether same 
            tags[t] = name;                    // otherwise insert new tag in map
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

        size_t GetEntitycount() const { return entitycount; }
        size_t GetComponentcount() const { return componentcount; }

    };
}