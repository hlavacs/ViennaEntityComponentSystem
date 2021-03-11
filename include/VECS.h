#ifndef VECS_H
#define VECS_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <variant>
#include <array>
#include <memory>
#include <optional>
#include <functional>
#include <chrono>
#include "VTLL.h"
#include "VECSTable.h"
#include "VECSCompSystem.h"
#include "VECSCompUser.h"  //user defined component types and entity types

using namespace std::chrono_literals;

namespace vecs {

	/** 
	* \brief Component type list: a list with all component types that an entity can have.
	* 
	* It is the sum of the components of the engine part, and the components as defined by the engine user
	*/
	using VecsComponentTypeList = vtll::cat< VeComponentTypeListSystem, VeComponentTypeListUser >;

	/**
	* \brief Entity type list: a list with all possible entity types the ECS can deal with.
	* 
	* An entity type is a collection of component types. 
	* The list is the sum of the entitiy types of the engine part, and the entity types as defined by the engine user.
	*/
	using VecsEntityTypeList = vtll::cat< VeEntityTypeListSystem, VeEntityTypeListUser >;

	/**
	* \brief Table size map: a VTLL map specifying the default sizes for component tables.
	* 
	* It is the sum of the maps of the engine part, and the maps as defined by the engine user
	* Every entity type can have an entry, but does not have to have one.
	* An entry is alwyas a vtll::value_list<A,B>, where A defines the default size of segments, and B defines
	* the max number of entries allowed. Both A and B are actually exponents with base 2. So segemnts have size 2^A.
	*/
	using VecsTableSizeMap = vtll::cat< VeTableSizeMapSystem, VeTableSizeMapUser >;

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
	* Since the sum of all max sizes is probably not divisible by the segment size, get the next multiple of the VecsTableMaxSeg.
	*/
	using VecsTableMaxSize = std::integral_constant<size_t, VecsTableMaxSeg::value * (VecsTableMaxSizeSum::value / VecsTableMaxSeg::value + 1)>;

	using VecsTableLayoutMap = vtll::cat< VeTableLayoutMapSystem, VeTableLayoutMapUser >;


	/**
	* Declarations of the main VECS classes
	*/

	class VecsHandle;
	class VecsLock;
	template <typename E> class VecsEntityProxy;
	template<typename E> class VecsComponentTable;
	template<typename E, size_t I> class VecsComponentTableDerived;
	class VecsRegistryBaseClass;
	template<typename E> class VecsRegistry;
	template<typename... Cs> class VecsIterator;
	template<typename E, typename... Cs> class VecsIteratorDerived;

	/** basic concepts */
	template<typename C>
	concept is_component_type = (vtll::has_type<VecsComponentTypeList, std::decay_t<C>>::value); ///< C is a component

	template<typename... Cs>
	concept are_component_types = (is_component_type<Cs> && ...);	///< Cs are all components

	template<typename E>
	concept is_entity_type = (vtll::has_type<VecsEntityTypeList, std::decay_t<E>>::value); ///< E is an entity type

	template<typename E, typename C>
	concept is_component_of = (vtll::has_type<E, std::decay_t<C>>::value);	///< C is a component of E

	template<typename E, typename... Cs>
	concept are_components_of = (is_component_of<E, Cs> && ...);	///< Cs are all components of E

	template<typename E, typename... Cs>
	concept is_composed_of = (vtll::is_same<E, std::decay_t<Cs>...>::value);	///< E is composed of Cs

	template<typename ET, typename E = vtll::front<ET>>
	concept is_entity = (is_entity_type<E> && std::is_same_v<std::decay_t<ET>, VecsEntityProxy<E>>); ///< ET is a VecsEntityProxy


	/** 
	* \brief General functor type that can hold any function, and depends in a number of component types.
	*/
	template<typename... Cs>
	requires are_component_types<Cs...>
	using Functor = void(VecsHandle, Cs&...);


	//-------------------------------------------------------------------------
	//entity handle

	/**
	* \brief Handles are IDs of entities. Use them to access entitites.
	* 
	* VecsHandle are used to ID entities of type E by storing their type as an index.
	*/
	class VecsHandle {
		friend class VecsLock;
		friend VecsRegistryBaseClass;
		template<typename E> friend class VecsRegistry;
		template<typename E> friend class VecsComponentTable;
		template<typename E, size_t I> friend class VecsComponentTableDerived;

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

		/** \returns the type index of the handle. */
		auto type() const noexcept -> uint32_t { return static_cast<uint32_t>(m_type_index); };

		auto is_valid() noexcept -> bool;	///< The data in the handle is non null
		auto has_value() noexcept -> bool;	///< The entity that is pointed to exists in the ECS

		template<typename E>
		requires is_entity_type<E>
		auto proxy() noexcept -> VecsEntityProxy<E>; ///< Get local copy (VecsEntityProxy) of entity data

		template<typename C>
		requires is_component_type<C> 
		auto component() noexcept -> std::optional<C>;		///< Get a component of type C of the entity (first found is copied)

		template<typename ET>
		requires is_entity<ET>
		auto update(ET&& ent) noexcept -> bool;				///< Update this entity using a VecsEntityProxy of the same type

		template<typename C>
		requires is_component_type<C>
		auto update(C&& comp) noexcept -> bool;				///< Update a component of type C

		auto erase() noexcept -> bool;						///< Erase the entity
	};

	//-------------------------------------------------------------------------
	//locking

	/**
	* \brief VecsLock is used to lock access to single entities. 
	*/
	class VecsLock {
		std::atomic_flag* m_flag{ nullptr };	///< Flag used for locking access
		auto lock() noexcept -> void;			///< Acquire the lock
	public:
		VecsLock(std::atomic_flag* flag) noexcept;	///< Acquire the lock
		VecsLock(VecsHandle handle) noexcept;		///< Acquire the lock
		auto is_valid() noexcept -> bool;
		~VecsLock() noexcept;						///< Release the lock
	};

	/**
	* \brief VecsEntityProxy can hold a copy of the data of an entity of type E. This includes its handle
	* and all components.
	* 
	* VecsEntityProxy is a local copy of an entity of type E and all its components. The components can be retrieved and
	* changes locally. By calling update() the local copies are stored back into the ECS. By calling erase() the entity is
	* erased from the ECS. 
	*/

