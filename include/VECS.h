#pragma once

#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <set>
#include <bitset>
#include <unordered_set>
#include <bitset> 
#include <optional> 
#include <ranges>
#include <functional>
#include <tuple>
#include <algorithm>
#include <limits>
#include <cassert>

#include "VTLL.h"
#include "VSTY.h"
#include "VLLT.h"



namespace vecs {

	//for handles
	using stack_index_t = vllt::stack_index_t;
	using generation_t = vsty::strong_type_t< uint64_t, vsty::counter<>, std::integral_constant<uint64_t, std::numeric_limits<uint64_t>::max()> >;

	const size_t NBITS = 46;

	/// <summary>
	/// A VecsIndex is a 64-bit integer, with the first NBITS bits encoding the index
	/// of the uint64_t, and the rest encoding a generation counter. The generation counter is incremented each time the index
	/// is erased, so that old indices can be detected.
	/// </summary>
	struct VecsIndex : public vsty::strong_type_t< uint64_t, vsty::counter<>, std::integral_constant<uint64_t, std::numeric_limits<uint64_t>::max()> > {
		stack_index_t get_index() const { return stack_index_t{ get_bits(0, NBITS) }; }
		generation_t get_generation() const { return generation_t{ get_bits(NBITS) }; }
		void set_index( stack_index_t index) { return set_bits( (uint32_t)index, 0, NBITS ); }
		void set_generation( generation_t gen) { return set_bits( (uint32_t)gen, NBITS ); }
		stack_index_t increase_index() { set_bits( (uint32_t)get_index() + 1, 0, NBITS); return get_index(); }
		generation_t increase_generation() { set_bits( (uint32_t)get_generation() + 1, NBITS); return get_generation(); }
		VecsIndex() = default;
		explicit VecsIndex( stack_index_t index, generation_t gen) { set_index(index); set_generation(gen); }
		explicit VecsIndex( stack_index_t index) { set_index(index); set_generation( generation_t{0ULL} ); }
	};

	/// <summary>
	/// A VecsHandle is a 64-bit integer, with the first NBITS bits encoding the index
	/// of the uint64_t, and the rest encoding a generation counter. The generation counter is incremented each time the index
	/// is erased, so that old indices can be detected.
	/// </summary>
	struct VecsHandle : public VecsIndex {  
		VecsHandle() = default;
		explicit VecsHandle( stack_index_t index, generation_t gen) { set_index(index); set_generation(gen); }
		explicit VecsHandle( stack_index_t index) { set_index(index); set_generation( generation_t{0ULL} ); }
	};

	template<typename TL, typename... Ts>
	concept has_all_types = ((vtll::unique< vtll::tl<Ts... > >::value) && 
							vtll::has_all_types<TL, vtll::tl<Ts...> >::value);

	template<typename TL>
	concept unique_and_no_handle = (vtll::unique<TL>::value && !vtll::has_type<TL, VecsHandle>::value);

	template<typename TL> requires vtll::unique<TL>::value class VecsSystem;
	template<typename TL> class VecsArchetypeBase;
	template<typename TL, typename... As> class VecsArchetype;
	template<typename GL, typename SL> requires vtll::unique<vtll::cat<GL, SL>>::value class VecsEntityManagerBase;


	//-----------------------------------------------------------------------------------------------------------------

	/// <summary>
	/// The main ECS class. It stores entities in archetypes, which are groups of entities with the same component types.
	///  - Entities are stored in a VlltStack, and indexed by a VecsHandle. The handle is a 64-bit integer, with the first NBITS bits encoding the index,
	///	and the rest encoding a generation counter. The generation counter is incremented each time the index is erased, so that old indices can be detected.
	/// </summary>
	/// <typeparam name="TL">A typelist storing all possible component types. Types MUST be UNIQUE!</typeparam>
	template<typename TL> requires vtll::unique<TL>::value
	class VecsSystem {

