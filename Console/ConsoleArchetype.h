#pragma once
#include <iostream>
#include <list>
#include <map>

#include "ConsoleEntity.h"


namespace Console {
	class Registry;
	// Representation of Archetypes for the Console
	class Archetype {
	private:
		std::map<size_t, Entity> entities;
		std::list<std::string> dataTypes;
		std::list<size_t> tags;
		size_t hash;
		Registry* registry{ nullptr };

	public:
		Archetype(size_t hash = 0) : hash(hash) {}
		Archetype(Archetype const& org) {
			CopyArchetype(org);
			entities = org.entities;
			for (auto& e : entities) e.second.SetArchetype(this);
		}
		Archetype& operator=(Archetype const& org) {
			CopyArchetype(org);
			entities = org.entities;
			for (auto& e : entities) e.second.SetArchetype(this);
			return *this;
		}
		/// @brief copy another archetype
		/// @param org original Archetype
		/// @return reference to copy of archetype
		Archetype& CopyArchetype(Archetype const& org) {
			Clear();
			dataTypes = org.dataTypes;
			tags = org.tags;
			hash = org.hash;
			return *this;
		}
		/// @brief set the used registry
		/// @param r Pointer to the registry
		void SetRegistry(Registry* r = nullptr);

		/// @brief get the registry
		/// @return registry pointer
		Registry* GetRegistry() { return registry; }

		/// @brief Clear Archetype contents
		void Clear() {
			entities.clear();
			dataTypes.clear();
			tags.clear();
			hash = 0;
		}

		/// @brief get entities of archetype
		/// @return map of entities with their handles
		std::map<size_t, Entity>& GetEntities() {
			return entities;
		}

		/// @brief add an entity to the archetype
		/// @param e entity 
		void AddEntity(Entity& e);

		/// @brief find an entity in the archetype
		/// @param id handle of the entity
		/// @returns entity pointer if found, else nullptr
		Entity* FindEntity(size_t id) {
			auto i = entities.find(id);
			if (i != entities.end()) {
				return &i->second;
			}
			else return nullptr;
		}

		/// @brief get tags of the archetype
		/// @returns list of tags
		std::list<std::size_t>& GetTags() {
			return tags;
		}

		/// @brief add a tag to the archetype
		/// @param tagid entity 
		int AddTag(size_t tagid) {
			tags.push_back(tagid);
			return 0;
		}

		/// @brief get hash of the archetype
		size_t GetHash() {
			return hash;
		}

		/// @brief get hash of the archetype as a string
		std::string ToString() {
			return std::to_string(hash);
		}

	};

} // namespace Console
