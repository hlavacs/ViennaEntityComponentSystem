#ifndef VECSITERATOR_H
#define VECSITERATOR_H


namespace vecs {

	//-------------------------------------------------------------------------
	//iterator

	/**
	* \brief Base class for an iterator that iterates over a set of entity types.
	*
	* The iterator can be used to loop through the entities. By providing component types Ts..., only
	* entities are included that contain ALL component types.
	*/

	template<typename ETL, typename CTL>
	class VecsIteratorBaseClass {
		protected:
			using array_type = std::array<std::unique_ptr<VecsIteratorEntityBaseClass<ETL, CTL>>, vtll::size<ETL>::value>;

			array_type m_dispatch;				///< Subiterators for each entity type E

			type_index_t	m_current_iterator{ 0 };	///< Current iterator of type E that is used
			table_index_t	m_current_index{ 0 };		///< Current index in the VecsComponentTable<E>
			VecsHandle		m_current_handle{};
			size_t			m_size{ 0 };				///< Number of entities max covered by the iterator
			size_t			m_current_sizeE{ 0 };		///< Number of entities max covered by the current sub iterator
			bool			m_is_end{ false };			///< True if this is an end iterator (for stopping the loop)

			VecsIteratorBaseClass(std::nullopt_t n) noexcept {};		///< Needed for derived iterator to call

		public:
			using value_type		= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandle>, CTL > >;
			using reference			= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandle>, vtll::to_ref<CTL> > >;
			using pointer			= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandle>, vtll::to_ptr<CTL> > >;
			using iterator_category = std::forward_iterator_tag;
			using difference_type	= size_t;
			using last_type			= vtll::back<ETL>;	///< last type for end iterator

			static inline value_type			m_dummy;				///< Dummy values for returning references to if the handle is not valid
			static inline std::atomic<uint32_t> m_dummy_mutex;			///< Dummy mutex for returning pointer to if the handle is not valid

			VecsIteratorBaseClass(bool is_end = false) noexcept;				///< Constructor that should be called always from outside
			VecsIteratorBaseClass(const VecsIteratorBaseClass<ETL,CTL>& v) noexcept;		///< Copy constructor

			auto operator=(const VecsIteratorBaseClass<ETL,CTL>& v) noexcept	-> VecsIteratorBaseClass<ETL,CTL>;	///< Copy
			auto operator+=(size_t N) noexcept									-> VecsIteratorBaseClass<ETL,CTL>;	///< Increase and set
			auto operator+(size_t N) noexcept									-> VecsIteratorBaseClass<ETL,CTL>;	///< Increase
			auto operator!=(const VecsIteratorBaseClass<ETL,CTL>& v) noexcept	-> bool;	///< Unqequal
			auto operator==(const VecsIteratorBaseClass<ETL,CTL>& v) noexcept	-> bool;	///< Equal

			virtual auto handle() noexcept			-> VecsHandle;				///< Return handle of the current entity
			virtual auto mutex() noexcept			-> std::atomic<uint32_t>*;	///< Return poiter to the mutex of this entity
			virtual auto has_value() noexcept		-> bool;					///< Is currently pointint to a valid entity
			virtual	auto operator*() noexcept		-> reference;				///< Access the data
			virtual auto operator++() noexcept		-> VecsIteratorBaseClass<ETL,CTL>&;	///< Increase by 1
			virtual auto operator++(int) noexcept	-> VecsIteratorBaseClass<ETL,CTL>&;	///< Increase by 1
			virtual auto is_vector_end() noexcept	-> bool;					///< Is currently at the end of any sub iterator
			virtual auto size() noexcept			-> size_t;					///< Number of valid entities
	};


