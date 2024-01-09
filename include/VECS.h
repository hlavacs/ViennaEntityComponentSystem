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

#include "VTLL.h"
#include "VSTY.h"
#include "VLLT.h"


namespace vecs {

	using stack_index_t = vllt::stack_index_t;

	template<typename DATA, size_t N0 = 1 << 10, bool ROW = true, size_t MINSLOTS = 16, size_t NUMBITS1 = 40>
	using VlltStack = vllt::VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>;

	template<typename TL> class VecsSystem;
	template<typename TL> class VecsArchetypeBase;
	template<typename TL, typename AL> class VecsArchetype;

	template<typename TL, typename... Ts>
	concept has_all_types = ((vtll::unique<vtll::tl<Ts...>>::value) && vtll::has_all_types<TL, vtll::tl<Ts...>>::value);

	const size_t NBITS = 40;

	using generation_t = vsty::strong_type_t< uint64_t, vsty::counter<>, std::integral_constant<uint64_t, std::numeric_limits<uint64_t>::max()> >;

	/// <summary>
	/// A VecsIndex is a 64-bit integer, with the first NBITS bits encoding the index
	/// of the uint64_t, and the rest encoding a generation counter. The generation counter is incremented each time the index
	/// is erased, so that old indices can be detected.
	/// </summary>
	struct index_t : public vsty::strong_type_t< uint64_t, vsty::counter<>, std::integral_constant<uint64_t, std::numeric_limits<uint64_t>::max()> > {
		stack_index_t get_index() const { return stack_index_t{ get_bits(0, NBITS) }; }
		generation_t get_generation() const { return generation_t{ get_bits(NBITS) }; }
		void set_index( stack_index_t index) { return set_bits( (uint32_t)index, 0, NBITS ); }
		void set_generation( generation_t gen) { return set_bits( (uint32_t)gen, NBITS ); }
		index_t() = default;
		index_t( stack_index_t index, generation_t gen) { set_index(index); set_generation(gen); }
		index_t( stack_index_t index) { set_index(index); set_generation( generation_t{0ULL} ); }
	};

	template<auto Phantom = vsty::counter<>> struct handle_index_t : public index_t{};
	using VecsHandleIndex = handle_index_t<>; 

	template<auto Phantom = vsty::counter<>> struct archetype_index_t : public index_t{};
	using VecsArchetypeIndex = archetype_index_t<>;

	template<auto Phantom = vsty::counter<>> struct handle_t : public index_t{};
	using VecsHandle = handle_t<>; 

	union VecsEntityIndex { VecsHandleIndex m_handle_index; VecsArchetypeIndex m_archetype_index; }; //can be used for both handles and archetypes


	//-----------------------------------------------------------------------------------------------------------------

	/// <summary>
	/// The main ECS class.
	/// </summary>
	/// <typeparam name="TL">A typelist storing all possible component types. Types MUST be UNIQUE!</typeparam>
	/// <typeparam name="NBITS">Number of bits for the index in the 64-bit handle. The rest are used for generation.</typeparam>
	template<typename TL>
	class VecsSystem {
		//template<typename TL, typename T, typename... Ts> friend class VecsIterator;
		//template<typename TL, typename T, typename... Ts> friend class VecsRange;

	protected:
		static_assert(vtll::unique<TL>::value, "VecsSystem types are not unique!");
		static const size_t BITSTL = vtll::size<TL>::value;	//Number of bits in the bitset, i.e. number of component types ingroup

		/// <summary>
		/// An enity is a list of components stored in an archetype. The VecsHandle indexes into the entity map storing VecsEntity structs.
		/// A VecsEntity points to an archetype, and has a VecsIndex, which also holds the generation counter.
		/// </summary>
		struct VecsEntity {
			std::shared_ptr<VecsArchetypeBase<TL>> m_archetype_ptr;	//The group this entity belongs to.
			VecsEntityIndex m_index; //Index of the entity in the archetype. It is also used to point to the next free slot.
		};

	public:	
		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		using optional_ref_tuple = std::optional< vtll::to_ref_tuple< vtll::tl<Ts...> > >;

		VecsSystem(){};
		inline auto erase(VecsHandle handle) -> bool;
		inline auto valid(VecsHandle handle) -> bool { return get_entity(std::forward<const VecsHandle>(handle)) != nullptr; };

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		[[nodiscard]] auto insert(Ts&&... Args) -> VecsHandle;

		template<typename... Ts> 
		[[nodiscard]] auto get(VecsHandle handle) -> optional_ref_tuple<Ts...>;


	private:
		inline auto get_entity(VecsHandle handle) -> VecsEntity*;
		inline auto get_entity(VecsHandleIndex index) -> VecsEntity*;

