#pragma once

#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <bitset> 
#include "VTLL.h"

namespace std {

	inline void hash_combine(std::size_t& seed, size_t v) {		//For combining hashes
		seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	/*template<>
	struct hash<type_index_set_t> {
		std::size_t operator()(const type_index_set_t& cset) const {
			std::size_t seed = 0;
			for (auto& ct : cset) {
				hash_combine(seed, std::hash<std::type_index>()(ct));
			}
			return seed;
		};
	};*/
}


namespace vecs {


	template<typename TYPELIST>
	class VECSSystem {

	public:
		static const int BITS = vtll::size<TYPELIST>::value;

		struct VECSEntity;
		using VECSHandle = VECSEntity*;

		struct VECSComponentContainerBase {
		};

		template<typename T>
		struct VECSComponentContainer : VECSComponentContainerBase {
			std::unordered_map<VECSHandle,T> m_data;
		};

		struct VECSGroup {
			std::unordered_set<VECSHandle> m_group_entities;
		};

		struct VECSEntity {
			std::bitset<BITS> m_types;
		};

	private:
		std::unordered_map<int, VECSComponentContainerBase> m_container;
		std::unordered_set<VECSHandle>						m_entities;

		std::unordered_map<std::bitset<BITS>, VECSGroup> m_groups;
		std::unordered_multimap<int, std::bitset<BITS>> m_group_index;

	public:

		~VECSSystem() {
			for (auto& entity : m_entities) delete entity;
		}

		template<typename... Ts>
		VECSHandle createEntity() {
			VECSHandle entity = new VECSEntity();
			m_entities.insert(entity);

			auto f = [&]<typename T>() {
				static const int idx = vtll::index_of<T, TYPELIST>::value;
				entity->m_types.set(idx);
				VECSComponentContainer<T>& container = m_container[idx];
				container.m_data[entity];
			};

			(f.template operator() < Ts > (), ...);

			bool contains_group = m_groups.contains(entity->m_types);
			VECSGroup &group = m_groups[entity->m_types];
			group.m_group_entities.insert(entity);

			if (!contains_group) {
				auto g = [&]<typename T>() {
					static const int idx = vtll::index_of<T, TYPELIST>::value;
					m_group_index.insert( std::pair(idx, entity->m_types) );
				};

				(g.template operator() < Ts > (), ...);
			}
			return entity;
		}

		void eraseEntity(VECSHandle handle) {

		}

		template<typename T>
		std::shared_ptr<T> component(VECSHandle handle) {
			std::shared_ptr<VECSComponentContainer<T>> container = m_container[std::type_index(type_id(T))];
			if (!container) container = std::make_shared<VECSComponentContainer<T>>;

			if (container->m_data.count(handle) == 0) return {};

			return container->m_data[handle];
		}

		template<typename T>
		std::shared_ptr<T> removeComponent(VECSHandle handle) {
			if (m_container.count(std::type_index(type_id(T))) > 0) {
				VECSComponentContainer<T> container = m_container[std::type_index(type_id(T))];
				container->m_data.erase(handle);
			}
		}
	};

}