	template <typename E>
	class VecsEntityProxy {
	public:
		using tuple_type = vtll::to_tuple<E>;	///< A tuple holding all entity components.

	protected:
		VecsHandle	m_handle{};			///< The entity handle.
		tuple_type	m_component_data{};	///< The local copy of the entity components.

	public:

		/**
		* \brief Constructor of the VecsEntityProxy class.
		* 
		* \param[in] h Handle of the entity.
		* \param[in] tup The copy of the entity data to be stored in the instance.
		*/
		VecsEntityProxy(VecsHandle h, const tuple_type& tup) noexcept : m_handle{ h }, m_component_data{ tup } {};

		VecsEntityProxy() {};	///< Empty constructor results in an invalid proxy

		auto handle() const noexcept -> VecsHandle {	///< \returns the handle of the entity. 
			return m_handle; 
		}

		auto has_value() noexcept	 -> bool;			///< Check whether the entity still exists in the ECS
		auto update() noexcept		 -> bool;			///< Update the entity in the ECS
		auto erase() noexcept		 -> bool;			///< Erase the entity from the ECS

		template<size_t I>
		auto component() noexcept -> std::optional<vtll::Nth_type<E,I>> {	///< \returns the Ith component of the entity.
			return { std::get<I>(m_component_data) };
		};
		
		template<typename C>
		requires is_component_of<E,C>
		auto component() noexcept -> C {	///< \returns the first component of type C. 
			return std::get<vtll::index_of<E,std::decay_t<C>>::value>(m_component_data);
		};

		/** 
		* \brief Update the local copy with type C
		* \param[in] comp A universal reference to the new component value. 
		*/
		template<typename C>
		requires is_component_of<E, C>
		auto local_update( C&& comp ) noexcept -> void {
			std::get<vtll::index_of<E, std::decay_t<C>>::value>(m_component_data) = comp;
		};
	};



	//-------------------------------------------------------------------------
	//component vector - each entity type has them


	/**
	* \brief This class stores all components of entities of type E.
	*/
	template<typename E>
	class VecsComponentTable : public VecsMonostate<VecsComponentTable<E>> {
		friend class VecsRegistryBaseClass;
		template<typename E> friend class VecsRegistry;
		template<typename... Cs> friend class VecsIterator;
		template<typename E, typename... Cs> friend class VecsIteratorDerived;

	protected:
		using value_type = vtll::to_tuple<E>;		///< A tuple storing all components of entity of type E
		using layout_type = vtll::map<VecsTableLayoutMap, E, VECS_LAYOUT_DEFAULT>;

		using info = vtll::type_list<VecsHandle, std::atomic_flag*>;	///< List of management data per entity (only a handle)
		static const size_t c_handle = 0;		///< Component index of the handle info
		static const size_t c_flag = 1;			///< Component index of the handle info
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
		static inline std::array<std::unique_ptr<VecsComponentTable<E>>, vtll::size<VecsComponentTypeList>::value> m_dispatch;

		virtual auto updateC(index_t entidx, size_t compidx, void* ptr, size_t size) noexcept		-> bool;
		virtual auto componentE(index_t entidx, size_t compidx, void* ptr, size_t size)  noexcept	-> bool;
		auto remove_deleted_tail() noexcept -> void;

		VecsComponentTable(size_t r = 1 << c_max_size) noexcept;

		template<typename... Cs>
		requires is_composed_of<E, Cs...> [[nodiscard]]
		auto insert(VecsHandle handle, std::atomic_flag* flag, Cs&&... args) noexcept	-> index_t;

		auto values(const index_t index) noexcept				-> value_type;
		auto handle(const index_t index) noexcept				-> VecsHandle;
		auto flag(const index_t index) noexcept					-> std::atomic_flag*;

		/** \returns the number of entries currently in the table, can also be invalid ones */
		auto size() noexcept									-> size_t { return m_data.size(); };
		auto erase(const index_t idx) noexcept					-> bool;
		auto compress() noexcept								-> void;
		auto clear() noexcept									-> size_t ;

		template<typename C>
		requires is_component_of<E, C>
		auto component(const index_t index) noexcept			-> C&;

		template<typename ET>
		requires is_entity<ET, E>
		auto update(const index_t index, ET&& ent) noexcept		-> bool;
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
	inline auto VecsComponentTable<E>::insert(VecsHandle handle, std::atomic_flag* flag, Cs&&... args) noexcept -> index_t {
		auto idx = m_data.push_back();			///< Allocate space a the end of the table
		if (!idx.has_value()) return idx;		///< Check if the allocation was successfull
		m_data.update<c_handle>(idx, handle);	///< Update the handle data
		m_data.update<c_flag>(idx, flag);		///< Update the flag
		(m_data.update<c_info_size + vtll::index_of<E,std::decay_t<Cs>>::value> (idx, std::forward<Cs>(args)), ...); ///< Update the entity components
		return idx;								///< Return the index of the new data
	};