		vllt::VlltStack< vtll::tl<VecsEntity>, 1<<10, false > m_entities; //Container for all entities
		std::atomic<VecsHandleIndex> m_empty_start{}; //Index of first empty entity to reuse, initialized to NULL value
		std::unordered_map<std::bitset<BITSTL>, std::shared_ptr<VecsArchetypeBase<TL>>> m_archetypes; //Entity groups
		std::unordered_multimap<size_t, std::shared_ptr<VecsArchetypeBase<TL>>> m_map_type_to_archetype;	//Map for going from a component type to groups of entities with them
	};


	template<typename TL>
	inline auto VecsSystem<TL>::erase(VecsHandle handle) -> bool {

	}


	template<typename TL>
	template<typename... Ts>
	[[nodiscard]] auto VecsSystem<TL>::get(VecsHandle handle) -> optional_ref_tuple<Ts...> {
		auto entity_ptr = get_entity(handle);
		if( !entity_ptr ) return {};
		return(entity_ptr->m_archetype_ptr->get<Ts...>(entity_ptr->m_index.m_archetype_index));
	}


	template<typename TL>
	inline auto VecsSystem<TL>::get_entity(VecsHandle handle) -> VecsEntity* {
		auto entity = m_entities.template get<0>( stack_index_t{ handle.get_index() } );
		if( !entity.has_value() || entity.value().get().m_index.m_handle_index.get_generation() != handle.get_generation() ) return nullptr;
		return &entity.value().get();
	}

	template<typename TL>
	inline auto VecsSystem<TL>::get_entity(VecsHandleIndex index) -> VecsEntity* {
		auto entity = m_entities.template get<0>( stack_index_t{ index.get_index() } );
		if( !entity.has_value()) return nullptr;
		return &entity.value().get();
	}
	

	//-----------------------------------------------------------------------------------------------------------------
	//Archetypes

	template<typename TL>
	class VecsArchetypeBase {

	protected:
		static_assert(vtll::unique<TL>::value, "VecsArchetype types are not unique!");
		static const size_t BITSTL = vtll::size<TL>::value;	//Number of bits in the bitset, i.e. number of component types ingroup
		using component_ptrs_t = std::array<void*, vtll::size<TL>::value>;

	public:

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		using optional_ref_tuple = typename VecsSystem<TL>::template optional_ref_tuple<Ts...>;

		VecsArchetypeBase(VecsSystem<TL>& system, std::bitset<BITSTL> types) : m_system{system}, m_types{types} {};

		template<typename... Ts>
		[[nodiscard]] auto get(VecsHandle handle) -> optional_ref_tuple<Ts...>;

	protected:
		virtual auto get_pointers(stack_index_t index, component_ptrs_t& component_ptrs ) -> bool{ return true;}; //Get pointers to the components in the archetype, fill into m_components

		VecsSystem<TL>&	m_system; //Reference to the the system
		std::bitset<BITSTL> m_types; //One bit for each type in the type list
	};


	template<typename TL>
	template<typename... Ts>
	auto VecsArchetypeBase<TL>::get(VecsHandle handle) -> optional_ref_tuple<Ts...> {
		auto ent = m_entities.template get<0>( stack_index_t{ handle.get_index() } );
		if( !ent.has_value() || ent.value().get().m_index.get_generation() != handle.get_generation() ) return {};
		VecsEntity& entity = ent.value().get();

		component_ptrs_t m_component_ptrs;
		get_pointers(entity.m_index.get_index());	//Get pointers to the components in the archetype, fill into m_component_ptrs

		optional_ref_tuple<Ts...> tuple;
		vtll::static_for<size_t, 0, vtll::size< vtll::tl<Ts...>::value > >( // Loop over all components
			[&](auto i) { 
				using T = vtll::type_at<vtll::tl<Ts...>, i>; //Get type of component
				static const size_t idx = vtll::index_of<TL, T>::value; //Get index of component in type list
				if(!m_types.test(idx)) return {}; //Does the entity contain the type? If not, return empty tuple
				std::get<i>(tuple) = std::ref( *static_cast<T*>(m_component_ptrs[idx]) ); //Get pointer to component
			} 
		);
		return { tuple };
	}


	template<typename TL, typename AL>
	class VecsArchetype : public VecsArchetypeBase<TL> {
		//template<typename TL, typename T, typename... Ts> friend class VecsIterator;
		//template<typename TL, typename T, typename... Ts> friend class VecsSRange;
		//template<typename TL> friend class VecsSystem;

