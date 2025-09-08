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
		Archetype& CopyArchetype(Archetype const& org) {
			Clear();
			dataTypes = org.dataTypes;
			tags = org.tags;
			hash = org.hash;
			return *this;
		}

		void SetRegistry(Registry* r = nullptr);
		Registry* GetRegistry() { return registry; }

		void Clear() {
			entities.clear();
			dataTypes.clear();
			tags.clear();
			hash = 0;
		}

		std::map<size_t, Entity>& GetEntities() {
			return entities;
		}

		void AddEntity(Entity& e);


		Entity* FindEntity(size_t id) {
			auto i = entities.find(id);
			if (i != entities.end()) {
				return &i->second;
			}
			else return nullptr;
		}

		std::list<std::size_t>& GetTags() {
			return tags;
		}

		int AddTag(size_t tagid) {
			tags.push_back(tagid);
			return 0;
		}

		size_t GetHash() {
			return hash;
		}

		std::string ToString() {
			return std::to_string(hash);
		}

	};

} // namespace Console