	/**
	* \brief Copy-constructor of class VecsIteratorbaseClass
	*
	* \param[in] v Other Iterator that should be copied
	*/
	template<typename ETL, typename CTL>
	inline VecsIteratorBaseClass<ETL,CTL>::VecsIteratorBaseClass(const VecsIteratorBaseClass<ETL,CTL>& v) noexcept : VecsIteratorBaseClass<ETL,CTL>(v.m_is_end) {

		//std::cout << "VecsIteratorBaseClass<ETL,CTL>(const VecsIteratorBaseClass<ETL,CTL>& v): " << typeid(ETL).name() << std::endl << std::endl;

		m_current_iterator = v.m_current_iterator;
		m_current_index = v.m_current_index;
		m_size = v.m_size;
		if (m_is_end) return;
		for (int i = 0; i < m_dispatch.size(); ++i) {
			*m_dispatch[i] = *v.m_dispatch[i];
		}
	};

	/**
	* \brief Determines whether the iterator currently points to avalid entity.
	* The call is dispatched to the respective sub-iterator that is currently used.
	*
	* \returns true if the iterator points to a valid entity.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::has_value() noexcept	-> bool {
		if (m_is_end || is_vector_end()) return false;
		return m_dispatch[m_current_iterator]->has_value();
	}

	/**
	* \brief Retrieve the handle of the current entity.
	* \returns the handle of the current entity.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::handle() noexcept	-> VecsHandle {
		return m_dispatch[m_current_iterator]->handle();
	}

	/**
	* \brief Retrieve the handle of the current entity.
	* \returns the handle of the current entity.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::mutex() noexcept	-> std::atomic<uint32_t>* {
		return m_dispatch[m_current_iterator]->mutex();
	}

	/**
	* \brief Copy assignment operator.
	*
	* \param[in] v The source iterator for the assignment.
	* \returns *this.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::operator=(const VecsIteratorBaseClass<ETL, CTL>& v) noexcept	-> VecsIteratorBaseClass<ETL,CTL> {
		m_current_iterator = v.m_current_iterator;
		m_current_index = v.m_current_index;
		m_size = v.m_size;
		for (int i = 0; i < m_dispatch.size(); ++i) {
			*m_dispatch[i] = *v.m_dispatch[i];
		}
		return *this;
	}

	/**
	* \brief Get the values of the entity that the iterator is currently pointing to.
	*
	* \returns a tuple holding the components Ts of the entity the iterator is currently pointing to.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::operator*() noexcept	-> reference {
		return *(*m_dispatch[m_current_iterator]);
	};

	/**
	* \brief Goto next entity.
	*
	* \returns *this.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::operator++() noexcept		-> VecsIteratorBaseClass<ETL,CTL>& {
		if (m_is_end) return *this;
		(*m_dispatch[m_current_iterator])++;

		if (m_dispatch[m_current_iterator]->is_vector_end() && m_current_iterator.value < m_dispatch.size() - 1) [[unlikely]] {
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
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::operator++(int) noexcept	-> VecsIteratorBaseClass<ETL,CTL>& {
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
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::operator+=(size_t N) noexcept -> VecsIteratorBaseClass<ETL,CTL> {
		if (m_is_end) return *this;
		size_t left = N;
		while (left > 0) {
			size_t num = std::max((size_t)(m_dispatch[m_current_iterator]->size() - m_current_index), (size_t)0);
			num = std::min(num, left);

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
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::operator+(size_t N) noexcept	-> VecsIteratorBaseClass<ETL,CTL> {
		VecsIteratorBaseClass<ETL,CTL> temp{ *this };
		temp += N;
		return temp;
	}

	/**
	* \brief Test for unequality
	*
	* \returns true if the iterators are not equal.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::operator!=(const VecsIteratorBaseClass<ETL,CTL>& v) noexcept	-> bool {
		return !(*this == v);
	}

	/**
	* \brief Test for equality.
	*
	* \returns true if the iterators are equal.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::operator==(const VecsIteratorBaseClass<ETL,CTL>& v) noexcept	-> bool {
		return	v.m_current_iterator == m_current_iterator && v.m_current_index == m_current_index;
	}

	/**
	* \brief Test whether the curerntly used sub-iterator is at its end point.
	* The call is dispatched to the current subiterator.
	*
	* \returns true if the current subiterator is at its end.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::is_vector_end() noexcept		-> bool {
		return m_dispatch[m_current_iterator]->is_vector_end();
	}

	/**
	* \brief Get the total number of all entities that are covered by this iterator.
	* This is simply the sum of the entities covered by the subiterators.
	* This number is set when the iterator is created.
	*
	* \returns the total number of entities that have all components Ts.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<ETL,CTL>::size() noexcept	-> size_t {
		return m_size;
	}


	//----------------------------------------------------------------------------------------------
	//VecsIterator - derived from VecsIteratorBaseClass


	/**
	* \brief Primary template for iterators
	*/
	template<typename... Ts>
	class VecsIterator;