	protected:
		static_assert(vtll::unique<std::decay_t<TL>>::value, "VecsSystem types are not unique!");
		static const size_t BITSTL = vtll::size<TL>::value;	//Number of bits in the bitset, i.e. number of component types ingroup

		/// <summary>
		/// An enity is a list of components stored in an archetype. The VecsHandle indexes into the entity map storing VecsEntity structs.
		/// A VecsEntity points to an archetype, and has a VecsIndex, which also holds the generation counter.
		/// </summary>
		struct VecsEntity {
			std::shared_ptr<VecsArchetypeBase<TL>> m_archetype_ptr;	//The group this entity belongs to.
			VecsIndex m_index; //Index of the entity in the archetype. It is also used to point to the next free slot.
		};

	public:	
		using TTL = vtll::cat<TL, vtll::tl<VecsHandle> >;	//Typelist of all component types in the archetype, plus the handle

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		using ref_tuple = vtll::to_ref_tuple< vtll::tl<Ts...> >;

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		using optional_ref_tuple = std::optional< ref_tuple<Ts...> >;

		using component_ptrs_t = std::array<void*, vtll::size<TL>::value>;

		VecsSystem(){};
		inline auto erase(VecsHandle handle) -> bool;
		inline auto valid(VecsHandle handle) -> bool { return get_entity_from_handle(std::forward<const VecsHandle>(handle)) != nullptr; };

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		[[nodiscard]] auto insert(Ts&&... Args) -> VecsHandle;

		template<typename... Ts> 
		[[nodiscard]] auto get(VecsHandle handle) -> optional_ref_tuple<Ts...>; //Get a tuple of references to the components of the entity

		template<typename... Ts, typename... As> requires has_all_types<vtll::tl<Ts...>, As...>
		auto transform(VecsHandle handle, As&&... Args) -> bool; //Transform the entity into a new archetype, adding or removing components


	private:
		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		inline auto get_archetype() -> std::shared_ptr<VecsArchetype<TL, Ts...>>; //Get the archetype for the types
		inline auto get_new_entity() -> std::pair<VecsIndex, VecsEntity*>; //Get a new entity slot, and a pointer to it
		inline auto get_entity_from_handle(VecsHandle handle) -> VecsEntity*; //Get a pointer to the entity from the handle
		inline auto get_entity_from_index(VecsIndex index) -> VecsEntity*; //Get a pointer to the entity from the index

		template<typename... Ts> requires has_all_types<TL, Ts...>	//compute the bitset representing the types
		constexpr inline auto bitset() -> std::bitset<BITSTL>;

		vllt::VlltStack< vtll::tl<VecsEntity>, 1<<10, false > m_entities; //Container for all entities
		std::atomic<VecsIndex> m_empty_start{}; //Index of first empty entity to reuse, initialized to NULL value
		std::unordered_map<std::bitset<BITSTL>, std::shared_ptr<VecsArchetypeBase<TL>>> m_archetypes; //Entity archetypes, indexed by bitset representing the types
		std::unordered_multimap<size_t, std::shared_ptr<VecsArchetypeBase<TL>>> m_map_type_to_archetype; //Map for going from a component type to archetype of entities with them
	};


	/// <brief>
	/// Insert a new entity into the system, with the given components. The components are forwarded to the constructor of the components.
	/// </brief>
	/// <param name="Args">The components to insert</param>
	/// <returns>A handle to the inserted entity</returns>
	template<typename TL> requires vtll::unique<TL>::value
	template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
	[[nodiscard]] auto VecsSystem<TL>::insert(Ts&&... Args) -> VecsHandle { 
		auto [handle_index, entity_ptr] = get_new_entity(); //Get a new entity slot, and a pointer to it	
		entity_ptr->m_archetype_ptr = get_archetype<Ts...>(); //Get the archetype for the types
		VecsHandle handle{ handle_index.get_index(), entity_ptr->m_index.get_generation() }; //Create a handle to the entity
		entity_ptr->m_index = std::dynamic_pointer_cast<VecsArchetype<TL, Ts...>>(entity_ptr->m_archetype_ptr)->insert(handle, std::forward<Ts>(Args)...);
		return handle; 
	}

