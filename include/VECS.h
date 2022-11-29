#pragma once

#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <bitset> 
#include <optional> 
#include "VTLL.h"

namespace std {

	template<typename T>
	inline size_t hash_combine(const size_t&& seed, T& v) {		//For combining hashes
		return seed ^ std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
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

	/// <summary>
	/// 
	/// </summary>
	/// <typeparam name="TYPELIST"></typeparam>
	/// <typeparam name="SIZETYPEMAP"></typeparam>
	/// <typeparam name="SIZEENTITIES"></typeparam>
	template<typename TYPELIST, uint32_t SIZEENTITIES = 500, typename SIZETYPEMAP = vtll::vl<>>
	class VECSSystem {

	public:

		static const int BITS = vtll::size<TYPELIST>::value;
		static const uint32_t null_idx = std::numeric_limits<uint32_t>::max();

		/// <summary>
		/// 
		/// </summary>
		struct VECSComponentContainerBase {};

		/// <summary>
		/// 
		/// </summary>
		/// <typeparam name="T"></typeparam>
		template<typename T>
		class VECSComponentContainer : VECSComponentContainerBase {

		public:
			using SIZES = vtll::map<SIZETYPEMAP, T, vtll::vl<100>>;
			static const uint32_t SIZESLOTS = vtll::front<SIZES>::value;

			struct entry_t {
				uint32_t* m_index{ nullptr };	//Pointer to the index pointing to this component.
				T		  m_component{};
			};

			VECSComponentContainer() : VECSComponentContainerBase() { m_data.reserve(SIZESLOTS); };

			entry_t& add(uint32_t* index, T& data) {
				return m_data.emplace_back( index, data );
			}

			entry_t& add( uint32_t *index, T&& data) {
				return m_data.emplace_back( index, data );
			}

			entry_t& component( uint32_t index ) {
				return m_data[index];
			}

			void erase( uint32_t index) {
				if (m_data.size() - 1 > index) {
					std::swap(m_data[index], m_data[m_data.size() - 1]);
					*m_data[index].m_index = index;
				}
				m_data.pop_back();
			}

		private:
			std::vector<entry_t> m_data;	//The data stored in this container
		};

		/// <summary>
		/// 
		/// </summary>
		class VECSGroup {

		public:
			VECSGroup(std::bitset<BITS> types) {
				m_component_index_map.reserve(vtll::size<TYPELIST>::value);
				uint32_t ones = 0;
				for (uint32_t i = 0; i < vtll::size<TYPELIST>::value; ++i) {
					if (types.test(i)) { m_component_index_map[i] = ones++; }
					else { m_component_index_map[i] = null_idx; }
				}
				m_num_indices = ones + 1;	//Component type indices + generation counter
				m_indices.reserve(m_num_indices * SIZEENTITIES);
			}

			uint32_t add() {
				if (m_empty_start != null_idx) {	//There are empty slots
					uint32_t next = m_empty_start;	//Reuse the first one found
					m_empty_start = m_indices[m_empty_start];	//New start of empty chain
					return next;
				}
				uint32_t entity_start = m_indices.size();
				m_indices.resize(entity_start + m_num_indices, 0);
				return entity_start;
			}

			void erase(uint32_t entity_indices_start) {
				m_indices[entity_indices_start + m_num_indices - 1]++;		//Increase generation counter
				m_indices[entity_indices_start] = m_empty_start;			//Relink this slot into the empty chain
				m_empty_start = entity_indices_start;						//Reclaim empty entity later
			}

			template<typename T>
			uint32_t& index(uint32_t entity_indices_start) {
				return m_indices[entity_indices_start + m_component_index_map[vtll::index_of<TYPELIST, T>::value] ];
			}

			uint32_t generation(uint32_t entity_indices_start) {
				return m_indices[entity_indices_start + m_num_indices - 1];
			}

		private:
			uint32_t				m_num_indices{ 0 };			//Number of types of this group, plus 1 for the generation counter
			std::bitset<BITS>		m_types;					//One bit for each type in the type list
			std::vector<uint32_t>	m_component_index_map;		//Map type index in type list to index in index list
			std::vector<uint32_t>	m_indices;					//Component type indices + generation counter
			uint32_t				m_empty_start{ null_idx };	//Singly linked list of empty slots to be reused later, ends with null_idx
		};


		/// <summary>
		/// 
		/// </summary>
		struct VECSHandle {
			VECSGroup&	m_group;				//The group this entity belongs to
			uint32_t	m_entity_indices_start;	//Start index for generation counter and component indices for this entity in the group
			uint32_t	m_generation;			//The generation counter of this entity
		};

		VECSSystem() {
			m_container.resize(vtll::size<TYPELIST>::value, nullptr);
		}

		template<typename... Ts>
		VECSHandle create(Ts&&... Args) {

			std::bitset<BITS> types;

			auto getGroup = [&]<typename T>() {
				static const int idx = vtll::index_of<TYPELIST, T>::value;
				types.set(idx);	//Fill the bits of bitset representing the types
				if( !m_container[idx] ) m_container[idx] = std::make_shared<VECSComponentContainerBase>();	//Make sure the containers exist
			};
			(getGroup.template operator() < Ts > (), ...);

			VECSGroup &group = m_groups[types];

			VECSHandle handle{group, group.add(), 0};
			handle.m_generation = group.generation(handle.m_entity_indices_start);

			auto createComponents = [&]<typename T>(T&& Arg) {
				m_container[vtll::index_of<TYPELIST, T>::value]->add( &group.index<T>(handle.m_entity_indices_start), Arg);
			};
			(createComponents.template operator() < Ts > (Args...), ...);

			return handle;
		}

		void erase(VECSHandle handle) {

		}

		template<typename T>
		std::optional<T&> component(VECSHandle handle) {

		}

	private:
		std::vector<std::shared_ptr<VECSComponentContainerBase>> m_container;			//Container for the components
		std::unordered_map<std::bitset<BITS>, VECSGroup>		 m_groups;				//Entity groups
		std::unordered_multimap<uint32_t, VECSGroup*>			 m_map_type_to_group;	//Map for going from a component type to groups of entities with them
	};

}