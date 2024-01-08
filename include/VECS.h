#pragma once

#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <bitset> 
#include <optional> 
#include <ranges>
#include <functional>
#include <tuple>
#include <algorithm>
#include <limits>

#include "VTLL.h"
#include "VSTY.h"
#include "VLLT.h"


namespace vecs {

	const size_t NBITS = 40;

	using vllt::stack_index_t;
	using generation_t = vsty::strong_type_t< uint64_t, vsty::counter<>, std::integral_constant<uint64_t, std::numeric_limits<uint64_t>::max()> >;

	/// <summary>
	/// A VecsIndex is a 64-bit integer, with the first NBITS bits encoding the index
	/// of the index, and the rest encoding a generation counter. The generation counter is incremented each time the index
	/// is erased, so that old indices can be detected.
	/// </summary>
	struct VecsIndex : public vsty::strong_type_t< uint64_t, vsty::counter<>, std::integral_constant<uint64_t, std::numeric_limits<uint64_t>::max()> > {
		stack_index_t get_index() const { return stack_index_t{ get_bits(0, NBITS) }; }
		generation_t get_generation() const { return generation_t{ get_bits(NBITS) }; }
		void set_index( stack_index_t index) { return set_bits( (uint32_t)index, 0, NBITS ); }
		void set_generation( stack_index_t index) { return set_bits( (uint32_t)index, NBITS ); }
	};

	template<auto Phantom = vsty::counter<>> struct handle_t : public VecsIndex{};
	using VecsHandle = handle_t<>; /// This is to prevent accidentally using a VecsIndex as a handle.


	/// <summary>
	/// The main ECS class.
	/// </summary>
	/// <typeparam name="TL">A typelist storing all possible component types. Types MUST be UNIQUE!</typeparam>
	/// <typeparam name="NBITS">Number of bits for the index in the 64-bit handle. The rest are used for generation.</typeparam>
	template<typename TL>
	class VecsSystem {
		//template<typename TL, typename T, typename... Ts> friend class VecsIterator;
		//template<typename TL, typename T, typename... Ts> friend class VecsRange;

		static_assert(vtll::unique<TL>::value, "VecsSystem types are not unique!");
		static const int BITSTL = vtll::size<TL>::value;	//Number of bits in the bitset, i.e. number of component types ingroup

		class VecsArchetype;

		/// <summary>
		/// An enity is a list of components stored in an archetype. The VecsHandle indexes into the entity map storing VecsEntity structs.
		/// A VecsEntity points to an archetype, and has a VecsIndex, which also holds the generation counter.
		/// </summary>
		struct VecsEntity {
			std::shared_ptr<VecsArchetype> m_archetype;	//The group this entity belongs to.
			VecsIndex m_index; //Index of the entity in the archetype. It is also used to point to the next free slot.
		};

	public:	

		VecsSystem(){ valid( VecsHandle{}); };
		auto erase(const VecsHandle&& handle) -> bool;
		auto valid(const VecsHandle&& handle) -> bool;

		template<typename... Ts>
		[[nodiscard]] auto create(Ts&&... Args) -> VecsHandle;

		template<typename... Ts> 
		[[nodiscard]] auto get(VecsHandle handle) -> std::optional<std::tuple<Ts&...>>;

	private:
		vllt::VlltStack< vtll::tl<VecsEntity>, 1<<10, false > m_entities; //Container for all entities
		VecsIndex m_empty_start{}; //Index of first empty entity to reuse
		std::unordered_map<std::bitset<BITSTL>, std::shared_ptr<VecsArchetype>> m_archetypes; //Entity groups
		std::unordered_multimap<size_t, std::shared_ptr<VecsArchetype>> m_map_type_to_group;	//Map for going from a component type to groups of entities with them
	};