	/// <brief>
	/// Erase the entity with the given handle
	/// </brief>
	/// <param name="handle">The handle of the entity to erase</param>
	/// <returns>True if the entity was erased, false if the handle was invalid</returns>
	template<typename TL> requires vtll::unique<TL>::value
	inline auto VecsSystem<TL>::erase(VecsHandle handle) -> bool {
		auto entity_ptr = get_entity_from_handle(handle); //Get a pointer to the entity from the handle
		if( !entity_ptr ) return false;
		entity_ptr->m_index.increase_generation(); //Increase the generation counter to invalidate any handle
		
		VecsHandle moved = entity_ptr->m_archetype_ptr->erase(entity_ptr->m_index); //Erase the entity from the archetype, and get the handle of the entity that was moved to the erased entity's slot
		auto entity_moved_ptr = get_entity_from_handle(moved);
		//if( !res ) return false;

		VecsIndex old_handle_index = m_empty_start.load();
		VecsIndex new_handle_index{ old_handle_index.get_index() };

		while( old_handle_index.has_value() 
			&& !m_empty_start.compare_exchange_weak( old_handle_index, new_handle_index ) ) {
			new_handle_index.set_index( old_handle_index.get_index() );
		}

		return true;
	}

	/// <brief>
	/// Get a tuple of references to the components of the entity with the given handle
	/// </brief>
	/// <param name="handle">The handle of the entity to get</param>
	/// <returns>A tuple of references to the components of the entity</returns>
	template<typename TL> requires vtll::unique<TL>::value
	template<typename... Ts>
	[[nodiscard]] auto VecsSystem<TL>::get(VecsHandle handle) -> optional_ref_tuple<Ts...> {
		auto entity_ptr = get_entity_from_handle(handle);
		if( !entity_ptr ) return {};
		//if( bitset<Ts...>() != entity_ptr->m_archetype_ptr->bitset() ) return {}; //Check that the types match
		return entity_ptr->m_archetype_ptr->template get<Ts...>(entity_ptr->m_index);
	}

	/// <brief>
	/// Transform the entity with the given handle into a new archetype, adding or removing components
	/// </brief>
	/// <param name="handle">The handle of the entity to transform</param>
	/// <param name="Args">The components to insert</param>
	/// <returns>True if the entity was transformed, false if the handle was invalid</returns>
	template<typename TL> requires vtll::unique<TL>::value
	template<typename... Ts, typename... As> requires has_all_types<vtll::tl<Ts...>, As...>
	inline auto VecsSystem<TL>::transform(VecsHandle handle, As&&... Args) -> bool {
		auto entity_ptr = get_entity_from_handle(handle);
		if( !entity_ptr ) return false;

		std::shared_ptr<VecsArchetype<TL, Ts...>> new_archtetype_ptr = get_archetype<Ts...>(); //Get the archetype for the types
		if(new_archtetype_ptr == entity_ptr->m_archetype_ptr) return false; //No change in archetype

		component_ptrs_t component_ptrs{nullptr};
		entity_ptr->m_archetype_ptr->get_pointers(entity_ptr->m_index.get_index(), component_ptrs); //Get pointers to the components in the archetype, fill into m_component_ptrs
		
		VecsIndex new_index = new_archtetype_ptr->insert_from_pointers(handle, component_ptrs, std::forward<As>(Args)...); //Insert the new components into the archetype

		entity_ptr->m_archetype_ptr->erase(entity_ptr->m_index); //Erase the old components from the archetype
		entity_ptr->m_archetype_ptr = new_archtetype_ptr; //Update the archetype of the entity
		entity_ptr->m_index = new_index; //Update the index of the entity
		return true;
	}

