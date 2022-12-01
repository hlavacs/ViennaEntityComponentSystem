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
	template<typename TYPELIST>
	class VECSSystem {
		template<typename TL> friend class VECSIterator;

	public:
		static_assert(vtll::unique<TYPELIST>::value, "VECSSystem TYPELIST is not unique!");

		static const int BITS = vtll::size<TYPELIST>::value;	//Number of bits in the bitset, i.e. number of component types
		static const uint32_t null_idx = std::numeric_limits<uint32_t>::max(); //Max number is defined to be NULL - no value


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

		class VECSGroup;

		struct VECSEntity {
			VECSGroup*  m_group;			//The group this entity belongs to
			uint32_t	m_indices_start;	//Start index for generation counter and component indices for this entity in the group
			uint32_t	m_generation;		//The generation counter of this entity
		};


		//-----------------------------------------------------------------------------------------------------------------
		//Component containers

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
			VECSComponentContainer(VECSSystem<TYPELIST>& system, uint32_t number) : m_system{ system }, VECSComponentContainerBase(number) {
				m_data.reserve(100);
			};

			/// <summary>
			/// Add a new component to the container.
			/// </summary>
			/// <param name="index">Pointer to the index of this component.</param>
			/// <param name="data">LReference to the component data.</param>
			/// <returns>The entry of the component.</returns>
			auto add(uint32_t entity, T& data) -> uint32_t {
				m_data.emplace_back(entity, data );
				return (uint32_t)m_data.size() - 1;
			}

			/// <summary>
			/// Add a new component to the container.
			/// </summary>
			/// <param name="index">Pointer to the index of this component.</param>
			/// <param name="data">RReference to the component data.</param>
			/// <returns>The entry of the component.</returns>
			auto add(uint32_t entity, T&& data) -> uint32_t {
				m_data.emplace_back(entity, std::move(data));
				return (uint32_t)m_data.size() - 1;
			}

			/// <summary>
			/// Get a reference to a component entry.
			/// </summary>
			/// <param name="index">Index in the container.</param>
			/// <returns>Reference to the compoennt.</returns>
			auto entry( uint32_t index ) -> entry_t& {
				return m_data[index];
			}

			auto handle(uint32_t index) -> entry_t& {
				return system->handle( m_data[index] );
			}

			/// <summary>
			/// Get access to a component container
			/// </summary>
			/// <returns>Reference to a component container.</returns>
			auto data() -> std::vector<entry_t>& {
				return m_data;
			}

			/// <summary>
			/// Erase a component. If this is not the last component, then swap it with the last 
			/// component, so we fill up the hole that is made. This also entails changing the
			/// index in the VECSGroup that points to this component.
			/// </summary>
			/// <param name="index">Index of the component to erase.</param>
			void erase(uint32_t index, std::vector<VECSEntity>& entities) override;

		private:
			VECSSystem<TYPELIST>&	m_system;	//Vector holding entity data
			std::vector<entry_t>	m_data;		//The data stored in this container
		};

		template<typename T>
		using entry_t = VECSComponentContainer<T>::entry_t;

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

			template<typename TYPELIST> friend class VECSSystem;

		public:

			/// <summary>
			/// Group class constructor. Go through the bitset and calculate the number of types
			/// this group entities store. Add 1 for the index of the owner entity. Then reserve enough space
			/// so that the vector does not have to be reallocated too often.
			/// </summary>
			/// <param name="types">Bitset representing the group type</param>
			/// <param name="container">Vector with pointers to the containers this group needs</param>
			VECSGroup(std::bitset<BITS> types, VECSSystem<TYPELIST>& system ) : m_num_indices{ 0 }, m_types{ types }, m_system{ system } {
				m_component_index_map.reserve(BITS);
				for (uint32_t i = 0; i < BITS; ++i) {
					if (types.test(i)) { 
						m_component_index_map.push_back(m_num_indices++);	//Map from bit in bitset (0...BITS-1) to relative component index (0...m_num_indices-1)
						m_container_map.emplace_back(m_system.m_container[i]);	//Map from index index to pointer to container (for erasing)
					}
					else { 
						m_component_index_map.push_back(null_idx); //Do not need this component
					}
				}
				m_num_indices++;	//One more index for entity. If this is the null_idx then the slot is empty
				m_indices.reserve(m_num_indices * 100);
			}

			template<typename... Ts>
			auto add(uint32_t entity, Ts&&... Args) -> uint32_t {
				uint32_t indices_start = (uint32_t)m_indices.size();	//Need a new slot at end of the vector
				m_indices.resize(indices_start + m_num_indices, 0);		//Create new space in the vector for the indices, fill with 0

				/// <summary>
				/// This lambda goes through all arguments, and adds them to the respective container they belong into.
				/// The returned indices are stored in the entity's index list.
				/// </summary>
				auto createComponents = [&]<typename T>(T&& Arg) {
					static const int idx = vtll::index_of<TYPELIST, T>::value;
					VECSComponentContainer<T>& container = *std::dynamic_pointer_cast<VECSComponentContainer<T>>(m_system.m_container[idx]); 
					m_indices[indices_start + m_component_index_map[idx]] = container.add(entity, std::forward<T>(Arg));
				};
				(createComponents.template operator() < Ts > (std::forward<Ts>(Args)), ...);

				m_indices[indices_start + m_num_indices - 1] = entity; //Last index is the index of the entity owning these components
				++m_size;
				return indices_start;
			}

			void erase(uint32_t indices_start, std::vector<VECSEntity>& entities) {
				if(indices_start >= m_size * m_num_indices) return;

				for (uint32_t i = 0; i < m_num_indices - 1; ++i) {	//Erase the components from the containers
					m_container_map[i]->erase(m_indices[indices_start + i], entities); 
				}

				if (indices_start + m_num_indices < m_indices.size() - 1) {			//Last slot? No - move the last slot to fill the gap
					uint32_t last = (uint32_t)m_indices.size() - m_num_indices;		//Index start of last entity
					uint32_t entity = m_indices[last + m_num_indices - 1];			//This entity is moved to fill the gap
					for (uint32_t i = 0; i < m_num_indices; ++i) { m_indices[indices_start + i] = m_indices[last + i]; }
					for (uint32_t i = 0; i < m_num_indices; ++i) { m_indices.pop_back(); }
					entities[entity].m_indices_start = indices_start;
				}
				--m_size;
			}

			template<typename T>
			auto index(uint32_t indices_start) -> uint32_t& {
				return m_indices[indices_start + m_component_index_map[vtll::index_of<TYPELIST, T>::value] ];
			}

			auto handle(uint32_t indices_start) -> VECSHandle {
				return m_system.handle( m_indices[indices_start + m_num_indices - 1] );
			}

			auto size() -> size_t {
				return m_size;
			}

		private:
			size_t					m_size{0};
			uint32_t				m_num_indices{ 0 };		//Number of types of this group, plus 1 for the generation counter
			std::bitset<BITS>		m_types;				//One bit for each type in the type list
			std::vector<uint32_t>	m_component_index_map;	//Map type index in type list to index in index list
			VECSSystem<TYPELIST>&	m_system;				//Reference to the vector of entities in the system
			container_vector		m_container_map;		//Maps index list (0...) to container pointer
			std::vector<uint32_t>	m_indices;				//Component type indices + owning entity index
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
		[[nodiscard]] auto create(Ts&&... Args) -> VECSHandle {
			static_assert(vtll::unique<vtll::tl<Ts...>>::value, "VECSSystem::create() arguments are not unique!"); //Check uniqueness

			std::bitset<BITS> types;
			
			/// <summary>
			/// This lambda goes through all types for the entity, creates a bitset (IDing the group) representing this entity,
			/// and makes sure all component containers exist.
			/// </summary>
			auto groupType = [&]<typename T>() {
				static const int idx = vtll::index_of<TYPELIST, T>::value;
				if (!m_container[idx]) {	//Is the container pointer for this component type still zero?
					auto container = std::make_shared<VECSComponentContainer<T>>(*this, idx);		//No - create one
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
	
			m_entities[entity_index].m_indices_start = m_entities[entity_index].m_group->add(entity_index, std::forward<Ts>(Args)...); //Add entity to group
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
			m_empty_start = handle.m_entity;
			return true;
		}

		/// <summary>
		/// Get a reference to the component of type T of an entity.
		/// </summary>
		/// <typeparam name="T">Component type.</typeparam>
		/// <param name="handle">Entity handle.</param>
		/// <returns>Reference to the component of this entity.</returns>
		template<typename T>
		auto component(VECSHandle handle) -> std::optional<std::reference_wrapper<T>> {
			VECSEntity& entity = m_entities[handle.m_entity];
			if (entity.m_generation != handle.m_generation) return {};		//Is the handle still valid?
			if (!entity.m_group->m_types.test(vtll::index_of<TYPELIST, T>::value)) return {}; //Does the entity contain the type?
			auto base = m_container[vtll::index_of<TYPELIST, T>::value];	//Pointer to the container
			auto container = std::dynamic_pointer_cast<VECSComponentContainer<T>>(base);
			return { container->entry(entity.m_group->index<T>(entity.m_indices_start)).m_component };
		}

		template<typename T>
		auto const_component(VECSHandle handle) -> std::optional<std::reference_wrapper<const T>> {
			auto res = component(handle);
			if (!res) return {};
			return { res.value() };
		}

		template<typename T>
		auto container(VECSHandle handle) -> std::optional<std::reference_wrapper<std::vector<entry_t<T>>>> {
			static const int idx = vtll::index_of<TYPELIST, T>::value;
			if (!m_entities[handle.m_entity].m_group->m_types.test(idx)) return {}; //Does the entity contain the type?
			if (!m_container[idx]) {	//Is the container pointer for this component type still zero?
				auto container = std::make_shared<VECSComponentContainer<T>>(*this, idx);		//No - create one
				m_container[idx] = std::static_pointer_cast<VECSComponentContainerBase>(container); //Save in vector
			}
			auto container = std::dynamic_pointer_cast<VECSComponentContainer<T>>(m_container[idx]);
			return container->data();
		}

		template<typename T>
		auto const_container(VECSHandle handle) -> std::optional<std::reference_wrapper<const std::vector<entry_t<T>>>> {
			auto res = container(handle);
			if (!res) return {};
			return { res.value() };
		}

		auto handle(uint32_t entity) -> VECSHandle {
			return { entity, m_entities[entity].m_generation };
		}

		bool valid(VECSHandle handle) {
			return m_entities[handle.m_entity].m_generation == handle.m_generation;
		}

	private:

		auto getGroup( std::bitset<BITS>& types) -> VECSGroup* {
			if (!m_groups.contains(types)) {									//Does the group exist yet?
				auto res = m_groups.emplace(types, VECSGroup{ types, *this } );	//Create it
				return &res.first->second;										//Return its address
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
	template<typename TL>
	template<typename T>
	inline void VECSSystem<TL>::VECSComponentContainer<T>::erase(uint32_t index, std::vector<VECSEntity>& entities) {
		if (m_data.size() - 1 > index) {
			std::swap(m_data[index], m_data[m_data.size() - 1]);
			entry_t& entry = m_data[index];
			auto& entity = entities[entry.m_entity];	//Reference to the entity that owns this component
			entity.m_group->m_indices[entity.m_indices_start + entity.m_group->m_component_index_map[this->m_number]] = index; //Reset index
		}
		m_data.pop_back();	//Remove last element, which is the component to be removed
	}


	//-----------------------------------------------------------------------------------------------------------------
	//Iterator

	template<typename TL>
	class VECSIterator {
		using group_ptr = VECSSystem<TL>::VECSGroup*;

	public:
		using difference_type = int64_t;						// Difference type
		using value_type = VECSSystem<TL>::VECSHandle;			// Value type - use refs to deal with atomics and unique_ptr
		using pointer = value_type*;							// Pointer to value
		using reference = value_type&;							// Reference type
		using iterator_category = std::forward_iterator_tag;	// Forward iterator

		VECSIterator(group_ptr group) noexcept : m_group{ group } {}
		VECSIterator(const VECSIterator<TL>& v) noexcept = default;	// Copy constructor
		VECSIterator(VECSIterator<TL>&& v) noexcept = default;		// Move constructor
		auto operator=(const VECSIterator<TL>& v) noexcept	-> VECSIterator<TL> & = default;	// Copy
		auto operator=(VECSIterator<TL>&& v) noexcept		-> VECSIterator<TL> & = default;	// Move

		auto operator*() noexcept -> value_type { return m_group->handle(m_indices_start); }	// Access the data
		auto operator*() const noexcept	-> value_type {	return m_group->handle(m_indices_start); }	// Access const data
		auto operator->() noexcept { return operator*(); };			// Access
		auto operator->() const noexcept { return operator*(); };	// Access

		auto operator++() noexcept -> VECSIterator<TL>& { m_indices_start += m_group->m_num_indices; }	// Increase by 1
		auto operator++(int) noexcept -> VECSIterator<TL> { m_indices_start += m_group->m_num_indices; } // Increase by 1
		auto operator+=(difference_type N) noexcept	-> VECSIterator<TL>& { 	// Increase by N
			m_indices_start = std::clamp( m_indices_start + N * m_group->m_num_indices, 0, m_group->m_num_indices * m_group->m_size );
		}
		auto operator+(difference_type N) noexcept	-> VECSIterator<TL> { 	// Increase by N
			m_indices_start = std::clamp( m_indices_start + N * m_group->m_num_indices, 0, m_group->m_num_indices * m_group->m_size);
		}

		bool operator!=(const VECSIterator<TL>& i) noexcept {	///< Unequal
			return m_group != i.m_group || m_indices_start != i.m_indices_start;
		}

		friend auto operator==(const VECSIterator<TL>& i1, const VECSIterator<TL>& i2) noexcept -> bool {	///< Equal
			return i1.m_group == i2.m_group && i1.m_indices_start == i2.m_indices_start;
		}

	private:
		group_ptr m_group;
		uint32_t  m_indices_start{ 0 };
	};


	//-----------------------------------------------------------------------------------------------------------------
	//Ranges

	template<typename TL>
	class VECSRange {
		using group_ptr = VECSSystem<TL>::VECSGroup*;
		std::vector<group_ptr> m_groups;

	public:
		VECSRange() {}

	};




}