	/// <summary>
	/// Create a new entity.
	/// </summary>
	/// <typeparam name="...Ts">Types of the components of this entity.</typeparam>
	/// <param name="...Args">Component data to be copied/moved into the component containers.</param>
	/// <returns>The entity handle.</returns>
	template<typename TL>
	template<typename... Ts>
	[[nodiscard]] auto VecsSystem<TL>::create(Ts&&... Args) -> VecsHandle {
		static_assert(vtll::unique<vtll::tl<Ts...>>::value, "VecsSystem::create() arguments are not unique!"); //Check uniqueness
		/*std::bitset<BITSGOTL> outtypes;
		
		/// <summary>
		/// This lambda goes through all types for the entity, creates a bitset (IDing the group) representing this entity,
		/// and makes sure all component containers exist.
		/// </summary>
		auto groupType = [&]<typename T>() {
			static const uint32_t idx = vtll::index_of<GOTL, T>::value;
			if (!m_container[idx]) {	//Is the container pointer for this component type still zero?
				auto container = std::make_shared<VecsComponentContainer<T>>(*this, idx);		//No - create one
				m_container[idx] = std::static_pointer_cast<VecsComponentContainerBase>(container); //Save in vector
			}
			outtypes.set(vtll::index_of<GOTL, T>::value);	//Fill the bits of bitset representing the types
		};
		(groupType.template operator() < Ts > (), ...); //Create bitset representing the group and create necessary containers
		//Create the entity, or reuse an old one.
		uint32_t entity_index;
		if (m_empty_start != null_idx) {		//Reuse an old entity that has been erased previously
			entity_index = m_empty_start;
			m_empty_start = m_entities[m_empty_start].m_index;
			m_entities[entity_index].m_group = getGroup<Ts...>(outtypes);	//Remember the group 
		}
		else {
			entity_index = (uint32_t)m_entities.size();		//create a new entity
			m_entities.emplace_back(getGroup<Ts...>(outtypes), entity_index, 0);
		}

		VecsHandle handle{ entity_index, m_entities[entity_index].m_generation };
		m_entities[entity_index].m_index = m_entities[entity_index].m_group->add(handle, std::forward<Ts>(Args)...); //Add entity to group
		return handle; //Return the handle
		*/
		return {};
	}


	/// <summary>
	/// Erase an entity by its handle.
	/// </summary>
	/// <param name="handle">The handle of the entity to be erased.</param>
	template<typename TL>
	auto VecsSystem<TL>::erase(const VecsHandle&& handle) -> bool {
		/*
		VecsEntity& entity = m_entities[handle.m_entity];
		if (entity.m_generation != handle.m_generation) return false;	//Is the handle still valid?
		entity.m_generation++;						//Invalidate all handles to this entity
		entity.m_group->erase(entity.m_index);		//Erase from group, and also the components
		entity.m_index = m_empty_start;				//Include into empty list
		m_empty_start = handle.m_entity;
		return true;
		*/
	}


	/// <summary>
	/// Get a reference to the component of type T of an entity.
	/// </summary>
	/// <typeparam name="Ts">Component types.</typeparam>
	/// <param name="handle">Entity handle.</param>
	/// <returns>Tuple with references to the components of the entity.</returns>
	template<typename TL>
	template<typename... Ts>
	auto VecsSystem<TL>::get(VecsHandle handle) -> ::std::optional<std::tuple<Ts&...>> {
		/*
		VecsEntity& entity = m_entities[handle.m_entity];
		if (entity.m_generation != handle.m_generation) return {};		//Is the handle still valid?
		auto check = [&]<typename T>() {
			if (!entity.m_group->m_gl_types.test(vtll::index_of<GOTL, T>::value)) return false; //Does the entity contain the type?
			return true;
		};
		if (!(check.template operator() < Ts > () && ...)) return {};
		return { getTuple<Ts...>(entity.m_group, entity.m_index) };
		*/
	}



	template<typename TL>
	auto VecsSystem<TL>::valid(const VecsHandle&& handle) -> bool {
		auto ent = m_entities.template get<0>( stack_index_t{ handle.get_index() } );
		if( ent.has_value() && ent.value().get().m_index.get_generation() == handle.get_generation() ) return true;
		return false;
	}


	//-----------------------------------------------------------------------------------------------------------------

	class VecsArchetype {
		//template<typename TL, typename T, typename... Ts> friend class VecsIterator;
		//template<typename TL, typename T, typename... Ts> friend class VecsSRange;
		//template<typename TL> friend class VecsSystem;

	public:

	private:

	};


} //namespace




#ifdef XXX
namespace vecsOLD {

	/// <summary>
	/// The main ECS class.
	/// </summary>
	/// <typeparam name="LOTL">A typelist storing all possible component types stored inside of groups.</typeparam>
	/// <typeparam name="GOTL">A typelist storing all possible component types stored outside of groups.</typeparam>
	template<typename LOTL, typename GOTL>
	class VECSSystem {
		template<typename LOTL, typename GOTL, typename T, typename... Ts> friend class VECSIterator;
		template<typename LOTL, typename GOTL, typename T, typename... Ts> friend class VECSRange;