	/// <brief>
	/// Get the archetype for the given types
	/// </brief>
	/// <param name="handle">The handle of the entity to transform</param>
	/// <param name="Args">The components to insert</param>
	/// <returns>True if the entity was transformed, false if the handle was invalid</returns>
	template<typename TL> requires vtll::unique<TL>::value
	template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
	inline auto VecsSystem<TL>::get_archetype() -> std::shared_ptr<VecsArchetype<TL, Ts...>> {
		std::bitset<BITSTL> types = bitset<Ts...>();	//Create bitset representing the archetype
		std::shared_ptr<VecsArchetypeBase<TL>> archetype_ptr; //create a new archetype if necessary
		if( !m_archetypes.contains(types) ) {
			auto res = m_archetypes.emplace(types, std::make_shared<VecsArchetype<TL, Ts...>>( *this, types ) );	//Create it
			archetype_ptr = res.first->second;

			auto f = [&]< typename T>() { //Register the archetype in the type map, for iterating over all archetypes with a certain type
				static const size_t idx = vtll::index_of<TL, T>::value;
				m_map_type_to_archetype.insert( std::make_pair( idx, archetype_ptr ) ); //each type creates a new entry in the map
			};
			(f.template operator()<Ts>(), ...);
		} else {
			archetype_ptr = m_archetypes.at(types); //archetype already exists - return its address
		}
		return std::dynamic_pointer_cast<VecsArchetype<TL, Ts...>>(archetype_ptr);
	}

	/// <brief>
	/// Get a new entity slot, and a pointer to it
	/// </brief>
	/// <returns>A pair of the index of the new entity slot, and a pointer to it</returns>
	template<typename TL> requires vtll::unique<TL>::value
	inline auto VecsSystem<TL>::get_new_entity() -> std::pair<VecsIndex, VecsEntity*> {
		VecsIndex handle_index = m_empty_start.load();
		VecsEntity *entity_ptr{nullptr};
		if( handle_index.has_value() ) { //there is a free slot to make use of
			entity_ptr = get_entity_from_index(handle_index);
			VecsIndex new_handle_index = entity_ptr->m_index; //Get the next free slot
			while( handle_index.has_value() 
				&& !m_empty_start.compare_exchange_weak( handle_index, new_handle_index ) ) {
					entity_ptr = get_entity_from_index(handle_index);
					new_handle_index = entity_ptr->m_index;
			}
		}
		else {
			handle_index = VecsIndex{ m_entities.push( VecsEntity{ {nullptr}, VecsIndex{stack_index_t{0}, generation_t{0}} } ) };
			entity_ptr = get_entity_from_index( handle_index );
		}
		return {handle_index, entity_ptr};
	}

	/// <brief>
	/// Get a pointer to the entity from the handle
	/// </brief>
	/// <param name="handle">The handle of the entity to get</param>
	/// <returns>A pointer to the entity</returns>
	template<typename TL> requires vtll::unique<TL>::value
	inline auto VecsSystem<TL>::get_entity_from_handle(VecsHandle handle) -> VecsEntity* {
		auto entity = m_entities.template get<0>( stack_index_t{ handle.get_index() } );
		if( !entity.has_value() || entity.value().get().m_index.get_generation() != handle.get_generation() ) return nullptr;
		return &entity.value().get();
	}

	/// <brief>
	/// Get a pointer to the entity from the index
	/// </brief>
	/// <param name="index">The index of the entity to get</param>
	/// <returns>A pointer to the entity</returns>
	template<typename TL> requires vtll::unique<TL>::value
	inline auto VecsSystem<TL>::get_entity_from_index(VecsIndex index) -> VecsEntity* {
		auto entity = m_entities.template get<0>( stack_index_t{ index.get_index() } );
		if( !entity.has_value()) return nullptr;
		return &entity.value().get();
	}

