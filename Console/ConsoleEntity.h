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
		/// @brief get entity index
		size_t GetIndex() { return index; }

		/// @brief get entity version
		size_t GetVersion() { return version; }

		/// @brief get entity storage index
		size_t GetStorageIndex() { return stgindex; }

		/// @brief get entity value, the handle of the entity
		size_t GetValue() { return value; }

		/// @brief set the archetype of the entity
		/// @param a archetype pointer or default nullptr
		void SetArchetype(Archetype* a = nullptr) { archetype = a; }

		/// @brief get the archetype of the entity
		/// @return archetype or nullptr
		Archetype* GetArchetype() const { return archetype; }

		/// @brief clear entity data
		void Clear() {
			index = version = stgindex = value = 0;
			components.clear();
		}

		/// @brief get the components of the entity
		/// @return list of components
		std::list<Component>& GetComponents() {
			return components;
		}

		/// @brief add a component to the entity
		/// @param c component to be added
		void AddComponent(Component& c) {
			components.push_back(c);
		}

		//Live view utility

		/// @brief set modified bit in flags
		void SetModified() {
			flags |= modified; 
		}

		/// @brief return whether modified bit is set in flags
		bool IsModified() {
			return flags & modified; 
		}

		/// @brief set deleted bit in flags
		void SetDeleted() {
			flags |= deleted; 
		}

		/// @brief return whether deleted bit is set in flags
		bool IsDeleted() {
			return flags & deleted;
		}

		/// @brief return entity index as string
		std::string ToString() {
			return std::to_string(index);
		}

	};

} // namespace Console