	public:
		static_assert(vtll::unique< vtll::cat<LOTL,GOTL>>::value, "VECSSystem LOTL/GOTL types are not unique!");

		static const int BITSLOTL = vtll::size<LOTL>::value;	//Number of bits in the bitset, i.e. number of component types ingroup
		static const int BITSGOTL = vtll::size<GOTL>::value;	//Number of bits in the bitset, i.e. number of component types outgroup

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
			VECSGroup*  m_group;		//The group this entity belongs to
			uint32_t	m_index;		//Index of the entity data eitehr in local or global component tables
			uint32_t	m_generation;	//The generation counter of this entity
		};


		//-----------------------------------------------------------------------------------------------------------------
		//Component containers

		/// <summary>
		/// Base class for all containers.
		/// </summary>
		class VECSComponentContainerBase {
			template<typename LOTL, typename GOTL, typename T, typename... Ts> friend class VECSIterator;
			template<typename LOTL, typename GOTL, typename T, typename... Ts> friend class VECSRange;

		protected:
			uint32_t m_number;	//Number in the list of containers. Needed for erasing.
		public:
			VECSComponentContainerBase(uint32_t number) : m_number{ number } {};
			virtual void erase(uint32_t index) {};	//Erase a component
		};

		using container_vector = std::vector<std::shared_ptr<VECSComponentContainerBase>>;

		/// <summary>
		/// Container for type T.
		/// </summary>
		/// <typeparam name="T">Type of the component to store in.</typeparam>
		template<typename T>
		class VECSComponentContainer : public VECSComponentContainerBase {
			template<typename LOTL, typename GOTL, typename T, typename... Ts> friend class VECSIterator;
			template<typename LOTL, typename GOTL, typename T, typename... Ts> friend class VECSRange;

		public:

			/// <summary>
			/// This container stores a vector of this struct.
			/// </summary>
			struct entry_t {
				VECSHandle	m_handle;		//Entity handle
				T			m_component{};	//The component data.
			};

			/// <summary>
			/// Component container constructor. Reserves up front memory for the components.
			/// </summary>
			/// <param name="number">Number in the global component pointer vector pointing to this container.</param>
			VECSComponentContainer(VECSSystem<LOTL, GOTL>& system, uint32_t number) : m_system{ system }, VECSComponentContainerBase(number) {
				m_data.reserve(100);
			};

			/// <summary>
			/// Add a new component to the container.
			/// </summary>
			/// <param name="index">Pointer to the index of this component.</param>
			/// <param name="data">LReference to the component data.</param>
			/// <returns>The entry of the component.</returns>
			auto add(VECSHandle handle, T& data) -> uint32_t {
				m_data.emplace_back(handle, data );
				return (uint32_t)m_data.size() - 1;
			}

