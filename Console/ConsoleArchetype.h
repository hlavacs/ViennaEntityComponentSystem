#pragma once
#include <iostream>
#include <list>
#include <map>

#include "ConsoleEntity.h"

namespace Console {

	class Registry;
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
			copyArchetype(org);
			entities = org.entities;
			for (auto& e : entities) e.second.SetArchetype(this);
		}
		Archetype& operator=(Archetype const& org) {
			copyArchetype(org);
			entities = org.entities;
			for (auto& e : entities) e.second.SetArchetype(this);
			return *this;
		}
		Archetype& copyArchetype(Archetype const& org) {
			clear();
			dataTypes = org.dataTypes;
			tags = org.tags;
			hash = org.hash;
			return *this;
		}

		void SetRegistry(Registry* r = nullptr);
		Registry* GetRegistry() { return registry; }

		void clear() {
			entities.clear();
			dataTypes.clear();
			tags.clear();
			hash = 0;
		}

		std::map<size_t, Entity>& getEntities() {
			return entities;
		}

		int addEntity(Entity& e);

		int deleteEntity(std::string) {
			//TODO
			return 0;
		}

		Entity* findEntity(size_t id) {
			auto i = entities.find(id);
			if (i != entities.end()) {
				return &i->second;
			}
			else return nullptr;
		}


		std::list<std::size_t>& getTags() {
			return tags;
		}

		int addTag(size_t tagid) {
			tags.push_back(tagid);
			return 0;
		}

		int deleteTag(size_t tagid) {
			//TODO
			return 0;
		}

		int findTag(size_t taggid) {
			//TODO
			return 0;
		}



		size_t getHash() {
			return hash;
		}

		std::string toString() {
			return std::to_string(hash);
		}


	};

} // namespace Console
