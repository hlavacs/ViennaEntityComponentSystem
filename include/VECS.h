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
#include "VECSComponent.h"

//user defined component types and entity types
#include "VECSUser.h" 

using namespace std::chrono_literals;

namespace vecs {

	//-------------------------------------------------------------------------
	//component type list and pointer

	using VecsComponentTypeList = vtll::cat< VeComponentTypeListSystem, VeComponentTypeListUser >;
	using VecsComponentPtr = vtll::to_variant<vtll::to_ptr<VecsComponentTypeList>>;


	//-------------------------------------------------------------------------
	//entity type list

	using VecsEntityTypeList = vtll::cat< VeEntityTypeListSystem, VeEntityTypeListUser >;

	//-------------------------------------------------------------------------
	//definition of the types used in VECS

	class VecsHandle;

	template <typename E>
	class VecsEntity;

	class VecsRegistryBaseClass;

	template<typename E>
	class VecsRegistry;

	//-------------------------------------------------------------------------
	//entity handle

	/**
	* \brief Handles are IDs of entities. Use them to access entitites.
	* VecsHandle are used to ID entities of type E by storing their type as an index
	*/

	class VecsHandle {
		template<typename E>
		friend class VecsRegistry;

	protected:
		index_t		m_entity_index{};			//the slot of the entity in the entity list
		counter16_t	m_generation_counter{};		//generation counter
		index16_t	m_type_index{};				//type index

	public:
		VecsHandle() {};
		VecsHandle(index_t idx, counter16_t cnt, index16_t type) : m_entity_index{ idx }, m_generation_counter{ cnt }, m_type_index{ type } {};

		uint32_t index() const { return static_cast<uint32_t>(m_type_index.value); };

		bool is_valid();

		template<typename E>
		std::optional<VecsEntity<E>> entity();

		template<typename C>
		std::optional<C> component();

		template<typename ET>
		bool update(ET&& ent);

		template<typename C>
		requires vtll::has_type<VecsComponentTypeList, C>::value
		bool update(C&& comp);

		bool erase();
	};


	using handle_t = VecsHandle;


	/**
	* \brief This struct can hold the data of an entity of type E. This includes its handle
	* and all components.
	* 
	* VecsEntity is a local copy of an entity of type E and all its components. The components can be retrieved and
	* changes locally. By calling update() the local copies are stored in the ECS. By calling erase() the entity is
	* erased from the ECS. 
	*/

	template <typename E>
	class VecsEntity {
	public:
		using tuple_type = vtll::to_tuple<E>;

	protected:
		VecsHandle	m_handle;
		tuple_type	m_component_data;

	public:
		VecsEntity(const VecsHandle& h, const tuple_type& tup) noexcept : m_handle{ h }, m_component_data{ tup } {};

		VecsHandle handle() {
			return m_handle;
		}

		bool is_valid() {
			return m_handle.is_valid();
		}

		template<typename C>
		std::optional<C> component() noexcept {
			if constexpr (vtll::has_type<E,C>::value) {
				return { std::get<vtll::index_of<E,C>::value>(m_component_data) };
			}
			return {};
		};

		template<typename C>
		requires vtll::has_type<VecsComponentTypeList, C>::value
		void local_update(C&& comp ) noexcept {
			if constexpr (vtll::has_type<E,C>::value) {
				std::get<vtll::index_of<E,C>::value>(m_component_data) = comp;
			}
			return;
		};

		bool update() {
			return m_handle.update(*this);
		};

		bool erase() {
			return m_handle.erase(*this);
		};

		std::string name() {
			return typeid(E).name();
		};
	};


	//-------------------------------------------------------------------------
	//component vector - each entity type has them


	/**
	* \brief This class stores all components of entities of type E
	*/

	template<typename E>
	class VecsComponentVector : public VecsMonostate<VecsComponentVector<E>> {
		friend class VecsRegistryBaseClass;

