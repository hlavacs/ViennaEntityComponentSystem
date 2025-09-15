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
        size_t entityCount{ 0 }, componentCount{ 0 };
        std::chrono::high_resolution_clock::time_point tstampFetched, tstampParsed;

    public:
        Registry() {}
        Registry(Registry const& org) {
            types = org.types;
            tags = org.tags;
            archetypes = org.archetypes;
            entities = org.entities;
            entityCount = org.entityCount;
            componentCount = org.componentCount;
            tstampFetched = org.tstampFetched;
            tstampParsed = org.tstampParsed;
            for (auto& a : archetypes) a.second.SetRegistry(this);
        }
        Registry& operator=(Registry const& org) {
            Clear();
            types = org.types;
            tags = org.tags;
            archetypes = org.archetypes;
            entities = org.entities;
            entityCount = org.entityCount;
            componentCount = org.componentCount;
            tstampFetched = org.tstampFetched;
            tstampParsed = org.tstampParsed;
            for (auto& a : archetypes) a.second.SetRegistry(this);
            return *this;
        }

        /// @brief clear registry data
        void Clear() {
            types.clear();
            tags.clear();
            archetypes.clear();
            entities.clear();
            entityCount = componentCount = 0;
            tstampFetched = {};
            tstampParsed = {};
        }

        /// @brief set json snapshot string and timestamp
        /// @param json snapshot string
        void SetJsonsnap(std::string json) {
            tstampFetched = std::chrono::high_resolution_clock::now();
            jsonsnap = json;
        }

        /// @brief return snapshot json string
        std::string GetJsonsnap() {
            return jsonsnap;
        }

        /// @brief set timestamp when parsing is complete
        void SetParsed() {
            tstampParsed = std::chrono::high_resolution_clock::now();
        }

        /// @brief get timestamp when json has been received
        std::chrono::high_resolution_clock::time_point GetJsonTS() const { return tstampFetched; }

        /// @brief get timestamp when json has been parsed
        std::chrono::high_resolution_clock::time_point GetParsedTS() const { return tstampParsed; }

        // archetype handling

        /// @brief get archetype
        /// @return map of Archetypes with their hash
        std::map<size_t, Archetype>& GetArchetypes() {
            return archetypes;
        }

        /// @brief add an archetype to the registry
        /// @param a archetype to be added
        void AddArchetype(Archetype& a) {
            archetypes[a.GetHash()] = a;
            archetypes[a.GetHash()].SetRegistry(this);
        }

        /// @brief add an entity to an archetype
        /// @param archetype archetype 
        /// @param e entity to be added to the archetype
        void AddEntity(Entity& e, size_t archetype) {
            entityCount++;
            componentCount += e.GetComponents().size();
            entities[e.GetValue()] = archetype;
        }

        /// @brief find an entity by the handle
        /// @param handle entity handle
        /// @return entity or nullptr
        Entity* FindEntity(size_t handle) {
            auto hashit = entities.find(handle);
            if (hashit != entities.end()) {
                auto arch = FindArchetype(hashit->second);
                if (arch != nullptr)
                    return arch->FindEntity(handle);
            }
            return nullptr;
        }

        /// @brief find an archetype by the hash
        /// @param hash archetype hash
        /// @return archetype or nullptr
        Archetype* FindArchetype(size_t hash) {
            auto i = archetypes.find(hash);
            if (i != archetypes.end()) {
                return &i->second;
            }
            else return nullptr;
        }

        // type name handling 

        /// @brief get types
        /// @return map of type hashes with their typenames
        std::map<size_t, std::string>& GetTypes() { return types; }

        /// @brief add typename
        /// @param t type hash
        /// @param name typename
        /// @return true if added, false if hash already existing with another name
        bool AddTypeName(size_t t, std::string name) {
            if (HasTypeName(t))
                return name != GetTypeName(t);  // report whether same
            types[t] = name;                    // otherwise insert new type in map
            return true;
        }

        /// @brief check if typename existing 
        /// @param t type hash
        /// @returns true if already existing
        bool HasTypeName(size_t t) {
            return types.find(t) != types.end();
        }

        /// @brief get typename from type hash 
        /// @param t type hash
        /// @returns type name string
        std::string GetTypeName(size_t t) {
            auto it = types.find(t);
            assert(it != types.end());
            return it->second;
        }

        // tag name handling 


        /// @brief add tag
        /// @param t tag number
        /// @param name tagname
        /// @return true if added, false if number already existing with another name
        bool AddTag(size_t t, std::string name) {
            if (HasTag(t))
                return name != GetTagName(t);  // report whether same 
            tags[t] = name;                    // otherwise insert new tag in map
            return true;
        }

        /// @brief check if tag existing 
        /// @param t tag number
        /// @returns true if already existing
        bool HasTag(size_t t) {
            return tags.find(t) != tags.end();
        }

        /// @brief get tagname from tag number 
        /// @param t tag number
        /// @returns tag name string
        std::string GetTagName(size_t t) {
            auto it = tags.find(t);
            assert(it != tags.end());
            return it->second;
        }

        /// @brief get number of entities
        size_t GetEntityCount() const { return entityCount; }

        /// @brief get number of components
        size_t GetComponentCount() const { return componentCount; }

    };
}
