#pragma once

#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <bitset> 
#include <optional> 
#include "VTLL.h"


namespace vecs {

	/// <summary>
	/// The main ECS class.
	/// </summary>
	/// <typeparam name="TYPELIST">A typelist storing all possible component types.</typeparam>
	/// <typeparam name="SIZETYPEMAP">Number of entities the group storage should be initialized with.</typeparam>
	/// <typeparam name="SIZEENTITIES">A typelist map, mapping component type to initialized component storage.</typeparam>
	template<typename TYPELIST, uint32_t SIZEENTITIES = 500, typename SIZETYPEMAP = vtll::vl<>>
	class VECSSystem {

	public:

		static const int BITS = vtll::size<TYPELIST>::value;	//Number of bits in the bitset, i.e. number of component types
		static const uint32_t null_idx = std::numeric_limits<uint32_t>::max(); //Max number is defined to be NULL - no value

		//-----------------------------------------------------------------------------------------------------------------
		//Component containers

		class VECSGroup;
		class VECSEntity;

		/// <summary>
		/// Base class for all containers.
		/// </summary>
		class VECSComponentContainerBase {
		protected:
			uint32_t m_number;	//Number in the list of containers. Needed for erasing.
		public:
			VECSComponentContainerBase(uint32_t number) : m_number{ number } {};
			virtual void erase(uint32_t index, std::vector<VECSEntity>& entities) {};	//Erase a component
		};

		using container_vector = std::vector<std::shared_ptr<VECSComponentContainerBase>>;

		/// <summary>
		/// Container for type T.
		/// </summary>
		/// <typeparam name="T">Type of the component to store in.</typeparam>
		template<typename T>
		class VECSComponentContainer : public VECSComponentContainerBase {

		public:

			/// <summary>
			/// This container stores a vector of this struct.
			/// </summary>
			struct entry_t {
				uint32_t	m_entity;		//Index into entity container IDing the entity this component belongs to
				T			m_component{};	//The component data.
			};

			/// <summary>
			/// Component container constructor. Reserves up front memory for the components.
			/// </summary>
			/// <param name="number">Number in the global component pointer vector pointing to this container.</param>
			VECSComponentContainer(uint32_t number) : VECSComponentContainerBase(number) {
				m_data.reserve(vtll::front_value < vtll::map<SIZETYPEMAP, T, vtll::vl<100>> >::value );
			};

			/// <summary>
			/// Add a new component to the container.
			/// </summary>
			/// <param name="index">Pointer to the index of this component.</param>
			/// <param name="data">LReference to the component data.</param>
			/// <returns>The entry of the component.</returns>
			uint32_t add(uint32_t entity, T& data) {
				m_data.emplace_back(entity, data );
				return (uint32_t)m_data.size() - 1;
			}

			/// <summary>
			/// Add a new component to the container.
			/// </summary>
			/// <param name="index">Pointer to the index of this component.</param>
			/// <param name="data">RReference to the component data.</param>
			/// <returns>The entry of the component.</returns>
			uint32_t add(uint32_t entity, T&& data) {
				m_data.emplace_back(entity, std::move(data));
				return (uint32_t)m_data.size() - 1;
			}

			/// <summary>
			/// Get a reference to a component entry.
			/// </summary>
			/// <param name="index">Index in the container.</param>
			/// <returns>Reference to the compoennt.</returns>
			entry_t& entry( uint32_t index ) {
				return m_data[index];
			}

			/// <summary>
			/// Erase a component. If this is not the last component, then swap it with the last 
			/// component, so we fill up the hole that is made. This also entails changing the
			/// index in the VECSGroup that points to this component.
			/// </summary>
			/// <param name="index">Index of the component to erase.</param>
			void erase(uint32_t index, std::vector<VECSEntity>& entities);

		private:
			std::vector<entry_t> m_data;	//The data stored in this container
		};


		//-----------------------------------------------------------------------------------------------------------------
		//Entity groups


		/// <summary>
		/// A group holds a number of entities of same type. An entity is just a list of components.
		/// Here we store indices into component containers. Since each entity needs the same number,
		/// we simply store all of them in sequence in a bit vector. Each entity gets the same number
		/// of indices.
		/// Additionally we also store the index of the owning entity.
		/// </summary>
		class VECSGroup {

			template<typename TYPELIST, uint32_t SIZEENTITIES, typename SIZETYPEMAP>
			friend class VECSSystem;

		public:

