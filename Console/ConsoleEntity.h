#pragma once

#include <iostream>
#include <list>
#include <vector>
#include "ConsoleComponent.h"

namespace Console {

	class Archetype;
	//Representation of Entities for the Console
	class Entity {
	private:
		size_t index;
		size_t version;
		size_t stgindex;
		size_t value;
		size_t flags{ 0 };

		enum {
			modified = 1,
			deleted = 2
		};

		std::list<Component> components;
		Archetype* archetype{ nullptr };

	public:
		Entity(size_t index = 0, size_t version = 0, size_t stgindex = 0, size_t value = 0) : index(index), version(version), stgindex(stgindex), value(value) {}
		Entity(Entity const& org) {
			index = org.index;
			version = org.version;
			stgindex = org.stgindex;
			value = org.value;
			components = org.components;
		}
		Entity& operator=(Entity const& org) {
			Clear();
			index = org.index;
			version = org.version;
			stgindex = org.stgindex;
			value = org.value;
			components = org.components;
			return *this;
		}

		size_t GetIndex() { return index; }
		size_t GetVersion() { return version; }
		size_t GetStorageIndex() { return stgindex; }
		size_t GetValue() { return value; }

		void SetArchetype(Archetype* a = nullptr) { archetype = a; }
		Archetype* GetArchetype() const { return archetype; }

		void Clear() {
			index = version = stgindex = value = 0;
			components.clear();
		}

		std::list<Component>& GetComponents() {
			return components;
		}

		int AddComponent(Component& c) {
			components.push_back(c);
			return 0;
		}

		void SetModified() {
			flags |= modified; //set modified bit in flags
		}

		bool IsModified() {
			return flags & modified; // return wether modified bit is set in flags
		}


		void SetDeleted() {
			flags |= deleted; //set deleted bit in flags
		}

		bool IsDeleted() {
			return flags & deleted; // return wether deleted bit is set in flags
		}


		std::string ToString() {
			return std::to_string(index);
		}

	};

} // namespace Console
