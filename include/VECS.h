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

	/**
	* Declarations of the main VECS classes
	*/
	class VecsHandle;
	template <typename E> class VecsEntity;
	template<typename E> class VecsComponentTable;
	template<typename E, size_t I> class VecsComponentTableDerived;
	class VecsRegistryBaseClass;
	template<typename E> class VecsRegistry;

	//-------------------------------------------------------------------------
	//entity handle

	/**
	* \brief Handles are IDs of entities. Use them to access entitites.
	* 
	* VecsHandle are used to ID entities of type E by storing their type as an index.
	*/
	class VecsHandle {
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
		auto type() const noexcept -> uint32_t { return static_cast<uint32_t>(m_type_index.value); };

		auto has_value() noexcept -> bool;

		template<typename E>
		auto entity() noexcept -> std::optional<VecsEntity<E>>;

		template<typename C>
		requires vtll::has_type<VecsComponentTypeList, std::decay_t<C>>::value
		auto component() noexcept -> std::optional<C>;

		template<typename ET>
		auto update(ET&& ent) noexcept -> bool;

		template<typename C>
		requires vtll::has_type<VecsComponentTypeList, std::decay_t<C>>::value
		auto update(C&& comp) noexcept -> bool;

		auto erase() noexcept -> bool;
	};


	/**
	* \brief VecsEntity can hold a copy of the data of an entity of type E. This includes its handle
	* and all components.
	* 
	* VecsEntity is a local copy of an entity of type E and all its components. The components can be retrieved and
	* changes locally. By calling update() the local copies are stored back into the ECS. By calling erase() the entity is
	* erased from the ECS. 
	*/

	template <typename E>
	class VecsEntity {
	public:
		using tuple_type = vtll::to_tuple<E>;	///< A tuple holding all entity components.

	protected:
		VecsHandle	m_handle;			///< The entity handle.
		tuple_type	m_component_data;	///< The local copy of the entity components.

	public:

		/**
		* \brief Constructor of the VecsEntity class.
		* 
		* \param[in] h Handle of the entity.
		* \param[in] tup The copy of the entity data to be stored in the instance.
		*/
		VecsEntity(VecsHandle h, const tuple_type& tup) noexcept : m_handle{ h }, m_component_data{ tup } {};
		
		/** \returns the handle of the entity. */
		auto handle() const noexcept -> VecsHandle { return m_handle; }

		auto has_value() noexcept	 -> bool;
		auto update() noexcept		 -> bool;
		auto erase() noexcept		 -> bool;
		auto name() const noexcept	 -> std::string { return typeid(E).name(); };

		/** \returns the Ith component of the entity. */
		template<size_t I>
		auto component() noexcept -> std::optional<vtll::Nth_type<E,I>> {
			return { std::get<I>(m_component_data) };
		};

		/** \returns the first component of type C. */
		template<typename C>
		requires vtll::has_type<E, std::decay_t<C>>::value
		auto component() noexcept -> C {
			return std::get<vtll::index_of<E,std::decay_t<C>>::value>(m_component_data);
		};

		/** \brief Update the local copy with type C
		* \param[in] comp A universal reference to the new component value. */
		template<typename C>
		requires vtll::has_type<E, std::decay_t<C>>::value
		auto local_update( C&& comp ) noexcept -> void {
			std::get<vtll::index_of<E, std::decay_t<C>>::value>(m_component_data) = comp;
		};
	};


	//-------------------------------------------------------------------------
	//component vector - each entity type has them


	/**
	* \brief This class stores all components of entities of type E
	*/
	template<typename E>
	class VecsComponentTable : public VecsMonostate<VecsComponentTable<E>> {
		friend class VecsRegistryBaseClass;
		template<typename E> friend class VecsRegistry;

	public:
		using value_type = vtll::to_tuple<E>;		///< A tuple storing all components of entity of type E

		using info = vtll::type_list<VecsHandle>;	///< List of management data per entity (only a handle)
		static const size_t c_handle = 0;			///< Component index of the handle info
		static const size_t c_info_size = 1;		///< Index where the entity data starts

		using types = vtll::cat< info, E >;					///< List with management and component types
		using types_deleted = vtll::type_list< index_t >;	///< List with types for holding info about erased entities 

		/** Power of 2 exponent for the size of segments inthe tables */
		static const size_t c_segment_size	= vtll::front_value< vtll::map< VecsTableSizeMap, E, VeTableSizeDefault > >::value;
		
		/** Power of 2 exponent for the max number of entries in the tables */
		static const size_t c_max_size		= vtll::back_value<  vtll::map< VecsTableSizeMap, E, VeTableSizeDefault > >::value;

	protected:
		static inline VecsTable<types, c_segment_size>			m_data;		///< Data per entity
		static inline VecsTable<types_deleted, c_segment_size>	m_deleted;	///< Table holding the indices of erased entities

		/** Each component type C of the entity type E gets its own specialized class instance */
		static inline std::array<std::unique_ptr<VecsComponentTable<E>>, vtll::size<VecsComponentTypeList>::value> m_dispatch;

		/** 
		* \brief Dispatch an update call to the correct specialized class instance.
		* 
		* \param[in] entidx Entity index in the component table.
		* \param[in] compidx Index of the component of the entity.
		* \param[in] ptr Pointer to the component data to use.
		* \param[in] size Size of the component data.
		* \returns true if the update was successful
		*/
		virtual auto updateC(index_t entidx, size_t compidx, void* ptr, size_t size) noexcept -> bool {
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
		virtual auto componentE(index_t entidx, size_t compidx, void* ptr, size_t size)  noexcept -> bool {
			return m_dispatch[compidx]->componentE(entidx, compidx, ptr, size);
		}

		auto remove_deleted_tail() noexcept -> void;

	public:
		VecsComponentTable(size_t r = 1 << c_max_size) noexcept;

		template<typename... Cs>
		requires vtll::is_same<E, std::decay_t<Cs>...>::value [[nodiscard]]
		auto insert(VecsHandle handle, Cs&&... args) noexcept	-> index_t;

		auto values(const index_t index) noexcept				-> value_type;
		auto handle(const index_t index) noexcept				-> VecsHandle;

		/** \returns the number of entries currently in the table, can also be invalid ones */
		auto size() noexcept									-> size_t { return m_data.size(); };
		auto erase(const index_t idx) noexcept					-> bool;
		auto compress() noexcept								-> void;
		auto clear() noexcept									-> size_t ;

		template<typename C>
		requires vtll::has_type<E, C>::value
		auto component(const index_t index) noexcept			-> C&;

		template<typename ET>
		requires (std::is_same_v<VecsEntity<E>, std::decay_t<ET>>)
		auto update(const index_t index, ET&& ent) noexcept		-> bool;
	};

	/**
	* \brief Insert data for a new entity into the component table.
	* 
	* \param[in] handle The handle of the new entity.
	* \param[in] args The entity components to be stored
	* \returns the index of the new entry in the component table.
	*/
	template<typename E> 
	template<typename... Cs>
	requires vtll::is_same<E, std::decay_t<Cs>...>::value [[nodiscard]]
	inline auto VecsComponentTable<E>::insert(VecsHandle handle, Cs&&... args) noexcept -> index_t {
		auto idx = m_data.push_back();			///< Allocate space a the end of the table
		if (!idx.has_value()) return idx;		///< Check if the allocation was successfull
		m_data.update<c_handle>(idx, handle);	///< Update the handle data
		(m_data.update<c_info_size + vtll::index_of<E,std::decay_t<Cs>>::value> (idx, std::forward<Cs>(args)), ...); ///< Update the entity components
		return idx;								///< Return the index of the new data
	};

	/**
	* \param[in] index The index of the data in the component table.
	* \returns a tuple holding the components of an entity.
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::values(const index_t index) noexcept -> typename VecsComponentTable<E>::value_type {
		assert(index.value < m_data.size());
		auto tup = m_data.tuple_value(index);											///< Get the whole data from the data
		return vtll::sub_tuple< c_info_size, std::tuple_size_v<decltype(tup)> >(tup);	///< Return only entity components
	}

	/**
	* \param[in] index The index of the entity in the component table.
	* \returns the handle of an entity from the component table.
	*/
	template<typename E> 
	inline auto VecsComponentTable<E>::handle(const index_t index) noexcept -> VecsHandle {
		assert(index.value < m_data.size());
		return m_data.comp_ref_idx<c_handle>(index);	///< Get ref to the handle and return it
	}

	/**
	* \param[in] index Index of the entity in the component table.
	* \returns the component of type C for an entity.
	*/
	template<typename E>
	template<typename C>
	requires vtll::has_type<E, C>::value
	inline auto VecsComponentTable<E>::component(const index_t index) noexcept -> C& {
		assert(index.value < m_data.size());
		return m_data.comp_ref_idx<c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index); ///< Get ref to the entity and return component
	}

	/**
	* \brief Update all components using a reference to a VecsEntity instance.
	* \param[in] index Index of the entity to be updated in the component table.
	* \param[in] ent Universal reference to the new entity data.
	* \returns true if the update was successful.
	*/
	template<typename E>
	template<typename ET>
	requires (std::is_same_v<VecsEntity<E>, std::decay_t<ET>>)
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
		assert(index.value < m_data.size());
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

		/** \brief Constructor of class VecsComponentTableDerived 
		*	\param[in] r Max number of entries allowed in the component table */
		VecsComponentTableDerived( size_t r = 1 << VecsComponentTable<E>::c_max_size) noexcept : VecsComponentTable<E>(r) {};

		/** \brief Update the component C of entity E
		* \param[in] index Index of the entity in the component table.
		* \param[in] comp Universal reference to the new component data.
		* \returns true if the update was successful. */
		auto update(const index_t index, C&& comp) noexcept -> bool {
			if constexpr (vtll::has_type<E, std::decay_t<C>>::value) {
				this->m_data.comp_ref_idx<this->c_info_size + vtll::index_of<E, std::decay_t<C>>::value>() = comp;
				return true;
			}
			return false;
		}

		/** \brief Update the component C of entity E.
		* \param[in] index Index of the entity in the component table.
		* \param[in] compidx Component index incomponent type list.
		* \param[in] ptr Pointer to where the data comes from.
		* \param[in] size Size of the component data.
		* \returns true if the update was successful. */
		auto updateC(index_t index, size_t compidx, void* ptr, size_t size) noexcept -> bool {
			if constexpr (vtll::has_type<E, std::decay_t<C>>::value) {
				this->m_data.comp_ref_idx<this->c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index) = *((C*)ptr);
				return true;
			}
			return false;
		};

		/** \brief Get component data.
		* \param[in] index Index of the entity in the component table.
		* \param[in] compidx Component index incomponent type list.
		* \param[in] ptr Pointer to where the data should be copied to.
		* \param[in] size Size of the component data.
		* \returns true if the retrieval was successful. */
		auto componentE(index_t entidx, size_t compidx, void* ptr, size_t size)  noexcept -> bool {
			if constexpr (vtll::has_type<E,C>::value) {
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
		m_data.max_capacity(r);			//set max capacities
		m_deleted.max_capacity(r);

		vtll::static_for<size_t, 0, vtll::size<VecsComponentTypeList>::value >(
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
	protected:

		/** \brief Entity info stored in the main table. All info can be accessed with at most one cache miss. */
		struct map_t{
			index_t				m_index{};				///< Index in component table, or next free entry
			counter16_t			m_generation_counter{};	///< Generation counter for determining if handle still valid
			index16_t			m_type_index{};			///< Index of entity type 
			std::atomic_flag	m_flag;					///< Synchronize read and write access to entity
		};

		using types = vtll::type_list<map_t>;	///< Type for the table
		static const uint32_t c_map_data{ 0 };	///< Index for accessing the map data

		static inline VecsTable<types, VecsTableMaxSegExp::value>	m_entity_table;	///< The main mapping table
		static inline index_t										m_first_free{};	///< First free entry to be reused
		static inline std::atomic<uint32_t>							m_size{0};		///< Number of valid entities in the map

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
		auto insert(Cs&&... args) noexcept	-> VecsHandle;

		//-------------------------------------------------------------------------
		//get data

		template<typename E>
		auto entity(VecsHandle handle) noexcept -> std::optional<VecsEntity<E>>;

		template<typename C>
		requires vtll::has_type<VecsComponentTypeList, std::decay_t<C>>::value
		auto component(VecsHandle handle) noexcept -> std::optional<C>;

		//-------------------------------------------------------------------------
		//update data

		template<typename ET>
		auto update(VecsHandle handle, ET&& ent) noexcept -> bool;

		template<typename C>
		requires vtll::has_type<VecsComponentTypeList, std::decay_t<C>>::value
		auto update(VecsHandle handle, C&& comp) noexcept -> bool;

		//-------------------------------------------------------------------------
		//erase

		auto clear() noexcept -> size_t;

		template<typename E = void>
		auto clear() noexcept -> size_t;

		virtual
		auto erase(VecsHandle handle) noexcept -> bool;

		auto compress() noexcept -> void;

		template<typename E = void>
		auto compress() noexcept -> void;

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
		auto begin() noexcept		-> VecsIterator<Cs...>;

		template<typename... Cs>
		auto end() noexcept			-> VecsIterator<Cs...>;

		virtual
		auto contains(VecsHandle handle) noexcept	-> bool;
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
	requires vtll::has_type<VecsComponentTypeList, std::decay_t<C>>::value
	auto VecsRegistryBaseClass::component(VecsHandle handle) noexcept -> std::optional<C> {
		if (!handle.m_type_index.has_value() || handle.m_type_index.value >= vtll::size<VecsEntityTypeList>::value) return {};
		C res{};

		///< Dispatch to the correct subclass
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
	requires vtll::has_type<VecsComponentTypeList, std::decay_t<C>>::value
	auto VecsRegistryBaseClass::update(VecsHandle handle, C&& comp) noexcept -> bool {
		if (!handle.m_type_index.has_value() || handle.m_type_index.value >= vtll::size<VecsEntityTypeList>::value) return false;

		/// Dispatch the call to the correct subclass and return result
		return m_dispatch[handle.type()]->updateC(handle, vtll::index_of<VecsComponentTypeList, std::decay_t<C>>::value, (void*)&comp, sizeof(C));
	}

	/**
	* \brief Erase an entity from the ECS. The call is dispatched to the correct subclass for entity type E.
	* \param[in] handle The entity handle.
	* \returns true if the operation was successful.
	*/
	auto VecsRegistryBaseClass::erase(VecsHandle handle) noexcept -> bool {
		if (!handle.m_type_index.has_value() || handle.m_type_index.value >= vtll::size<VecsEntityTypeList>::value) return false;
		return m_dispatch[handle.type()]->erase(handle); ///< Dispatch to the correct subclass for type E
	}

	/**
	* \brief Entry point for checking whether a handle is still valid.
	* The call is dispatched to the correct subclass for entity type E.
	*
	* \param[in] handle The handle to check.
	* \returns true if the ECS contains this entity.
	*/
	auto VecsRegistryBaseClass::contains(VecsHandle handle) noexcept	-> bool {
		if (!handle.m_type_index.has_value() || handle.m_type_index.value >= vtll::size<VecsEntityTypeList>::value) return false;
		return m_dispatch[handle.type()]->contains(handle);
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

		auto clearE() noexcept		-> size_t { 		///< Forward to component table of type E
			m_sizeE = 0;
			return VecsComponentTable<E>().clear(); 
		};

	public:
		/** Constructors for class VecsRegistry<E>. */
		VecsRegistry(size_t r = 1 << c_max_size) noexcept : VecsRegistryBaseClass() { VecsComponentTable<E>{r}; };
		VecsRegistry(std::nullopt_t u) noexcept : VecsRegistryBaseClass() {};

		//-------------------------------------------------------------------------
		//insert data

		template<typename... Cs> 
		requires vtll::is_same<E, std::decay_t<Cs>...>::value [[nodiscard]] 
		auto insert(Cs&&... args) noexcept					-> VecsHandle;

		//-------------------------------------------------------------------------
		//get data

		auto entity(VecsHandle h) noexcept					-> std::optional<VecsEntity<E>>;

		template<typename C>
		requires (vtll::has_type<E, std::decay_t<C>>::value)
		auto component(VecsHandle handle) noexcept			->std::optional<C> ;

		//-------------------------------------------------------------------------
		//update data

		template<typename ET>
		requires (std::is_same_v<E, vtll::front<std::decay_t<ET>>>)
		auto update(VecsHandle handle, ET&& ent) noexcept	-> bool;

		template<typename C> 
		requires (vtll::has_type<E, std::decay_t<C>>::value)
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
		if (!contains(handle)) return {};
		return VecsComponentTable<E>().updateC(m_entity_table.comp_ref_idx<c_map_data>(handle.m_entity_index).m_index, compidx, ptr, size);
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
		return VecsComponentTable<E>().componentE(m_entity_table.comp_ref_idx<c_map_data>(handle.m_entity_index).m_index, compidx, ptr, size);
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
	requires vtll::is_same<E, std::decay_t<Cs>...>::value [[nodiscard]]
	inline auto VecsRegistry<E>::insert(Cs&&... args) noexcept	-> VecsHandle {
		index_t idx{};
		if (m_first_free.has_value()) {
			idx = m_first_free;
			m_first_free = m_entity_table.comp_ref_idx<c_map_data>(m_first_free).m_index;
		}
		else {
			idx = m_entity_table.push_back();	
			if (!idx.has_value()) return {};
			m_entity_table.comp_ref_idx<c_map_data>(idx).m_generation_counter = counter16_t{ 0 };		//start with counter 0
		}

		m_entity_table.comp_ref_idx<c_map_data>(idx).m_type_index = index16_t{ vtll::index_of<VecsEntityTypeList, E>::value };

		VecsHandle handle{ idx, m_entity_table.comp_ref_idx<c_map_data>(idx).m_generation_counter, index16_t{ vtll::index_of<VecsEntityTypeList, E>::value } };
		index_t compidx = VecsComponentTable<E>().insert(handle, args...);	//add data as tuple
		m_entity_table.comp_ref_idx<c_map_data>(idx).m_index = compidx;						//index in component vector 
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
		if (!handle.m_entity_index.has_value() || !handle.m_generation_counter.has_value() || !handle.m_type_index.has_value()) return false;
		if (handle.m_type_index.value != vtll::index_of<VecsEntityTypeList, E>::value ) return false;
		if (handle.m_entity_index.value >= m_entity_table.size()) return false;
		if (handle.m_generation_counter != m_entity_table.comp_ref_idx<c_map_data>(handle.m_entity_index).m_generation_counter) return false;
		if (handle.m_type_index != m_entity_table.comp_ref_idx<c_map_data>(handle.m_entity_index).m_type_index) return false;
		return true;
	}

	/**
	* \brief Retrieve all values for a given entity.
	*
	* \param[in] handle The entity handle.
	* \returns a std::optional that either contains the VecsEntity<E>, or is empty.
	*/
	template<typename E>
	inline auto VecsRegistry<E>::entity(VecsHandle handle) noexcept -> std::optional<VecsEntity<E>> {
		if (!contains(handle)) return {};
		VecsEntity<E> res(handle, VecsComponentTable<E>().values(m_entity_table.comp_ref_idx<c_map_data>(handle.m_entity_index).m_index));
		return { res };
	}

	/**
	* \brief Retrieve a component of type C from an entity of type E.
	*
	* \param[in] handle The entity handle.
	* \returns std::optional containing the component, or is empty.
	*/
	template<typename E>
	template<typename C>
	requires (vtll::has_type<E, std::decay_t<C>>::value)
	inline auto VecsRegistry<E>::component( VecsHandle handle) noexcept -> std::optional<C> {
		if constexpr (!vtll::has_type<E, std::decay_t<C>>::value) return {};
		if (!contains(handle)) return {};	///< Return the empty std::optional
		auto& comp_table_idx = m_entity_table.comp_ref_idx<c_map_data>(handle.m_entity_index).m_index; ///< Get reference to component
		return { VecsComponentTable<E>().component<C>(comp_table_idx) };	///< Return the std::optional
	}

	/**
	* \brief Update all components of an entity of type E from a VecsEntity<E>
	*
	* \param[in] handle The Enity handle. 
	* \param[in] ent A universal reference to the VecsEntity<E> that contains the data.
	* \returns true if the operation was successful.
	*/
	template<typename E>
	template<typename ET>
	requires (std::is_same_v<E, vtll::front<std::decay_t<ET>>>)
	inline auto VecsRegistry<E>::update( VecsHandle handle, ET&& ent) noexcept -> bool {
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
	requires (vtll::has_type<E, std::decay_t<C>>::value)
	inline auto VecsRegistry<E>::update( VecsHandle handle, C&& comp) noexcept -> bool {
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
		if (!contains(handle)) return false;
		m_size--;	///< Decrease sizes
		m_sizeE--;

		VecsComponentTable<E>().erase(m_entity_table.comp_ref_idx <c_map_data>(handle.m_entity_index).m_index); ///< Erase from comp table

		m_entity_table.comp_ref_idx<c_map_data>(handle.m_entity_index).m_generation_counter++;		///< Invalidate the entity handle
		m_entity_table.comp_ref_idx<c_map_data>(handle.m_entity_index).m_index = m_first_free;		///< Put old entry into free list
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

		VecsIterator() noexcept {};			///< Needed for derived iterator to call

	public:
		using value_type = std::tuple<VecsHandle, Cs&...>; ///< Tuple containing all component values

		VecsIterator( bool is_end ) noexcept ;				///< Constructor that should be called always from outside
		VecsIterator(const VecsIterator& v) noexcept;		///< Copy constructor

		auto operator=(const VecsIterator& v) noexcept			-> VecsIterator<Cs...>&;	///< Copy
		auto operator+=(size_t N) noexcept						-> VecsIterator<Cs...>&;	///< Increase and set
		auto operator+(size_t N) noexcept						-> VecsIterator<Cs...>&;	///< Increase
		auto operator!=(const VecsIterator<Cs...>& v) noexcept	-> bool;	///< Unqequal
		auto operator==(const VecsIterator<Cs...>& v) noexcept	-> bool;	///< Equal

		virtual auto has_value() noexcept		-> bool;					///< Is currently pointint to a valid entity
		virtual	auto operator*() noexcept		-> value_type;				///< Access the data
		virtual auto operator++() noexcept		-> VecsIterator<Cs...>&;	///< Increase by 1
		virtual auto operator++(int) noexcept	-> VecsIterator<Cs...>&;	///< Increse by 1
		virtual auto is_vector_end() noexcept	-> bool;					///< Is currently at the end of any sub iterator
		virtual auto size() noexcept			-> size_t;					///< Number of valid entities
	};


	/**
	* \brief
	*
	* \param[in]
	*/
	template<typename... Cs>
	inline VecsIterator<Cs...>::VecsIterator(const VecsIterator& v) noexcept : VecsIterator(v.m_is_end) {
		m_current_iterator = v.m_current_iterator;
		m_current_index = v.m_current_index;
		if (m_is_end) return;
		for (int i = 0; i < m_dispatch.size(); ++i) { m_dispatch[i]->m_current_index = v.m_dispatch[i]->m_current_index; }
	};

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::has_value() noexcept		-> bool {
		if (m_is_end || is_vector_end()) return false;
		return m_dispatch[m_current_iterator.value]->has_value();
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator=(const VecsIterator& v) noexcept	-> VecsIterator<Cs...>& {
		m_current_iterator = v.m_current_iterator;
		m_current_index = v.m_current_index;
		for (int i = 0; i < m_dispatch.size(); ++i) { m_dispatch[i]->m_current_index = v.m_dispatch[i]->m_current_index; }
		return *this;
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator*() noexcept	-> value_type {
		return *(*m_dispatch[m_current_iterator.value]);
	};

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator++() noexcept		-> VecsIterator<Cs...>& {
		if (m_is_end) return *this;
		(*m_dispatch[m_current_iterator.value])++;

		if (m_dispatch[m_current_iterator.value]->is_vector_end() && m_current_iterator.value < m_dispatch.size() - 1) {
			++m_current_iterator.value;
			m_current_index.value = 0;
		}
		m_current_index = m_dispatch[m_current_iterator.value]->m_current_index;
		return *this;
	};

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator++(int) noexcept		-> VecsIterator<Cs...>& {
		return operator++();
	};

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator+=(size_t N) noexcept -> VecsIterator<Cs...>& {
		if (m_is_end) return;
		size_t left = N;
		while (left > 0) {
			int num = std::max(m_dispatch[m_current_iterator.value]->size() - m_current_index.value, 0);
			left -= num;
			m_dispatch[m_current_iterator.value]->m_current_index.value += num;
			m_current_index = m_dispatch[m_current_iterator.value]->m_current_index;

			if (m_dispatch[m_current_iterator.value]->is_vector_end()) {
				if (m_current_iterator.value < m_dispatch.size() - 1) {
					++m_current_iterator.value;
					m_current_index.value = 0;
				}
				else return *this;
			}
		}
		return *this;
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator+(size_t N) noexcept	-> VecsIterator<Cs...>& {
		VecsIterator<Cs...> temp{ *this };
		temp += N;
		return temp;
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator!=(const VecsIterator<Cs...>& v) noexcept	-> bool {
		return !(*this == v);
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::operator==(const VecsIterator<Cs...>& v) noexcept	-> bool {
		return	v.m_current_iterator == m_current_iterator && v.m_current_index == m_current_index;
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::is_vector_end() noexcept		-> bool {
		return m_dispatch[m_current_iterator.value]->is_vector_end();
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsIterator<Cs...>::size() noexcept	-> size_t {
		size_t sum = 0;
		for (int i = 0; i < m_dispatch.size(); ++i) { sum += m_dispatch[i]->size(); };
		return sum;
	}






	//----------------------------------------------------------------------------------------------

	/**
	* \brief Iterator that iterates over a VecsComponentTable of type E
	* and that is intested into components Cs
	*/

	template<typename E, typename... Cs>
	class VecsIteratorDerived : public VecsIterator<Cs...> {
	protected:
		size_t m_size{0};

	public:

		/**
		* \brief
		*
		* \param[in]
		*/
		VecsIteratorDerived(bool is_end = false) noexcept { //empty parent default constructor does not create new children
			m_size = VecsComponentTable<E>().size();
			if (is_end) {
				this->m_current_index.value = static_cast<decltype(this->m_current_index.value)>(m_size);
			}
		};

		/**
		* \brief
		*
		* \param[in]
		* \returns
		*/
		auto has_value() noexcept		-> bool {
			return VecsComponentTable<E>().handle(this->m_current_index).has_value();
		}

		/**
		* \brief
		*
		* \param[in]
		* \returns
		*/
		auto operator*() noexcept		-> typename VecsIterator<Cs...>::value_type {
			return std::make_tuple(VecsComponentTable<E>().handle(this->m_current_index), std::ref(VecsComponentTable<E>().component<Cs>(this->m_current_index))...);
		};

		/**
		* \brief
		*
		* \param[in]
		* \returns
		*/
		auto operator++() noexcept		-> VecsIterator<Cs...>& {
			if (!is_vector_end()) ++this->m_current_index; 
			return *this; 
		};
		
		/**
		* \brief
		*
		* \param[in]
		* \returns
		*/
		auto operator++(int) noexcept	-> VecsIterator<Cs...>& {
			if (!is_vector_end()) ++this->m_current_index;
			return *this; 
		};
		
		/**
		* \brief
		*
		* \param[in]
		* \returns
		*/
		auto is_vector_end() noexcept	-> bool {
			return this->m_current_index.value >= m_size; 
		};

		/**
		* \brief
		*
		* \param[in]
		* \returns
		*/
		auto size() noexcept			-> size_t {
			return m_size; 
		}

	};


	/**
	* \brief
	*
	* \param[in]
	*/
	template<typename... Cs>
	inline VecsIterator<Cs...>::VecsIterator(bool is_end) noexcept : m_is_end{ is_end } {
		if (is_end) {
			m_current_iterator.value = static_cast<decltype(m_current_iterator.value)>(m_dispatch.size() - 1);
			m_current_index.value = static_cast<decltype(m_current_index.value)>(VecsRegistry<last_type>().size<last_type>());
			return;
		}

		vtll::static_for<size_t, 0, vtll::size<entity_types>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<entity_types, i>;
				m_dispatch[i] = std::make_unique<VecsIteratorDerived<type, Cs...>>(is_end);
			}
		);
	};


	template<typename... Cs>
	using Functor = void(VecsIterator<Cs...>&);

	/**
	* \brief
	*
	* \param[in]
	*/
	template<typename... Cs>
	inline auto for_each(VecsIterator<Cs...>& b, VecsIterator<Cs...>& e, std::function<Functor<Cs...>> f) -> void {
		for (; b != e; b++) {
			if(b.has_value()) f(b);
		}
	}

	/**
	* \brief
	*
	* \param[in]
	*/
	template<typename... Cs>
	inline auto for_each(std::function<Functor<Cs...>> f) -> void {
		auto b = VecsRegistry().begin<Cs...>();
		auto e = VecsRegistry().end<Cs...>();
		for_each(b, e, f);
	}


	//-------------------------------------------------------------------------
	//left over implementations that depend on definition of classes

	//-------------------------------------------------------------------------
	//VecsComponentTable

	/**
	* \brief
	*
	* \param[in]
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::remove_deleted_tail() noexcept -> void {
		while (m_data.size() > 0) {
			auto & handle = m_data.comp_ref_idx<c_handle>(index_t{ m_data.size() - 1 });
			if (handle.has_value()) return;
			m_data.pop_back();
		}
	}

	/**
	* \brief
	*
	* \param[in]
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::compress() noexcept -> void {
		for (size_t i = 0; i < m_data.size(); ++i) {
			remove_deleted_tail();
			auto& index = m_deleted.comp_ref_idx<0>(index_t{i});
			if (index.value < m_data.size()) {
				auto tup = m_data.tuple_value(index_t{ m_data.size() - 1 });
				m_data.update(index, tup);
			}
		}
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename E>
	inline auto VecsComponentTable<E>::clear() noexcept -> size_t {
		size_t num = 0;
		for (size_t i = 0; i < m_data.size(); ++i) {
			auto& handle = m_data.comp_ref_idx<c_handle>(index_t{ i });
			if( VecsRegistry<E>().erase(handle) ) ++num;
		}
		return num;
	}

	//-------------------------------------------------------------------------
	//VecsRegistryBaseClass

	/**
	* \brief
	*
	* \param[in]
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
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename E>
	inline auto VecsRegistryBaseClass::entity( VecsHandle handle) noexcept -> std::optional<VecsEntity<E>> {
		return VecsRegistry<E>().entity(handle);
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	inline auto VecsRegistryBaseClass::clear() noexcept			-> size_t {
		size_t num = 0;
		vtll::static_for<size_t, 0, vtll::size<VecsEntityTypeList>::value >(
			[&](auto i) {
				num += VecsRegistry<vtll::Nth_type<VecsEntityTypeList,i>>().clearE();
			}
		);
		return num;
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename E>
	inline auto VecsRegistryBaseClass::clear() noexcept			-> size_t {
		return VecsRegistry<E>().clearE();
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	inline auto VecsRegistryBaseClass::compress() noexcept		-> void {
		vtll::static_for<size_t, 0, vtll::size<VecsEntityTypeList>::value >(
			[&](auto i) {
				VecsRegistry<vtll::Nth_type<VecsEntityTypeList, i>>().compressE();
			}
		);
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename E>
	inline auto VecsRegistryBaseClass::compress() noexcept		-> void {
		VecsRegistry<E>().compressE();
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns true if the operation was successful.
	*/
	template<typename ET>
	inline auto VecsRegistryBaseClass::update( VecsHandle handle, ET&& ent) noexcept	-> bool {
		return VecsRegistry<vtll::front<ET>>().update({ handle }, std::forward<ET>(ent));
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsRegistryBaseClass::begin() noexcept			-> VecsIterator<Cs...> {
		return VecsIterator<Cs...>(false);
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename... Cs>
	inline auto VecsRegistryBaseClass::end() noexcept			-> VecsIterator<Cs...> {
		return VecsIterator<Cs...>(true);
	}


	//-------------------------------------------------------------------------
	//VecsHandle

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	inline auto VecsHandle::has_value() noexcept				-> bool {
		return VecsRegistryBaseClass().contains(*this);
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename E>
	inline auto VecsHandle::entity() noexcept					-> std::optional<VecsEntity<E>> {
		return VecsRegistry<E>().entity(*this);
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template<typename C>
	requires vtll::has_type<VecsComponentTypeList, std::decay_t<C>>::value
	inline auto VecsHandle::component() noexcept				-> std::optional<C> {
		return VecsRegistryBaseClass().component<C>(*this);
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns true if the operation was successful.
	*/
	template<typename ET>
	inline auto VecsHandle::update(ET&& ent) noexcept			-> bool {
		using type = vtll::front<std::decay_t<ET>>;
		return VecsRegistry<type>().update(*this, std::forward<ET>(ent));
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns true if the operation was successful.
	*/
	template<typename C>
	requires vtll::has_type<VecsComponentTypeList, std::decay_t<C>>::value
	inline auto VecsHandle::update(C&& comp) noexcept			-> bool {
		return VecsRegistryBaseClass().update<C>(*this, std::forward<C>(comp));
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns true if the operation was successful.
	*/
	inline auto VecsHandle::erase() noexcept					-> bool {
		return VecsRegistryBaseClass().erase(*this);
	}

	//-------------------------------------------------------------------------
	//VecsEntity

	/**
	* \brief
	*
	* \param[in]
	* \returns
	*/
	template <typename E>
	auto VecsEntity<E>::has_value() noexcept -> bool { 
		return VecsRegistry<E>().contains(m_handle); 
	}

	/**
	* \brief
	*
	* \param[in]
	* \returns true if the operation was successful.
	*/
	template <typename E>
	auto VecsEntity<E>::update() noexcept -> bool {
		return VecsRegistry<E>().update(m_handle, *this); 
	};

	/**
	* \brief
	*
	* \param[in]
	* \returns true if the operation was successful.
	*/
	template <typename E>
	auto VecsEntity<E>::erase() noexcept -> bool {
		return VecsRegistry<E>().erase(m_handle);
	};


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
