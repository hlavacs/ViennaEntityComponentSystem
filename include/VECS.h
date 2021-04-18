#ifndef VECS_H
#define VECS_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <variant>
#include <array>
#include <memory>
#include <optional>
#include <functional>
#include <chrono>
#include <ranges>
#include "VTLL.h"
#include "VECSTable.h"

using namespace std::chrono_literals;


//system related defined component types and entity types
#ifndef VECS_SYSTEM_DATA
#include "VECSCompSystem.h"
#endif

//user defined component types and entity types
#ifndef VECS_USER_DATA
#include "VECSCompUser.h"  
#endif

namespace vecs {

	/** 
	* \brief Component type list: a list with all component types that an entity can have.
	* 
	* It is the sum of the components of the engine part, and the components as defined by the engine user
	*/
	using VecsComponentTypeList = vtll::cat< VeSystemComponentTypeList, VeUserComponentTypeList >;

	/**
	* \brief Entity type list: a list with all possible entity types the ECS can deal with.
	* 
	* An entity type is a collection of component types. 
	* The list is the sum of the entitiy types of the engine part, and the entity types as defined by the engine user.
	*/
	using VecsEntityTypeList = vtll::cat< VeSystemEntityTypeList, VeUserEntityTypeList >;

	/**
	* \brief Table size map: a VTLL map specifying the default sizes for component tables.
	* 
	* It is the sum of the maps of the engine part, and the maps as defined by the engine user
	* Every entity type can have an entry, but does not have to have one.
	* An entry is alwyas a vtll::value_list<A,B>, where A defines the default size of segments, and B defines
	* the max number of entries allowed. Both A and B are actually exponents with base 2. So segemnts have size 2^A.
	*/
	using VecsTableSizeMap = vtll::cat< VeSystemTableSizeMap, VeUserTableSizeMap >;

	/**
	* \brief This struct implements the exponential function power of 2.
	* 
	* It is used to transfer table size entries to their real values, so we can compute max and sum of all values.
	*/
	template<typename T>
	struct left_shift_1 {
		using type = std::integral_constant<size_t, 1 << T::value>;
	};

	/**
	* Table constants retrieves mappings for all entity types from the VecsTableSizeMap (which have the format vtll::value_list<A,B>).
	* Then it turns the value lists to type lists, each value is stored in a std::integral_constant<size_t, V> type.
	*/
	using VeTableSizeDefault = vtll::value_list< 10, 16 >;
	using VecsTableConstants = vtll::transform < vtll::apply_map<VecsTableSizeMap, VecsEntityTypeList, VeTableSizeDefault>, vtll::value_to_type>;

	/**
	* Get the maximal exponent from the list of segment size exponents.
	* 
	* This is used as segment size (exponent) for the map table in the VecsRegistryBaseClass. Since this table holds info to 
	* all entities in the ECS (the aggregate), it makes sense to take the maximum for segmentation.
	*/
	using VecsTableMaxSegExp	= vtll::max< vtll::transform< VecsTableConstants, vtll::front > >;
	using VecsTableMaxSeg		= typename left_shift_1<VecsTableMaxSegExp>::type;

	/**
	* Compute the sum of all max sizes of for all entity types.
	* 
	* First get the second number (vtll::back) from the map, i.e. the exponents of the max number of entities of type E.
	* Then use the 2^ function (vtll::function<L, left_shift_1>) to get the real max number. Then sum over all entity types.
	*/
	using VecsTableMaxSizeSum = vtll::sum< vtll::function< vtll::transform< VecsTableConstants, vtll::back >, left_shift_1 > >;

	/**
	* Since the sum of all max sizes is probably not divisible by the segment size, get the next power of 2 larger or equ VecsTableMaxSeg.
	*/
	using VecsTableMaxSize = vtll::smallest_pow2_larger_eq<VecsTableMaxSeg>;

	using VecsTableLayoutMap = vtll::cat< VeSystemTableLayoutMap, VeUserTableLayoutMap >;

	/** basic concepts */
	template<typename C>
	concept is_component_type = (vtll::has_type<VecsComponentTypeList, std::decay_t<C>>::value); ///< C is a component

	template<typename... Cs>
	concept are_component_types = (is_component_type<Cs> && ...);	///< Cs are all components

	template<typename E>
	concept is_entity_type = (vtll::has_type<VecsEntityTypeList, std::decay_t<E>>::value); ///< E is an entity type

	template<typename... Es>
	concept are_entity_types = (is_entity_type<Es> && ...);		///< Es are all entity types

	template<typename E, typename C>
	concept is_component_of = (vtll::has_type<E, std::decay_t<C>>::value);	///< C is a component of E

	template<typename E, typename... Cs>
	concept are_components_of = (is_component_of<E, Cs> && ...);	///< Cs are all components of E

	template<typename E, typename... Cs>
	concept is_composed_of = (vtll::is_same<E, std::decay_t<Cs>...>::value);	///< E is composed of Cs

	template<typename ET, typename E = vtll::front<ET>>
	concept is_tuple = (is_entity_type<E> && std::is_same_v<std::decay_t<ET>, vtll::to_tuple<E>>); ///< ET is a std::tuple

	template<typename E, typename... Cs>
	concept is_iterator = (std::is_same_v<std::decay_t<E>, VecsIterator<Cs...>>);	///< E is composed of Cs

	/**
	* Declarations of the main VECS classes
	*/
	class VecsReadLock;
	class VecsWriteLock;
	class VecsHandle;
	template<typename E> class VecsComponentAccessor;
	template<typename E, size_t I> class VecsComponentAccessorDerived;
	template<typename E> class VecsComponentTable;
	class VecsRegistryBaseClass;
	template<typename E> class VecsRegistry;
	template<typename... Ts> requires (are_component_types<Ts...> || are_entity_types<Ts...>) class VecsIterator;
	template<typename... Cs> class VecsIteratorEntityBaseClass;
	template<typename E, typename... Cs> class VecsIteratorEntity;
	template<typename... Cs> requires (are_component_types<Cs...> || are_entity_types<Cs...>) class VecsRange;


	/** 
	* \brief General functor type that can hold any function, and depends in a number of component types.
	* 
	* Used in for_each to iterate over entities and call the functor for each entity.
	*/
	template<typename C>
	struct Functor;

	template<template<typename...> typename Seq, typename... Cs>
	struct Functor<Seq<Cs...>> {
		using type = void(VecsHandle, Cs&...);
	};

	//-------------------------------------------------------------------------
	//entity handle

	/**
	* \brief Handles are IDs of entities. Use them to access entitites.
	* 
	* VecsHandle are used to ID entities of type E by storing their type as an index.
	*/
	class VecsHandle {
		friend class VecsReadLock;
		friend class VecsWriteLock;
		friend VecsRegistryBaseClass;
		template<typename E> friend class VecsRegistry;
		template<typename E> friend class VecsComponentTable;

	protected:
		index_t		m_entity_index{};			///< The slot of the entity in the entity list
		counter16_t	m_generation_counter{};		///< Generation counter
		index16_t	m_type_index{};				///< Type index

	public:
		VecsHandle() noexcept {}; ///< Empty constructor of class VecsHandle

		/** 
		* \brief Constructor of class VecsHandle.
		* \param[in] idx The index of the handle in the aggregate slot map of class VecsRegistryBaseClass
		* \param[in] cnt Current generation counter identifying this entity on in the slot.
		* \param[in] type Type index for the entity type E.
		*/
		VecsHandle(index_t idx, counter16_t cnt, index16_t type) noexcept
			: m_entity_index{ idx }, m_generation_counter{ cnt }, m_type_index{ type } {};