			/// <summary>
			/// Add a new component to the container.
			/// </summary>
			/// <param name="index">Pointer to the index of this component.</param>
			/// <param name="data">RReference to the component data.</param>
			/// <returns>The entry of the component.</returns>
			auto add(VECSHandle handle, T&& data) -> uint32_t {
				m_data.emplace_back(handle, std::move(data));
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

			/// <summary>
			/// Erase a component. If this is not the last component, then swap it with the last 
			/// component, so we fill up the hole that is made. This also entails changing the
			/// index in the VECSGroup that points to this component.
			/// </summary>
			/// <param name="index">Index of the component to erase.</param>
			void erase(uint32_t index) override;

			size_t size() { return m_data.size(); }

			auto begin() -> VECSIterator<LOTL, GOTL, T> { return { m_system, this, 0 }; };
			auto end()   -> VECSIterator<LOTL, GOTL, T> { return { m_system, this, (uint32_t)m_data.size() }; };

		private:
			VECSSystem<LOTL,GOTL>&	m_system;	//Need system for erasing
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
			template<typename LOTL, typename GOTL, typename T, typename... Ts> friend class VECSIterator;
			template<typename LOTL, typename GOTL, typename T, typename... Ts> friend class VECSRange;
			template<typename LOTL, typename GOTL> friend class VECSSystem;

		public:

			/// <summary>
			/// Group class constructor. Go through the bitset and calculate the number of types
			/// this group entities store. Add 1 for the index of the owner entity. Then reserve enough space
			/// so that the vector does not have to be reallocated too often.
			/// </summary>
			/// <param name="types">Bitset representing the group type</param>
			/// <param name="container">Vector with pointers to the containers this group needs</param>
			VECSGroup(std::bitset<BITSGOTL> gl_types, VECSSystem<LOTL,GOTL>& system ) : m_num_indices{ 0 }, m_gl_types{ gl_types }, m_system{ system } {
				m_gl_component_index_map.reserve(BITSGOTL);
				for (uint32_t i = 0; i < BITSGOTL; ++i) {
					if (gl_types.test(i)) { 
						m_gl_component_index_map.push_back(m_num_indices++);	//Map from bit in bitset (0...BITSOUT-1) to relative component index (0...m_num_indices-1)
						m_gl_container_map.emplace_back(m_system.m_container[i]);	//Map from index index to pointer to container (for erasing)
					}
					else { 
						m_gl_component_index_map.push_back(null_idx); //Do not need this component
					}
				}
				m_num_indices += 2;	//One more index for entity, and one for the generation.
				m_gl_indices.reserve(m_num_indices * 100);
			}

			template<typename... Ts>
			auto add(VECSHandle handle, Ts&&... Args) -> uint32_t {
				uint32_t indices_start = (uint32_t)m_gl_indices.size();	//Need a new slot at end of the vector
				m_gl_indices.resize(indices_start + m_num_indices, 0);		//Create new space in the vector for the indices, fill with 0

				/// <summary>
				/// This lambda goes through all arguments, and adds them to the respective container they belong into.
				/// The returned indices are stored in the entity's index list.
				/// </summary>
				auto createComponents = [&]<typename T>(T&& Arg) {
					static const uint32_t idx = vtll::index_of<GOTL, T>::value;
					VECSComponentContainer<T>& container = *std::dynamic_pointer_cast<VECSComponentContainer<T>>(m_system.m_container[idx]); 
					m_gl_indices[indices_start + m_gl_component_index_map[idx]] = container.add(handle, std::forward<T>(Arg));
				};
				(createComponents.template operator() < Ts > (std::forward<Ts>(Args)), ...);

				m_gl_indices[indices_start + m_num_indices - 2] = handle.m_entity;		//Store part of handle
				m_gl_indices[indices_start + m_num_indices - 1] = handle.m_generation; //Store part of handle

				++m_size;
				return indices_start;
			}

			void erase(uint32_t indices_start) {
				if(indices_start >= m_size * m_num_indices) return;

				for (uint32_t i = 0; i < m_num_indices - 2; ++i) {	//Erase the components from the containers
					m_gl_container_map[i]->erase(m_gl_indices[indices_start + i]); 
				}

				if (indices_start + m_num_indices < m_gl_indices.size() - 1) {			//Last slot? No - move the last slot to fill the gap
					uint32_t last = (uint32_t)m_gl_indices.size() - m_num_indices;		//Index start of last entity
					uint32_t entity = m_gl_indices[last + m_num_indices - 2];			//This entity is moved to fill the gap
					for (uint32_t i = 0; i < m_num_indices; ++i) { m_gl_indices[indices_start + i] = m_gl_indices[last + i]; }
					for (uint32_t i = 0; i < m_num_indices; ++i) { m_gl_indices.pop_back(); }
					m_system.m_entities[entity].m_index = indices_start;
				}
				--m_size;
			}

			template<typename T>
			auto index(uint32_t indices_start) -> uint32_t& {
				return m_gl_indices[indices_start + m_gl_component_index_map[vtll::index_of<GOTL, T>::value] ];
			}

			auto handle(uint32_t indices_start) -> VECSHandle {
				return { m_gl_indices[indices_start + m_num_indices - 2], m_gl_indices[indices_start + m_num_indices - 1] };
			}

			auto size() -> size_t {
				return m_size;
			}


		private:
			VECSSystem<LOTL,GOTL>&	m_system;				//Reference to the vector of entities in the system
			size_t					m_size{0};
			uint32_t				m_num_indices{ 0 };		//Number of types of this group, plus 2 for the ent / generation counter
			std::vector<VECSHandle> m_handles;

			std::bitset<BITSGOTL>	m_gl_types;				//One bit for each type in the type list
			std::vector<uint32_t>	m_gl_component_index_map;	//Map type index in type list to index in index list
			container_vector		m_gl_container_map;		//Maps index list (0...) to container pointer
			std::vector<uint32_t>	m_gl_indices;				//Component type indices + owning entity index
		};


