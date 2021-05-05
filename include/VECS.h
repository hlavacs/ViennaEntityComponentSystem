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
	* \brief Entity type list without tags: a list with all possible entity types the ECS can deal with.
	* 
	* An entity type is a collection of component types. 
	* The list is the sum of the entitiy types of the engine part, and the entity types as defined by the engine user.
	* The entities do not contain any tags in this list. Below they will be expanded to hold all partial sets of
	* tag combinations at the end.
	*/
	using VecsEntityTypeListWOTags = vtll::cat< VeSystemEntityTypeList, VeUserEntityTypeList >;

	/**
	* \brief Entity tag map: The sum of all tag maps.
	*
	* An entity tag map defines the possible tags for each entity type. Tags are special components that
	* can be appended to the end of the component list of entities. Entries in the map are type_lists of tags possible for
	* each entry type.
	*/
	using VecsEntityTagMap = vtll::cat< VeSystemEntityTagMap, VeUserEntityTagMap >;

	/**
	* \brief List of all tags that are used for any entity type.
	*
	* Tags are special components that can be appended to the end of the component list of entities. 
	*/
	using VecsEntityTagList = vtll::flatten< vtll::transform< VecsEntityTagMap, vtll::back > >;

	/**
	* \brief Struct to expand tags to all possible combinations and append them to their entity type component lists.
	*/
	template<typename T>
	struct expand_tags_for_one {
		using type =
			vtll::transform_front<	///< Cat all partial tag sets after the elements of Ts
				vtll::power_set<	///< Create the set of all partial tag sets
					vtll::map< VecsEntityTagMap, T, vtll::type_list<> > >	///< Get tags for entity type Ts from tag map
				, vtll::cat			///< Use cat function on each partial tag set
				, T					///< And put contents of Ts (the entity components) in front of the partial tag list
			>;
	};

	/**
	* \brief Expand the entity types of a list for all their tags.
	*
	* An entity type is a collection of component types. In the tag map, tags can be defined for an entity type.
	* Thus there are variants of this entity type that also hold one or more tags as components.
	* expand_tags takes a list of entity types, uses the tags from the tag map and creates all variants of each 
	* entity type, holding zero to all tags for this entity type.
	*/
	template<typename TL>
	using expand_tags = vtll::remove_duplicates< vtll::flatten< vtll::function< TL, expand_tags_for_one > > >;


	/**
	* \brief Entity type list: a list with all possible entity types the ECS can deal with.
	*
	* An entity type is a collection of component types.
	* The list is the sum of the entitiy types of the engine part, and the entity types as defined by the engine user.
	* This list contains all possible entity types, but also their variants created by tag combinations.
	*/
	using VecsEntityTypeList = expand_tags< VecsEntityTypeListWOTags >;

	static_assert(vtll::are_unique<VecsEntityTypeList>::value, "The elements of VecsEntityTypeList lists are not unique!");

	/**
	* \brief A list of all components but excuding the tags
	*
	* It is the sum of the components of the engine part, and the components as defined by the engine user, but w/o tags.
	*/
	using VecsComponentTypeListWOTags = vtll::remove_types< vtll::remove_duplicates< vtll::flatten<VecsEntityTypeList> >, VecsEntityTagList>;

	/**
	* \brief Component type list: a list with all component types that an entity can have.
	*
	* It is the sum of the components of the engine part, and the components as defined by the engine user, including tags 
	* at the END of the list.
	*/
	using VecsComponentTypeList = vtll::cat< VecsComponentTypeListWOTags, VecsEntityTagList>;

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
	* \brief Table constants retrieves mappings for all entity types from the VecsTableSizeMap (which have the format vtll::value_list<A,B>).
	* Then it turns the value lists to type lists, each value is stored in a std::integral_constant<size_t, V> type.
	*/
	using VeTableSizeDefault = vtll::value_list< 1<<10 >;
	using VecsTableConstants = vtll::transform < vtll::apply_map<VecsTableSizeMap, VecsEntityTypeList, VeTableSizeDefault>, vtll::value_to_type>;

	/**
	* \brief Get the maximal exponent from the list of segment size exponents.
	* 
	* This is used as segment size (exponent) for the map table in the VecsRegistryBaseClass. Since this table holds info to 
	* all entities in the ECS (the aggregate), it makes sense to take the maximum for segmentation.
	*/
	using VecsTableMaxSegExp = vtll::max< vtll::transform< VecsTableConstants, vtll::front > >;	///< Get max exponent from map

	/**
	* \brief Defines whether the data layout for entity type E is row or column wise. The default is columns wise.
	*/
	using VecsTableLayoutMap = vtll::cat< VeSystemTableLayoutMap, VeUserTableLayoutMap >;

	//-------------------------------------------------------------------------
	//concepts

	template<typename C>
	concept is_component_type = (vtll::has_type<VecsComponentTypeList, std::decay_t<C>>::value); ///< C is a component

	template<typename... Cs>
	concept are_component_types = (is_component_type<Cs> && ...);	///< Cs are all components

	template<typename T>
	concept is_entity_tag = (vtll::has_type<VecsEntityTagList, std::decay_t<T>>::value); ///< T is a tag

	template<typename... Ts>
	concept are_entity_tags = (is_entity_tag<Ts> && ...);	///< Ts are all tags

	template<typename E>
	concept is_entity_type = (vtll::has_type<VecsEntityTypeList, std::decay_t<E>>::value); ///< E is an entity type

	template<typename... Es>
	concept are_entity_types = (is_entity_type<Es> && ...);		///< Es are all entity types

	template<typename ETL>
	struct is_entity_type_list;									///< Es are all entity types

	template<template<typename...> typename ETL, typename... Es>
	struct is_entity_type_list<ETL<Es...>> {					///< A list of entity types
		static const bool value = (is_entity_type<Es> && ...);
	};

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

	//-------------------------------------------------------------------------
	//declaration of Vecs classes

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
	template<typename... Ts> class VecsIterator;
	template<typename ETL, typename CTL> class VecsIteratorEntityBaseClass;
	template<typename E, typename ETL, typename CTL> class VecsIteratorEntity;
	template<typename ETL, typename CTL> class VecsRangeBaseClass;
	template<typename... Ts> class VecsRange;

	//-------------------------------------------------------------------------
	//Functor for for_each looping

	/** 
	* \brief General functor type that can hold any function, and depends on a number of component types.
	* 
	* Used in for_each to iterate over entities and call the functor for each entity.
	*/
	template<typename C>
	struct Functor;

	template<template<typename...> typename Seq, typename... Cs>
	struct Functor<Seq<Cs...>> {
		using type = void(VecsHandle, Cs&...);	///< Arguments for the functor
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
		table_index_t	m_map_index{};				///< The slot of the entity in the entity map
		counter_t		m_generation_counter{};		///< Generation counter

	public:
		VecsHandle() noexcept {}; ///< Empty constructor of class VecsHandle

		/** 
		* \brief Constructor of class VecsHandle.
		* \param[in] idx The index of the handle in the aggregate slot map of class VecsRegistryBaseClass
		* \param[in] cnt Current generation counter identifying this entity on in the slot.
		* \param[in] type Type index for the entity type E.
		*/
		VecsHandle(table_index_t idx, counter_t cnt) noexcept : m_map_index{ idx }, m_generation_counter{ cnt } {};

		inline auto is_valid() noexcept	-> bool;	///< The data in the handle is non null
		auto has_value() noexcept		-> bool;	///< The entity that is pointed to exists in the ECS

		template<typename C>
		requires is_component_type<C>
		auto has_component() noexcept	-> bool;	///< Return true if the entity type has a component C

		template<typename C>
		requires is_component_type<C>
		auto component() noexcept		-> C& { return *component_ptr<C>(); };	///< Get a component of type C of the entity (first found is copied) 

		template<typename C>
		requires is_component_type<C>
		auto component_ptr() noexcept	-> C*;		///< Get a pointer to component of type C of the entity (first found is copied) 

		template<typename... Cs>
		requires are_component_types<Cs...>
		auto update(Cs&&... args) noexcept -> bool;	///< Update components of type C
		
		auto erase() noexcept -> bool;				///< Erase the entity 

		auto map_index() noexcept	-> table_index_t { return m_map_index; }; ///< \returns index of entity in the map
		auto table_index() noexcept -> table_index_t;	///< Get index of this entity in the component table
		auto type_index() noexcept  -> type_index_t;	///< Get type of this entity

		std::atomic<uint32_t>* mutex();				///< \returns address of the VECS mutex for this entity

		bool operator==(const VecsHandle& rhs) {	///< Equality operator
			return m_map_index == rhs.m_map_index && m_generation_counter == rhs.m_generation_counter;
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
		virtual auto updateC(table_index_t index, size_t compidx, void* ptr, size_t size, bool move=false) noexcept	-> bool = 0;	///< Empty update
		virtual auto componentE_ptr(table_index_t entidx, size_t compidx) noexcept						-> void* = 0;	///< Empty component read
		virtual auto has_componentE()  noexcept															-> bool = 0;	///< Test for component
	};


	/**
	* \brief This class stores all components of entities of type E.
	* 
	* Since templated functions cannot be virtual, the class emulates virtual functions to access components
	* by creating a dispatch table. Each component from the component table gets its own entry, even if the 
	* entity type does not contain this component type.
	*/
	template<typename E>
	class VecsComponentTable : public VecsMonostate<VecsComponentTable<E>> {
		template<typename E, size_t I> friend class VecsComponentAccessorDerived;
		friend class VecsRegistryBaseClass;
		template<typename E> friend class VecsRegistry;
		template<typename... Ts> friend class VecsIterator;
		template<typename E, typename ETL, typename CTL> friend class VecsIteratorEntity;

	protected:
		using tuple_value_t = vtll::to_tuple<E>;			///< A tuple storing all components of entity of type E
		using tuple_ref_t	= vtll::to_ref_tuple<E>;		///< A tuple storing references to all components of entity of type E
		using tuple_ptr_t	= vtll::to_ptr_tuple<E>;		///< A tuple storing pointers to all components of entity of type E
		using layout_type_t = vtll::map<VecsTableLayoutMap, E, VECS_LAYOUT_DEFAULT>; ///< ROW or COLUMN

		using info = vtll::type_list<VecsHandle, std::atomic<uint32_t>*>;	///< List of management data per entity (handle and mutex)
		static const size_t c_handle = 0;		///< Component index of the handle info
		static const size_t c_mutex = 1;		///< Component index of the handle info
		static const size_t c_info_size = 2;	///< Index where the entity data starts

		using types = vtll::cat< info, E >;						///< List with management (info) and component (data) types
		using types_deleted = vtll::type_list< table_index_t >;	///< List with types for holding info about erased entities 

		static const size_t c_segment_size = vtll::front_value< vtll::map< VecsTableSizeMap, E, VeTableSizeDefault > >::value;
		static inline VecsTable<types,			c_segment_size, layout_type_t::value>	m_data;		///< Data per entity
		static inline VecsTable<types_deleted,  c_segment_size, VECS_LAYOUT_ROW::value>	m_deleted;	///< Table holding the indices of erased entities

		using array_type = std::array<std::unique_ptr<VecsComponentAccessor<E>>, vtll::size<VecsComponentTypeList>::value>;
		static inline array_type m_dispatch;	///< Each component type C of the entity type E gets its own specialized class instance

		//-------------------------------------------------------------------------

		auto updateC(table_index_t entidx, size_t compidx, void* ptr, size_t size, bool move = false) noexcept		-> bool;	///< For dispatching
		auto componentE_ptr(table_index_t entidx, size_t compidx) noexcept						-> void*;	///< For dispatching
		auto has_componentE(size_t compidx)  noexcept											-> bool;	///< Test for component
		auto remove_deleted_tail() noexcept														-> void;	///< Remove empty slots at the end of the table

		VecsComponentTable() noexcept;			///< Protected constructor that does not allocate the dispatch table

		//-------------------------------------------------------------------------
		//insert

		template<typename... Cs>
		requires are_components_of<E, Cs...> [[nodiscard]]
		auto insert(VecsHandle handle, std::atomic<uint32_t>* mutex, Cs&&... args) noexcept	-> table_index_t; ///< Insert new entity

		//-------------------------------------------------------------------------
		//read data

		template<typename C>
		requires is_component_of<E, C>
		auto component(const table_index_t index) noexcept -> C& { return *component_ptr<C>(index); };	///< Get reference to a component

		template<typename C>
		requires is_component_of<E, C>
		auto component_ptr(const table_index_t index) noexcept	-> C*;		///< Get pointer to a component

		auto tuple(const table_index_t index) noexcept			-> tuple_ref_t;	///< \returns tuple with copies of the components
		auto tuple_ptr(const table_index_t index) noexcept		-> tuple_ptr_t;	///< \returns tuple with pointers to the components

		//-------------------------------------------------------------------------
		//update data

		template<typename... Cs>
		requires are_components_of<E, Cs...>
		auto update(const table_index_t index, Cs&&... args) noexcept	-> bool;	///< Update a component
		
		//-------------------------------------------------------------------------
		//erase data

		auto erase(const table_index_t idx) noexcept			-> bool;	///< Mark a row as erased
		auto clear() noexcept									-> size_t;	///< Mark all rows as erased

		//-------------------------------------------------------------------------
		//utilities

		auto handle(const table_index_t index) noexcept			-> VecsHandle;	///< \returns handle for an index in the table
		auto mutex(const table_index_t index) noexcept			-> std::atomic<uint32_t>*; ///< \returns pointer to the mutex for a given index
		auto size() noexcept -> size_t { return m_data.size(); };	///< \returns the number of entries currently in the table, can also be invalid ones
		auto compress() noexcept								-> void;	///< Compress the table
		auto capacity(size_t) noexcept							-> size_t;	///< \returns the max number of entities that can be stored
		auto swap(table_index_t n1, table_index_t n2) -> bool { return m_data.swap(n1, n2); }	///< Swap places for two rows
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
	inline auto VecsComponentTable<E>::updateC(table_index_t entidx, size_t compidx, void* ptr, size_t size, bool move ) noexcept -> bool {
		return m_dispatch[compidx]->updateC(entidx, compidx, ptr, size, move);
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
	* \brief Get pointer to a component.
	*
	* \param[in] entidx Entity index in the component table.
	* \param[in] compidx Index of the component of the entity.
	* \returns pointer to the component.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::componentE_ptr(table_index_t entidx, size_t compidx)  noexcept -> void* {
		return m_dispatch[compidx]->componentE_ptr(entidx, compidx);
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
	requires are_components_of<E, Cs...> [[nodiscard]]
	inline auto VecsComponentTable<E>::insert(VecsHandle handle, std::atomic<uint32_t>* mutex, Cs&&... args) noexcept -> table_index_t {
		auto idx = m_data.push_back();			///< Allocate space a the end of the table
		if (!idx.has_value()) return idx;		///< Check if the allocation was successfull
		m_data.update<c_handle>(idx, handle);	///< Update the handle data
		m_data.update<c_mutex>(idx, mutex);		///< Update the mutex pointer

		(m_data.update<c_info_size + vtll::index_of<E,std::decay_t<Cs>>::value> (idx, std::forward<Cs>(args)), ...); ///< Update the entity components
		return idx;								///< Return the index of the new data
	};


	/**
	* \param[in] index Index of the entity in the component table.
	* \returns the component of type C for an entity.
	*/
	template<typename E>
	template<typename C>
	requires is_component_of<E, C>
		inline auto VecsComponentTable<E>::component_ptr(const table_index_t index) noexcept -> C* {
		assert(index < m_data.size());
		return m_data.component_ptr<c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index); ///< Get ref to the entity and return component
	}

	/**
	* \param[in] index The index of the data in the component table.
	* \returns a tuple holding the components of an entity.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::tuple_ptr(const table_index_t index) noexcept -> tuple_ptr_t {
		assert(index < m_data.size());
		auto tup = m_data.tuple_ptr(index);												///< Get the whole data from the data
		return vtll::sub_tuple< c_info_size, std::tuple_size_v<decltype(tup)> >(tup);	///< Return only entity components in a subtuple
	}

	/**
	* \param[in] index The index of the data in the component table.
	* \returns a tuple holding the components of an entity.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::tuple(const table_index_t index) noexcept -> tuple_ref_t {
		assert(index < m_data.size());
		auto tup = m_data.tuple_ref(index);													///< Get the whole data from the data
		return vtll::sub_ref_tuple< c_info_size, std::tuple_size_v<decltype(tup)> >(tup);	///< Return only entity components in a subtuple
	}

	/**
	* \brief Update a component of type C for an entity of type E.
	*
	* \param[in] handle The entity handle.
	* \param[in] comp The component data.
	* \returns true if the operation was successful.
	*/
	template<typename E>
	template<typename... Cs>
	requires are_components_of<E, Cs...>
		inline auto VecsComponentTable<E>::update(const table_index_t index, Cs&&... args) noexcept -> bool {
		using types = vtll::tl<Cs...>;
		auto tuple = std::forward_as_tuple(args...);
		vtll::static_for<size_t, 0, sizeof...(Cs) >([&](auto i) {
			using type = vtll::Nth_type<types, i>;
			m_data.update<c_info_size + vtll::index_of<E, type>::value>(index, std::forward<type>(std::get<i>(tuple)));
			});

		return true;
	}

	/**
	* \brief Erase the component data for an entity.
	* 
	* The data is not really erased but the handle is invalidated. The index is also pushed to the deleted table.
	* \param[in] index The index of the entity data in the component table.
	* \returns true if the data was set to invalid.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::erase(const table_index_t index) noexcept -> bool {
		assert(index < m_data.size());
		*m_data.component_ptr<c_handle>(index) = {};	///< Invalidate handle	
		m_deleted.push_back(std::make_tuple(index));	///< Push the index to the deleted table.

		vtll::static_for<size_t, 0, vtll::size<E>::value >(			///< Loop over all components
			[&](auto i) {
				using type = vtll::Nth_type<E, i>;
				if constexpr (std::is_destructible_v<type> && !std::is_trivially_destructible_v<type>) {
					m_data.component_ptr<c_info_size + i>(index)->~type();	///< Call destructor
				}
			}
		);

		return true;
	}

	/**
	* \param[in] index The index of the entity in the component table.
	* \returns the handle of an entity from the component table.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::handle(const table_index_t index) noexcept -> VecsHandle {
		assert(index < m_data.size());
		return *m_data.component_ptr<c_handle>(index);	///< Get ref to the handle and return it
	}

	/**
	* \param[in] index The index of the entity in the component table.
	* \returns pointer to the sync mutex of the entity.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::mutex(const table_index_t index) noexcept -> std::atomic<uint32_t>* {
		assert(index < m_data.size());
		return *m_data.component_ptr<c_mutex>(index);		///< Get ref to the mutex and return it
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
	inline auto VecsComponentTable<E>::capacity(size_t r) noexcept -> size_t {
		m_data.capacity(r);
		return m_deleted.capacity(r);
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
		VecsComponentAccessorDerived() noexcept : VecsComponentAccessor<E>() {} ///< Constructor of class VecsComponentAccessorDerived 

	protected:

		/**
		* \brief Update the component C of entity E.
		* \param[in] index Index of the entity in the component table.
		* \param[in] compidx Component index incomponent type list.
		* \param[in] ptr Pointer to where the data comes from.
		* \param[in] size Size of the component data.
		* \returns true if the update was successful.
		*/
		auto updateC(table_index_t index, size_t compidx, void* ptr, size_t size, bool move ) noexcept -> bool {
			if constexpr (is_component_of<E, C>) {
				if (move) {
					*VecsComponentTable<E>().m_data.component_ptr<VecsComponentTable<E>::c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index) = std::move(*((C*)ptr));
				}
				else {
					*VecsComponentTable<E>().m_data.component_ptr<VecsComponentTable<E>::c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index) = *((C*)ptr);
				}
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
		* \brief Get pointer to component data.
		* \param[in] index Index of the entity in the component table.
		* \param[in] compidx Component index incomponent type list.
		* \returns pointer to the component.
		*/
		auto componentE_ptr(table_index_t entidx, size_t compidx)  noexcept -> void* {
			if constexpr (is_component_of<E, C>) {
				return (void*)VecsComponentTable<E>().m_data.component_ptr<VecsComponentTable<E>::c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(entidx);
			}
			return nullptr;
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
	inline VecsComponentTable<E>::VecsComponentTable() noexcept {
		if (!this->init()) return;

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
		template<typename E> friend class VecsRegistry;

	protected:
		using types = vtll::type_list<table_index_t, counter_t, type_index_t, std::atomic<uint32_t>>;	///< Type for the table
		static const uint32_t c_index{ 0 };		///< Index for accessing the index to next free or entry in component table
		static const uint32_t c_counter{ 1 };	///< Index for accessing the generation counter
		static const uint32_t c_type{ 2 };		///< Index for accessing the type index
		static const uint32_t c_mutex{ 3 };		///< Index for accessing the lock mutex

		static const size_t c_segment_size = VecsTableMaxSegExp::value;
		static inline VecsTable<types, c_segment_size, VECS_LAYOUT_ROW::value> m_map_table;	///< The main mapping table
		static inline std::mutex				m_mutex;		///< Mutex for syncing insert and erase
		static inline table_index_t				m_first_free{};	///< First free entry to be reused
		static inline std::atomic<uint32_t>		m_size{0};		///< Number of valid entities in the map

		/// Every subclass for entity type E has an entry in this table.
		static inline std::array<std::unique_ptr<VecsRegistryBaseClass>, vtll::size<VecsEntityTypeList>::value> m_dispatch;

		/// Virtual function for dispatching component updates to the correct subclass for entity type E.
		virtual auto updateC(VecsHandle handle, size_t compidx, void* ptr, size_t size, bool move = false) noexcept -> bool { return false; };

		/// Virtual function for dispatching component reads to the correct subclass for entity type E.
		virtual auto componentE_ptr(VecsHandle handle, size_t compidx) noexcept -> void* { return nullptr; };

		/// Virtual function for dispatching whether an entity type E contains a component type C
		virtual auto has_componentE(VecsHandle handle, size_t compidx) noexcept -> bool { return false; };

		/// Virtual function for dispatching erasing an entity from a component table 
		virtual auto eraseE(table_index_t index) noexcept	-> void { };

		virtual auto sizeE() noexcept -> std::atomic<uint32_t>& { return m_size; };

	public:
		VecsRegistryBaseClass() noexcept;

		//-------------------------------------------------------------------------
		//get data

		template<typename C>
		requires is_component_type<C>
		auto has_component(VecsHandle handle) noexcept	-> bool;	///< \returns true if the entity of type E has a component of type C

		template<typename C>
		requires is_component_type<C>
		auto component(VecsHandle handle) noexcept -> C& { return *component_ptr<C>(handle); };	///< Get a component of type C

		template<typename C>
		requires is_component_type<C>
		auto component_ptr(VecsHandle handle) noexcept -> C*;		///< Get a component of type C

		//-------------------------------------------------------------------------
		//update data

		template<typename... Cs>
		requires are_component_types<Cs...>
		auto update(VecsHandle handle, Cs&&... args) noexcept -> bool;	///< Update component of type C of an entity
		
		//-------------------------------------------------------------------------
		//erase

		virtual auto erase(VecsHandle handle) noexcept -> bool;		///< Erase a specific entity

		template<typename... Es>
		requires (are_entity_types<Es...>)
		auto clear() noexcept				-> size_t;		///< Clear the whole ECS


		//-------------------------------------------------------------------------
		//for_each 

		template<typename ETL, typename CTL>
		auto for_each(VecsRangeBaseClass<ETL, CTL>&& r, std::function<typename Functor<CTL>::type> f) -> void;	///< Loop over entities

		template<typename... Ts>
		auto for_each(std::function<typename Functor<typename VecsIterator<Ts...>::component_types>::type> f) -> void { 
			for_each(VecsRange<Ts...>{}, f);		///< Loop over entities
		}	

		//-------------------------------------------------------------------------
		//utility

		template<typename... Es>
		requires (are_entity_types<Es...>)
		auto size() noexcept				-> size_t;		///< \returns the total number of valid entities of types Es

		auto compress() noexcept -> void;							///< Compress all component tables
		auto index(VecsHandle h) noexcept	-> table_index_t;	///< \returns row index in component table
		auto type(VecsHandle h) noexcept	-> type_index_t;	///< \returns type of entity
		virtual auto swap( VecsHandle h1, VecsHandle h2 ) noexcept -> bool;	///< Swap places of two entities in the component table
		virtual auto contains(VecsHandle handle) noexcept	-> bool;		///< \returns true if the ECS still holds this entity 
		virtual void print_type(VecsHandle handle) { 
			auto t = type(handle);
			m_dispatch[t]->print_type(handle); 
		};
	};


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
		return m_dispatch[type(handle)]->has_componentE(handle, vtll::index_of<VecsComponentTypeList, std::decay_t<C>>::value);
	}

	/**
	* \brief Get pointer to a component of type C for a given handle.
	*
	* \param[in] handle The handle of the entity to get the data from.
	* \returns pointer to a component of type C.
	*/
	template<typename C>
	requires is_component_type<C>
		auto VecsRegistryBaseClass::component_ptr(VecsHandle handle) noexcept -> C* {
		if (!handle.is_valid()) return nullptr;
		/// Dispatch to the correct subclass and return result
		return (C*)m_dispatch[type(handle)]->componentE_ptr(handle, vtll::index_of<VecsComponentTypeList, std::decay_t<C>>::value);
	}

	/**
	* \brief Update a component of an entity. The call is dispatched to the correct subclass for entity type E.
	* \param[in] handle The entity handle.
	* \param[in] comp The component data.
	* \returns true if the update was successful.
	*/
	template<typename... Cs>
	requires are_component_types<Cs...>
	auto VecsRegistryBaseClass::update(VecsHandle handle, Cs&&... args) noexcept -> bool {
		if (!handle.is_valid()) return false;
		/// Dispatch the call to the correct subclass and return result
		( m_dispatch[type(handle)]->updateC(handle, vtll::index_of<VecsComponentTypeList, std::decay_t<Cs>>::value, (void*)&args, sizeof(Cs), std::is_rvalue_reference<decltype(args)>::value), ... );
		return true;
	}

	/**
	* \brief Erase an entity from the ECS. The call is dispatched to the correct subclass for entity type E.
	* \param[in] handle The entity handle.
	* \returns true if the operation was successful.
	*/
	auto VecsRegistryBaseClass::erase(VecsHandle handle) noexcept -> bool {
		if (!handle.is_valid()) return false;
		return m_dispatch[type(handle)]->erase(handle); ///< Dispatch to the correct subclass for type E and return result
	}

	/**
	* \brief Return index of an entity in the component table.
	* \param[in] handle The entity handle.
	* \returns the index of the entity in the component table.
	*/
	auto VecsRegistryBaseClass::index(VecsHandle h) noexcept -> table_index_t {
		if (!h.is_valid()) return {};
		return *m_map_table.component_ptr<VecsRegistryBaseClass::c_index>(h.m_map_index);
	}

	/**
	* \brief Return index of an entity in the component table.
	* \param[in] handle The entity handle.
	* \returns the index of the entity in the component table.
	*/
	auto VecsRegistryBaseClass::type(VecsHandle h) noexcept -> type_index_t {
		if (!h.is_valid()) return {};
		return *m_map_table.component_ptr<VecsRegistryBaseClass::c_type>(h.m_map_index);
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
		if (!h1.is_valid() || !h1.is_valid() || type(h1) != type(h2)) return false;
		return m_dispatch[type(h1)]->swap(h1, h2);				///< Dispatch to the correct subclass for type E and return result
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
		return m_dispatch[type(handle)]->contains(handle); ///< Dispatch to the correct subclass for type E and return result
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
		static inline VecsComponentTable<E>		m_component_table;
		static inline std::atomic<uint32_t>		m_sizeE{ 0 };	///< Store the number of valid entities of type E currently in the ECS

		/// Implementations of functions that receive dispatches from the base class.
		auto updateC(VecsHandle handle, size_t compidx, void* ptr, size_t size, bool move = false) noexcept	-> bool; ///< Dispatch from base class
		auto componentE(VecsHandle handle, size_t compidx, void* ptr, size_t size) noexcept	-> bool; ///< Dispatch from base class
		auto componentE_ptr(VecsHandle handle, size_t compidx) noexcept						-> void*;
		auto has_componentE(VecsHandle handle, size_t compidx) noexcept						-> bool; ///< Dispatch from base class
		auto eraseE(table_index_t index) noexcept	-> void { m_component_table.erase(index); };			 ///< Dispatch from base class
		auto compressE() noexcept	-> void { return m_component_table.compress(); };	///< Dispatch from base class
		auto clearE() noexcept		-> size_t { return m_component_table.clear(); };	///< Dispatch from base class
		auto sizeE() noexcept		-> std::atomic<uint32_t>& { return m_sizeE; };

	public:
		VecsRegistry() noexcept : VecsRegistryBaseClass() {};					///< Constructor of class VecsRegistry<E>
		VecsRegistry(std::nullopt_t u) noexcept : VecsRegistryBaseClass() { 	///< Constructor of class VecsRegistry<E>
			m_sizeE = 0; 
		};

		//-------------------------------------------------------------------------
		//insert data

		template<typename... Cs>
		requires are_components_of<E, Cs...> [[nodiscard]]
		auto insert(Cs&&... args) noexcept						-> VecsHandle;	///< Insert new entity of type E into VECS

		template<typename... Cs>
		auto transform(VecsHandle handle, Cs&&... args) noexcept -> bool;	///< transform entity into new type

		//-------------------------------------------------------------------------
		//get data

		auto tuple(VecsHandle handle) noexcept		-> vtll::to_ref_tuple<E>;	///< Return a tuple with copies of the components
		auto tuple_ptr(VecsHandle handle) noexcept	-> vtll::to_ptr_tuple<E>;	///< Return a tuple with pointers to the components

		template<typename C>
		requires is_component_type<C>
		auto has_component() noexcept				-> bool {	///< Return true if the entity type has a component C
			return is_component_of<E,C>;
		}

		template<typename C>
		requires is_component_of<E, C>
		auto component(VecsHandle handle) noexcept -> C& { return *component_ptr<C>(handle); };	///< Get to a component

		template<typename C>
		requires is_component_of<E, C>
		auto component_ptr(VecsHandle handle) noexcept	-> C*;	///< Get pointer to a component
			
		//-------------------------------------------------------------------------
		//update data

		template<typename... Cs> 
		requires are_components_of<E, Cs...>
		auto update(VecsHandle handle, Cs&&... args) noexcept	-> bool;		///< Update one component of an entity
		
		//-------------------------------------------------------------------------
		//erase

		auto erase(VecsHandle handle) noexcept				-> bool;		///< Erase an entity from VECS
		auto clear() noexcept -> size_t { return clearE(); };				///< Clear entities of type E

		//-------------------------------------------------------------------------
		//utility

		auto size() noexcept -> size_t { return m_sizeE.load(); };			///< \returns the number of valid entities of type E
		auto compress() noexcept -> void { compressE(); }					///< Remove erased rows from the component table
		auto capacity(size_t) noexcept						-> size_t;		///< Set max number of entities of this type
		auto swap(VecsHandle h1, VecsHandle h2) noexcept	-> bool;		///< Swap rows in component table
		auto contains(VecsHandle handle) noexcept			-> bool;		///< Test if an entity is still in VECS
		virtual void print_type(VecsHandle handle) { std::cout << "<" << typeid(E).name() << ">"; };
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
	inline auto VecsRegistry<E>::updateC(VecsHandle handle, size_t compidx, void* ptr, size_t size, bool move ) noexcept -> bool {
		if (!contains(handle)) return false;
		return m_component_table.updateC(*m_map_table.component_ptr<c_index>(handle.m_map_index), compidx, ptr, size, move);
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
		if (!contains(handle)) return {};
		return m_component_table.componentE(*m_map_table.component_ptr<c_index>(handle.m_map_index), compidx, ptr, size);
	}

	template<typename E>
	inline auto VecsRegistry<E>::componentE_ptr(VecsHandle handle, size_t compidx) noexcept -> void* {
		if (!contains(handle)) return {};	///< Return the empty component
		auto* comp_table_idx = m_map_table.component_ptr<c_index>(handle.m_map_index); ///< Get reference to component
		return m_component_table.componentE_ptr(*comp_table_idx, compidx);	///< Return the component
	};

	/**
	* \brief Insert a new entity into the ECS.
	* This call has been dispatched from base class using the parameter list.
	*
	* \param[in] args The component arguments.
	* \returns the handle of the new entity.
	*/
	template<typename E>
	template<typename... Cs>
	requires are_components_of<E, Cs...> [[nodiscard]]
	inline auto VecsRegistry<E>::insert(Cs&&... args) noexcept	-> VecsHandle {
		table_index_t map_idx{};	///< index in the map

		bool locked = m_mutex.try_lock();			///< Try to acquire the lock on m_first_free

		if (locked && m_first_free.has_value()) {	///< If we got the lock and there is a free slot -> reuse the free slot
			map_idx = m_first_free;
			m_first_free = *m_map_table.component_ptr<c_index>(map_idx);
			m_mutex.unlock();
		}
		else {										///< Either did not get the lock or no free slot -> create new map entry
			if (locked) m_mutex.unlock();			///< If we got the lock, unlock again
			map_idx = m_map_table.push_back();
			if (!map_idx.has_value()) return {};
			*m_map_table.component_ptr<c_counter>(map_idx) = counter_t{ 0 };		//start with counter 0
		}

		*m_map_table.component_ptr<c_type>(map_idx) = table_index_t{ vtll::index_of<VecsEntityTypeList, E>::value }; ///< Entity type index
		
		VecsHandle handle{ map_idx, *m_map_table.component_ptr<c_counter>(map_idx) }; ///< The new handle
		
		*m_map_table.component_ptr<c_index>(map_idx) ///< Need new handle here so we can put it into the component table 
			= m_component_table.insert(handle, m_map_table.component_ptr<c_mutex>(map_idx), std::forward<Cs>(args)...);	///< add data into component table

		m_size++;
		m_sizeE++;
		return handle;
	};

	/**
	* \brief Transform an entity into a new type.
	* 
	* First all components of the old entity that can be moved to the new form are moved. Then
	* the new parameters given are moved over the components that are fitting.
	*
	* \param[in] handle Handle of the entity to transform.
	* \param[in] args The component arguments.
	* \returns the handle of the entity.
	*/
	template<typename E>
	template<typename... Cs>
	inline auto VecsRegistry<E>::transform(VecsHandle handle, Cs&&... args) noexcept	-> bool {

		if (!contains(handle)) return false;	///< Return the empty component

		//VecsRegistryBaseClass::print_type(handle);
		//std::cout << " type " << VecsRegistry{}.type(handle) << std::endl;

		auto& map_type = *m_map_table.component_ptr<c_type>(handle.m_map_index);	///< Old type index

		//std::cout << "Transform from " << map_type.value << " to " << typeid(E).name() << " " << vtll::index_of<VecsEntityTypeList, E>::value << std::endl << std::endl;

		if (map_type == vtll::index_of<VecsEntityTypeList, E>::value) return true;	///< New type = old type -> do nothing

		auto index = m_component_table.insert(handle, m_map_table.component_ptr<c_mutex>(handle.table_index()));	///< add data into component table
			
		vtll::static_for<size_t, 0, vtll::size<E>::value >(	///< Move old components to new entity type
			[&](auto i) {
				using type = vtll::Nth_type<E, i>;
				auto ptr = componentE_ptr(handle, vtll::index_of<VecsComponentTypeList, type>::value);
				if( ptr != nullptr ) {
					m_component_table.update<type>(index, std::move(*static_cast<type*>(ptr)));
				}
			}
		);

		if constexpr (sizeof...(Cs) > 0) {
			m_component_table.update<Cs>(index, std::forward<Cs>(args)...);			///< Move the arguments to the new entity type
		}

		auto& map_index = *m_map_table.component_ptr<c_index>(handle.m_map_index);	///< Index of old component table in the map
		m_dispatch[map_type]->eraseE(map_index);								///< Erase the entity from old component table

		sizeE()++;
		m_dispatch[map_type]->sizeE()--;

		map_index = index;														///< Index in new component table
		map_type = vtll::index_of<VecsEntityTypeList,E>::value;					///< New type index

		return true;
	}
	
	/**
	* \brief Return a tuple with pointers to the components. 
	*
	* \param[in] handle Entity handle.
	* \returns tuple with pointers to the components.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::tuple_ptr(VecsHandle handle) noexcept -> vtll::to_ptr_tuple<E> {
		if (!contains(handle)) return {};	///< Return the empty component
		auto& comp_table_idx = *m_map_table.component_ptr<c_index>(handle.m_map_index); ///< Get reference to component
		return m_component_table.tuple_ptr( comp_table_idx );
	}

	/**
	* \brief Return a tuple with copies of the components.
	*
	* \param[in] handle Entity handle.
	* \returns tuple with copies of the components.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::tuple(VecsHandle handle) noexcept -> vtll::to_ref_tuple<E> {
		auto& comp_table_idx = *m_map_table.component_ptr<c_index>(handle.m_map_index); ///< Get component copies
		return m_component_table.tuple(comp_table_idx);
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
		if (!h1.is_valid() || !h1.is_valid() || type(h1) != type(h2)) return false;
		if (type(h1) != vtll::index_of<VecsEntityTypeList, E>::value) return false;
		
		table_index_t& i1 = *m_map_table.component_ptr<c_index>(h1.m_map_index);
		table_index_t& i2 = *m_map_table.component_ptr<c_index>(h2.m_map_index);
		std::swap(i1, i2);								///< Swap in map
		auto res = m_component_table.swap(i1, i2);		///< Swap in component table
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
		if (!handle.is_valid()) return false;
		if ( handle.m_generation_counter != *m_map_table.component_ptr<c_counter>(handle.m_map_index)) return false;
		return true;
	}

	/**
	* \brief Retrieve pointer to a component of type C from an entity of type E.
	*
	* \param[in] handle The entity handle.
	* \returns the pointer to the component.
	*/
	template<typename E>
	template<typename C>
	requires is_component_of<E, C>
	auto VecsRegistry<E>::component_ptr(VecsHandle handle) noexcept							-> C* {
		if (!contains(handle)) return {};	///< Return the empty component
		auto& comp_table_idx = *m_map_table.component_ptr<c_index>(handle.m_map_index); ///< Get reference to component
		return &m_component_table.component<C>(comp_table_idx);	///< Return the component
	}

	/**
	* \brief Update a component of type C for an entity of type E.
	*
	* \param[in] handle The entity handle.
	* \param[in] comp The component data.
	* \returns true if the operation was successful.
	*/
	template<typename E>
	template<typename... Cs>
	requires are_components_of<E, Cs...>
	inline auto VecsRegistry<E>::update( VecsHandle handle, Cs&&... args) noexcept -> bool {
		if (!contains(handle)) return false;
		m_component_table.update(handle.m_map_index, std::forward<Cs>(args)...);
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
		if (!contains(handle)) return false;
		m_component_table.erase(*m_map_table.component_ptr <c_index>(handle.m_map_index)); ///< Erase from comp table
		(*m_map_table.component_ptr<c_counter>(handle.m_map_index))++;			///< Invalidate the entity handle

		m_size--;	///< Decrease sizes
		m_sizeE--;

		std::lock_guard<std::mutex> mlock(m_mutex);										///< Protect free list
		*m_map_table.component_ptr<c_index>(handle.m_map_index) = m_first_free;		///< Put old entry into free list
		m_first_free = handle.m_map_index;

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
	inline auto VecsRegistry<E>::capacity(size_t r) noexcept -> size_t {
		auto maxE = m_component_table.capacity(r);
		size_t sum_max = 0;
		vtll::static_for<size_t, 0, vtll::size<VecsEntityTypeList>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<VecsEntityTypeList, i>;
				sum_max += VecsRegistry<type>.capacity(0);
			}
		);

		m_map_table.capacity(sum_max);
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
		VecsRegistry() noexcept : VecsRegistryBaseClass() {};					///< Constructor of class VecsRegistry<vtll::type_list<>>
		VecsRegistry(std::nullopt_t u) noexcept : VecsRegistryBaseClass() {};	///< Constructor of class VecsRegistry<vtll::type_list<>>
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
			auto & handle = *m_data.component_ptr<c_handle>(table_index_t{ m_data.size() - 1 });
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
			auto index = *m_deleted.component_ptr<0>(table_index_t{i});		///< Get next deleted entity from deleted table
			if (index.value < m_data.size()) {								///< Is it inside the table still?		
				m_data.move(index, table_index_t{ m_data.size() - 1 });		///< Yes, move last entity to this position
				auto& handle = *m_data.component_ptr<c_handle>(index);		///< Handle of moved entity
				*VecsRegistryBaseClass().m_map_table.component_ptr<VecsRegistryBaseClass::c_index>(handle.m_map_index) = index; ///< Change map entry of moved last entity
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
			handle = *m_data.component_ptr<c_handle>(table_index_t{ i });
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
	inline VecsRegistryBaseClass::VecsRegistryBaseClass() noexcept {
		if (!this->init()) [[likely]] return;

		vtll::static_for<size_t, 0, vtll::size<VecsEntityTypeList>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<VecsEntityTypeList, i>;
				m_dispatch[i] = std::make_unique<VecsRegistry<type>>(std::nullopt);
			}
		);
	}

	/**
	* \brief Return the total number of valid entities of types Es
	* \returns the total number of valid entities of types Es 
	*/
	template<typename... Es>
	requires (are_entity_types<Es...>)
	inline auto VecsRegistryBaseClass::size() noexcept	-> size_t {
		using entity_list = std::conditional_t< (sizeof...(Es) > 0), expand_tags< vtll::tl<Es...> >, VecsEntityTypeList >;

		size_t sum = 0;
		vtll::static_for<size_t, 0, vtll::size<entity_list>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<entity_list, i>;
				sum += VecsRegistry<type>().size();
			}
		);
		return sum;
	}

	/**
	* \brief Erase all entities from the ECS.
	* 
	* The entities are not really removed, but rather invalidated. Call compress()
	* to actually remove the entities.
	* 
	* \returns the total number of erased entities.
	*/
	template<typename... Es>
	requires (are_entity_types<Es...>)
	inline auto VecsRegistryBaseClass::clear() noexcept	 -> size_t {
		using entity_list = std::conditional_t< (sizeof...(Es) > 0), expand_tags< vtll::tl<Es...> >, VecsEntityTypeList >;

		size_t num = 0;
		vtll::static_for<size_t, 0, vtll::size<entity_list>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<entity_list, i>;
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


	//-------------------------------------------------------------------------
	//for_each

	/**
	* \brief takes two iterators and loops from begin to end, and for each entity calls the provided function.
	*
	* \param[in] range Range to iterate over
	* \param[in] f Functor to be called for every entity the iterator visits.
	*/
	template<typename ETL, typename CLT>
	inline auto VecsRegistryBaseClass::for_each(VecsRangeBaseClass<ETL, CLT>&& range, std::function<typename Functor<CLT>::type> f) -> void {
		auto b = range.begin();
		auto e = range.end();
		for (; b != e; ++b) {
			VecsWriteLock lock(b.mutex());		///< Read lock
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
		return	m_map_index.has_value() && m_generation_counter.has_value();
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
	inline auto VecsHandle::component_ptr() noexcept				-> C* {
		return VecsRegistryBaseClass().component_ptr<C>(*this);
	}

	/**
	* \brief Update a component of type C for the entity that this handle belongs to.
	*
	* \param[in] comp Universal reference to the component data.
	* \returns true if the operation was successful.
	*/
	template<typename... Cs>
	requires are_component_types<Cs...>
	inline auto VecsHandle::update(Cs&&... args) noexcept			-> bool {
		return VecsRegistryBaseClass().update(*this, std::forward<Cs>(args)...);
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
	auto VecsHandle::table_index() noexcept -> table_index_t {
		return VecsRegistryBaseClass{}.index(*this);
	}

	/**
	* \brief Get index of this entity in the component table.
	*
	* \returns index of this entity in the component table.
	*/
	auto VecsHandle::type_index() noexcept -> type_index_t {
		return VecsRegistryBaseClass{}.type(*this);
	}

	/**
	* \brief Return a pointer to the mutex of this entity.
	*
	* \returns a pointer to the mutex of this entity.
	*/
	inline std::atomic<uint32_t>* VecsHandle::mutex() {
		if (!is_valid()) return nullptr;
		return VecsRegistryBaseClass::m_map_table.component_ptr<VecsRegistryBaseClass::c_mutex>(m_map_index);
	}

}

#include "VECSIterator.h"

#endif