		inline auto is_valid() noexcept	-> bool;	///< The data in the handle is non null (internally synchronized)
		auto has_value() noexcept		-> bool;	///< The entity that is pointed to exists in the ECS (externally synchronized)

		template<typename C>
		requires is_component_type<C>
		auto has_component() noexcept	-> bool;	///< Return true if the entity type has a component C (internally synchronized)

		template<typename C>
		requires is_component_type<C> 
		auto component() noexcept -> C;				///< Get a component of type C of the entity (first found is copied)  (internally synchronized)

		template<typename ET>
		requires is_tuple<ET>
		auto update(ET&& ent) noexcept -> bool;		///< Update this entity using a std::tuple of the same type (internally synchronized)

		template<typename C>
		requires is_component_type<C>
		auto update(C&& comp) noexcept -> bool;		///< Update a component of type C (internally synchroinized)

		auto erase() noexcept -> bool;				///< Erase the entity (internally synchroinized)

		auto index() noexcept -> index_t;			///< Get index of this entity in the component table (externally synchronized)
		auto type() noexcept  -> index16_t;

		std::atomic<uint32_t>* mutex();				///< \returns address of the VECS mutex for this entity (internally synchronized)

		bool operator==(const VecsHandle& rhs) {	///< Equality operator (externally synchronized)
			return m_entity_index == rhs.m_entity_index && m_generation_counter == rhs.m_generation_counter && m_type_index == rhs.m_type_index;
		}
	};


	//-------------------------------------------------------------------------
	//component vector - each entity type has them

	/**
	* \brief Base class for dispatching accesses to entity components if the entity type is not known.
	*/
	template<typename E>
	class VecsComponentAccessor {
	public:
		VecsComponentAccessor() noexcept {};	///<Constructor
		virtual auto updateC(index_t index, size_t compidx, void* ptr, size_t size) noexcept		-> bool = 0;		///< Empty update
		virtual auto componentE(index_t entidx, size_t compidx, void* ptr, size_t size)  noexcept	-> bool = 0;	///< Empty component read
		virtual auto has_componentE()  noexcept														-> bool = 0;	///< Test for component
	};


	/**
	* \brief This class stores all components of entities of type E.
	*/
	template<typename E>
	class VecsComponentTable : public VecsMonostate<VecsComponentTable<E>> {
		template<typename E, size_t I> friend class VecsComponentAccessorDerived;
		friend class VecsRegistryBaseClass;
		template<typename E> friend class VecsRegistry;
		template<typename... Ts> requires (are_component_types<Ts...> || are_entity_types<Ts...>) friend class VecsIterator;
		template<typename E, typename... Cs> friend class VecsIteratorEntity;

	protected:
		using value_type = vtll::to_tuple<E>;			///< A tuple storing all components of entity of type E
		using ref_type   = vtll::to_ref_tuple<E>;		///< A tuple storing references to all components of entity of type E
		using ptr_type	 = vtll::to_ptr_tuple<E>;		///< A tuple storing references to all components of entity of type E
		using layout_type = vtll::map<VecsTableLayoutMap, E, VECS_LAYOUT_DEFAULT>;

		using info = vtll::type_list<VecsHandle, std::atomic<uint32_t>*>;	///< List of management data per entity (handle and mutex)
		static const size_t c_handle = 0;		///< Component index of the handle info
		static const size_t c_mutex = 1;		///< Component index of the handle info
		static const size_t c_info_size = 2;	///< Index where the entity data starts

		using types = vtll::cat< info, E >;					///< List with management and component types
		using types_deleted = vtll::type_list< index_t >;	///< List with types for holding info about erased entities 

		/** Power of 2 exponent for the size of segments inthe tables */
		static const size_t c_segment_size	= vtll::front_value< vtll::map< VecsTableSizeMap, E, VeTableSizeDefault > >::value;
		
		/** Power of 2 exponent for the max number of entries in the tables */
		static const size_t c_max_size		= vtll::back_value<  vtll::map< VecsTableSizeMap, E, VeTableSizeDefault > >::value;

		static inline VecsTable<types,			c_segment_size, layout_type::value>		m_data;		///< Data per entity
		static inline VecsTable<types_deleted,  c_segment_size, VECS_LAYOUT_ROW::value>	m_deleted;	///< Table holding the indices of erased entities

		/** Each component type C of the entity type E gets its own specialized class instance */
		static inline std::array<std::unique_ptr<VecsComponentAccessor<E>>, vtll::size<VecsComponentTypeList>::value> m_dispatch;

		auto updateC(index_t entidx, size_t compidx, void* ptr, size_t size) noexcept		-> bool;
		auto componentE(index_t entidx, size_t compidx, void* ptr, size_t size)  noexcept	-> bool;
		auto has_componentE(size_t compidx)  noexcept										-> bool; ///< Test for component
		auto remove_deleted_tail() noexcept -> void;	///< Remove empty slots at the end of the table

		VecsComponentTable(size_t r = 1 << c_max_size) noexcept;

		template<typename... Cs>
		requires is_composed_of<E, Cs...> [[nodiscard]]
		auto insert(VecsHandle handle, std::atomic<uint32_t>* mutex, Cs&&... args) noexcept	-> index_t;

		auto pointers(const index_t index) noexcept				-> ptr_type;
		auto values(const index_t index) noexcept				-> value_type;
		auto handle(const index_t index) noexcept				-> VecsHandle;
		auto mutex(const index_t index) noexcept				-> std::atomic<uint32_t>*;

		/** \returns the number of entries currently in the table, can also be invalid ones */
		auto size() noexcept									-> size_t { return m_data.size(); };
		auto erase(const index_t idx) noexcept					-> bool;
		auto compress() noexcept								-> void;
		auto clear() noexcept									-> size_t;
		auto max_capacity(size_t) noexcept						-> size_t;

		template<typename C>
		requires is_component_of<E, C>
		auto component(const index_t index) noexcept			-> C&;

		template<typename ET>
		requires is_tuple<ET, E>
		auto update(const index_t index, ET&& ent) noexcept		-> bool;

		template<typename C>
		requires is_component_of<E, C>
		auto update(const index_t index, C&& comp) noexcept		-> bool;

		auto swap(index_t n1, index_t n2) -> bool { return m_data.swap(n1, n2); }

	};

	/**
	* \brief Dispatch an update call to the correct specialized class instance.
	*
	* \param[in] entidx Entity index in the component table.
	* \param[in] compidx Index of the component of the entity.
	* \param[in] ptr Pointer to the component data to use.
	* \param[in] size Size of the component data.
	* \returns true if the update was successful
	*/
	template<typename E> 
	inline auto VecsComponentTable<E>::updateC(index_t entidx, size_t compidx, void* ptr, size_t size) noexcept -> bool {
		return m_dispatch[compidx]->updateC(entidx, compidx, ptr, size);
	}