	protected:
		using VecsArchetypeBase<TL>::BITSTL;
		using VecsArchetypeBase<TL>::m_system;
		using VecsArchetypeBase<TL>::m_types;
		using typename VecsArchetypeBase<TL>::component_ptrs_t;

		static_assert(vtll::unique<AL>::value, "VecsArchetype types are not unique!");
		using AAL = vtll::cat<AL, vtll::tl<VecsHandle> >;	//Typelist of all component types in the archetype, plus the handle

	public:
		VecsArchetype(VecsSystem<TL>& system, std::bitset<BITSTL> types) : VecsArchetypeBase<TL>{ system, types } {};

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		[[nodiscard]] auto insert(Ts&&... Args) -> VecsArchetypeIndex;

	private:
		virtual auto get_pointers(stack_index_t index, component_ptrs_t& component_ptrs) -> bool override; //Get pointers to the components in the archetype, fill into m_components
		VlltStack<AAL> m_data; //The data stored in this archetype
	};


	template<typename TL, typename AL>
	template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
	[[nodiscard]] auto VecsArchetype<TL, AL>::insert(Ts&&... Args) -> VecsArchetypeIndex { 
		stack_index_t idx = m_data.push( std::forward<Ts>(Args)..., VecsHandle{} );
		VecsArchetypeIndex aidx{ };
		aidx.set_index( idx );
		return aidx; 
	}


	template<typename TL, typename AL>
	auto VecsArchetype<TL, AL>::get_pointers( stack_index_t index, component_ptrs_t& component_ptrs ) -> bool {
		auto tuple = m_data.get_tuple(index);
		if( !tuple.has_value() ) return false;
		vtll::static_for<size_t, 0, vtll::size<AL>::value >( // Loop over all components
			[&](auto i) { 
				using T = vtll::Nth_type<AL, i>; //Get type of component
				static const auto idx = vtll::index_of<TL, T>::value; //Get index of component in type list
				component_ptrs[idx] = &std::get<i>(tuple.value()); //Get pointer to component
			} 
		);
		return true;
	}


	//-----------------------------------------------------------------------------------------------------------------
	//Leftovers

	template<typename TL>
	template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
	[[nodiscard]] auto VecsSystem<TL>::insert(Ts&&... Args) -> VecsHandle { 
		std::bitset<BITSTL> bits;

		auto groupType = [&]<typename T>() { //Create bitset representing the archetype
			static const uint32_t idx = vtll::index_of<TL, T>::value;
			bits.set(idx);	//Fill the bits of bitset representing the types
		};
		(groupType.template operator() < Ts > (), ...);

		std::shared_ptr<VecsArchetypeBase<TL>> archetype_ptr; //create a new archetype is necessary
		using archetype = VecsArchetype<TL, vtll::tl<Ts...>>;
		if( !m_archetypes.contains(bits) ) {
			auto res = m_archetypes.emplace(bits, std::make_shared<archetype>( *this, bits ) );	//Create it
			archetype_ptr = res.first->second;

			auto f = [&]< typename T>() { //Register the archetype in the type map, for iterating over all archetypes with a certain type
				static const size_t idx = vtll::index_of<TL, T>::value;
				m_map_type_to_archetype.insert( std::make_pair( idx, archetype_ptr ) ); //each type creates a new entry in the map
			};
			(f.template operator()<Ts>(), ...);
		} else {
			//archetype_ptr = m_archetypes.at(bits); //It exists - return its address
		}
		auto ptr = std::dynamic_pointer_cast<archetype>(archetype_ptr);
		VecsArchetypeIndex arch_index = ptr->insert(std::forward<Ts>(Args)...);
	
		/*
		VecsHandleIndex handle_index = m_empty_start.load();
		VecsEntity *entity_ptr{nullptr};
		if( handle_index.has_value() ) { //there is a free slot to make use of
			entity_ptr = get_entity(handle_index);
			VecsHandleIndex new_handle_index = entity_ptr->m_index.m_handle_index; //Get the next free slot
			while( handle_index.has_value() 
				&& !m_empty_start.compare_exchange_weak( handle_index, new_handle_index ) ) {
					entity_ptr = get_entity(handle_index);
					new_handle_index = entity_ptr->m_index.m_handle_index;
			}
		}
		else {
			handle_index.set_index( m_entities.push( VecsEntity{ archetype_ptr, arch_index } ) );
			entity_ptr = get_entity(handle_index);
		}
		auto generation = entity_ptr->m_index.m_handle_index.get_generation();

		VecsHandle result;
		result.set_index( handle_index.get_index() );
		result.set_generation( generation_t { generation.value() + 1 } );
		return {result};
		*/
		return {};
	}






} //namespace