	/**
	* \brief Iterator for given list of entity types. Entity types are taken as is, and NOT extended by possible tags from the tags map.
	*/
	template<typename ETL>
	using it_CTL_entity_list = vtll::remove_types< vtll::intersection< ETL >, VecsEntityTagList >;

	template<typename ETL>
	requires (is_entity_type_list<ETL>::value)
	class VecsIterator<ETL> : public VecsIteratorBaseClass< ETL, it_CTL_entity_list<ETL> > {
	public:
		using component_types = it_CTL_entity_list<ETL> ;
		VecsIterator(bool end = false) noexcept : VecsIteratorBaseClass< ETL, it_CTL_entity_list<ETL> >(end) {};
	};

	/**
	* \brief Iterator for given entity types.
	* 
	* Entity types are the given entity types, but also their extensions using the tag map.
	* Component types are the intersection of the entity types.
	*/
	template<typename... Es>
	using it_CTL_entity_types = vtll::remove_types< vtll::intersection< vtll::tl<Es...> >, VecsEntityTagList >;

	template<typename... Es>
	requires (sizeof...(Es) > 0 && are_entity_types<Es...>)
	class VecsIterator<Es...> : public VecsIteratorBaseClass< expand_tags< vtll::tl<Es...> >, it_CTL_entity_types<Es...> > {
	public:
		using component_types = it_CTL_entity_types<Es...>;
		VecsIterator(bool end = false) noexcept : VecsIteratorBaseClass< expand_tags< vtll::tl<Es...> >, it_CTL_entity_types<Es...> >(end) {};
	};

	/**
	* \brief Iterator for given component types.
	*/
	template<typename... Cs>
	using it_CTL_types = vtll::remove_types< vtll::tl<Cs...>, VecsEntityTagList >;

	template<typename... Cs>
	requires (sizeof...(Cs) > 0 && are_component_types<Cs...>)
	class VecsIterator<Cs...> : public VecsIteratorBaseClass< vtll::filter_have_all_types< VecsEntityTypeList, vtll::tl<Cs...> >, it_CTL_types<Cs...> > {
	public:
		using component_types = it_CTL_types<Cs...>;
		VecsIterator(bool end = false) noexcept : VecsIteratorBaseClass< vtll::filter_have_all_types< VecsEntityTypeList, vtll::tl<Cs...> >, it_CTL_types<Cs...> >(end) {};
	};

	/**
	* \brief Iterator for given entity type that has all tags.
	*/
	template<typename E, typename... Ts>
	using it_CTL_entity_tags = vtll::remove_types< E, VecsEntityTagList >;

	template<typename E, typename... Ts>
	requires (is_entity_type<E> && (sizeof...(Ts) > 0) && are_entity_tags<Ts...>)
	class VecsIterator<E,Ts...> : public VecsIteratorBaseClass< vtll::filter_have_all_types< expand_tags<vtll::tl<E>>, vtll::tl<Ts...> >, it_CTL_entity_tags<E> > {
	public:
		using component_types = it_CTL_entity_tags<E>;
		VecsIterator(bool end = false) noexcept : VecsIteratorBaseClass< vtll::filter_have_all_types< expand_tags<vtll::tl<E>>, vtll::tl<Ts...> >, it_CTL_entity_tags<E> >(end) {};
	};

	/**
	* \brief Iterator for all entities.
	*/
	using it_CTL_all_entities = vtll::remove_types< vtll::intersection< VecsEntityTypeList >, VecsEntityTagList >;