	/**
	* \brief Test whether an entity has a component with an index
	*
	* \param[in] entidx Entity index in the component table.
	* \param[in] compidx Index of the component of the entity.
	* \returns true if the entity contains the component
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::has_componentE(size_t compidx)  noexcept -> bool {
		return m_dispatch[compidx]->has_componentE();
	}

	/**
	* \brief Dispatch an component call to the correct specialized class instance.
	*
	* \param[in] entidx Entity index in the component table.
	* \param[in] compidx Index of the component of the entity.
	* \param[in] ptr Pointer to the component data to store into.
	* \param[in] size Size of the component data.
	* \returns true if the retrieval was successful
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::componentE(index_t entidx, size_t compidx, void* ptr, size_t size)  noexcept -> bool {
		return m_dispatch[compidx]->componentE(entidx, compidx, ptr, size);
	}

	/**
	* \brief Insert data for a new entity into the component table.
	* 
	* \param[in] handle The handle of the new entity.
	* \param[in] args The entity components to be stored
	* \returns the index of the new entry in the component table.
	*/
	template<typename E> 
	template<typename... Cs>
	requires is_composed_of<E, Cs...> [[nodiscard]]
	inline auto VecsComponentTable<E>::insert(VecsHandle handle, std::atomic<uint32_t>* mutex, Cs&&... args) noexcept -> index_t {
		auto idx = m_data.push_back();			///< Allocate space a the end of the table
		if (!idx.has_value()) return idx;		///< Check if the allocation was successfull
		m_data.update<c_handle>(idx, handle);	///< Update the handle data
		m_data.update<c_mutex>(idx, mutex);		///< Update the mutex pointer

		auto f = [&]<typename T>(T&& tup) {
			vtll::static_for<size_t, 0, std::tuple_size<T>::value >(							///< Loop over all components
				[&](auto i) {
					using type = std::tuple_element_t<i, T>;
					m_data.update<c_info_size + i>(idx, std::forward<type>(std::get<i>(tup))); ///< Update the entity components
				}
			);
		};
		f(std::forward_as_tuple(args...));

		//(m_data.update<c_info_size + vtll::index_of<E,std::decay_t<Cs>>::value> (idx, std::forward<Cs>(args)), ...); ///< Update the entity components
		return idx;								///< Return the index of the new data
	};

	/**
	* \param[in] index The index of the data in the component table.
	* \returns a tuple holding the components of an entity.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::pointers(const index_t index) noexcept -> typename VecsComponentTable<E>::ptr_type {
		assert(index < m_data.size());
		auto tup = m_data.tuple_ptr(index);												///< Get the whole data from the data
		return vtll::sub_tuple< c_info_size, std::tuple_size_v<decltype(tup)> >(tup);	///< Return only entity components in a subtuple
	}


	/**
	* \param[in] index The index of the data in the component table.
	* \returns a tuple holding the components of an entity.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::values(const index_t index) noexcept -> typename VecsComponentTable<E>::value_type {
		assert(index < m_data.size());
		auto tup = m_data.tuple_value(index);											///< Get the whole data from the data
		return vtll::sub_tuple< c_info_size, std::tuple_size_v<decltype(tup)> >(tup);	///< Return only entity components in a subtuple
	}

	/**
	* \param[in] index The index of the entity in the component table.
	* \returns the handle of an entity from the component table.
	*/
	template<typename E> 
	inline auto VecsComponentTable<E>::handle(const index_t index) noexcept -> VecsHandle {
		assert(index < m_data.size());
		return m_data.comp_ref_idx<c_handle>(index);	///< Get ref to the handle and return it
	}