	/// <brief>
	/// Compute the bitset representing the types
	/// </brief>
	/// <returns>The bitset representing the types</returns>
	template<typename TL> requires vtll::unique<TL>::value
	template<typename... Ts> requires has_all_types<TL, Ts...>
	constexpr inline auto VecsSystem<TL>::bitset() -> std::bitset<BITSTL> {
		std::bitset<BITSTL> bits;
		auto groupType = [&]<typename T>() { //Create bitset representing the archetype
			static const uint32_t idx = vtll::index_of<TL, T>::value;
			bits.set(idx);	//Fill the bits of bitset representing the types
		};
		(groupType.template operator() < Ts > (), ...);
		return bits;
	}

	
	//-----------------------------------------------------------------------------------------------------------------
	//VecsArchetypeBase

	/// <summary>
	/// The base class for archetypes. It stores entities with the same component types.
	/// </summary>
	/// <typeparam name="TL">A typelist storing all possible component types. Types MUST be UNIQUE!</typeparam>
	template<typename TL>
	class VecsArchetypeBase {
		friend class VecsSystem<TL>;

	protected:
		static_assert(vtll::unique<TL>::value, "VecsArchetype types are not unique!");
		static const size_t BITSTL = vtll::size<TL>::value;	//Number of bits in the bitset, i.e. number of component types ingroup
		using component_ptrs_t = VecsSystem<TL>::component_ptrs_t;

	public:
		using TTL = VecsSystem<TL>::TTL;	//Typelist of all component types in the archetype, plus the handle

		template<typename... Ts> requires has_all_types<TL, Ts...>	//A tuple of pointers to the components
		using ptr_tuple = vtll::to_ptr_tuple< vtll::tl<Ts...> >;

		template<typename... Ts> requires has_all_types<TL, Ts...>	//A tuple of references to the components
		using ref_tuple = vtll::to_ref_tuple< vtll::tl<Ts...> >;

		template<typename... Ts> requires has_all_types<TL, Ts...>	//An optional tuple of references to the components
		using optional_ref_tuple = std::optional< ref_tuple<Ts...> >;

		VecsArchetypeBase(VecsSystem<TL>& system, std::bitset<BITSTL> types) : m_system{system}, m_types{types} {};

		template<typename... Ts>
		[[nodiscard]] inline auto get(VecsIndex index) -> optional_ref_tuple<Ts...>;

		virtual inline auto erase(VecsIndex index) -> VecsHandle = 0; //Erase the entity at the given index

		inline auto bitset() -> std::bitset<BITSTL> { return m_types; } //Get the bitset representing the types

	protected:
		virtual inline auto get_pointers(stack_index_t index, component_ptrs_t& component_ptrs ) -> bool = 0; //Get pointers to the components in the archetype, fill into m_components

		VecsSystem<TL>&	m_system; //Reference to the the system
		std::bitset<BITSTL> m_types; //One bit for each type in the type list
	};

	/// <brief>
	/// Get a tuple of references to the components of the entity with the given index
	/// </brief>
	/// <param name="index">The index of the entity to get</param>
	/// <returns>A tuple of references to the components of the entity</returns>
	template<typename TL>
	template<typename... Ts>
	inline auto VecsArchetypeBase<TL>::get(VecsIndex index) -> optional_ref_tuple<Ts...> {
		component_ptrs_t component_ptrs;
		if(!get_pointers(index.get_index(), component_ptrs)) return {};	//Get pointers to the components in the archetype, fill into m_component_ptrs
		return { std::tie( *static_cast<Ts*>(component_ptrs[vtll::index_of<TL, Ts>::value])... ) };
	}


	//-----------------------------------------------------------------------------------------------------------------
	//VecsArchetype

	/// <summary>
	/// An archetype. It stores entities with the same component types.
	/// </summary>
	/// <typeparam name="TL">A typelist storing all possible component types. Types MUST be UNIQUE!</typeparam>
	/// <typeparam name="As">The component types stored in the archetype</typeparam>
	template<typename TL, typename... As>
	class VecsArchetype : public VecsArchetypeBase<TL> {
		friend class VecsSystem<TL>;