	template<>
	class VecsIterator<> : public VecsIteratorBaseClass < VecsEntityTypeList, it_CTL_all_entities > {
	public:
		using component_types = it_CTL_all_entities;
		VecsIterator(bool end = false) noexcept :  VecsIteratorBaseClass < VecsEntityTypeList, it_CTL_all_entities >(end) {};
	};


	//-------------------------------------------------------------------------
	//VecsIteratorEntityBaseClass

	/**
	* \brief Base class for Iterators that iterate over a VecsComponentTables.
	*/
	template<typename ETL, typename CTL>
	class VecsIteratorEntityBaseClass {
		friend class VecsIteratorBaseClass<ETL, CTL>;

	protected:
		table_index_t	m_current_index{ 0 };		///< Current index in the VecsComponentTable<E>
		size_t			m_sizeE{ 0 };				///< Number of valid and invalid entities of type E

	public:
		VecsIteratorEntityBaseClass(bool is_end = false) noexcept {};
		virtual auto handle() noexcept		-> VecsHandle = 0;
		virtual auto mutex() noexcept		-> std::atomic<uint32_t>* = 0;
		virtual auto has_value() noexcept	-> bool = 0;
		virtual auto operator*() noexcept	-> typename VecsIteratorBaseClass<ETL, CTL>::reference = 0;

		auto operator=(const VecsIteratorEntityBaseClass& rhs) noexcept	-> void;
		auto operator++() noexcept			-> void;
		auto operator++(int) noexcept		-> void;
		auto is_vector_end() noexcept		-> bool;
		auto size() noexcept				-> size_t;	///< Total number of valid and invalid entities in the component table for type E
	};


	/**
	* \brief Copy operator
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorEntityBaseClass<ETL, CTL>::operator=(const VecsIteratorEntityBaseClass& rhs) noexcept -> void {
		m_current_index = rhs.m_current_index;
		m_sizeE = rhs.m_sizeE;
	};

	/**
	* \brief Point to the next entity.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorEntityBaseClass<ETL,CTL>::operator++() noexcept		-> void {
		if (!is_vector_end()) [[likely]] ++m_current_index;
	};

	/**
	* \brief Point to the next entity.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorEntityBaseClass<ETL,CTL>::operator++(int) noexcept	-> void {
		if (!is_vector_end()) [[likely]] ++m_current_index;
	};

	/**
	* \brief Determine whether the iterator points beyond the last entity of its type E.
	* This means it has reached its end.
	* \returns true if the iterator points beyond its last entity.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorEntityBaseClass<ETL,CTL>::is_vector_end() noexcept	-> bool {
		return m_current_index >= m_sizeE;
	};

	/**
	* \brief Return the total number of valid and invalid entities in the component table for type E.
	* \returns the total number of valid and invalid entities in the component table for type E.
	*/
	template<typename ETL, typename CTL>
	inline auto VecsIteratorEntityBaseClass<ETL,CTL>::size() noexcept			-> size_t {
		return this->m_sizeE;
	}


	//-------------------------------------------------------------------------
	//VecsIteratorEntity

	/**
	* \brief Iterator that iterates over a VecsComponentTable of type E and that is intested into components Ts
	*/

	template<typename E, typename ETL, typename CTL>
	class VecsIteratorEntity;

	template<typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	class VecsIteratorEntity<E, ETL, CTL<Cs...>> : public VecsIteratorEntityBaseClass<ETL,CTL<Cs...>> {

		using reference = VecsIteratorBaseClass<ETL,CTL<Cs...>>::reference;

		static inline VecsComponentTable<E> m_component_table;

	public:
		VecsIteratorEntity(bool is_end = false) noexcept;
		auto handle() noexcept			-> VecsHandle;
		auto mutex() noexcept			-> std::atomic<uint32_t>*;
		auto has_value() noexcept		-> bool;
		auto operator*() noexcept		-> reference;
	};


