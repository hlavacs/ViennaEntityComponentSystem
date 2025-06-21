#pragma once

#include <iostream>
#include <list>
#include <vector>
#include "ConsoleComponent.h"

namespace Console {

	class Archetype;
	class Entity {
	private:
		size_t index;
		size_t version;
		size_t stgindex;

		std::list<Component> components;
		Archetype* archetype{ nullptr };

	public:
		Entity(size_t index = 0, size_t version = 0, size_t stgindex = 0) : index(index), version(version), stgindex(stgindex) {}
		Entity(Entity const& org) {
			index = org.index;
			version = org.version;
			stgindex = org.stgindex;
			components = org.components;
		}
		Entity& operator=(Entity const& org) {
			clear();
			index = org.index;
			version = org.version;
			stgindex = org.stgindex;
			components = org.components;
			return *this;
		}

		size_t GetIndex() { return index; }
		size_t GetVersion() { return version; }
		size_t GetStorageIndex() { return stgindex; }

		void SetArchetype(Archetype* a = nullptr) { archetype = a; }
		Archetype* GetArchetype() { return archetype; }

	public:

		void clear() {
			index = version = stgindex = 0;;
			components.clear();
		}

		std::list<Component>& getComponents() {
			return components;
		}

		int addComponent(Component& c) {
			components.push_back(c);
			return 0;
		}

		int deleteComponent(std::string) {
			//TODO
			return 0;
		}

		int findComponent(std::string) {
			//TODO
			return 0;
		}


		std::string toString() {
			return std::to_string(index);
		}



	};

} // namespace Console