	/**
	* \param[in] index The index of the data in the component table.
	* \returns a tuple holding the components of an entity.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::values(const index_t index) noexcept -> typename VecsComponentTable<E>::value_type {
		assert(index < m_data.size());
		auto tup = m_data.tuple_value(index);											///< Get the whole data from the data
		return vtll::sub_tuple< c_info_size, std::tuple_size_v<decltype(tup)> >(tup);	///< Return only entity components
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
	* \returns pointer to the sync flag of the entity.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::flag(const index_t index) noexcept -> std::atomic_flag* {
		assert(index < m_data.size());
		return m_data.comp_ref_idx<c_flag>(index);	///< Get ref to the flag and return it
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
	* \brief Update all components using a reference to a VecsEntityProxy instance.
	* \param[in] index Index of the entity to be updated in the component table.
	* \param[in] ent Universal reference to the new entity data.
	* \returns true if the update was successful.
	*/
	template<typename E>
	template<typename ET>
	requires is_entity<ET, E>
	inline auto VecsComponentTable<E>::update(const index_t index, ET&& ent) noexcept -> bool {
		vtll::static_for<size_t, 0, vtll::size<E>::value >(	///< Loop over all components
			[&](auto i) {
				m_data.update<c_info_size + i>(index, ent.component<i>().value()); ///< Update each component
			}
		);
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
	inline auto VecsComponentTable<E>::erase(const index_t index) noexcept -> bool {
		assert(index < m_data.size());
		m_data.comp_ref_idx<c_handle>(index) = {};		///< Invalidate handle	
		m_deleted.push_back(std::make_tuple(index));	///< Push the index to the deleted table.
		return true;
	}


	//-------------------------------------------------------------------------
	//comnponent vector derived class

	/**
	* \brief This class is derived from the component vector and is used to update or
	* return components C with indx I of entities of type E
	*/

	template<typename E, size_t I>
	class VecsComponentTableDerived : public VecsComponentTable<E> {
	public:
		using C = vtll::Nth_type<VecsComponentTypeList,I>;	///< Component type

		/** 
		* \brief Constructor of class VecsComponentTableDerived 
		* \param[in] r Max number of entries allowed in the component table 
		*/
		VecsComponentTableDerived( size_t r = 1 << VecsComponentTable<E>::c_max_size) noexcept : VecsComponentTable<E>(r) {};

	protected:

		/** 
		* \brief Update the component C of entity E
		* \param[in] index Index of the entity in the component table.
		* \param[in] comp Universal reference to the new component data.
		* \returns true if the update was successful. 
		*/
		auto update(const index_t index, C&& comp) noexcept -> bool {
			if constexpr (is_component_of<E,C>) {
				this->m_data.comp_ref_idx<this->c_info_size + vtll::index_of<E, std::decay_t<C>>::value>() = comp;
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
			if constexpr (is_component_of<E,C>) {
				this->m_data.comp_ref_idx<this->c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index) = *((C*)ptr);
				return true;
			}
			return false;
		};

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
				*((C*)ptr) = this->m_data.comp_ref_idx<this->c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(entidx);
				return true;
			}
			return false;
		};
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
				m_dispatch[i] = std::make_unique<VecsComponentTableDerived<E, i>>(r);
			}
		);
	};


	//-------------------------------------------------------------------------
	//entity table base class

	template<typename... Cs>
	class VecsIterator;

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

		friend class VecsLock;
		template<typename E> friend class VecsComponentTable;

	protected:

		using types = vtll::type_list<index_t, counter16_t, index16_t, std::atomic_flag>;	///< Type for the table
		static const uint32_t c_index{ 0 };		///< Index for accessing the index to next free or entry in component table
		static const uint32_t c_counter{ 1 };	///< Index for accessing the generation counter
		static const uint32_t c_type{ 2 };		///< Index for accessing the type index
		static const uint32_t c_flag{ 3 };		///< Index for accessing the lock flag

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

	public:
		VecsRegistryBaseClass( size_t r = VecsTableMaxSize::value ) noexcept;

		//-------------------------------------------------------------------------
		//insert data

		template<typename... Cs> [[nodiscard]]
		auto insert(Cs&&... args) noexcept	-> VecsHandle;	///< Insert a new entity into the ECS

		//-------------------------------------------------------------------------
		//get data

		template<typename E>
		auto proxy(VecsHandle handle) noexcept -> VecsEntityProxy<E>;	///< Get a local copy of an entity

		template<typename C>
		requires is_component_type<C>
		auto component(VecsHandle handle) noexcept -> std::optional<C>;		///< Get a component of type C

		//-------------------------------------------------------------------------
		//update data

		template<typename ET>
		auto update(VecsHandle handle, ET&& ent) noexcept -> bool;		///< Update an entity 

		template<typename C>
		requires is_component_type<C>
		auto update(VecsHandle handle, C&& comp) noexcept -> bool;		///< Update component of type C of an entity

		//-------------------------------------------------------------------------
		//erase

		auto clear() noexcept -> size_t;					///< Clear the whole ECS

		template<typename E>
		requires is_entity_type<E>
		auto clear() noexcept -> size_t;					///< Erase all entities of type E

		virtual
		auto erase(VecsHandle handle) noexcept -> bool;		///< Erase a specific entity

		auto compress() noexcept -> void;					///< Compress all component tables

		template<typename E>
		requires is_entity_type<E>
		auto compress() noexcept -> void;					///< Compress component table for entities of type E

		//-------------------------------------------------------------------------
		//utility

		template<typename E = void>
		auto size() noexcept		-> size_t {		///< \returns the number of valid entities of type E
			return VecsRegistry<E>::m_sizeE.load(); 
		};

		template<>
		auto size<>() noexcept		-> size_t {		///< \returns the total number of valid entities
			return m_size.load(); 
		};

		template<typename... Cs>
		auto begin() noexcept		-> VecsIterator<Cs...>;		///< Get iterator pointing to first entity with all components

		template<typename... Cs>
		auto end() noexcept			-> VecsIterator<Cs...>;		///< Get an end iterator for entities with all components

		template<typename... Cs>
		requires are_component_types<Cs...>
		auto for_each(VecsIterator<Cs...>& b, VecsIterator<Cs...>& e, std::function<Functor<Cs...>> f) -> void;

		template<typename... Cs>
		requires are_component_types<Cs...>
		auto for_each(std::function<Functor<Cs...>> f) -> void;

		virtual
		auto contains(VecsHandle handle) noexcept	-> bool;	///< \returns true if the ECS still holds this entity (externally synced)
	};


	/**
	* \brief Insert a new entity into the ECS.  The call is dispatched to the correct subclass for entity type E.
	*
	* The correct entity type E is determined using the component types Cs. Then the
	* call id forwarded to the correct subclass for E.
	* \param[in] args The components for the entity.
	* \returns the handle of thw new entity.
	*/
	template<typename... Cs> [[nodiscard]]
	auto VecsRegistryBaseClass::insert(Cs&&... args) noexcept	-> VecsHandle {
		static_assert(vtll::is_same<VeEntityType<std::decay_t<Cs>...>, std::decay_t<Cs>...>::value);
		return VecsRegistry<VeEntityType<std::decay_t<Cs>...>>().insert(std::forward<Cs>(args)...);
	}

	/**
	* \brief Get the data of a component of type C for a given handle.
	*
	* The first component of type C is retrieved. So component types should be unique if you want to
	* use this function. Then the call is dispatched to the correct subclass for entity type E.
	* \param[in] handle The handle of the entity to get the data from.
	* \returns a std::optional holding the desired data, or an empty std::optional if the data is not available.
	*/
	template<typename C>
	requires is_component_type<C>
	auto VecsRegistryBaseClass::component(VecsHandle handle) noexcept -> std::optional<C> {
		if (!handle.is_valid()) return {};
		C res{};

		/// Dispatch to the correct subclass and return result
		if (m_dispatch[handle.type()]->componentE(handle, vtll::index_of<VecsComponentTypeList, std::decay_t<C>>::value, (void*)&res, sizeof(C))) {
			return { res };
		}
		return {};
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

	template<typename E = void>
	class VecsRegistry : public VecsRegistryBaseClass {

		friend class VecsRegistryBaseClass;

	protected:
		static inline std::atomic<uint32_t> m_sizeE{0};	///< Store the number of valid entities of type E currently in teh ECS

		/** Maximum number of entites of type E that can be stored. */
		static const size_t c_max_size = vtll::back_value<vtll::map< VecsTableSizeMap, E, VeTableSizeDefault > >::value;

		/** Implementations of functions that receive dispatches from the base class. */
		auto updateC(VecsHandle handle, size_t compidx, void* ptr, size_t size) noexcept	-> bool ;
		auto componentE(VecsHandle handle, size_t compidx, void* ptr, size_t size) noexcept	-> bool;
		auto compressE() noexcept	-> void { return VecsComponentTable<E>().compress(); };	///< Forward to component table of type E

		/**
		* \brief Erase all entites of type E
		*/
		auto clearE() noexcept		-> size_t { 		///< Forward to component table of type E
			m_sizeE = 0;
			return VecsComponentTable<E>().clear();		///< Call clear() in the correct component table
		};

	public:
		/** Constructors for class VecsRegistry<E>. */
		VecsRegistry(size_t r = 1 << c_max_size) noexcept : VecsRegistryBaseClass() { VecsComponentTable<E>{r}; };
		VecsRegistry(std::nullopt_t u) noexcept : VecsRegistryBaseClass() {};

		//-------------------------------------------------------------------------
		//insert data

		template<typename... Cs> 
		requires is_composed_of<E, Cs...> [[nodiscard]] 
		auto insert(Cs&&... args) noexcept					-> VecsHandle;

		//-------------------------------------------------------------------------
		//get data

		auto proxy(VecsHandle h) noexcept					-> VecsEntityProxy<E>;

		template<typename C>
		requires is_component_of<E, C>
		auto component(VecsHandle handle) noexcept			->std::optional<C> ;

		//-------------------------------------------------------------------------
		//update data

		template<typename ET>
		requires is_entity<ET, E>
		auto update(VecsHandle handle, ET&& ent) noexcept	-> bool;

		template<typename C> 
		requires is_component_of<E, C>
		auto update(VecsHandle handle, C&& comp) noexcept	-> bool;

		//-------------------------------------------------------------------------
		//erase

		auto erase(VecsHandle handle) noexcept				-> bool;

		//-------------------------------------------------------------------------
		//utility

		auto contains(VecsHandle handle) noexcept			-> bool;
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
		VecsLock lock(handle);
		if (!contains(handle)) return {};
		return VecsComponentTable<E>().updateC(m_entity_table.comp_ref_idx<c_index>(handle.m_entity_index), compidx, ptr, size);
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
		VecsLock lock(handle);
		if (!contains(handle)) return {};
		return VecsComponentTable<E>().componentE(m_entity_table.comp_ref_idx<c_index>(handle.m_entity_index), compidx, ptr, size);
	}

	/**
	* \brief Insert a new entity into the ECS.
	* This call has been dispatched from base class using the parameter list.
	*
	* \param[in] args The component arguments.
	* \returns the handle of the new entity.
	*/
	template<typename E>
	template<typename... Cs> 
	requires is_composed_of<E, Cs...> [[nodiscard]]
	inline auto VecsRegistry<E>::insert(Cs&&... args) noexcept	-> VecsHandle {
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

		m_entity_table.comp_ref_idx<c_type>(idx) = index16_t{ vtll::index_of<VecsEntityTypeList, E>::value };
		
		VecsHandle handle{ idx, m_entity_table.comp_ref_idx<c_counter>(idx), m_entity_table.comp_ref_idx<c_type>(idx) };
		
		m_entity_table.comp_ref_idx<c_index>(idx) = VecsComponentTable<E>().insert(handle, &m_entity_table.comp_ref_idx<c_flag>(idx), args...);		//add data as tuple

		m_size++;
		m_sizeE++;
		return handle;
	};

	/**
	* \brief Check whether the ECS contains an entity. 
	*
	* \param[in] handle The entity handle.
	* \return true if the entity is still contained in the ECS.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::contains(VecsHandle handle) noexcept -> bool {
		if (!handle.is_valid() || handle.m_type_index != vtll::index_of<VecsEntityTypeList,E>::value ) return false;
		auto& type = m_entity_table.comp_ref_idx<c_type>(handle.m_entity_index);
		auto& cnt = m_entity_table.comp_ref_idx<c_counter>(handle.m_entity_index);
		if ( handle.m_generation_counter != cnt || handle.m_type_index	!= type ) return false;
		return true;
	}

	/**
	* \brief Retrieve all values for a given entity.
	*
	* \param[in] handle The entity handle.
	* \returns a VecsEntityProxy<E>
	*/
	template<typename E>
	inline auto VecsRegistry<E>::proxy(VecsHandle handle) noexcept -> VecsEntityProxy<E> {
		VecsLock lock(handle);
		if (!contains(handle)) return {};
		return VecsEntityProxy<E>(handle, VecsComponentTable<E>().values(m_entity_table.comp_ref_idx<c_index>(handle.m_entity_index)));
	}

	/**
	* \brief Retrieve a component of type C from an entity of type E.
	*
	* \param[in] handle The entity handle.
	* \returns std::optional containing the component, or is empty.
	*/
	template<typename E>
	template<typename C>
	requires is_component_of<E, C>
	inline auto VecsRegistry<E>::component( VecsHandle handle) noexcept -> std::optional<C> {
		VecsLock lock(handle);
		if constexpr (!vtll::has_type<E, std::decay_t<C>>::value) return {};
		if (!contains(handle)) return {};	///< Return the empty std::optional
		auto& comp_table_idx = m_entity_table.comp_ref_idx<c_index>(handle.m_entity_index); ///< Get reference to component
		return { VecsComponentTable<E>().component<C>(comp_table_idx) };	///< Return the std::optional
	}

	/**
	* \brief Update all components of an entity of type E from a VecsEntityProxy<E>
	*
	* \param[in] handle The Enity handle. 
	* \param[in] ent A universal reference to the VecsEntityProxy<E> that contains the data.
	* \returns true if the operation was successful.
	*/
	template<typename E>
	template<typename ET>
	requires is_entity<ET, E>
	inline auto VecsRegistry<E>::update( VecsHandle handle, ET&& ent) noexcept -> bool {
		VecsLock lock(handle);
		if (!contains(handle)) return false;
		VecsComponentTable<E>().update(handle.m_entity_index, std::forward<ET>(ent));
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
		VecsLock lock(handle);
		if constexpr (!vtll::has_type<E, std::decay_t<C>>::value) { return false; }
		if (!contains(handle)) return false;
		VecsComponentTable<E>().update<C>(handle.m_entity_index, std::forward<C>(comp));
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
			VecsLock lock(handle);
			if (!contains(handle)) return false;
			VecsComponentTable<E>().erase(m_entity_table.comp_ref_idx <c_index>(handle.m_entity_index)); ///< Erase from comp table
			m_entity_table.comp_ref_idx<c_counter>(handle.m_entity_index)++;				///< Invalidate the entity handle
		}

		m_size--;	///< Decrease sizes
		m_sizeE--;

		std::lock_guard<std::mutex> mlock(m_mutex);										///< Protect free list
		m_entity_table.comp_ref_idx<c_index>(handle.m_entity_index) = m_first_free;		///< Put old entry into free list
		m_first_free = handle.m_entity_index;

		return true; 
	}


	//-------------------------------------------------------------------------
	//VecsRegistry<void> specialization for void

	/**
	* \brief A specialized VecsRegistry<E> to act as convenient access interface instead of the base class.
	*/

	template<>
	class VecsRegistry<void> : public VecsRegistryBaseClass {
	public:
		VecsRegistry(size_t r = VecsTableMaxSize::value) noexcept : VecsRegistryBaseClass(r) {};
		VecsRegistry(std::nullopt_t u) noexcept : VecsRegistryBaseClass() {};
	};


	//-------------------------------------------------------------------------
	//iterator

	/**
	* \brief Base class for an iterator that iterates over a VecsComponentTable of any type 
	* and that is intested into components Cs
	* 
	* The iterator can be used to loop through the entities. By providing component types Cs..., only
	* entities are included that contain ALL component types.
	*/

	template<typename... Cs>
	class VecsIterator {
		
		template<typename E, typename... Cs>
		friend class VecsIteratorDerived;

	protected:
		using entity_types = vtll::filter_have_all_types< VecsEntityTypeList, vtll::type_list<Cs...> >; ///< List with entity types
		using last_type = vtll::back<entity_types>;	///< last type for end iterator

		std::array<std::unique_ptr<VecsIterator<Cs...>>, vtll::size<entity_types>::value> m_dispatch; ///< Subiterators for each entity type E
		index_t m_current_iterator{ 0 };	///< Current iterator of type E that is used
		index_t m_current_index{ 0 };		///< Current index in the VecsComponentTable<E>
		bool	m_is_end{ false };			///< True if this is an end iterator (for stopping the loop)
		size_t	m_size{0};					///< Number of entities max covered by the iterator

		VecsIterator(std::nullopt_t n) noexcept {};			///< Needed for derived iterator to call

	public:
		using value_type = std::tuple<VecsHandle, Cs...>; ///< Tuple containing all component values
		using ref_type = std::tuple<VecsHandle, Cs&...>; ///< Tuple containing all component values

		VecsIterator( bool is_end = false ) noexcept ;		///< Constructor that should be called always from outside
		VecsIterator(const VecsIterator& v) noexcept;		///< Copy constructor

		auto begin() { return *this; };
		auto end() { return VecsRegistryBaseClass().end<Cs...>(); };

		auto operator=(const VecsIterator& v) noexcept			-> VecsIterator<Cs...>&;	///< Copy
		auto operator+=(size_t N) noexcept						-> VecsIterator<Cs...>&;	///< Increase and set
		auto operator+(size_t N) noexcept						-> VecsIterator<Cs...>&;	///< Increase
		auto operator!=(const VecsIterator<Cs...>& v) noexcept	-> bool;	///< Unqequal
		auto operator==(const VecsIterator<Cs...>& v) noexcept	-> bool;	///< Equal

		virtual auto handle() noexcept			-> VecsHandle;
		virtual auto flag() noexcept			-> std::atomic_flag*;
		virtual auto has_value() noexcept		-> bool;					///< Is currently pointint to a valid entity
		virtual	auto operator*() noexcept		-> ref_type;				///< Access the data
		virtual auto operator++() noexcept		-> VecsIterator<Cs...>&;	///< Increase by 1
		virtual auto operator++(int) noexcept	-> VecsIterator<Cs...>&;	///< Increase by 1
		virtual auto is_vector_end() noexcept	-> bool;					///< Is currently at the end of any sub iterator
		virtual auto size() noexcept			-> size_t;					///< Number of valid entities
	};


	/**
	* \brief Copy-constructor of class VecsIterator
	*
	* \param[in] v Other Iterator that should be copied
	*/
	template<typename... Cs>
	inline VecsIterator<Cs...>::VecsIterator(const VecsIterator& v) noexcept : VecsIterator(v.m_is_end) {
		m_current_iterator = v.m_current_iterator;
		m_current_index = v.m_current_index;
		if (m_is_end) return;
		for (int i = 0; i < m_dispatch.size(); ++i) { m_dispatch[i]->m_current_index = v.m_dispatch[i]->m_current_index; }
	};

	/**
	* \brief Determines whether the iterator currently points to avalid entity.
	* The call is dispatched to the respective sub-iterator that is currently used.
	*
	* \returns true if the iterator points to a valid entity.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::has_value() noexcept	-> bool {
		if (m_is_end || is_vector_end()) return false;
		return m_dispatch[m_current_iterator]->has_value();
	}

	/**
	* \brief Retrieve the handle of the current entity.
	* \returns the handle of the current entity.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::handle() noexcept	-> VecsHandle {
		return m_dispatch[m_current_iterator]->handle();
	}

	/**
	* \brief Retrieve the handle of the current entity.
	* \returns the handle of the current entity.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::flag() noexcept	-> std::atomic_flag* {
		return m_dispatch[m_current_iterator]->flag();
	}

	/**
	* \brief Copy assignment operator.
	*
	* \param[in] v The source iterator for the assignment.
	* \returns *this.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator=(const VecsIterator& v) noexcept	-> VecsIterator<Cs...>& {
		m_current_iterator = v.m_current_iterator;
		m_current_index = v.m_current_index;
		for (int i = 0; i < m_dispatch.size(); ++i) { m_dispatch[i]->m_current_index = v.m_dispatch[i]->m_current_index; }
		return *this;
	}

	/**
	* \brief Get the values of the entity that the iterator is currently pointing to.
	*
	* \returns a tuple holding the components Cs of the entity the iterator is currently pointing to.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator*() noexcept	-> ref_type {
		return *(*m_dispatch[m_current_iterator]);
	};

	/**
	* \brief Goto next entity.
	*
	* \returns *this.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator++() noexcept		-> VecsIterator<Cs...>& {
		if (m_is_end) return *this;
		(*m_dispatch[m_current_iterator])++;

		if (m_dispatch[m_current_iterator]->is_vector_end() && m_current_iterator.value < m_dispatch.size() - 1) {
			++m_current_iterator;
		}
		m_current_index = m_dispatch[m_current_iterator]->m_current_index;
		return *this;
	};

	/**
	* \brief Goto next entity.
	*
	* \returns *this.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator++(int) noexcept	-> VecsIterator<Cs...>& {
		return operator++();
	};

	/**
	* \brief Jump to an entity that is N positions away.
	* 
	* This can involve iterating over K subiterators.
	*
	* \param[in] N The number of positions to jump forward.
	* \returns *this.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator+=(size_t N) noexcept -> VecsIterator<Cs...>& {
		if (m_is_end) return *this;
		size_t left = N;
		while (left > 0) {
			int num = std::max(m_dispatch[m_current_iterator]->size() - m_current_index, 0);
			left -= num;
			m_dispatch[m_current_iterator]->m_current_index += static_cast<decltype(m_current_index.value)>(num);
			m_current_index = m_dispatch[m_current_iterator]->m_current_index;

			if (m_dispatch[m_current_iterator]->is_vector_end()) {
				if (m_current_iterator.value < m_dispatch.size() - 1) {
					++m_current_iterator;
					m_current_index = 0;
				}
				else return *this;
			}
		}
		return *this;
	}

	/**
	* \brief Create an iterator that is N positions further
	*
	* \param[in] N Number of positions to jump.
	* \returns The new iterator.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator+(size_t N) noexcept	-> VecsIterator<Cs...>& {
		VecsIterator<Cs...> temp{ *this };
		temp += N;
		return temp;
	}

	/**
	* \brief Test for unequality
	*
	* \returns true if the iterators are not equal.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator!=(const VecsIterator<Cs...>& v) noexcept	-> bool {
		return !(*this == v);
	}

	/**
	* \brief Test for equality.
	*
	* \returns true if the iterators are equal.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator==(const VecsIterator<Cs...>& v) noexcept	-> bool {
		return	v.m_current_iterator == m_current_iterator && v.m_current_index == m_current_index;
	}

	/**
	* \brief Test whether the curerntly used sub-iterator is at its end point.
	* The call is dispatched to the current subiterator.
	*
	* \returns true if the current subiterator is at its end.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::is_vector_end() noexcept		-> bool {
		return m_dispatch[m_current_iterator]->is_vector_end();
	}

	/**
	* \brief Get the total number of all entities that are covered by this iterator.
	* This is simply the sum of the entities covered by the subiterators. 
	* This number is set when the iterator is created.
	*
	* \returns the total number of entities that have all components Cs.
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::size() noexcept	-> size_t {
		return m_size;
	}



	//----------------------------------------------------------------------------------------------

	/**
	* \brief Iterator that iterates over a VecsComponentTable of type E
	* and that is intested into components Cs
	*/

	template<typename E, typename... Cs>
	class VecsIteratorDerived : public VecsIterator<Cs...> {
	protected:
		size_t m_sizeE{ 0 };

	public:
		VecsIteratorDerived(bool is_end = false) noexcept;
		auto handle() noexcept			-> VecsHandle;
		auto flag() noexcept			-> std::atomic_flag*;
		auto has_value() noexcept		-> bool;
		auto operator*() noexcept		-> typename VecsIterator<Cs...>::ref_type;
		auto operator++() noexcept		-> VecsIterator<Cs...>&;
		auto operator++(int) noexcept	-> VecsIterator<Cs...>&;
		auto is_vector_end() noexcept	-> bool;
		auto size() noexcept			-> size_t;
	};


	/**
	* \brief Constructor of class VecsIteratorDerived<E, Cs...>. Calls empty constructor of base class.
	*
	* \param[in] is_end If true, then the iterator belongs to an end-iterator.
	*/
	template<typename E, typename... Cs>
	inline VecsIteratorDerived<E, Cs...>::VecsIteratorDerived(bool is_end) noexcept : VecsIterator<Cs...>(std::nullopt) {
		m_sizeE = VecsComponentTable<E>().size(); ///< iterate over ALL entries, also the invalid ones!
		if (is_end) {
			this->m_current_index = static_cast<decltype(this->m_current_index)>(m_sizeE);
		}
	};

	/**
	* \brief Test whether the iterator points to a valid entity.
	* \returns true if the iterator points to a valid entity.
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorDerived<E, Cs...>::has_value() noexcept		-> bool {
		return VecsComponentTable<E>().handle(this->m_current_index).has_value();
	}

	/**
	* \brief 
	* \returns
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorDerived<E, Cs...>::handle() noexcept		-> VecsHandle {
		return VecsComponentTable<E>().handle(this->m_current_index);
	}

	/**
	* \brief
	* \returns
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorDerived<E, Cs...>::flag() noexcept		-> std::atomic_flag* {
		return VecsComponentTable<E>().flag(this->m_current_index);
	}

	/**
	* \brief Access operator retrieves all relevant components Cs from the entity it points to.
	* \returns all components Cs from the entity the iterator points to.
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorDerived<E, Cs...>::operator*() noexcept		-> typename VecsIterator<Cs...>::ref_type {
		return std::forward_as_tuple(VecsComponentTable<E>().handle(this->m_current_index), VecsComponentTable<E>().component<Cs>(this->m_current_index)...);
	};

	/**
	* \brief Point to the next entity.
	* \returns *this.
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorDerived<E, Cs...>::operator++() noexcept		-> VecsIterator<Cs...>& {
		if (!is_vector_end())++this->m_current_index;
		return *this;
	};

	/**
	* \brief Point to the next entity.
	* \returns *this.
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorDerived<E, Cs...>::operator++(int) noexcept	-> VecsIterator<Cs...>& {
		if (!is_vector_end()) ++this->m_current_index;
		return *this;
	};

	/**
	* \brief Determine whether the iterator points beyond the last entity of its type E.
	* This means it has reached its end.
	* \returns true if the iterator points beyond its last entity.
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorDerived<E, Cs...>::is_vector_end() noexcept	-> bool {
		return this->m_current_index >= m_sizeE;
	};

	/**
	* \brief Return the number of entities covered by this iterator.
	* \returns the number of entities this iterator covers.
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorDerived<E, Cs...>::size() noexcept			-> size_t {
		return m_sizeE;
	}


	//-------------------------------------------------------------------------
	//left over implementations that depend on definition of classes

	//-------------------------------------------------------------------------
	//VecsIterator

	/**
	* \brief Constructor of class VecsIterator. If its not an end iterator then it also
	* creates subiterators for all entity types that have all component types.
	* \param[in] is_end If true then this iterator is an end iterator.
	*/
	template<typename... Cs>
	inline VecsIterator<Cs...>::VecsIterator(bool is_end) noexcept : m_is_end{ is_end } {
		if (is_end) {
			m_current_iterator = static_cast<decltype(m_current_iterator)>(m_dispatch.size() - 1);
			m_current_index = static_cast<decltype(m_current_index)>(VecsRegistry<last_type>().size<last_type>());
			return;
		}

		vtll::static_for<size_t, 0, vtll::size<entity_types>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<entity_types, i>;
				m_dispatch[i] = std::make_unique<VecsIteratorDerived<type, Cs...>>(is_end);
				auto size = m_dispatch[i]->size();
				m_size += size;
				if (size == 0 && i + 1 < vtll::size<entity_types>::value) m_current_iterator++;
			}
		);
	};

	//-------------------------------------------------------------------------
	//VecsComponentTable

	/**
	* \brief Remove invalid entity data at the end of the component table.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::remove_deleted_tail() noexcept -> void {
		while (m_data.size() > 0) {
			auto & handle = m_data.comp_ref_idx<c_handle>(index_t{ m_data.size() - 1 });
			if (handle.has_value()) return; ///< is last entity is valid, then return
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
	*
	* \param[in]
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::compress() noexcept -> void {
		for (size_t i = 0; i < m_data.size(); ++i) {
			remove_deleted_tail();											///< Remove invalid entities at end of table
			auto index = m_deleted.comp_ref_idx<0>(index_t{i});			///< Get next deleted entity from deleted table
			if (index.value < m_data.size()) {								///< Is it inside the table still?
				auto tup = m_data.tuple_value(index_t{ m_data.size() - 1 });///< Yes, move last entity to this position
				m_data.update(index, tup);

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
				VecsLock(m_data.comp_ref_idx<c_flag>(index_t{ i }));
				handle = m_data.comp_ref_idx<c_handle>(index_t{ i });
			}
			if( VecsRegistry<E>().erase(handle) ) ++num;
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
		if (!this->init()) return;
		m_entity_table.max_capacity(r);

		vtll::static_for<size_t, 0, vtll::size<VecsEntityTypeList>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<VecsEntityTypeList, i>;
				m_dispatch[i] = std::make_unique<VecsRegistry<type>>(std::nullopt);
			}
		);
	}

	/**
	* \brief Retrieve the data for an entity from the ECS. The data is stored in an instance of VecsEntityProxy<E>.
	*
	* \param[in] handle The entity handle.
	* \returns a VecsEntityProxy<E>.
	*/
	template<typename E>
	inline auto VecsRegistryBaseClass::proxy( VecsHandle handle) noexcept -> VecsEntityProxy<E> {
		return VecsRegistry<E>().proxy(handle);
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
				num += VecsRegistry<vtll::Nth_type<VecsEntityTypeList,i>>().clearE();
			}
		);
		return num;
	}

	/**
	* \brief Erase all entities of type E from the ECS.
	*
	* The entities are not really removed, but rather invalidated. Call compress()
	* to actually remove the entities.
	* 
	* \returns the number of erased entities.
	*/
	template<typename E>
	requires is_entity_type<E>
	inline auto VecsRegistryBaseClass::clear() noexcept		-> size_t {
		return VecsRegistry<E>().clearE();
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
	* \brief Compress the component table for entities of type E.
	* This removes all invalidated entities.
	*/
	template<typename E>
	requires is_entity_type<E>
	inline auto VecsRegistryBaseClass::compress() noexcept	-> void {
		VecsRegistry<E>().compressE();
	}

	/**
	* \brief Update the components of an entity. The call is forwarded to the correct VecsRegistry<E>.
	*
	* \param[in] handle The entity handle.
	* \param[in] ent The entity data.
	* \returns true if the operation was successful.
	*/
	template<typename ET>
	inline auto VecsRegistryBaseClass::update( VecsHandle handle, ET&& ent) noexcept	-> bool {
		return VecsRegistry<vtll::front<ET>>().update({ handle }, std::forward<ET>(ent));
	}

	/**
	* \brief Return an iterator that points to the first entity in the ECS that contains all given components Cs.
	* \returns an iterator that points to the first entity in the ECS that contains all given components Cs.
	*/
	template<typename... Cs>
	inline auto VecsRegistryBaseClass::begin() noexcept			-> VecsIterator<Cs...> {
		return VecsIterator<Cs...>(false);
	}

	/**
	* \brief Returns an end iterator.
	* \returns an end iterator.
	*/
	template<typename... Cs>
	inline auto VecsRegistryBaseClass::end() noexcept			-> VecsIterator<Cs...> {
		return VecsIterator<Cs...>(true);
	}


	/**
* \brief takes two iterators and loops from begin to end, and for each entity calls the provided function.
*
* \param[in] b Begin iterator.
* \param[in] e End iterator.
* \param[in] f Functor to be called for every entity the iterator visits.
*/
	template<typename... Cs>
	requires are_component_types<Cs...>
	inline auto VecsRegistryBaseClass::for_each(VecsIterator<Cs...>& b, VecsIterator<Cs...>& e, std::function<Functor<Cs...>> f) -> void {
		for (; b != e; ++b) {
			typename VecsIterator<Cs...>::value_type tup;
			{
				VecsLock lock{ b.flag() };		///< Make a local copy
				if (!b.has_value()) continue;
				tup = *b;
			}
			std::apply(f, tup);					///< Run the function on the copy

			VecsLock lock{ b.flag() };			///< Copy back to ECS
			if (!b.has_value()) continue;
			*b = tup;
		}
	}

	/**
	* \brief Visits all entities in the ECS that have the given components CS...
	* \param[in] f Functor to be called for every visited entity.
	*/
	template<typename... Cs>
	requires are_component_types<Cs...>
	inline auto VecsRegistryBaseClass::for_each(std::function<Functor<Cs...>> f) -> void {
		auto b = begin<Cs...>();
		auto e = end<Cs...>();
		for_each(b, e, f);
	}



	//-------------------------------------------------------------------------
	//VecsHandle

	/**
	* \brief Check whether the data in a handle is non null
	* \returns true if the data in the handle is not null
	*/
	inline auto VecsHandle::is_valid() noexcept				-> bool {
		return m_entity_index.has_value() && m_generation_counter.has_value() 
			&& m_type_index.has_value() && m_type_index.value < vtll::size<VecsEntityTypeList>::value;
	}

	/**
	* \brief Check whether a handle belongs to an entity that is still in the ECS.
	* \returns true if the entity this handle belongs to is still in the ECS.
	*/
	inline auto VecsHandle::has_value() noexcept				-> bool {
		return is_valid() && VecsRegistryBaseClass().contains(*this);
	}

	/**
	* \brief Get an entity of type E, the handle belongs to.
	* \returns a VecsEntityProxy<E>.
	*/
	template<typename E>
	requires is_entity_type<E>
	inline auto VecsHandle::proxy() noexcept					-> VecsEntityProxy<E> {
		return VecsRegistry<E>().proxy(*this);
	}

	/**
	* \brief Get the component of type C from the entity this handle belongs to.
	* \returns std::optional containing the component, or an empty std::optional.
	*/
	template<typename C>
	requires is_component_type<C>
	inline auto VecsHandle::component() noexcept				-> std::optional<C> {
		return VecsRegistryBaseClass().component<C>(*this);
	}

	/**
	* \brief Update the data for the entity this handle belongs to.
	*
	* \param[in] ent Universal reference to the data that is to be copied over the old data.
	* \returns true if the operation was successful.
	*/
	template<typename ET>
	requires is_entity<ET>
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

	//-------------------------------------------------------------------------
	//VecsEntityProxy

	/**
	* \brief Check whether the entity that this local copy represents is still valid.
	*
	* \returns true if the entity that this local copy represents is still valid.
	*/
	template <typename E>
	auto VecsEntityProxy<E>::has_value() noexcept -> bool {
		VecsLock lock(m_handle);
		return VecsRegistry<E>().contains(m_handle); 
	}

	/**
	* \brief Update all components for the entity this local copy represents.
	*
	* \returns true if the operation was successful.
	*/
	template <typename E>
	auto VecsEntityProxy<E>::update() noexcept -> bool {
		return VecsRegistry<E>().update(m_handle, *this); 
	};

	/**
	* \brief Erase the entity this local copy represents.
	*
	* \returns true if the operation was successful.
	*/
	template <typename E>
	auto VecsEntityProxy<E>::erase() noexcept -> bool {
		return VecsRegistry<E>().erase(m_handle);
	};


	//-------------------------------------------------------------------------
	//locking

	/**
	* \brief Acquire a lock to an entity
	*/
	inline auto VecsLock::lock() noexcept -> void {
		if (m_flag == nullptr) return;
		while (m_flag->test_and_set(std::memory_order_acquire)) {
			while (m_flag->test(std::memory_order_relaxed));
		}
	}

	/**
	* \brief Constructor of class VecsLock. Acquires a lock to an entity.
	* \param[in] flag Pointer to an atomic flag used for locking an entity.
	*/
	inline VecsLock::VecsLock(std::atomic_flag* flag) noexcept : m_flag(flag) {
		lock();
	}

	/**
	* \brief Constructor of class VecsLock. Acquires a lock to an entity.
	* \param[in] handle The handle to an entity that should be locked.
	*/
	inline VecsLock::VecsLock(VecsHandle handle) noexcept {
		if (!handle.is_valid()) return;
		m_flag = &VecsRegistryBaseClass::m_entity_table.comp_ref_idx<VecsRegistryBaseClass::c_flag>(handle.m_entity_index);
		lock();
	}

	/**
	* \returns true if the lock has locked to a valid entity.
	*/
	inline auto VecsLock::is_valid() noexcept -> bool {
		return m_flag != nullptr;
	}

	/**
	* \brief Destructor of class VecsLock. Releases the lock.
	*/
	inline VecsLock::~VecsLock() noexcept {
		if (m_flag) m_flag->clear(std::memory_order_release);
	}


	//-------------------------------------------------------------------------
	//system


	/**
	* \brief Systems can access all components in sequence
	*/

	template<typename T, typename... Cs>
	class VecsSystem : public VecsMonostate<VecsSystem<T>> {
	protected:
	public:
		VecsSystem() = default;
	};


}

#endif
