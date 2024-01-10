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
	template<typename TL, typename... As> class VecsArchetype;

	template<typename TL, typename... Ts>
	concept has_all_types = ((vtll::unique<vtll::tl<Ts...>>::value) && vtll::has_all_types<TL, vtll::tl<Ts...>>::value);

	const size_t NBITS = 40;

	using generation_t = vsty::strong_type_t< uint64_t, vsty::counter<>, std::integral_constant<uint64_t, std::numeric_limits<uint64_t>::max()> >;

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

	struct VecsHandle : public VecsIndex {  
		VecsHandle() = default;
		explicit VecsHandle( stack_index_t index, generation_t gen) { set_index(index); set_generation(gen); }
		explicit VecsHandle( stack_index_t index) { set_index(index); set_generation( generation_t{0ULL} ); }
	};

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
			VecsIndex m_index; //Index of the entity in the archetype. It is also used to point to the next free slot.
		};

	public:	
		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		using ref_tuple = vtll::to_ref_tuple< vtll::tl<Ts...> >;

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		using optional_ref_tuple = std::optional< ref_tuple<Ts...> >;

		VecsSystem(){};
		inline auto erase(VecsHandle handle) -> bool;
		inline auto valid(VecsHandle handle) -> bool { return get_entity_from_handle(std::forward<const VecsHandle>(handle)) != nullptr; };

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		[[nodiscard]] auto insert(Ts&&... Args) -> VecsHandle;

		template<typename... Ts> 
		[[nodiscard]] auto get(VecsHandle handle) -> optional_ref_tuple<Ts...>;

	private:
		inline auto get_entity_from_handle(VecsHandle handle) -> VecsEntity*;
		inline auto get_entity_from_index(VecsIndex index) -> VecsEntity*;

		template<typename... Ts> requires has_all_types<TL, Ts...>	//compute the bitset representing the types
		constexpr inline auto bitset() -> std::bitset<BITSTL>;

		vllt::VlltStack< vtll::tl<VecsEntity>, 1<<10, false > m_entities; //Container for all entities
		std::atomic<VecsIndex> m_empty_start{}; //Index of first empty entity to reuse, initialized to NULL value
		std::unordered_map<std::bitset<BITSTL>, std::shared_ptr<VecsArchetypeBase<TL>>> m_archetypes; //Entity groups
		std::unordered_multimap<size_t, std::shared_ptr<VecsArchetypeBase<TL>>> m_map_type_to_archetype;	//Map for going from a component type to groups of entities with them
	};


	template<typename TL>
	template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
	[[nodiscard]] auto VecsSystem<TL>::insert(Ts&&... Args) -> VecsHandle { 
		std::bitset<BITSTL> bits = bitset<Ts...>();	//Create bitset representing the archetype

		std::shared_ptr<VecsArchetypeBase<TL>> archetype_ptr; //create a new archetype if necessary
		using archetype = VecsArchetype<TL, Ts...>;
		if( !m_archetypes.contains(bits) ) {
			auto res = m_archetypes.emplace(bits, std::make_shared<archetype>( *this, bits ) );	//Create it
			archetype_ptr = res.first->second;

			auto f = [&]< typename T>() { //Register the archetype in the type map, for iterating over all archetypes with a certain type
				static const size_t idx = vtll::index_of<TL, T>::value;
				m_map_type_to_archetype.insert( std::make_pair( idx, archetype_ptr ) ); //each type creates a new entry in the map
			};
			(f.template operator()<Ts>(), ...);
		} else {
			archetype_ptr = m_archetypes.at(bits); //archetype already exists - return its address
		}
		VecsIndex arch_index = std::dynamic_pointer_cast<archetype>(archetype_ptr)->insert(std::forward<Ts>(Args)...);
	
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
			handle_index = VecsIndex{ m_entities.push( VecsEntity{ archetype_ptr, arch_index } ) };
			entity_ptr = get_entity_from_index(  handle_index );
		}
	
		return VecsHandle{handle_index.get_index(), entity_ptr->m_index.increase_generation()};
	}

	template<typename TL>
	inline auto VecsSystem<TL>::erase(VecsHandle handle) -> bool {

	}

	template<typename TL>
	template<typename... Ts>
	[[nodiscard]] auto VecsSystem<TL>::get(VecsHandle handle) -> optional_ref_tuple<Ts...> {
		auto entity_ptr = get_entity_from_handle(handle);
		if( !entity_ptr ) return {};
		if( bitset<Ts...>() != entity_ptr->m_archetype_ptr->bitset() ) return {}; //Check that the types match
		return entity_ptr->m_archetype_ptr->template get<Ts...>(entity_ptr->m_index);
	}

	template<typename TL>
	inline auto VecsSystem<TL>::get_entity_from_handle(VecsHandle handle) -> VecsEntity* {
		auto entity = m_entities.template get<0>( stack_index_t{ handle.get_index() } );
		if( !entity.has_value() || entity.value().get().m_index.get_generation() != handle.get_generation() ) return nullptr;
		return &entity.value().get();
	}

	template<typename TL>
	inline auto VecsSystem<TL>::get_entity_from_index(VecsIndex index) -> VecsEntity* {
		auto entity = m_entities.template get<0>( stack_index_t{ index.get_index() } );
		if( !entity.has_value()) return nullptr;
		return &entity.value().get();
	}

	template<typename TL>
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

	template<typename TL>
	class VecsArchetypeBase {

	protected:
		static_assert(vtll::unique<TL>::value, "VecsArchetype types are not unique!");
		static const size_t BITSTL = vtll::size<TL>::value;	//Number of bits in the bitset, i.e. number of component types ingroup
		using component_ptrs_t = std::array<void*, vtll::size<TL>::value>;

	public:
		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		using ptr_tuple = vtll::to_ptr_tuple< vtll::tl<Ts...> >;

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		using ref_tuple = vtll::to_ref_tuple< vtll::tl<Ts...> >;

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		using optional_ref_tuple = std::optional< ref_tuple<Ts...> >;

		VecsArchetypeBase(VecsSystem<TL>& system, std::bitset<BITSTL> types) : m_system{system}, m_types{types} {};

		template<typename... Ts>
		[[nodiscard]] auto get(VecsIndex index) -> optional_ref_tuple<Ts...>;

		inline auto bitset() -> std::bitset<BITSTL> { return m_types; } //Get the bitset representing the types

	protected:
		virtual auto get_pointers(stack_index_t index, component_ptrs_t& component_ptrs ) -> bool = 0; //Get pointers to the components in the archetype, fill into m_components

		VecsSystem<TL>&	m_system; //Reference to the the system
		std::bitset<BITSTL> m_types; //One bit for each type in the type list
	};


	template<typename TL>
	template<typename... Ts>
	auto VecsArchetypeBase<TL>::get(VecsIndex index) -> optional_ref_tuple<Ts...> {
		component_ptrs_t component_ptrs;
		if(!get_pointers(index.get_index(), component_ptrs)) return {};	//Get pointers to the components in the archetype, fill into m_component_ptrs
		return { std::tie( *static_cast<Ts*>(component_ptrs[vtll::index_of<TL, Ts>::value])... ) };
	}


	//-----------------------------------------------------------------------------------------------------------------
	//VecsArchetype


	template<typename TL, typename... As>
	class VecsArchetype : public VecsArchetypeBase<TL> {
		//template<typename TL, typename T, typename... Ts> friend class VecsIterator;
		//template<typename TL, typename T, typename... Ts> friend class VecsSRange;
		//template<typename TL> friend class VecsSystem;

	protected:
		using AL = vtll::tl<As...>;	//Typelist of all component types in the archetype

		using VecsArchetypeBase<TL>::BITSTL;
		using VecsArchetypeBase<TL>::m_system;
		using VecsArchetypeBase<TL>::m_types;
		using typename VecsArchetypeBase<TL>::component_ptrs_t;

		static_assert(vtll::unique<AL>::value, "VecsArchetype types are not unique!");
		using AAL = vtll::cat<AL, vtll::tl<VecsHandle> >;	//Typelist of all component types in the archetype, plus the handle

	public:
		VecsArchetype(VecsSystem<TL>& system, std::bitset<BITSTL> types) : VecsArchetypeBase<TL>{ system, types } {};

		template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
		[[nodiscard]] auto insert(Ts&&... Args) -> VecsIndex;

	private:
		virtual auto get_pointers(stack_index_t index, component_ptrs_t& component_ptrs) -> bool override; //Get pointers to the components in the archetype, fill into m_components
		VlltStack<AAL> m_data; //The data stored in this archetype
	};

	template<typename TL, typename... As>
	template<typename... Ts> requires has_all_types<TL, Ts...>	//Check that all types are in the type list
	[[nodiscard]] auto VecsArchetype<TL, As...>::insert(Ts&&... Args) -> VecsIndex { 
		return VecsIndex{ m_data.push( std::forward<Ts>(Args)..., VecsHandle{} ) };
	}

	template<typename TL, typename... As>
	auto VecsArchetype<TL, As...>::get_pointers( stack_index_t index, component_ptrs_t& component_ptrs ) -> bool {
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




} //namespace