		//-----------------------------------------------------------------------------------------------------------------
		//The VECS main member functions and data

		/// <summary>
		/// VECS main class constructor. Creates empty shared ptrs for the outgroup containers.
		/// </summary>
		VECSSystem() {
			m_container.resize(BITSGOTL, nullptr); //We have enough space for the shared pointers, but containers do not exist yet
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

			std::bitset<BITSGOTL> outtypes;
			
			/// <summary>
			/// This lambda goes through all types for the entity, creates a bitset (IDing the group) representing this entity,
			/// and makes sure all component containers exist.
			/// </summary>
			auto groupType = [&]<typename T>() {
				static const uint32_t idx = vtll::index_of<GOTL, T>::value;
				if (!m_container[idx]) {	//Is the container pointer for this component type still zero?
					auto container = std::make_shared<VECSComponentContainer<T>>(*this, idx);		//No - create one
					m_container[idx] = std::static_pointer_cast<VECSComponentContainerBase>(container); //Save in vector
				}
				outtypes.set(vtll::index_of<GOTL, T>::value);	//Fill the bits of bitset representing the types
			};
			(groupType.template operator() < Ts > (), ...); //Create bitset representing the group and create necessary containers

			//Create the entity, or reuse an old one.
			uint32_t entity_index;
			if (m_empty_start != null_idx) {		//Reuse an old entity that has been erased previously
				entity_index = m_empty_start;
				m_empty_start = m_entities[m_empty_start].m_index;
				m_entities[entity_index].m_group = getGroup<Ts...>(outtypes);	//Remember the group 
			}
			else {
				entity_index = (uint32_t)m_entities.size();		//create a new entity
				m_entities.emplace_back(getGroup<Ts...>(outtypes), entity_index, 0);
			}
	
			VECSHandle handle{ entity_index, m_entities[entity_index].m_generation };
			m_entities[entity_index].m_index = m_entities[entity_index].m_group->add(handle, std::forward<Ts>(Args)...); //Add entity to group
			return handle; //Return the handle
		}

		/// <summary>
		/// Erase an entity by its handle.
		/// </summary>
		/// <param name="handle">The handle of the entity to be erased.</param>
		bool erase(VECSHandle handle) {
			VECSEntity& entity = m_entities[handle.m_entity];
			if (entity.m_generation != handle.m_generation) return false;	//Is the handle still valid?
			entity.m_generation++;						//Invalidate all handles to this entity
			entity.m_group->erase(entity.m_index);		//Erase from group, and also the components
			entity.m_index = m_empty_start;				//Include into empty list
			m_empty_start = handle.m_entity;
			return true;
		}

		/// <summary>
		/// Get a reference to the component of type T of an entity.
		/// </summary>
		/// <typeparam name="Ts">Component types.</typeparam>
		/// <param name="handle">Entity handle.</param>
		/// <returns>Tuple with references to the components of the entity.</returns>
		template<typename... Ts>
		auto get(VECSHandle handle) -> std::optional<std::tuple<Ts&...>> {
			VECSEntity& entity = m_entities[handle.m_entity];
			if (entity.m_generation != handle.m_generation) return {};		//Is the handle still valid?

			auto check = [&]<typename T>() {
				if (!entity.m_group->m_gl_types.test(vtll::index_of<GOTL, T>::value)) return false; //Does the entity contain the type?
				return true;
			};
			if (!(check.template operator() < Ts > () && ...)) return {};

			return { getTuple<Ts...>(entity.m_group, entity.m_index) };
		}

		bool valid(VECSHandle handle) {
			return m_entities[handle.m_entity].m_generation == handle.m_generation;
		}

		template<typename T, typename... Ts>
		auto range() {
			return VECSRange<LOTL, GOTL, T, Ts...>(*this);
		}

	private:

		template<typename... Ts>
		auto getGroup( std::bitset<BITSGOTL>& types) -> VECSGroup* {
			if (!m_groups.contains(types)) {									//Does the group exist yet?
				auto res = m_groups.emplace(types, VECSGroup{ types, *this } );	//Create it

				auto f = [&]< typename T>() {
					static const uint32_t idx = vtll::index_of<GOTL, T>::value;
					m_map_type_to_group.insert( std::make_pair( idx, &res.first->second ) );
				};
				(f.template operator()<Ts>(), ...);

				return &res.first->second;	//Return its address
			}
			return &m_groups.at(types);		//It exists - return its address
		}