	/**
	* \param[in] index The index of the entity in the component table.
	* \returns pointer to the sync mutex of the entity.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::mutex(const index_t index) noexcept -> std::atomic<uint32_t>* {
		assert(index < m_data.size());
		return m_data.comp_ref_idx<c_mutex>(index);		///< Get ref to the mutex and return it
	}

	/**
	* \param[in] index Index of the entity in the component table.
	* \returns the component of type C for an entity.
	*/
	template<typename E>
	template<typename C>
	requires is_component_of<E, C>
	inline auto VecsComponentTable<E>::component(const index_t index) noexcept -> C& {
		assert(index < m_data.size());
		return m_data.comp_ref_idx<c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index); ///< Get ref to the entity and return component
	}

	/**
	* \brief Update all components using a reference to a std::tuple instance.
	* \param[in] index Index of the entity to be updated in the component table.
	* \param[in] ent Universal reference to the new entity data.
	* \returns true if the update was successful.
	*/
	template<typename E>
	template<typename ET>
	requires is_tuple<ET, E>
	inline auto VecsComponentTable<E>::update(const index_t index, ET&& ent) noexcept -> bool {
		vtll::static_for<size_t, 0, vtll::size<E>::value >(							///< Loop over all components
			[&](auto i) {
				using type = vtll::Nth_type<E,i>;
				m_data.update<c_info_size + i>(index, std::forward<type>(get<i>(ent)));	///< Update each component
			}
		);
		return true;
	}

	/**
	* \brief Update a component of type C for an entity of type E.
	*
	* \param[in] handle The entity handle.
	* \param[in] comp The component data.
	* \returns true if the operation was successful.
	*/
	template<typename E>
	template<typename C>
	requires is_component_of<E, C>
	inline auto VecsComponentTable<E>::update(const index_t index, C&& comp) noexcept -> bool {
		return m_data.update(index, std::forward<C>(comp));
	}

	/**
	* \brief Erase the component data for an entity.
	* 
	* The data is not really erased but the handle is invalidated. The index is also pushed to the deleted table.
	* \param[in] index The index of the entity data in the component table.
	* \returns true if the data was set to invalid.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::erase(const index_t index) noexcept -> bool {
		assert(index < m_data.size());
		m_data.comp_ref_idx<c_handle>(index) = {};		///< Invalidate handle	
		m_deleted.push_back(std::make_tuple(index));	///< Push the index to the deleted table.

		vtll::static_for<size_t, 0, vtll::size<E>::value >(			///< Loop over all components
			[&](auto i) {
				using type = vtll::Nth_type<E, i>;
				if constexpr (std::is_destructible_v<type> && !std::is_trivially_destructible_v<type>) {
					m_data.comp_ref_idx<c_info_size + i>(index).~type();
				}
			}
		);

		return true;
	}

	/**
	* \brief Set the max number of entries that can be stored in the component table.
	* 
	* If the new max is smaller than the previous one it will be ignored. Tables cannot shrink.
	*
	* \param[in] r New max number or entities that can be stored.
	* \returns the current max number, or the new one.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::max_capacity(size_t r) noexcept -> size_t {
		m_data.max_capacity(r);
		return m_deleted.max_capacity(r);
	}


	//-------------------------------------------------------------------------
	//component accessor derived class

	/**
	* \brief For dispatching accesses to entity components if the entity type is not known at compile time by the caller.
	* 
	* It is used to access component I of entity type E. This is called through a dispatch table, one entry
	* for each component of E. 
	*/
	template<typename E, size_t I>
	class VecsComponentAccessorDerived : public VecsComponentAccessor<E> {
	public:
		using C = vtll::Nth_type<VecsComponentTypeList, I>;	///< Component type

		/** Constructor of class VecsComponentAccessorDerived */
		VecsComponentAccessorDerived() noexcept : VecsComponentAccessor<E>() {}

	protected:

		/**
		* \brief Update the component C of entity E
		* \param[in] index Index of the entity in the component table.
		* \param[in] comp Universal reference to the new component data.
		* \returns true if the update was successful.
		*/
		auto update(const index_t index, C&& comp) noexcept -> bool {
			if constexpr (is_component_of<E, C>) {
				VecsComponentTable<E>().m_data.comp_ref_idx<VecsComponentTable<E>::c_info_size + vtll::index_of<E, std::decay_t<C>>::value>() = comp;
				return true;
			}
			return false;
		}

		/**
		* \brief Update the component C of entity E.
		* \param[in] index Index of the entity in the component table.
		* \param[in] compidx Component index incomponent type list.
		* \param[in] ptr Pointer to where the data comes from.
		* \param[in] size Size of the component data.
		* \returns true if the update was successful.
		*/
		auto updateC(index_t index, size_t compidx, void* ptr, size_t size) noexcept -> bool {
			if constexpr (is_component_of<E, C>) {
				VecsComponentTable<E>().m_data.comp_ref_idx<VecsComponentTable<E>::c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index) = *((C*)ptr);
				return true;
			}
			return false;
		}

		/**
		* \brief Test whether an entity of type E contains a component of type C.
		* \returns true if the retrieval was successful.
		*/
		auto has_componentE()  noexcept -> bool {
			if constexpr (is_component_of<E, C>) {
				return true;
			}
			return false;
		}

		/**
		* \brief Get component data.
		* \param[in] index Index of the entity in the component table.
		* \param[in] compidx Component index incomponent type list.
		* \param[in] ptr Pointer to where the data should be copied to.
		* \param[in] size Size of the component data.
		* \returns true if the retrieval was successful.
		*/
		auto componentE(index_t entidx, size_t compidx, void* ptr, size_t size)  noexcept -> bool {
			if constexpr (is_component_of<E, C>) {
				*((C*)ptr) = VecsComponentTable<E>().m_data.comp_ref_idx<VecsComponentTable<E>::c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(entidx);
				return true;
			}
			return false;
		}
	};


	/**
	* \brief Constructor of class VecsComponentTable.
	* 
	* Initializes the class and reserve memory for the tables, and initialize
	* the component dispatch. Every possible component gets its own entry in the dispatch
	* array. 
	* 
	* \param[in] r Max capacity for the data table.
	*/
	template<typename E>
	inline VecsComponentTable<E>::VecsComponentTable(size_t r) noexcept {
		if (!this->init()) return;
		m_data.max_capacity(r);			///< Set max capacities
		m_deleted.max_capacity(r);

		vtll::static_for<size_t, 0, vtll::size<VecsComponentTypeList>::value >( ///< Create dispatch table for all component types
			[&](auto i) {
				m_dispatch[i] = std::make_unique<VecsComponentAccessorDerived<E, i>>();
			}
		);
	};


	//-------------------------------------------------------------------------
	//entity registry base class

	/**
	* \brief This class stores all generalized handles of all entities, and can be used
	* to insert, update, read and delete all entity types. 
	* 
	* The base class does not depend on template parameters. If a member function is invoked,
	* the base class forwards the call to the respective derived class with the correct entity type E.
	* Since types have indexes, it needs an array holding instances of the derived classes for dispatching
	* the call to the correct subclass.
	* 
	* The base class has a mapping table storing information about all currently valid entities. From there
	* the respective subclass for type E can find the component data in the component table of type E.
	*/

	class VecsRegistryBaseClass : public VecsMonostate<VecsRegistryBaseClass> {

		friend class VecsHandle;
		template<typename E> friend class VecsComponentTable;

	protected:

		using types = vtll::type_list<index_t, counter16_t, index16_t, std::atomic<uint32_t>>;	///< Type for the table
		static const uint32_t c_index{ 0 };		///< Index for accessing the index to next free or entry in component table
		static const uint32_t c_counter{ 1 };	///< Index for accessing the generation counter
		static const uint32_t c_type{ 2 };		///< Index for accessing the type index
		static const uint32_t c_mutex{ 3 };		///< Index for accessing the lock mutex

		static inline VecsTable<types, VecsTableMaxSegExp::value, VECS_LAYOUT_ROW::value> m_entity_table;	///< The main mapping table
		static inline std::mutex				m_mutex;		///< Mutex for syncing insert and erase
		static inline index_t					m_first_free{};	///< First free entry to be reused
		static inline std::atomic<uint32_t>		m_size{0};		///< Number of valid entities in the map

		/** Every subclass for entity type E has an entry in this table. */
		static inline std::array<std::unique_ptr<VecsRegistryBaseClass>, vtll::size<VecsEntityTypeList>::value> m_dispatch;

		/** Virtual function for dispatching component updates to the correct subclass for entity type E. */
		virtual auto updateC(VecsHandle handle, size_t compidx, void* ptr, size_t size) noexcept -> bool { return false; };

		/** Virtual function for dispatching component reads to the correct subclass for entity type E. */
		virtual auto componentE(VecsHandle handle, size_t compidx, void*ptr, size_t size) noexcept -> bool { return false; };

		/** Virtual function for dispatching whether an entity type E contains a component type C */
		virtual auto has_componentE(VecsHandle handle, size_t compidx) noexcept -> bool { return false; };

	public:
		VecsRegistryBaseClass( size_t r = VecsTableMaxSize::value ) noexcept;

		//-------------------------------------------------------------------------
		//insert data

		template<typename E, typename... Cs>
		requires is_composed_of<E, Cs...> [[nodiscard]]
		auto insert(Cs&&... args) noexcept	-> VecsHandle;	///< Insert a new entity into the ECS (internally synchronized)

		//-------------------------------------------------------------------------
		//get data

		template<typename C>
		requires is_component_type<C>
		auto has_component(VecsHandle handle) noexcept	-> bool;	///< \returns true if the entity of type E has a component of type C (internally synchronized)

		template<typename C>
		requires is_component_type<C>
		auto component(VecsHandle handle) noexcept -> C;			///< Get a component of type C (internally synchronized)

		//-------------------------------------------------------------------------
		//update data

		template<typename ET>
		requires is_tuple<ET>
		auto update(VecsHandle handle, ET&& ent) noexcept -> bool;		///< Update a whole entity with a tuple (internally synchronized)

		template<typename C>
		requires is_component_type<C>
		auto update(VecsHandle handle, C&& comp) noexcept -> bool;		///< Update component of type C of an entity (internally synchronized)

		//-------------------------------------------------------------------------
		//erase

		virtual auto erase(VecsHandle handle) noexcept -> bool;		///< Erase a specific entity (internally synchronized)
		auto clear() noexcept -> size_t;					///< Clear the whole ECS (internally synchronized)
		auto compress() noexcept -> void;					///< Compress all component tables (externally synchronized)

		//-------------------------------------------------------------------------
		//looping 

		template<template<typename...> typename R, typename... Cs>
		requires (std::is_same_v<R<Cs...>, VecsRange<Cs...>> && are_component_types<Cs...>)
		auto for_each(R<Cs...>&& r, std::function<typename Functor<vtll::type_list<Cs...>>::type> f) -> void;	///< Loop over components (internally synchronized)

		template<typename... Cs>
		requires are_component_types<Cs...>
		auto for_each(std::function<typename Functor<vtll::type_list<Cs...>>::type> f) -> void { for_each(VecsRange<Cs...>{}, f); }		///< Loop over components (internally synchronized)

		template<template<typename...> typename R, typename... Es>
		requires (std::is_same_v<R<Es...>, VecsRange<Es...>> && are_entity_types<Es...>)
		auto for_each( R<Es...>&& r, std::function<typename Functor<typename VecsIterator<Es...>::component_types>::type> f) -> void;	///< Loop over entities (internally synchronized)

		template<typename... Es>
		requires are_entity_types<Es...>
		auto for_each(std::function<typename Functor<typename VecsIterator<Es...>::component_types>::type> f) -> void { for_each(VecsRange<Es...>{}, f); }	///< Loop over entities (internally synchronized)

		//-------------------------------------------------------------------------
		//utility

		auto index(VecsHandle h) noexcept	-> index_t;		///< \returns row index in component table (internally synchronized)
		auto type(VecsHandle h) noexcept	-> index16_t;		///< \returns type of entity (internally synchronized)
		virtual auto size() noexcept		-> size_t { return m_size.load(); };	///< \returns the total number of valid entities (internally synchronized)
		virtual auto swap( VecsHandle h1, VecsHandle h2 ) noexcept -> bool;	///< Swap places of two entities in the component table (internally synchronized)
		virtual auto contains(VecsHandle handle) noexcept	-> bool;		///< \returns true if the ECS still holds this entity  (externally synchronized)
	};


	/**
	* \brief Insert a new entity into the ECS.  The call is dispatched to the correct subclass for entity type E.
	*
	* The correct entity type E is determined using the component types Cs. Then the
	* call id forwarded to the correct subclass for E.
	* \param[in] args The components for the entity.
	* \returns the handle of thw new entity.
	*/
	template<typename E, typename... Cs>
	requires is_composed_of<E, Cs...> [[nodiscard]]
	auto VecsRegistryBaseClass::insert(Cs&&... args) noexcept	-> VecsHandle {
		return VecsRegistry<E>().insert(std::forward<Cs>(args)...);
	}

	/**
	* \brief Test whether an entity (pointed to by the handle) of type E contains a component of type C
	*
	* The call is dispatched to the correct subclass for entity type E.
	* \param[in] handle The handle of the entity to get the data from.
	* \returns true if entity type E contains component type C
	*/
	template<typename C>
	requires is_component_type<C>
	auto VecsRegistryBaseClass::has_component(VecsHandle handle) noexcept -> bool {
		if (!handle.is_valid()) return false;
		return m_dispatch[handle.type()]->has_componentE(handle, vtll::index_of<VecsComponentTypeList, std::decay_t<C>>::value);
	}

	/**
	* \brief Get the data of a component of type C for a given handle.
	*
	* The first component of type C is retrieved. So component types should be unique if you want to
	* use this function. Then the call is dispatched to the correct subclass for entity type E.
	* \param[in] handle The handle of the entity to get the data from.
	* \returns a component of type C holding the desired data.
	*/
	template<typename C>
	requires is_component_type<C>
	auto VecsRegistryBaseClass::component(VecsHandle handle) noexcept -> C {
		C res{};
		if (!handle.is_valid()) return res;
		/// Dispatch to the correct subclass and return result
		m_dispatch[handle.type()]->componentE(handle, vtll::index_of<VecsComponentTypeList, std::decay_t<C>>::value, (void*)&res, sizeof(C));
		return res;
	}

	/**
	* \brief Update a component of an entity. The call is dispatched to the correct subclass for entity type E.
	* \param[in] handle The entity handle.
	* \param[in] comp The component data.
	* \returns true if the update was successful.
	*/
	template<typename C>
	requires is_component_type<C>
	auto VecsRegistryBaseClass::update(VecsHandle handle, C&& comp) noexcept -> bool {
		if (!handle.is_valid()) return false;

		/// Dispatch the call to the correct subclass and return result
		return m_dispatch[handle.type()]->updateC(handle, vtll::index_of<VecsComponentTypeList, std::decay_t<C>>::value, (void*)&comp, sizeof(C));
	}

	/**
	* \brief Erase an entity from the ECS. The call is dispatched to the correct subclass for entity type E.
	* \param[in] handle The entity handle.
	* \returns true if the operation was successful.
	*/
	auto VecsRegistryBaseClass::erase(VecsHandle handle) noexcept -> bool {
		if (!handle.is_valid()) return false;
		return m_dispatch[handle.type()]->erase(handle); ///< Dispatch to the correct subclass for type E and return result
	}

	/**
	* \brief Return index of an entity in the component table.
	* \param[in] handle The entity handle.
	* \returns the index of the entity in the component table.
	*/
	auto VecsRegistryBaseClass::index(VecsHandle h) noexcept -> index_t {
		if (!h.is_valid()) return {};
		return m_entity_table.comp_ref_idx<VecsRegistryBaseClass::c_index>(h.m_entity_index);
	}

	/**
	* \brief Return index of an entity in the component table.
	* \param[in] handle The entity handle.
	* \returns the index of the entity in the component table.
	*/
	auto VecsRegistryBaseClass::type(VecsHandle h) noexcept -> index16_t {
		if (!h.is_valid()) return {};
		return m_entity_table.comp_ref_idx<VecsRegistryBaseClass::c_type>(h.m_entity_index);
	}

	/**
	* \brief Swap the rows of two entities of the same type.
	* The call is dispatched to the correct subclass for entity type E.
	*
	* \param[in] h1 First entity.
	* \param[in] h2 Second entity.
	* \returns true if operation was successful.
	*/
	auto VecsRegistryBaseClass::swap(VecsHandle h1, VecsHandle h2) noexcept	-> bool {
		if (!h1.is_valid() || !h1.is_valid() || h1.type() != h2.type()) return false;
		return m_dispatch[h1.type()]->swap(h1, h2);				///< Dispatch to the correct subclass for type E and return result
	}

	/**
	* \brief Entry point for checking whether a handle is still valid.
	* The call is dispatched to the correct subclass for entity type E.
	*
	* \param[in] handle The handle to check.
	* \returns true if the ECS contains this entity.
	*/
	auto VecsRegistryBaseClass::contains(VecsHandle handle) noexcept	-> bool {
		if (!handle.is_valid()) return false;
		return m_dispatch[handle.type()]->contains(handle); ///< Dispatch to the correct subclass for type E and return result
	}



	//-------------------------------------------------------------------------
	//VecsRegistry<E>

	/**
	* \brief VecsRegistry<E> is used as access interface for all entities of type E
	*/
	template<typename E = vtll::type_list<>>
	class VecsRegistry : public VecsRegistryBaseClass {

		friend class VecsRegistryBaseClass;

	protected:
		static inline std::atomic<uint32_t>		m_sizeE{0};	///< Store the number of valid entities of type E currently in the ECS
		static inline VecsComponentTable<E>		m_component_table;

		/** Maximum number of entites of type E that can be stored. */
		static const size_t c_max_size = vtll::back_value<vtll::map< VecsTableSizeMap, E, VeTableSizeDefault > >::value;

		/** Implementations of functions that receive dispatches from the base class. */
		auto updateC(VecsHandle handle, size_t compidx, void* ptr, size_t size) noexcept	-> bool; ///< Dispatch from base class (internally synchronized)
		auto componentE(VecsHandle handle, size_t compidx, void* ptr, size_t size) noexcept	-> bool; ///< Dispatch from base class (internally synchronized)
		auto has_componentE(VecsHandle handle, size_t compidx) noexcept						-> bool; ///< Dispatch from base class (internally synchronized)
		auto compressE() noexcept	-> void { return m_component_table.compress(); };	 ///< Compress component table of type E (externally synchronized)

		/** \brief Erase all entites of type E */
		auto clearE() noexcept		-> size_t { 			///< Forward to component table of type E
			return m_component_table.clear();	///< Call clear() in the correct component table
		};

	public:
		/** Constructors for class VecsRegistry<E>. */
		VecsRegistry(size_t r = 1 << c_max_size) noexcept : VecsRegistryBaseClass() { 	///< Constructor of class VecsRegistry<E>
			VecsComponentTable<E>{r}.max_capacity(r);
		};
		VecsRegistry(std::nullopt_t u) noexcept : VecsRegistryBaseClass() { 			///< Constructor of class VecsRegistry<E>
			m_sizeE = 0; 
		};

		//-------------------------------------------------------------------------
		//insert data

		template<typename... CCs>
		requires is_composed_of<E, CCs...> [[nodiscard]] 
		auto insert(CCs&&... args) noexcept			-> VecsHandle;			///< Insert new entity of type E into VECS (internally synchronized)

		//-------------------------------------------------------------------------
		//get data

		auto values(VecsHandle handle) noexcept		-> vtll::to_tuple<E>;	///< Return a tuple with copies of the components (internally synchronized)
		auto pointers(VecsHandle handle) noexcept	-> vtll::to_ptr_tuple<E>;	///< Return a tuple with pointers to the components (externally synchronized)

		template<typename C>
		requires is_component_type<C>
		auto has_component() noexcept				-> bool {	///< Return true if the entity type has a component C (internally synchronized)
			return is_component_of<E,C>;
		}

		template<size_t I, typename C = vtll::Nth_type<E, I>>
		requires is_component_of<E, C>
		auto component(VecsHandle handle) noexcept	-> C;		///< Get copy of a component given index of component (internally synchronized)

		template<typename C>
		requires is_component_of<E, C>
		auto component(VecsHandle handle) noexcept	-> C;		///< Get copy of a component given type of component (internally synchronized)

		//-------------------------------------------------------------------------
		//update data

		template<typename ET>
		requires is_tuple<ET, E>
		auto update(VecsHandle handle, ET&& ent) noexcept	-> bool;		///< Update a whole entity with the given tuple (internally synchronized)

		template<typename C> 
		requires is_component_of<E, C>
		auto update(VecsHandle handle, C&& comp) noexcept	-> bool;		///< Update one component of an entity (internally synchronized)

		//-------------------------------------------------------------------------
		//erase

		auto erase(VecsHandle handle) noexcept				-> bool;		///< Erase an entity from VECS (internally synchronized)

		//-------------------------------------------------------------------------
		//utility

		auto clear() noexcept -> size_t { return clearE(); };				///< Clear entities of type E (externally synchronized)

		auto compress() noexcept -> void { compressE(); }					///< Remove erased rows from the component table (externally synchronized)

		auto max_capacity(size_t) noexcept					-> size_t;		///< Set max number of entities of this type (externally synchronized)

		auto size() noexcept -> size_t { return m_sizeE.load(); };			///< \returns the number of valid entities of type E (internally synchronized)

		auto swap(VecsHandle h1, VecsHandle h2) noexcept	-> bool;		///< Swap rows in component table (internally synchronized)

		auto contains(VecsHandle handle) noexcept			-> bool;		///< Test if an entity is still in VECS (externally synchronized)
	};


	/**
	* \brief Update a component of an entity of type E.
	* This call has been dispatched from base class using the entity type index.
	*
	* \param[in] handle The entity handle.
	* \param[in] compidx The index of the component in the VecsComponentTypeList
	* \param[in] ptr Pointer to the data to be used.
	* \param[in] size Size of the component.
	* \returns true if the update was sucessful.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::updateC(VecsHandle handle, size_t compidx, void* ptr, size_t size) noexcept -> bool {
		VecsWriteLock lock(handle.mutex());
		if (!contains(handle)) return {};
		return m_component_table.updateC(m_entity_table.comp_ref_idx<c_index>(handle.m_entity_index), compidx, ptr, size);
	}

	/**
	* \brief Retrieve component data for an entity of type E.
	* This call has been dispatched from base class using the entity type index.
	*
	* \param[in] handle The entity handle.
	* \param[in] compidx The index of the component in the VecsComponentTypeList
	* \returns true if the retrieval was sucessful.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::has_componentE(VecsHandle handle, size_t compidx) noexcept -> bool {
		VecsReadLock lock(handle.mutex());
		if (!contains(handle)) return false;
		return m_component_table.has_componentE(compidx);
	}

	/**
	* \brief Retrieve component data for an entity of type E.
	* This call has been dispatched from base class using the entity type index.
	*
	* \param[in] handle The entity handle.
	* \param[in] compidx The index of the component in the VecsComponentTypeList
	* \param[in] ptr Pointer to the data to copy the result to.
	* \param[in] size Size of the component.
	* \returns true if the retrieval was sucessful.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::componentE(VecsHandle handle, size_t compidx, void* ptr, size_t size) noexcept -> bool {
		VecsReadLock lock(handle.mutex());
		if (!contains(handle)) return {};
		return m_component_table.componentE(m_entity_table.comp_ref_idx<c_index>(handle.m_entity_index), compidx, ptr, size);
	}

	/**
	* \brief Insert a new entity into the ECS.
	* This call has been dispatched from base class using the parameter list.
	*
	* \param[in] args The component arguments.
	* \returns the handle of the new entity.
	*/
	template<typename E>
	template<typename... CCs>
	requires is_composed_of<E, CCs...> [[nodiscard]]
	inline auto VecsRegistry<E>::insert(CCs&&... args) noexcept	-> VecsHandle {
		index_t idx{};

		std::lock_guard<std::mutex> guard(m_mutex);

		if (m_first_free.has_value()) {
			idx = m_first_free;
			m_first_free = m_entity_table.comp_ref_idx<c_index>(idx); 
		}
		else {
			idx = m_entity_table.push_back();
			if (!idx.has_value()) return {};
			m_entity_table.comp_ref_idx<c_counter>(idx) = counter16_t{ 0 };		//start with counter 0
		}

		m_entity_table.comp_ref_idx<c_type>(idx) = index16_t{ vtll::index_of<VecsEntityTypeList, E>::value }; ///< Entity type index
		
		VecsHandle handle{ idx, m_entity_table.comp_ref_idx<c_counter>(idx), m_entity_table.comp_ref_idx<c_type>(idx) }; ///< The handle
		
		m_entity_table.comp_ref_idx<c_index>(idx) 
			= m_component_table.insert(handle, &m_entity_table.comp_ref_idx<c_mutex>(idx), std::forward<CCs>(args)...);	///< add data into component table

		m_size++;
		m_sizeE++;
		return handle;
	};

	
	/**
	* \brief Return a tuple with pointers to the components. This call is externally synchronized!
	*
	* \param[in] handle Entity handle.
	* \returns tuple with pointers to the components.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::pointers(VecsHandle handle) noexcept -> vtll::to_ptr_tuple<E> {
		if (!contains(handle)) return {};	///< Return the empty component
		auto& comp_table_idx = m_entity_table.comp_ref_idx<c_index>(handle.m_entity_index); ///< Get reference to component
		return m_component_table.pointers( comp_table_idx );
	}

	/**
	* \brief Return a tuple with copies of the components.
	*
	* \param[in] handle Entity handle.
	* \returns tuple with copies of the components.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::values(VecsHandle handle) noexcept -> vtll::to_tuple<E> {
		VecsReadLock lock(handle.mutex());
		if (!contains(handle)) return {};	///< Return the empty component
		auto& comp_table_idx = m_entity_table.comp_ref_idx<c_index>(handle.m_entity_index); ///< Get component copies
		return m_component_table.values(comp_table_idx);
	}

	/**
	* \brief Swap the rows of two entities of the same type.
	* The call is dispatched to the correct subclass for entity type E.
	*
	* \param[in] h1 First entity.
	* \param[in] h2 Second entity.
	* \returns true if operation was successful.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::swap(VecsHandle h1, VecsHandle h2) noexcept -> bool {
		if (h1 == h2) return false;
		if (!h1.is_valid() || !h1.is_valid() || h1.type() != h2.type()) return false;
		if (h1.m_type_index != vtll::index_of<VecsEntityTypeList, E>::value) return false;
		if (h2.m_type_index != vtll::index_of<VecsEntityTypeList, E>::value) return false;
		
		if (h1.m_entity_index.value < h2.m_entity_index.value) {	//avoid deadlock by ordering 
			VecsWriteLock::lock(h1.mutex());
			VecsWriteLock::lock(h2.mutex());
		}
		else {
			VecsWriteLock::lock(h2.mutex());
			VecsWriteLock::lock(h1.mutex());
		}

		index_t& i1 = m_entity_table.comp_ref_idx<c_index>(h1.m_entity_index);
		index_t& i2 = m_entity_table.comp_ref_idx<c_index>(h2.m_entity_index);
		std::swap(i1, i2);											///< Swap in map

		auto res = m_component_table.swap(i1, i2);		///< Swap in component table

		VecsWriteLock::unlock(h1.mutex());
		VecsWriteLock::unlock(h2.mutex());

		return res;
	}

	/**
	* \brief Check whether the ECS contains an entity. 
	*
	* \param[in] handle The entity handle.
	* \return true if the entity is still contained in the ECS.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::contains(VecsHandle handle) noexcept -> bool {
		if (!handle.is_valid() || handle.m_type_index != vtll::index_of<VecsEntityTypeList, E>::value ) return false;
		auto& type = m_entity_table.comp_ref_idx<c_type>(handle.m_entity_index);
		auto& cnt = m_entity_table.comp_ref_idx<c_counter>(handle.m_entity_index);
		if ( handle.m_generation_counter != cnt || handle.m_type_index	!= type ) return false;
		return true;
	}

	/**
	* \brief Retrieve a component of type C from an entity of type E.
	*
	* \param[in] handle The entity handle.
	* \returns the component.
	*/
	template<typename E>
	template<size_t I, typename C>
	requires is_component_of<E, C>
	auto VecsRegistry<E>::component(VecsHandle handle) noexcept							-> C {
		VecsReadLock lock(handle.mutex());
		if (!contains(handle)) return {};	///< Return the empty component
		auto& comp_table_idx = m_entity_table.comp_ref_idx<c_index>(handle.m_entity_index); ///< Get reference to component
		return m_component_table.component<C>(comp_table_idx);	///< Return the component
	}

	/**
	* \brief Retrieve a component of type C from an entity of type E.
	*
	* \param[in] handle The entity handle.
	* \returns the component.
	*/
	template<typename E>
	template<typename C>
	requires is_component_of<E, C>
	inline auto VecsRegistry<E>::component(VecsHandle handle) noexcept -> C {
		return component<vtll::index_of<E,C>>();
	}

	/**
	* \brief Update all components of an entity of type E from a std::tuple<Cs...>
	*
	* \param[in] handle The Enity handle. 
	* \param[in] ent A universal reference to the std::tuple<Cs...> that contains the data.
	* \returns true if the operation was successful.
	*/
	template<typename E>
	template<typename ET>
	requires is_tuple<ET, E>
	inline auto VecsRegistry<E>::update(VecsHandle handle, ET&& ent) noexcept -> bool {
		VecsWriteLock lock(handle.mutex());
		if (!contains(handle)) return false;
		m_component_table.update(handle.m_entity_index, std::forward<ET>(ent));
		return true;
	}

	/**
	* \brief Update a component of type C for an entity of type E.
	*
	* \param[in] handle The entity handle.
	* \param[in] comp The component data.
	* \returns true if the operation was successful.
	*/
	template<typename E>
	template<typename C>
	requires is_component_of<E, C>
	inline auto VecsRegistry<E>::update( VecsHandle handle, C&& comp) noexcept -> bool {
		VecsWriteLock lock(handle.mutex());
		if constexpr (!vtll::has_type<E, std::decay_t<C>>::value) { return false; }
		if (!contains(handle)) return false;
		m_component_table.update<C>(handle.m_entity_index, std::forward<C>(comp));
		return true;
	}

	/**
	* \brief Erase an entity from the ECS.
	*
	* \param[in] handle The entity handle.
	* \returns true if the operation was successful.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::erase( VecsHandle handle) noexcept -> bool {
		{
			VecsWriteLock lock(handle.mutex());
			if (!contains(handle)) return false;
			m_component_table.erase(m_entity_table.comp_ref_idx <c_index>(handle.m_entity_index)); ///< Erase from comp table
			m_entity_table.comp_ref_idx<c_counter>(handle.m_entity_index)++;			///< Invalidate the entity handle
		}

		m_size--;	///< Decrease sizes
		m_sizeE--;

		std::lock_guard<std::mutex> mlock(m_mutex);										///< Protect free list
		m_entity_table.comp_ref_idx<c_index>(handle.m_entity_index) = m_first_free;		///< Put old entry into free list
		m_first_free = handle.m_entity_index;

		return true; 
	}

	/**
	* \brief Set the max number of entries that can be stored in the component table.
	*
	* If the new max is smaller than the previous one it will be ignored. Tables cannot shrink.
	*
	* \param[in] r New max number or entities that can be stored.
	* \returns the current max number, or the new one.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::max_capacity(size_t r) noexcept -> size_t {
		auto maxE = m_component_table.max_capacity(r);

		size_t sum_max = 0;

		vtll::static_for<size_t, 0, vtll::size<VecsEntityTypeList>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<VecsEntityTypeList, i>;
				sum_max += VecsRegistry<type>.max_capacity(0);
			}
		);

		m_entity_table.max_capacity(sum_max);

		return maxE;
	}


	//-------------------------------------------------------------------------
	//VecsRegistry<vtll::type_list<>> specialization for vtll::type_list<>, represents the base class!

	/**
	* \brief A specialized VecsRegistry<E> to act as convenient access interface instead of the base class.
	*/
	template<>
	class VecsRegistry<vtll::type_list<>> : public VecsRegistryBaseClass {
	public:
		VecsRegistry(size_t r = VecsTableMaxSize::value) noexcept : VecsRegistryBaseClass(r) {};	///< Constructor of class VecsRegistry<vtll::type_list<>>
		VecsRegistry(std::nullopt_t u) noexcept : VecsRegistryBaseClass() {};						///< Constructor of class VecsRegistry<vtll::type_list<>>
	};


	//-------------------------------------------------------------------------
	//left over implementations that depend on definition of classes



	//-------------------------------------------------------------------------
	//VecsComponentTable

	/**
	* \brief Remove invalid erased entity data at the end of the component table.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::remove_deleted_tail() noexcept -> void {
		while (m_data.size() > 0) {
			auto & handle = m_data.comp_ref_idx<c_handle>(index_t{ m_data.size() - 1 });
			if (handle.is_valid()) return; ///< is last entity is valid, then return
			m_data.pop_back();				///< Else remove it and continue the loop
		}
	}

	/**
	* \brief Remove all invalid entities from the component table.
	* 
	* The algorithm makes sure that there are no deleted entities at the end of the table.
	* Then it runs trough all deleted entities (m_deleted table) and tests whether it lies
	* inside the table. if it does, it is swapped with the last entity, which must be valid.
	* Then all invalid entities at the end are again removed.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::compress() noexcept -> void {
		for (size_t i = 0; i < m_deleted.size(); ++i) {
			remove_deleted_tail();											///< Remove invalid entities at end of table
			auto index = m_deleted.comp_ref_idx<0>(index_t{i});				///< Get next deleted entity from deleted table
			if (index.value < m_data.size()) {								///< Is it inside the table still?		
				m_data.move(index, index_t{ m_data.size() - 1 });			///< Yes, move last entity to this position
				auto& handle = m_data.comp_ref_idx<c_handle>(index);		///< Handle of moved entity
				VecsRegistryBaseClass().m_entity_table.comp_ref_idx<VecsRegistryBaseClass::c_index>(handle.m_entity_index) = index; ///< Change map entry of moved last entity
			}
		}
		m_deleted.clear();
	}

	/**
	* \brief Erase all entries of type E from the component table.
	* 
	* The entities are not really removed, but rather invalidated. Call compress()
	* to actually remove the entities.
	* 
	* \returns the number of erased entities.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::clear() noexcept -> size_t {
		size_t num = 0;
		VecsHandle handle;
		for (size_t i = 0; i < m_data.size(); ++i) {
			{
				VecsReadLock lock( m_data.comp_ref_idx<c_mutex>(index_t{ i }) );
				handle = m_data.comp_ref_idx<c_handle>(index_t{ i });
			}
			if( handle.is_valid() && VecsRegistry<E>().erase(handle) ) ++num;
		}
		return num;
	}

	//-------------------------------------------------------------------------
	//VecsRegistryBaseClass

	/**
	* \brief Constructor of class VecsRegistryBaseClass.
	* Creates the dispatch table, one entry for each VecsRegistry<E> sub type.
	*
	* \param[in] r Max size of all entities stored in the ECS.
	*/
	inline VecsRegistryBaseClass::VecsRegistryBaseClass(size_t r) noexcept {
		if (!this->init()) [[likely]] return;
		m_entity_table.max_capacity(r);

		vtll::static_for<size_t, 0, vtll::size<VecsEntityTypeList>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<VecsEntityTypeList, i>;
				m_dispatch[i] = std::make_unique<VecsRegistry<type>>(std::nullopt);
			}
		);
	}

	/**
	* \brief Erase all entities from the ECS.
	* 
	* The entities are not really removed, but rather invalidated. Call compress()
	* to actually remove the entities.
	* 
	* \returns the total number of erased entities.
	*/
	inline auto VecsRegistryBaseClass::clear() noexcept	 -> size_t {
		size_t num = 0;
		vtll::static_for<size_t, 0, vtll::size<VecsEntityTypeList>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<VecsEntityTypeList, i>;
				num += VecsRegistry<type>().clearE();
			}
		);
		return num;
	}

	/**
	* \brief Compress all component tables. This removes all invalidated entities.
	*/
	inline auto VecsRegistryBaseClass::compress() noexcept		-> void {
		vtll::static_for<size_t, 0, vtll::size<VecsEntityTypeList>::value >(
			[&](auto i) {
				VecsRegistry<vtll::Nth_type<VecsEntityTypeList, i>>().compressE();
			}
		);
	}

	/**
	* \brief Update the components of an entity. The call is forwarded to the correct VecsRegistry<E>.
	*
	* \param[in] handle The entity handle.
	* \param[in] ent The entity data.
	* \returns true if the operation was successful.
	*/
	template<typename ET>
	requires is_tuple<ET>
	inline auto VecsRegistryBaseClass::update( VecsHandle handle, ET&& ent) noexcept	-> bool {
		return VecsRegistry<vtll::front<ET>>().update({ handle }, std::forward<ET>(ent));
	}

	//-------------------------------------------------------------------------
	//for_each

	/**
	* \brief takes two iterators and loops from begin to end, and for each entity calls the provided function.
	*
	* \param[in] b Begin iterator.
	* \param[in] e End iterator.
	* \param[in] f Functor to be called for every entity the iterator visits.
	*/
	template<template<typename...> typename R, typename... Cs>
	requires (std::is_same_v<R<Cs...>, VecsRange<Cs...>> && are_component_types<Cs...>)
	inline auto VecsRegistryBaseClass::for_each( R<Cs...>&& range, std::function<typename Functor<vtll::type_list<Cs...>>::type> f) -> void {
		auto b = range.begin();
		auto e = range.end();
		for (; b != e; ++b) {
			VecsWriteLock lock( b.mutex() );		///< Write lock
			if (!b.has_value()) continue;
			std::apply(f, *b);						///< Run the function on the references
		}
	}


	/**
	* \brief takes two iterators and loops from begin to end, and for each entity calls the provided function.
	*
	* \param[in] b Begin iterator.
	* \param[in] e End iterator.
	* \param[in] f Functor to be called for every entity the iterator visits.
	*/
	template<template<typename...> typename R, typename... Es>
	requires (std::is_same_v<R<Es...>, VecsRange<Es...>> && are_entity_types<Es...>)
	inline auto VecsRegistryBaseClass::for_each(R<Es...>&& range, std::function<typename Functor<typename VecsIterator<Es...>::component_types>::type> f) -> void {
		auto b = range.begin();
		auto e = range.end();
		for (; b != e; ++b) {
			VecsWriteLock lock(b.mutex());		///< Write lock
			if (!b.has_value()) continue;
			std::apply(f, *b);					///< Run the function on the references
		}
	}


	//-------------------------------------------------------------------------
	//VecsHandle

	/**
	* \brief Check whether the data in a handle is non null
	* \returns true if the data in the handle is not null
	*/
	inline auto VecsHandle::is_valid() noexcept				-> bool {
		return	m_entity_index.has_value() 
				&& m_generation_counter.has_value() 
				&& m_type_index.has_value() 
				&& m_type_index.value < vtll::size<VecsEntityTypeList>::value;
	}

	/**
	* \brief Check whether a handle belongs to an entity that is still in the ECS.
	* \returns true if the entity this handle belongs to is still in the ECS.
	*/
	inline auto VecsHandle::has_value() noexcept				-> bool {
		return VecsRegistryBaseClass().contains(*this);
	}

	/**
	* \brief Test whether an entity of type E contains a component of type C.
	* \returns true if the entity type E has a component C
	*/
	template<typename C>
	requires is_component_type<C>
	inline auto VecsHandle::has_component() noexcept				-> bool {
		return VecsRegistryBaseClass().has_component<C>(*this);
	}

	/**
	* \brief Get the component of type C from the entity this handle belongs to.
	* \returns the component, or an empty component.
	*/
	template<typename C>
	requires is_component_type<C>
	inline auto VecsHandle::component() noexcept				-> C {
		return VecsRegistryBaseClass().component<C>(*this);
	}

	/**
	* \brief Update the data for the entity this handle belongs to.
	*
	* \param[in] ent Universal reference to the data that is to be copied over the old data.
	* \returns true if the operation was successful.
	*/
	template<typename ET>
	requires is_tuple<ET>
	inline auto VecsHandle::update(ET&& ent) noexcept			-> bool {
		using type = vtll::front<std::decay_t<ET>>;
		return VecsRegistry<type>().update(*this, std::forward<ET>(ent));
	}

	/**
	* \brief Update a component of type C for the entity that this handle belongs to.
	*
	* \param[in] comp Universal reference to the component data.
	* \returns true if the operation was successful.
	*/
	template<typename C>
	requires is_component_type<C>
	inline auto VecsHandle::update(C&& comp) noexcept			-> bool {
		return VecsRegistryBaseClass().update<C>(*this, std::forward<C>(comp));
	}

	/**
	* \brief Erase the entity this handle belongs to, from the ECS.
	*
	* \returns true if the operation was successful.
	*/
	inline auto VecsHandle::erase() noexcept					-> bool {
		return VecsRegistryBaseClass().erase(*this);
	}

	/**
	* \brief Get index of this entity in the component table.
	*
	* \returns index of this entity in the component table.
	*/
	auto VecsHandle::index() noexcept -> index_t {
		return VecsRegistryBaseClass{}.index(*this);
	}

	/**
	* \brief Get index of this entity in the component table.
	*
	* \returns index of this entity in the component table.
	*/
	auto VecsHandle::type() noexcept -> index16_t {
		return VecsRegistryBaseClass{}.type(*this);
	}

	/**
	* \brief Return a pointer to the mutex of this entity.
	*
	* \returns a pointer to the mutex of this entity.
	*/
	inline std::atomic<uint32_t>* VecsHandle::mutex() {
		if (!is_valid()) return nullptr;
		return &VecsRegistryBaseClass::m_entity_table.comp_ref_idx<VecsRegistryBaseClass::c_mutex>(m_entity_index);
	}

}

#include "VECSIterator.h"

#endif
