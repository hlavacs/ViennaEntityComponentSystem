#pragma once
#include <iostream>
#include <list>

#include "ConsoleEntity.h"

namespace Console {

	class Registry;
	class Archetype {
	private:
		std::list<Entity> entities;
		std::list<std::string> dataTypes;
		std::list<size_t> tags;
		size_t hash;
		Registry* registry{ nullptr };

	public:
		Archetype(size_t hash = 0) : hash(hash) {}
		Archetype(Archetype const& org) {
			entities = org.entities;
			for (auto& e : entities) e.SetArchetype(this);
			dataTypes = org.dataTypes;
			tags = org.tags;
			hash = org.hash;
		}
		Archetype& operator=(Archetype const& org) {
			clear();
			entities = org.entities;
			for (auto& e : entities) e.SetArchetype(this);
			dataTypes = org.dataTypes;
			tags = org.tags;
			hash = org.hash;
			return *this;
		}

		void SetRegistry(Registry* r = nullptr) { registry = r; }
		Registry* GetRegistry() { return registry; }

	public:
		void clear() {
			entities.clear();
			dataTypes.clear();
			tags.clear();
			hash = 0;
		}

		std::list<Entity>& getEntities() {
			return entities;
		}

		int addEntity(Entity& e) {
			entities.push_back(e);
			entities.back().SetArchetype(this);
			return 0;
		}

		int deleteEntity(std::string) {
			//TODO
			return 0;
		}

		int findEntity(std::string) {
			//TODO
			return 0;
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