		template<typename... Ts>
		auto getTuple(VECSGroup* group, uint32_t indices_start) -> std::tuple<Ts&...> {
			
			auto recurse = [&]<typename U, typename... Us>(auto & recurse_ref) {
				auto base = m_container[vtll::index_of<GOTL, U>::value];					//Pointer to the container
				auto container = std::dynamic_pointer_cast<VECSComponentContainer<U>>(base);	//Cast to correct type
				uint32_t cidx = group->index<U>(indices_start);
				auto& ref = container->entry(cidx).m_component;

				if constexpr (sizeof...(Us) == 0) {
					return std::tie(ref);
				}
				else {
					return std::tuple_cat(std::tie(ref), recurse_ref.template operator() < Us... > (recurse_ref));
				}
			};

			return recurse.template operator() < Ts... > (recurse);
		}

		std::vector<VECSEntity>								 m_entities;				//Container for all entities
		uint32_t											 m_empty_start{null_idx};//Index of first empty entity to reuse
		container_vector									 m_container;			//Container for the components
		std::unordered_map<std::bitset<BITSGOTL>, VECSGroup> m_groups;				//Entity groups
		std::unordered_multimap<uint32_t, VECSGroup*>		 m_map_type_to_group;	//Map for going from a component type to groups of entities with them
	};


	//-----------------------------------------------------------------------------------------------------------------
	//Leftover, this function needs to know groups and entities

	/// <summary>
	/// Erase a component. If this is not the last component, then swap it with the last 
	/// component, so we fill up the hole that is made. This also entails changing the
	/// index in the VECSGroup that points to this component.
	/// </summary>
	/// <param name="index">Index of the component to erase.</param>
	template<typename LOTL, typename GOTL>
	template<typename T>
	inline void VECSSystem<LOTL,GOTL>::VECSComponentContainer<T>::erase(uint32_t index) {
		if (m_data.size() - 1 > index) {
			std::swap(m_data[index], m_data[m_data.size() - 1]);
			entry_t& entry = m_data[index];
			auto& entity = m_system.m_entities[entry.m_handle.m_entity];	//Reference to the entity that owns this component
			entity.m_group->m_gl_indices[entity.m_index + entity.m_group->m_gl_component_index_map[this->m_number]] = index; //Reset index
		}
		m_data.pop_back();	//Remove last element, which is the component to be removed
	}


	//-----------------------------------------------------------------------------------------------------------------
	//Iterator
	
	template<typename LOTL, typename GOTL, typename T, typename... Ts>
	class VECSIterator {

		using group_ptr = typename VECSSystem<LOTL, GOTL>::VECSGroup*;

		template<typename U>
		using container_ptr = typename VECSSystem<LOTL, GOTL>::VECSComponentContainer<U>*;

		using ptr_type = std::conditional_t< (sizeof...(Ts) > 0), group_ptr, container_ptr<T> >;

		using handle = typename VECSSystem<LOTL, GOTL>::VECSHandle;

		using vt = std::conditional_t < (sizeof...(Ts) > 0), std::pair< std::tuple<T&, Ts&...>, handle >, std::pair< T&, handle > > ;

	public:
		using value_type = vt;								// Value type - use refs to deal with atomics and unique_ptr
		using reference = value_type&;						// Reference type
		using pointer = value_type*;						// Pointer to value
		using iterator_category = std::forward_iterator_tag;// Forward iterator
		using difference_type = int64_t;					// Difference type

		VECSIterator() noexcept = default;
		VECSIterator(VECSSystem<LOTL,GOTL>& system, group_ptr group, uint32_t index = 0) noexcept : m_system{&system}, m_ptr { group }, m_index{ index }, m_dN{ group->m_num_indices }, m_size{ group->m_size } {}
		VECSIterator(VECSSystem<LOTL,GOTL>& system, container_ptr<T> container, uint32_t index = 0) noexcept : m_system{ &system }, m_ptr{ container }, m_index{ index }, m_size{ (uint32_t)container->m_data.size() } {}