	/**
	* \brief Constructor of class VecsIteratorEntity<E, Ts...>. Calls empty constructor of base class.
	*
	* \param[in] is_end If true, then the iterator belongs to an end-iterator.
	*/
	template<typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline VecsIteratorEntity<E,ETL,CTL<Cs...>>::VecsIteratorEntity(bool is_end) noexcept : VecsIteratorEntityBaseClass<ETL,CTL<Cs...>>() {
		this->m_sizeE = m_component_table.size(); ///< iterate over ALL entries, also the invalid ones!
		if (is_end) {
			this->m_current_index = static_cast<decltype(this->m_current_index)>(this->m_sizeE);
		}
	};

	/**
	* \brief Test whether the iterator points to a valid entity.
	* \returns true if the iterator points to a valid entity.
	*/
	template<typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline auto VecsIteratorEntity<E,ETL,CTL<Cs...>>::has_value() noexcept		-> bool {
		return m_component_table.handle(this->m_current_index).has_value();
	}

	/**
	* \returns the handle of the current entity.
	*/
	template<typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline auto VecsIteratorEntity<E,ETL,CTL<Cs...>>::handle() noexcept		-> VecsHandle {
		return m_component_table.handle(this->m_current_index);
	}

	/**
	* \returns the mutex of the current entity.
	*/
	template<typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline auto VecsIteratorEntity<E,ETL,CTL<Cs...>>::mutex() noexcept		-> std::atomic<uint32_t>* {
		if (this->m_current_index.value >= this->m_sizeE) {
			return &VecsIteratorBaseClass<ETL,CTL<Cs...>>::m_dummy_mutex;
		}
		return m_component_table.mutex(this->m_current_index);
	}

	/**
	* \brief Access operator retrieves all relevant components Ts from the entity it points to.
	* \returns all components Ts from the entity the iterator points to.
	*/
	template<typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline auto VecsIteratorEntity<E,ETL,CTL<Cs...>>::operator*() noexcept	-> reference {
		if (this->m_current_index.value >= this->m_sizeE) {
			return std::forward_as_tuple(VecsHandle{}, std::get<Cs>(VecsIteratorBaseClass<ETL, CTL<Cs...>>::m_dummy)...);
		}
		return std::forward_as_tuple(m_component_table.handle(this->m_current_index), m_component_table.component<Cs>(this->m_current_index)...);
	};


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
	//VecsRangeBaseClass

	/**
	* \brief Range for iterating over all entities that contain the given component types.
	*/
	template<typename ETL, typename CTL>
	class VecsRangeBaseClass {
		VecsIteratorBaseClass<ETL,CTL> m_begin;
		VecsIteratorBaseClass<ETL,CTL> m_end;

		public:
			VecsRangeBaseClass() noexcept : m_begin{ false }, m_end{ true } {
				//std::cout << "VecsRangeBaseClass<ETL,CTL>: " << typeid(ETL).name() << std::endl << std::endl;
			};
			VecsRangeBaseClass(VecsIteratorBaseClass<ETL,CTL>& begin, VecsIteratorBaseClass<ETL,CTL>& end) noexcept : m_begin{ begin }, m_end{ end } {};
			VecsRangeBaseClass(const VecsRangeBaseClass<ETL,CTL>& rhs) noexcept : m_begin{ rhs.m_begin }, m_end{ rhs.m_end } {};

			auto operator=(const VecsRangeBaseClass<ETL,CTL>& rhs) noexcept -> VecsIteratorBaseClass<ETL,CTL> {
				m_begin = rhs.m_begin;
				m_end = rhs.m_end;
				return *this;
			}

			auto split(size_t N) noexcept {
				std::pmr::vector<VecsRangeBaseClass> result;	///< Result vector
				result.reserve(N);							///< Need exactly N slots
				size_t remain = m_begin.size();				///< Remaining entities
				size_t num = remain / N;					///< Put the same number in each slot
				if (num * N < remain) ++num;				///< We might need one more per slot
				auto b = m_begin;							///< Begin iterator
				while (remain > 0 && b != m_end) {			///< While there are remaining entities
					if (remain > num) {						///< At least two slots left
						size_t delta = (remain > num ? num : remain);
						VecsIteratorBaseClass<ETL,CTL> e = b + delta;
						result.emplace_back(VecsRangeBaseClass(b, e));
						remain -= delta;
						b = e + 1;
					}
					else {													///< One last slot left
						result.emplace_back(VecsRangeBaseClass(b, m_end));	///< Range lasts to the endf
						remain = 0;
					}
				};
				return result;
			}