	protected:
		using AL = vtll::tl<As...>;	//Typelist of all component types in the archetype
		static_assert(vtll::unique<AL>::value, "VecsArchetype types are not unique!");
		using AAL = vtll::cat<AL, vtll::tl<VecsHandle> >;	//Typelist of all component types in the archetype, plus the handle

		using typename VecsArchetypeBase<TL>::TTL;	//Typelist of all component types in the archetype, plus the handle
		using VecsArchetypeBase<TL>::BITSTL;
		using VecsArchetypeBase<TL>::m_system;
		using VecsArchetypeBase<TL>::m_types;
		using component_ptrs_t = VecsSystem<TL>::component_ptrs_t;

	public:
		VecsArchetype(VecsSystem<TL>& system, std::bitset<BITSTL> types) : VecsArchetypeBase<TL>{ system, types } {};

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		[[nodiscard]] inline auto insert(VecsHandle handle, Ts&&... Args) -> VecsIndex;

		inline auto erase(VecsIndex index) -> VecsHandle override;

	private:
		template<typename... Ts> requires has_all_types<TL, Ts...>
		[[nodiscard]] inline auto insert_from_pointers(VecsHandle handle, component_ptrs_t& ptr, Ts&&... Args) -> VecsIndex;

		virtual inline auto get_pointers(stack_index_t index, component_ptrs_t& component_ptrs) -> bool override; //Get pointers to the components in the archetype, fill into m_components

		vllt::VlltStack<AAL> m_data; //The data stored in this archetype
	};


	/// <brief>
	/// Insert a new entity into the archetype, with the given components. The components are forwarded to the constructor of the components.
	/// </brief>
	/// <param name="handle">Handle of the new entity.</param>
	/// <param name="Args">The components to insert.</param>
	/// <returns>A handle to the inserted entity.</returns>	
	template<typename TL, typename... As>
	template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
	[[nodiscard]] inline auto VecsArchetype<TL, As...>::insert(VecsHandle handle, Ts&&... Args) -> VecsIndex { 
		return VecsIndex{ m_data.push( std::forward<Ts>(Args)..., handle ) };
	}

	/// <brief>
	/// Get pointers to the components in the archetype, fill into m_components
	/// </brief>
	/// <param name="index">The index of the entity to get</param>
	/// <param name="component_ptrs">The array to fill with pointers to the components</param>
	/// <returns>True if the entity was found, false otherwise</returns>
	template<typename TL, typename... As>
	inline auto VecsArchetype<TL, As...>::get_pointers( stack_index_t index, component_ptrs_t& component_ptrs ) -> bool {
		auto tuple = m_data.get_tuple(index);
		if( !tuple.has_value() ) return false;
		vtll::static_for<size_t, 0, vtll::size<AL>::value >( // Loop over all components
			[&](auto i) { 
				component_ptrs[vtll::index_of<TL, vtll::Nth_type<AL, i>>::value] = &std::get<i>(tuple.value()); //Get pointer to component
			} 
		);
		return true;
	}

	/// <brief>
	/// Insert a new entity into the archetype, with the given components. The components are forwarded to the constructor of the components.
	/// </brief>
	/// <param name="handle">Handle of the new entity.</param>
	/// <param name="Args">The components to insert.</param>
	/// <returns>A handle to the inserted entity.</returns>
	template<typename TL, typename... As>
	template<typename... Ts> requires has_all_types<TL, Ts...>
	[[nodiscard]] inline auto VecsArchetype<TL, As...>::insert_from_pointers(VecsHandle handle, component_ptrs_t& component_ptrs, Ts&&... Args) -> VecsIndex {
		
		auto f = [&](auto&& Arg) { component_ptrs[vtll::index_of<TL, std::decay_t<decltype(Arg)>>::value] = &Arg; };
		( f( std::forward<Ts>(Args) ), ... ); //change pointers to new args

		bool res = true;
		auto g = [&]<typename T>() { res = res && component_ptrs[vtll::index_of<TL,T>::value ] != nullptr; };
		( g.template operator()<As>(), ... ); //check that all pointers are valid	
		assert( ((void)"Type has no value!\n", res) ); //all pointers must be valid

		//call push with pointers as arguments
		stack_index_t index = m_data.push( std::forward<As>( *static_cast<As*>(component_ptrs[vtll::index_of<TL, As>::value]))..., handle );

		return VecsIndex{ index }; //return index
	}