		template<typename E>
		friend class VecsRegistry;

	public:
		using tuple_type	 = vtll::to_tuple<E>;
		using tuple_type_ref = vtll::to_ref_tuple<E>;
		using tuple_type_vec = vtll::to_tuple<vtll::transform<E,std::pmr::vector>>;

	protected:
		struct entry_t {
			VecsHandle m_handle;
		};

		static inline std::vector<entry_t>	m_handles;
		static inline tuple_type_vec		m_components;

		static inline std::array<std::unique_ptr<VecsComponentVector<E>>, vtll::size<VecsComponentTypeList>::value> m_dispatch; //one for each component type

		virtual bool updateC(index_t entidx, size_t compidx, void* ptr, size_t size) {
			return m_dispatch[compidx]->updateC(entidx, compidx, ptr, size);
		}

		virtual bool componentE(index_t entidx, size_t compidx, void* ptr, size_t size) { 
			return m_dispatch[compidx]->componentE(entidx, compidx, ptr, size);
		}

	public:
		VecsComponentVector(size_t r = 1 << 10);

		[[nodiscard]] index_t	insert(VecsHandle& handle, tuple_type&& tuple);
		tuple_type				values(const index_t index);
		tuple_type_ref			references(const index_t index);
		VecsHandle				handle(const index_t index);

		template<typename C>
		C&		component(const index_t index);

		template<typename ET>
		bool	update(const index_t index, ET&& ent);

		size_t	size() { return m_handles.size(); };

		std::tuple<VecsHandle, index_t> erase(const index_t idx);
	};

	template<typename E>
	inline [[nodiscard]] index_t VecsComponentVector<E>::insert(VecsHandle& handle, tuple_type&& tuple) {
		m_handles.emplace_back(handle);

		vtll::static_for<size_t, 0, vtll::size<E>::value >(
			[&](auto i) {
				std::get<i>(m_components).push_back(std::get<i>(tuple));
			}
		);

		return index_t{ static_cast<typename index_t::type_name>(m_handles.size() - 1) };
	};

	template<typename E>
	inline typename VecsComponentVector<E>::tuple_type VecsComponentVector<E>::values(const index_t index) {
		assert(index.value < m_handles.size());

		auto f = [&]<typename... Cs>(std::tuple<std::pmr::vector<Cs>...>& tup) {
			return std::make_tuple(std::get<vtll::index_of<E, Cs>::value>(tup)[index.value]...);
		};

		return f(m_components);
	}

	template<typename E>
	inline typename VecsComponentVector<E>::tuple_type_ref VecsComponentVector<E>::references(const index_t index) {
		assert(index.value < m_handles.size());

		auto f = [&]<typename... Cs>(std::tuple<std::pmr::vector<Cs>...>& tup) {
			return std::tie( std::get<vtll::index_of<E,Cs>::value>(tup)[index.value]... );
		};

		return f(m_components);
	}

	template<typename E>
	inline VecsHandle VecsComponentVector<E>::handle(const index_t index) {
		assert(index.value < m_handles.size());
		return { m_handles[index.value].m_handle };
	}

	template<typename E>
	template<typename C>
	inline C& VecsComponentVector<E>::component(const index_t index) {
		assert(index.value < m_handles.size());
		return std::get<vtll::index_of<E, C>::value>(m_components)[index.value];
	}