			/// <summary>
			/// Group class constructor. Go through the bitset and calculate the number of types
			/// this group entities store. Add 1 for the index of the owner entity. Then reserve enough space
			/// so that the vector does not have to be reallocated too often.
			/// </summary>
			/// <param name="types">Bitset representing the group type</param>
			/// <param name="container">Vector with pointers to the containers this group needs</param>
			VECSGroup(std::bitset<BITS> types, container_vector& container) : m_num_indices{0}, m_types { types }, m_container(container) {
				m_component_index_map.reserve(BITS);
				for (uint32_t i = 0; i < BITS; ++i) {
					if (types.test(i)) { 
						m_component_index_map[i] = m_num_indices++;	//Map from bit in bitset (0...BITS-1) to relative component index (0...m_num_indices-1)
						m_container_map.emplace_back(container[i]);	//Map from index index to pointer to container (for erasing)
					}
					else { 
						m_component_index_map[i] = null_idx; //Do not need this component
					}
				}
				m_indices.reserve(m_num_indices * SIZEENTITIES);
			}

			template<typename... Ts>
			uint32_t add(uint32_t entity, Ts&&... Args) {
				uint32_t indices_start;
				if (m_empty_start != null_idx) {				//Are there empty slots
					indices_start = m_empty_start;				//Reuse the first one found
					m_empty_start = m_indices[m_empty_start];	//New start of empty chain
				}
				else {
					indices_start = (uint32_t)m_indices.size();			//Need a new slot at end of the vector
					m_indices.resize(indices_start + m_num_indices, 0);	//Create new space in the vector for the indices, fill with 0
				}

				/// <summary>
				/// This lambda goes through all arguments, and adds them to the respective container they belong into.
				/// The returned indices are stored in the entity's index list.
				/// </summary>
				auto createComponents = [&]<typename T>(T&& Arg) {
					static const int idx = vtll::index_of<TYPELIST, T>::value;
					VECSComponentContainer<T>& container = *std::dynamic_pointer_cast<VECSComponentContainer<T>>(m_container[idx]); 
					m_indices[m_component_index_map[idx]] = container.add(entity, std::forward<T>(Arg));
				};
				(createComponents.template operator() < Ts > (std::forward<Ts>(Args)), ...);

				return indices_start;
			}

			void erase(uint32_t entity_indices_start, std::vector<VECSEntity>& entities) {
				for (uint32_t i = 0; i < m_num_indices - 1; ++i) { m_container_map[i]->erase(m_indices[entity_indices_start + i], entities); }
				m_indices[entity_indices_start] = m_empty_start;	//Relink this slot into the empty chain
				m_empty_start = entity_indices_start;				//Reclaim empty entity later
			}

			template<typename T>
			uint32_t& index(uint32_t entity_indices_start) {
				return m_indices[entity_indices_start + m_component_index_map[vtll::index_of<TYPELIST, T>::value] ];
			}

		private:
			uint32_t				m_num_indices{ 0 };			//Number of types of this group, plus 1 for the generation counter
			std::bitset<BITS>		m_types;					//One bit for each type in the type list
			std::vector<uint32_t>	m_component_index_map;		//Map type index in type list to index in index list
			container_vector&		m_container;				//Container for the components (needed when erasing an entity)
			container_vector		m_container_map;			//Maps index list (0...) to container pointer
			std::vector<uint32_t>	m_indices;					//Component type indices + owning entity index
			uint32_t				m_empty_start{ null_idx };	//Singly linked list of empty slots to be reused later, ends with null_idx
		};


		//-----------------------------------------------------------------------------------------------------------------
		//Entity handle

		/// <summary>
		/// An entity handle can be used to access an entity.
		/// </summary>
		struct VECSHandle {
			uint32_t	m_entity;			//The index into the entity vector
			uint32_t	m_generation;		//The generation counter of this entity
		};


		//-----------------------------------------------------------------------------------------------------------------
		//Entity

		struct VECSEntity {
			VECSGroup*	m_group;			//The group this entity belongs to
			uint32_t	m_indices_start;	//Start index for generation counter and component indices for this entity in the group
			uint32_t	m_generation;		//The generation counter of this entity
		};


		//-----------------------------------------------------------------------------------------------------------------
		//The VECS main member functions and data

		/// <summary>
		/// VECS main class constructor. Creates empty shared ptrs for the containers.
		/// </summary>
		VECSSystem() {
			m_container.resize(BITS, nullptr); //We have enough space for the shared pointers, but containers do not exist yet
		}