			inline auto for_each(std::function<typename Functor<CTL>::type> f) -> void {
				auto b = begin();
				auto e = end();
				VecsRegistry reg{};
				for (; b != e; ++b) {
					VecsWriteLock lock(b.mutex());		///< Write lock
					if (!b.has_value()) continue;
					std::apply(f, *b);					///< Run the function on the references
				}
			}


			auto begin() noexcept	->	VecsIteratorBaseClass<ETL,CTL> {
				return m_begin;
			}

			auto end() noexcept		->	VecsIteratorBaseClass<ETL,CTL> {
				return m_end;
			}
	};


	/**
	* \brief Primary template for ranges.
	*/
	template<typename... Ts>
	class VecsRange;

	/**
	* \brief Range over a list of entity types. In this case, entity types are taken as is, and are NOT expanded with the tag map.
	*/
	template<typename ETL>
	requires (is_entity_type_list<ETL>::value)
	class VecsRange<ETL> : public VecsRangeBaseClass< ETL, it_CTL_entity_list<ETL> > {};

	/**
	* \brief Range over a set of entity types. Entity types are expanded using the tag map.
	*/
	template<typename... Es>
	requires (sizeof...(Es) > 0 && are_entity_types<Es...>)
	class VecsRange<Es...> : public VecsRangeBaseClass< expand_tags< vtll::tl<Es...> >, it_CTL_entity_types<Es...> > {};

	/**
	* \brief Range over a set of entities that contain all given component types.
	*/
	template<typename... Cs>
	requires (sizeof...(Cs) > 0 && are_component_types<Cs...>)
	class VecsRange<Cs...> : public VecsRangeBaseClass< vtll::filter_have_all_types< VecsEntityTypeList, vtll::tl<Cs...> >, it_CTL_types<Cs...> > {};

	/**
	* \brief Iterator for given entity tape that has all tags.
	*/
	template<typename E, typename... Ts>
	requires (is_entity_type<E> && (sizeof...(Ts) > 0) && are_entity_tags<Ts...>)
	class VecsRange<E, Ts...> : public VecsRangeBaseClass< vtll::filter_have_all_types< expand_tags<vtll::tl<E>>, vtll::tl<Ts...> >, it_CTL_entity_tags<E> > {};

	/**
	* \brief Range over all entity types in VECS. This includes all tag variants.
	*/
	template<>
	class VecsRange<> : public VecsRangeBaseClass < VecsEntityTypeList, it_CTL_all_entities > {};


	//-------------------------------------------------------------------------
	//VecsRangeBaseClass - left over

	/**
	* \brief Constructor of class VecsIteratorBaseClass. If its not an end iterator then it also
	* creates subiterators for all entity types that have all component types.
	* 
	* \param[in] is_end If true then this iterator is an end iterator.
	*/
	template<typename ETL, typename CTL>
	inline VecsIteratorBaseClass<ETL, CTL>::VecsIteratorBaseClass(bool is_end) noexcept : m_is_end{ is_end } {

		//std::cout << "VecsIteratorBaseClass<ETL,CTL>(): " << typeid(ETL).name() << std::endl << std::endl;

		if (is_end) {
			m_current_iterator = static_cast<decltype(m_current_iterator)>(m_dispatch.size() - 1);
			//std::cout << "last_type: " << typeid(last_type).name() << std::endl << std::endl;
			m_current_index = static_cast<decltype(m_current_index)>(VecsRegistry<last_type>().size());
		}

		vtll::static_for<size_t, 0, vtll::size<ETL>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<ETL, i>;
				m_dispatch[i] = std::make_unique<VecsIteratorEntity<type,ETL,CTL>>(is_end);
				auto size = m_dispatch[i]->size();
				m_size += size;
				if (!is_end && m_current_iterator == i && size == 0 && i + 1 < vtll::size<ETL>::value) {
					m_current_iterator++;
				}
			}
		);
	};

}


#endif