		VECSIterator(const VECSIterator<LOTL,GOTL,T,Ts...>& v) noexcept = default;	// Copy constructor
		VECSIterator(VECSIterator<LOTL,GOTL,T,Ts...>&& v)		noexcept = default;	// Move constructor

		auto operator=(const VECSIterator<LOTL,GOTL,T,Ts...>& v) noexcept -> VECSIterator<LOTL,GOTL,T,Ts...> & = default;	// Copy
		auto operator=(VECSIterator<LOTL,GOTL,T,Ts...>&& v)	  noexcept -> VECSIterator<LOTL,GOTL,T,Ts...> & = default;	// Move

		auto operator*() noexcept -> value_type { 		// Access the data
			if constexpr (sizeof...(Ts) > 0) {
				typename VECSSystem<LOTL,GOTL>::VECSHandle handle = m_ptr->handle(m_index);
				return std::make_pair( m_system->getTuple<T,Ts...>( m_ptr, m_index ), handle);
			}
			else {
				auto& entry = m_ptr->entry(m_index);
				return std::make_pair( std::ref(entry.m_component), entry.m_handle );
			}
		}

		auto operator*() const noexcept	-> value_type {		// Access const data
			if constexpr (sizeof...(Ts) > 0) {
				typename VECSSystem<LOTL,GOTL>::VECSHandle handle = m_ptr->handle(m_index);
				return std::make_pair(m_system->getTuple<T, Ts...>(m_ptr, m_index), handle);
			}
			else {
				auto& entry = m_ptr->entry(m_index);
				return std::make_pair(std::ref(entry.m_component), entry.m_handle);
			}
		}

		auto operator->() noexcept { return operator*(); };			// Access
		auto operator->() const noexcept { return operator*(); };	// Access

		auto operator++()    noexcept -> VECSIterator<LOTL,GOTL,T,Ts...>& { 	// Increase by m_dN - pre-increment
			operator+=(1); 
			return *this; 
		}
		auto operator++(int) noexcept -> VECSIterator<LOTL,GOTL,T,Ts...>  { 						// Increase by m_dN - post-increment
			auto cpy{ *this };
			operator+=(1);
			return cpy;
		}
		auto operator+=(difference_type N) noexcept	-> VECSIterator<LOTL,GOTL,T,Ts...>& { 		// Increase by N
			m_index = std::clamp((uint32_t)(m_index + N * m_dN), (uint32_t)0, (uint32_t)(m_dN * m_size));
			return *this;
		}
		auto operator+(difference_type N) noexcept	-> VECSIterator<LOTL,GOTL,T,Ts...> { 	// Increase by N
			m_index = std::clamp((uint32_t)(m_index + N * m_dN), (uint32_t)0, (uint32_t)(m_dN * m_size));
			return *this;
		}

		bool operator!=(const VECSIterator<LOTL,GOTL,T,Ts...>& i) noexcept {	// Unequal
			return m_ptr != i.m_ptr || m_index != i.m_index;
		}

		friend auto operator==(const VECSIterator<LOTL,GOTL,T,Ts...>& i1, const VECSIterator<LOTL,GOTL,T,Ts...>& i2) noexcept -> bool {	// Equal
			return i1.m_ptr == i2.m_ptr && i1.m_index == i2.m_index;
		}

	private:
		VECSSystem<LOTL,GOTL>*	m_system;
		ptr_type				m_ptr{ nullptr };
		uint32_t				m_index{ 0 };
		uint32_t				m_dN{ 1 };
		size_t					m_size{ 0 };
	};


	//using it = VECSIterator< vtll::tl<int>, int>;
	//static_assert(std::assignable_from< it&, it>, "Iterator");


	//-----------------------------------------------------------------------------------------------------------------
	//Ranges

	template<typename LOTL, typename GOTL, typename T, typename... Ts>
	class VECSRange {
		using type_list = vtll::tl<T, Ts...>;

		using group_ptr = typename VECSSystem<LOTL,GOTL>::VECSGroup*;

		template<typename U>
		using container_ptr = typename VECSSystem<LOTL,GOTL>::VECSComponentContainer<U>*;

		using ptr_type = std::conditional_t< (sizeof...(Ts) > 0), group_ptr, container_ptr<T> >;

		struct range_t {
			VECSIterator<LOTL, GOTL, T, Ts...>	m_begin{};
			VECSIterator<LOTL, GOTL, T, Ts...>	m_end{};
			size_t								m_size{ 0 };