	template<typename E>
	template<typename ET>
	inline bool VecsComponentVector<E>::update(const index_t index, ET&& ent) {
		vtll::static_for<size_t, 0, vtll::size<E>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<E, i>;
				std::get<i>(m_components)[index.value] = ent.component<type>().value();
			}
		);
		return true;
	}

	template<typename E>
	inline std::tuple<VecsHandle, index_t> VecsComponentVector<E>::erase(const index_t index) {
		assert(index.value < m_handles.size());
		if (index.value < m_handles.size() - 1) {
			std::swap(m_handles[index.value], m_handles[m_handles.size() - 1]);
			m_handles.pop_back();
			return std::make_pair(m_handles[index.value].m_handle, index);
		}
		m_handles.pop_back();
		return std::make_tuple(VecsHandle{}, index_t{});
	}


	//-------------------------------------------------------------------------
	//comnponent vector derived class

	/**
	* \brief This class is derived from the component vector and is used to update or
	* return components C of entities of type E
	*/

	template<typename E, typename C>
	class VecsComponentVectorDerived : public VecsComponentVector<E> {
	public:
		VecsComponentVectorDerived( size_t res = 1 << 10 ) : VecsComponentVector<E>(res) {};

		bool update(const index_t index, C&& comp) {
			if constexpr (vtll::has_type<E, C>::value) {
				std::get< vtll::index_of<E, C>::type::value >(this->m_components)[index.value] = comp;
				return true;
			}
			return false;
		}

		bool updateC(index_t entidx, size_t compidx, void* ptr, size_t size) {
			if constexpr (vtll::has_type<E, C>::value) {
				auto tuple = this->references(entidx);
				memcpy((void*)&std::get<vtll::index_of<E,C>::value>(tuple), ptr, size);
				return true;
			}
			return false;
		};

		bool componentE(index_t entidx, size_t compidx, void* ptr, size_t size) {
			if constexpr (vtll::has_type<E,C>::value) {
				auto tuple = this->references(entidx);
				memcpy(ptr, (void*)&std::get<vtll::index_of<E,C>::value>(tuple), size);
				return true;
			}
			return false;
		};
	};


	template<typename E>
	inline VecsComponentVector<E>::VecsComponentVector(size_t r) {
		if (!this->init()) return;
		m_handles.reserve(r);

		vtll::static_for<size_t, 0, vtll::size<VecsComponentTypeList>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<VecsComponentTypeList, i>;
				m_dispatch[i] = std::make_unique<VecsComponentVectorDerived<E, type>>(r);
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
	*/

	class VecsRegistryBaseClass : public VecsMonostate<VecsRegistryBaseClass> {
	protected:

		struct entry_t {
			index_t				m_next_free_or_comp_index{};	//next free slot or index of component table
			VecsReadWriteMutex	m_mutex;						//per entity synchronization
			counter16_t			m_generation_counter{ 0 };		//generation counter starts with 0

			entry_t() {};
			entry_t(const entry_t& other) {};
		};

		static inline std::vector<entry_t>	m_entity_table;
		static inline index_t				m_first_free{};

		static inline std::array<std::unique_ptr<VecsRegistryBaseClass>, vtll::size<VecsEntityTypeList>::value> m_dispatch;

		virtual bool updateC(const VecsHandle& handle, size_t compidx, void* ptr, size_t size) { return false; };
		virtual bool componentE(const VecsHandle& handle, size_t compidx, void*ptr, size_t size) { return false; };

	public:
		VecsRegistryBaseClass( size_t r = 1 << 10 );

		//-------------------------------------------------------------------------
		//insert data

		template<typename... Ts>
		[[nodiscard]] VecsHandle insert(Ts&&... args) {
			static_assert(vtll::is_same<VeEntityType<Ts...>, Ts...>::value);
			return VecsRegistry<VeEntityType<Ts...>>().insert(std::forward<Ts>(args)...);
		}

		//-------------------------------------------------------------------------
		//get data

		template<typename E>
		std::optional<VecsEntity<E>> entity(const VecsHandle& handle);

		template<typename C>
		requires vtll::has_type<VecsComponentTypeList, C>::value
		std::optional<C> component(const VecsHandle& handle) {
			C res;
			if (m_dispatch[handle.index()]->componentE(handle, vtll::index_of<VecsComponentTypeList, C>::value, (void*)&res, sizeof(C))) {
				return { res };
			}
			return {};
		}

		//-------------------------------------------------------------------------
		//update data

		template<typename ET>
		bool update(const VecsHandle& handle, ET&& ent);

		template<typename C>
		requires vtll::has_type<VecsComponentTypeList, C>::value
		bool update(const VecsHandle& handle, C&& comp) {
			return m_dispatch[handle.index()]->updateC(handle, vtll::index_of<VecsComponentTypeList,C>::value, (void*)&comp, sizeof(C));
		}

		//-------------------------------------------------------------------------
		//utility

		template<typename E = void>
		size_t size() { return VecsComponentVector<E>().size(); };

		template<>
		size_t size<>();

		template<typename... Cs>
		VecsIterator<Cs...> begin();

		template<typename... Cs>
		VecsIterator<Cs...> end();

		virtual bool contains(const VecsHandle& handle) {
			return m_dispatch[handle.index()]->contains(handle);
		}

		virtual bool erase(const VecsHandle& handle) {
			return m_dispatch[handle.index()]->erase(handle);
		}
	};


	template<>
	size_t VecsRegistryBaseClass::size<void>() {
		size_t sum = 0;
		vtll::static_for<size_t, 0, vtll::size<VecsEntityTypeList>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<VecsEntityTypeList, i>;
				sum += VecsComponentVector<type>().size();
			}
		);
		return sum;
	}



	//-------------------------------------------------------------------------
	//entity table

	/**
	* \brief This class is used as access interface for all entities of type E
	*/

	template<typename E = void>
	class VecsRegistry : public VecsRegistryBaseClass {
	protected:
		bool updateC(const VecsHandle& handle, size_t compidx, void* ptr, size_t size);
		bool componentE(const VecsHandle& handle, size_t compidx, void* ptr, size_t size);

	public:
		VecsRegistry(size_t r = 1 << 10);

		//-------------------------------------------------------------------------
		//insert data

		template<typename... Cs>
		requires vtll::is_same<E, Cs...>::value
		[[nodiscard]] VecsHandle insert(Cs&&... args);

		//-------------------------------------------------------------------------
		//get data

		std::optional<VecsEntity<E>> entity(const VecsHandle& h);

		template<typename C>
		std::optional<C> component(const VecsHandle& handle);

		//-------------------------------------------------------------------------
		//update data

		template<typename ET>
		bool update(const VecsHandle& handle, ET&& ent);

		template<typename C>
		requires (vtll::has_type<VecsComponentTypeList, C>::value)
		bool update(const VecsHandle& handle, C&& comp);

		//-------------------------------------------------------------------------
		//utility

		size_t size() { return VecsComponentVector<E>().size(); };

		bool contains(const VecsHandle& handle);

		bool erase(const VecsHandle& handle);
	};


	template<typename E>
	inline VecsRegistry<E>::VecsRegistry(size_t r) : VecsRegistryBaseClass(r) {}

	template<typename E>
	inline bool VecsRegistry<E>::updateC(const VecsHandle& handle, size_t compidx, void* ptr, size_t size) {
		if (!contains(handle)) return {};
		return VecsComponentVector<E>().updateC(m_entity_table[handle.m_entity_index.value].m_next_free_or_comp_index, compidx, ptr, size);
	}

	template<typename E>
	inline bool VecsRegistry<E>::componentE(const VecsHandle& handle, size_t compidx, void* ptr, size_t size) {
		if (!contains(handle)) return {};
		return VecsComponentVector<E>().componentE( m_entity_table[handle.m_entity_index.value].m_next_free_or_comp_index, compidx, ptr, size );
	}

	template<typename E>
	template<typename... Cs>
	requires vtll::is_same<E, Cs...>::value
	inline [[nodiscard]] VecsHandle VecsRegistry<E>::insert(Cs&&... args) {
		index_t idx{};
		if (!m_first_free.is_null()) {
			idx = m_first_free;
			m_first_free = m_entity_table[m_first_free.value].m_next_free_or_comp_index;
		}
		else {
			idx.value = static_cast<typename index_t::type_name>(m_entity_table.size());	//index of new entity
			m_entity_table.emplace_back();		//start with counter 0
		}

		VecsHandle handle{ idx, m_entity_table[idx.value].m_generation_counter, index16_t{ vtll::index_of<VecsEntityTypeList, E>::value } };
		index_t compidx = VecsComponentVector<E>().insert(handle, std::make_tuple(args...));	//add data as tuple
		m_entity_table[idx.value].m_next_free_or_comp_index = compidx;						//index in component vector 
		return { handle };
	};

	template<typename E>
	inline bool VecsRegistry<E>::contains(const VecsHandle& handle) {
		if (handle.m_entity_index.is_null() || handle.m_entity_index.value>= m_entity_table.size() ) return false;
		if (handle.m_generation_counter != m_entity_table[handle.m_entity_index.value].m_generation_counter) return false;
		return true;
	}

	template<typename E>
	inline std::optional<VecsEntity<E>> VecsRegistry<E>::entity(const VecsHandle& handle) {
		if (!contains(handle)) return {};
		VecsEntity<E> res( handle, VecsComponentVector<E>().values(m_entity_table[handle.m_entity_index.value].m_next_free_or_comp_index) );
		return { res };
	}

	template<typename E>
	template<typename C>
	inline std::optional<C> VecsRegistry<E>::component(const VecsHandle& handle) {
		if constexpr (!vtll::has_type<E,C>::value) { return {}; }
		if (!contains(handle)) return {};
		auto compidx = m_entity_table[handle.m_entity_index.value].m_next_free_or_comp_index;
		return { VecsComponentVector<E>().component<C>(compidx) };
	}

	template<typename E>
	template<typename ET>
	inline bool VecsRegistry<E>::update(const VecsHandle& handle, ET&& ent) {
		if (!contains(handle)) return false;
		VecsComponentVector<E>().update(handle.m_entity_index, std::forward<ET>(ent));
		return true;
	}

	template<typename E>
	template<typename C>
	requires (vtll::has_type<VecsComponentTypeList, C>::value)
	inline bool VecsRegistry<E>::update(const VecsHandle& handle, C&& comp) {
		if constexpr (!vtll::has_type<E, C>::value) { return false; }
		if (!contains(handle)) return false;
		VecsComponentVector<E>().update<C>(handle.m_entity_index, std::forward<C>(comp));
		return true;
	}

	template<typename E>
	inline bool VecsRegistry<E>::erase(const VecsHandle& handle) {
		if (!contains(handle)) return false;
		auto hidx = handle.m_entity_index.value;

		auto [corr_hndl, corr_index] = VecsComponentVector<E>().erase(m_entity_table[hidx].m_next_free_or_comp_index);
		if (!corr_index.is_null()) { m_entity_table[corr_hndl.m_entity_index.value].m_next_free_or_comp_index = corr_index; }

		m_entity_table[hidx].m_generation_counter.value++;															 //>invalidate the entity handle
		if( m_entity_table[hidx].m_generation_counter.is_null() ) { m_entity_table[hidx].m_generation_counter.value = 0; } //wrap to zero
		m_entity_table[hidx].m_next_free_or_comp_index = m_first_free;												 //>put old entry into free list
		m_first_free = handle.m_entity_index;
		return true;
	}


	//-------------------------------------------------------------------------
	//entity table specialization for void

	/**
	* \brief A specialized class to act as convenient access interface instead of the base class.
	*/

	template<>
	class VecsRegistry<void> : public VecsRegistryBaseClass {
	public:
		VecsRegistry(size_t r = 1 << 10) : VecsRegistryBaseClass(r) {};
	};


	//-------------------------------------------------------------------------
	//iterator

	/**
	* \brief Base class for an iterator that iterates over a VecsComponentVector of any type 
	* and that is intested into components Cs
	*/

	template<typename... Cs>
	class VecsIterator {
	protected:
		using entity_types = vtll::filter_have_all_types< VecsEntityTypeList, vtll::type_list<Cs...> >;

		std::array<std::unique_ptr<VecsIterator<Cs...>>, vtll::size<entity_types>::value> m_dispatch;
		index_t m_current_iterator{ 0 };
		index_t m_current_index{ 0 };
		bool	m_is_end{ false };

	public:
		using value_type = std::tuple<VecsHandle, Cs&...>;

		VecsIterator() {};
		VecsIterator( bool is_end );
		VecsIterator(const VecsIterator& v) : VecsIterator(v.m_is_end) {
			if (m_is_end) return;
			m_current_iterator = v.m_current_iterator;
			for (int i = 0; i < m_dispatch.size(); ++i) { m_dispatch[i]->m_current_index = v.m_dispatch[i]->m_current_index; }
		};

		virtual bool is_valid() {
			if (m_is_end || is_vector_end()) return false;
			return m_dispatch[m_current_iterator.value]->is_valid();
		}

		VecsIterator<Cs...>& operator=(const VecsIterator& v) {
			m_current_iterator = v.m_current_iterator;
			for (int i = 0; i < m_dispatch.size(); ++i) { m_dispatch[i]->m_current_index = v.m_dispatch[i]->m_current_index; }
			return *this;
		}

		virtual value_type operator*() { 
			return *(*m_dispatch[m_current_iterator.value]);
		};

		virtual VecsIterator<Cs...>& operator++() {
			(*m_dispatch[m_current_iterator.value])++;
			if (m_dispatch[m_current_iterator.value]->is_vector_end() && m_current_iterator.value < m_dispatch.size() - 1) {
				++m_current_iterator.value;
			}
			return *this;
		};

		virtual VecsIterator<Cs...>& operator++(int) { return operator++(); return *this; };

		void operator+=(size_t N) {
			size_t left = N;
			while (left > 0) {
				int num = std::max(m_dispatch[m_current_iterator.value]->size() 
									- m_dispatch[m_current_iterator.value]->m_current_index.value, 0);
				left -= num;
				m_dispatch[m_current_iterator.value]->m_current_index.value += num;
				if (m_dispatch[m_current_iterator.value]->is_vector_end()) {
					if (m_current_iterator.value < m_dispatch.size() - 1) { ++m_current_iterator.value; }
					else return;
				}
			}
		}

		VecsIterator<Cs...>& operator+(size_t N) {
			VecsIterator<Cs...> temp{ *this };
			temp += N;
			return temp;
		}

		bool operator!=(const VecsIterator<Cs...>& v) {
			return !( *this == v );
		}

		bool operator==(const VecsIterator<Cs...>& v) {
			return	v.m_current_iterator == m_current_iterator &&
					v.m_dispatch[m_current_iterator.value]->m_current_index == m_dispatch[m_current_iterator.value]->m_current_index;
		}

		virtual bool is_vector_end() { return m_dispatch[m_current_iterator.value]->is_vector_end(); }

		virtual size_t size() {
			size_t sum = 0;
			for (int i = 0; i < m_dispatch.size(); ++i) { sum += m_dispatch[i]->size(); };
			return sum;
		}
	};


	/**
	* \brief Iterator that iterates over a VecsComponentVector of type E
	* and that is intested into components Cs
	*/

	template<typename E, typename... Cs>
	class VecsIteratorDerived : public VecsIterator<Cs...> {
	protected:

	public:
		VecsIteratorDerived(bool is_end = false) { //empty constructor does not create new children
			if (is_end) this->m_current_index.value = static_cast<decltype(this->m_current_index.value)>(VecsComponentVector<E>().size());
		};

		bool is_valid() {
			return VecsComponentVector<E>().handle(this->m_current_index).is_valid();
		}

		typename VecsIterator<Cs...>::value_type operator*() {
			return std::make_tuple(VecsComponentVector<E>().handle(this->m_current_index), std::ref(VecsComponentVector<E>().component<Cs>(this->m_current_index))...);
		};

		VecsIterator<Cs...>& operator++() { ++this->m_current_index.value; return *this; };
		
		VecsIterator<Cs...>& operator++(int) { ++this->m_current_index.value; return *this; };
		
		bool is_vector_end() { return this->m_current_index.value >= VecsComponentVector<E>().size(); };

		virtual size_t size() { return VecsComponentVector<E>().size(); }

	};


	template<typename... Cs>
	VecsIterator<Cs...>::VecsIterator(bool is_end) : m_is_end{ is_end } {
		if (is_end) {
			m_current_iterator.value = static_cast<decltype(m_current_iterator.value)>(m_dispatch.size() - 1);
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

	template<typename... Cs>
	void for_each(VecsIterator<Cs...>& b, VecsIterator<Cs...>& e, std::function<Functor<Cs...>> f) {
		for (; b != e; b++) {
			if(b.is_valid()) f(b);
		}
		return; 
	}

	template<typename... Cs>
	void for_each(std::function<Functor<Cs...>> f) {
		auto b = VecsRegistry().begin<Cs...>();
		auto e = VecsRegistry().end<Cs...>();

		return for_each(b, e, f);
	}



	//-------------------------------------------------------------------------
	//left over implementations that depend on definition of classes

	//-------------------------------------------------------------------------
	//VecsRegistryBaseClass

	inline VecsRegistryBaseClass::VecsRegistryBaseClass(size_t r) {
		if (!this->init()) return;
		m_entity_table.reserve(r);

		vtll::static_for<size_t, 0, vtll::size<VecsEntityTypeList>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<VecsEntityTypeList, i>;
				m_dispatch[i] = std::make_unique<VecsRegistry<type>>();
			}
		);
	}

	template<typename E>
	inline std::optional<VecsEntity<E>> VecsRegistryBaseClass::entity(const VecsHandle& handle) {
		return VecsRegistry<E>().entity(handle);
	}

	template<typename ET>
	bool VecsRegistryBaseClass::update(const VecsHandle& handle, ET&& ent) {
		return VecsRegistry<vtll::front<ET>>().update({ handle }, std::forward<ET>(ent));
	}

	//-------------------------------------------------------------------------
	//VecsIterator

	template<typename... Cs>
	VecsIterator<Cs...> VecsRegistryBaseClass::begin() {
		return VecsIterator<Cs...>(false);
	}

	template<typename... Cs>
	VecsIterator<Cs...> VecsRegistryBaseClass::end() {
		return VecsIterator<Cs...>(true);
	}


	//-------------------------------------------------------------------------
	//VecsHandle

	bool VecsHandle::is_valid() {
		if (m_entity_index.is_null() || m_generation_counter.is_null() || m_type_index.is_null()) return false;
		return VecsRegistryBaseClass().contains(*this);
	}

	template<typename E>
	std::optional<VecsEntity<E>> VecsHandle::entity() {
		return VecsRegistry<E>().entity(*this);
	}

	template<typename C>
	std::optional<C> VecsHandle::component() {
		return VecsRegistryBaseClass().component<C>(*this);
	}

	template<typename ET>
	bool VecsHandle::update(ET&& ent) {
		using type = vtll::front<std::decay_t<ET>>;
		return VecsRegistry<type>().update(*this, std::forward<ET>(ent));
	}

	template<typename C>
	requires vtll::has_type<VecsComponentTypeList, C>::value
	bool VecsHandle::update(C&& comp) {
		return VecsRegistryBaseClass().update<C>(*this, std::forward<C>(comp));
	}

	bool VecsHandle::erase() {
		return VecsRegistryBaseClass().erase(*this);
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