		/// <summary>
		/// Create a new entity.
		/// </summary>
		/// <typeparam name="...Ts">Types of the components of this entity.</typeparam>
		/// <param name="...Args">Component data to be copied/moved into the component containers.</param>
		/// <returns>The entity handle.</returns>
		template<typename... Ts>
		VECSHandle create(Ts&&... Args) {

			std::bitset<BITS> types;

			/// <summary>
			/// This lambda goes through all types for the entity, creates a bitset (IDing the group) representing this entity,
			/// and makes sure all component containers exist.
			/// </summary>
			auto groupType = [&]<typename T>() {
				static const int idx = vtll::index_of<TYPELIST, T>::value;
				if (!m_container[idx]) {	//Is the container pointer for this component type still zero?
					auto container = std::make_shared<VECSComponentContainer<T>>(idx);						//No - create one
					m_container[idx] = std::static_pointer_cast<VECSComponentContainerBase>(container); //Save in vector
				}
				types.set(vtll::index_of<TYPELIST, T>::value);	//Fill the bits of bitset representing the types
			};
			(groupType.template operator() < Ts > (), ...); //Create bitset representing the group and create necessary containers

			//Create the entity, or reuse an old one.
			uint32_t entity_index;
			if (m_empty_start != null_idx) {		//Reuse an old entity that has been erased previously
				entity_index = m_empty_start;
				m_empty_start = m_entities[m_empty_start].m_indices_start;
				m_entities[entity_index].m_group = getGroup(types);	//Remember the group 
			}
			else {
				entity_index = (uint32_t)m_entities.size();		//create a new entity
				m_entities.emplace_back(getGroup(types), entity_index, 0);
			}

			m_entities[entity_index].m_indices_start = m_entities[entity_index].m_group->add(std::forward<Ts>(Args)...); //Add entity to group
			
			return { entity_index, m_entities[entity_index].m_generation }; //Return the handle
		}

		/// <summary>
		/// Erase an entity by its handle.
		/// </summary>
		/// <param name="handle">The handle of the entity to be erased.</param>
		bool erase(VECSHandle handle) {
			VECSEntity& entity = m_entities[handle.m_entity];
			if (entity.m_generation != handle.m_generation) return false;	//Is the handle still valid?
			entity.m_generation++;											//Invalidate all handles to this entity
			entity.m_group->erase(entity.m_indices_start, m_entities);		//Erase from group, and also the components
			entity.m_indices_start = m_empty_start;							//Include into empty list
			m_empty_start = entity;
			true;
		}

		/// <summary>
		/// Get a reference to the component of type T of an entity.
		/// </summary>
		/// <typeparam name="T">Component type.</typeparam>
		/// <param name="handle">Entity handle.</param>
		/// <returns>Reference to the component of this entity.</returns>
		template<typename T>
		std::optional<T&> component(VECSHandle handle) {
			VECSEntity& entity = m_entities[handle.m_entity];
			if (entity.m_generation != handle.m_generation) return;	//Is the handle still valid?
			VECSComponentContainer<T>& container = m_container[vtll::index_of<TYPELIST, T>::value];			//Pointer to the container
			return { container->entry(entity.m_group->index<T>(entity.m_indices_start)).m_component };
		}

		bool valid(VECSHandle handle) {
			return m_entities[handle.m_entity].m_generation == handle.m_generation;
		}

	private:

		VECSGroup* getGroup( std::bitset<BITS>& types) {
			if (!m_groups.contains(types)) {											//Does the group exist yet?
				auto res = m_groups.emplace(types, VECSGroup{ types, m_container } );	//Create it
				return &res.first->second;												//Return its address
			}
			return &m_groups.at(types);		//It exists - retrun its address
		}

		std::vector<VECSEntity>								m_entities;				//Container for all entities
		uint32_t											m_empty_start{null_idx};//Index of first empty entity to reuse
		container_vector									m_container;			//Container for the components
		std::unordered_map<std::bitset<BITS>, VECSGroup>	m_groups;				//Entity groups
		std::unordered_multimap<uint32_t, VECSGroup*>		m_map_type_to_group;	//Map for going from a component type to groups of entities with them
	};

	//-----------------------------------------------------------------------------------------------------------------
	//Leftover, this function needs to know groups and entities

	/// <summary>
	/// Erase a component. If this is not the last component, then swap it with the last 
	/// component, so we fill up the hole that is made. This also entails changing the
	/// index in the VECSGroup that points to this component.
	/// </summary>
	/// <param name="index">Index of the component to erase.</param>
	template<typename P, uint32_t S, typename M>
	template<typename T>
	inline void VECSSystem<P,S,M>::VECSComponentContainer<T>::erase(uint32_t index, std::vector<VECSEntity>& entities) {
		if (m_data.size() - 1 > index) {
			std::swap(m_data[index], m_data[m_data.size() - 1]);
			entry_t& entry = m_data[index];
			auto& entity = entities[entry.m_entity];	//Reference to the entity that owns this component
			entity.m_group->m_indices[entity.m_indices_start + entity.m_group->m_component_index_map[this->m_number]] = index; //Reset index
		}
		m_data.pop_back();	//Remove last element, which is the component to be removed
	}

}