	/// <brief>
	/// Erase the entity at the given index
	/// </brief>
	/// <param name="index">The index of the entity to erase</param>
	/// <returns>The handle of the entity that was moved to the erased entity's slot</returns>
	template<typename TL, typename... As>
	inline auto VecsArchetype<TL, As...>::erase(VecsIndex index) -> VecsHandle {
		if( m_data.size() <= index ) return {};
		m_data.erase(index.get_index());

		if( index <= m_data.size() ) { //there was a swap -> get handle of swapped entity
			auto tuple = m_data.get_tuple(index.get_index());
			if( !tuple.has_value() ) return {};
			return std::get<vtll::size<AAL>::value - 1>(tuple.value());
		}
		return {};
	}



	//-----------------------------------------------------------------------------------------------------------------
	//Iterator


	template<typename TL, typename T, typename... Ts>
	class VecsIterator {


	};


	template<typename TL, typename T, typename... Ts>
	class VecsRange {

	};


	//-----------------------------------------------------------------------------------------------------------------
	//Entity Manager


	/// <summary>
	/// A VecsEntityManager is a container for VecsSystems. It is used to manage the lifetime of entities.
	/// </summary>
	/// <typeparam name="GL">A typelist holding comonent types, the user of the gmae engine wants to use. Types MUST be UNIQUE!</typeparam>
	/// <typeparam name="SL">A typelist holding comonent types, the game engine wants to use. Types MUST be UNIQUE!</typeparam>
	template<typename GL, typename SL>
		requires vtll::unique<vtll::cat<GL, SL>>::value
	class VecsEntityManagerBase {
		using TL = vtll::cat<GL, SL>;	//Typelist of all component types including from game and from engine
		
	public:
		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		using ref_tuple = vtll::to_ref_tuple< vtll::tl<Ts...> >; //A tuple of references to the components

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		using optional_ref_tuple = std::optional< ref_tuple<Ts...> >; //An optional tuple of references to the components

		VecsEntityManagerBase() = default;

		inline auto erase(VecsHandle handle) -> bool { m_system.erase(handle); }; //Erase the entity with the given handle
		inline auto valid(VecsHandle handle) -> bool { m_system.valid(handle); }; //Check if the handle is valid

		template<typename... Ts> requires has_all_types<GL, Ts...>	//Check that all types are in the type list
		[[nodiscard]] auto insert(Ts&&... Args) -> VecsHandle { return m_system.insert(std::forward<Ts>(Args)...); };

		template<typename... Ts> 
		[[nodiscard]] auto get(VecsHandle handle) -> optional_ref_tuple<Ts...> { return m_system.get<Ts...>(handle); }; //Get a tuple of references to the components of the entity

		template<typename... Ts, typename... As> requires has_all_types<vtll::tl<Ts...>, As...>
		auto transform(VecsHandle handle, As&&... Args) -> bool { return m_system.transform<Ts...>(handle, std::forward<As>(Args)...); }; //Transform the entity into a new archetype, adding or removing components

	private:
		VecsSystem<TL> m_system;
	};





	#ifndef VecsUserEntityComponentList
	#define VecsUserEntityComponentList vtll::tl<>
	#endif

	#ifndef VecsSystemEntityComponentList
	#define VecsSystemEntityComponentList vtll::tl<>
	#endif

	using VecsEntityManager = VecsEntityManagerBase<  VecsUserEntityComponentList, VecsSystemEntityComponentList >;
	
}





/*
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
*/