			range_t() noexcept = default;
			range_t(const VECSIterator<LOTL, GOTL, T, Ts...>& b, const VECSIterator<LOTL, GOTL, T, Ts...>& e, size_t size) noexcept : m_begin{ b }, m_end{ e }, m_size{ size } {};
			range_t(VECSIterator<LOTL, GOTL, T, Ts...>&& b, VECSIterator<LOTL, GOTL, T, Ts...>&& e, size_t size) noexcept : m_begin{ b }, m_end{ e }, m_size{ size } {};
			range_t(const range_t&) = default;
			range_t(range_t&&) = default;
			auto operator=(const range_t& v) noexcept -> range_t & = default;
			auto operator=(range_t&& v) noexcept -> range_t & = default;

			auto begin() noexcept { return m_begin; };
			auto end() noexcept { return m_end; };
			auto size() const noexcept { return m_size; };
		};
		//ranges::range<T>&& std::same_as<ranges::iterator_t<T>, ranges::sentinel_t<T>>;

		static_assert( std::ranges::common_range<range_t> , "range_t");

		//using view_type = decltype(std::views::join( std::declval< std::vector<range_t> >() ));

	public:
		VECSRange() noexcept = default;
		VECSRange(const VECSRange<LOTL, GOTL, T, Ts...>&) noexcept = default;
		VECSRange(VECSRange<LOTL, GOTL, T, Ts...>&&) noexcept = default;

		auto operator=(const VECSRange<LOTL, GOTL, T, Ts...>& v) noexcept -> VECSRange<LOTL, GOTL,T,Ts...> & = default;
		auto operator=(VECSRange<LOTL, GOTL, T, Ts...>&& v) noexcept -> VECSRange<LOTL, GOTL, T, Ts...> & = default;

		VECSRange(VECSSystem<LOTL,GOTL>& system) noexcept {
			if constexpr (sizeof...(Ts) > 0) {

				std::bitset<VECSSystem<LOTL,GOTL>::BITSGOTL> types;
				auto f = [&]<typename U, U I>(std::integral_constant<U, I> i) {
					using type = vtll::Nth_type<type_list, I>;
					types.set(vtll::index_of<GOTL, type>::value);
				};
				vtll::static_for< int, 0, vtll::size<type_list>::value >(f);

				std::set<typename VECSSystem<LOTL, GOTL>::VECSGroup*> groups;
				auto g = [&]<typename U, U I>(std::integral_constant<U, I> i) {
					using type = vtll::Nth_type<type_list, I>;
					static const uint32_t idx = vtll::index_of<GOTL, type>::value;

					std::cout << typeid(type).name() << "\n";

					auto range = system.m_map_type_to_group.equal_range(idx);
					for (auto it = range.first; it != range.second; ++it) {
						if ((it->second->m_gl_types & types) == types) { groups.insert(it->second); }
					}
				};
				vtll::static_for< int, 0, vtll::size<type_list>::value >(g);

				for (auto* group : groups) {
					auto b = VECSIterator<LOTL, GOTL, T, Ts...>{ system, group, 0 };
					auto e = VECSIterator<LOTL, GOTL, T, Ts...>{ system, group, (uint32_t)group->m_size * group->m_num_indices };
					m_ranges.push_back(range_t(b, e, group->size()));
					m_size += group->size();
				}
			}
			else {
				static const uint32_t idx = vtll::index_of<GOTL, T>::value;
				auto base = system.m_container[vtll::index_of<GOTL, T>::value];					//Pointer to the container
				if (base) {
					auto container = std::dynamic_pointer_cast<typename VECSSystem<LOTL, GOTL>::VECSComponentContainer<T>>(base);	//Cast to correct type
					m_ranges.push_back(range_t{ container->begin(), container->end(), container->size()});
					m_size = container->size();
				}
			}
		}

		auto begin() noexcept {
			m_view = std::make_shared<decltype(std::views::join(m_ranges))>(std::views::join(m_ranges));
			return m_view->begin();
		}

		auto end() noexcept {
			return m_view->end();
		}

		auto size() { return m_size; }

	private:
		std::vector<range_t>									m_ranges;
		std::shared_ptr<decltype(std::views::join(m_ranges))>	m_view;
		size_t													m_size{ 0 };
	};
	
	//static_assert( std::ranges::range<VECSRange<vtll::tl<int>, int>> , "VECSRange");


}


#endif
