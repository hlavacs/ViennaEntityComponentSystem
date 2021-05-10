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

	template<typename P, typename ETL, typename CTL>
	class VecsIteratorBaseClass {

		//friends
		template<typename P, typename ETL, typename CTL> friend class VecsIteratorEntityBaseClass;
		template<typename P, typename E, typename ETL, typename CTL> friend class VecsIteratorEntity;

		protected:
			using array_type = std::array<std::unique_ptr<VecsIteratorEntityBaseClass<P, ETL, CTL>>, vtll::size<ETL>::value>;

			array_type				m_dispatch;							///< Subiterators for each entity type E
			type_index_t			m_current_iterator{ 0 };			///< Current iterator of type E that is used
			table_index_t			m_current_index{ 0 };				///< Current index in the VecsComponentTable<E>
			std::atomic<size_t>*	m_current_sizeE_ptr{nullptr};		///< Number of entities in the table of current subiterator
			size_t					m_current_sizeE{0};					///< Local copy of sizeE
			VecsHandleTemplate<P>*	m_current_handle_ptr{ nullptr };	///< Current handle the subiterator points to
			std::atomic<uint32_t>*	m_current_mutex_ptr{ nullptr };		///< Current mutex the subiterator points to
			
			size_t					m_size{ 0 };			///< Number of entities covered by the iterator (can change due to multithreading)
			bool					m_is_end{ false };		///< True if this is an end iterator (for stopping the loop)

			VecsIteratorBaseClass(std::nullopt_t n) noexcept {};		///< Needed for derived iterator to call

		public:
			using value_type		= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandleTemplate<P>>, CTL > >;
			using reference			= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandleTemplate<P>>, vtll::to_ref<CTL> > >;
			using pointer			= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandleTemplate<P>>, vtll::to_ptr<CTL> > >;
			using iterator_category = std::forward_iterator_tag;
			using difference_type	= size_t;
			using last_type			= vtll::back<ETL>;	///< last type for end iterator

			VecsIteratorBaseClass(bool is_end = false) noexcept;							///< Constructor that should be called always from outside
			VecsIteratorBaseClass(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept;	///< Copy constructor

			auto operator=(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept	-> VecsIteratorBaseClass<P, ETL,CTL>;	///< Copy
			auto operator+=(size_t N) noexcept										-> VecsIteratorBaseClass<P, ETL,CTL>;	///< Increase and set
			auto operator+(size_t N) noexcept										-> VecsIteratorBaseClass<P, ETL,CTL>;	///< Increase
			auto operator!=(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept	-> bool;	///< Unqequal
			auto operator==(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept	-> bool;	///< Equal

			auto handle() noexcept			-> VecsHandleTemplate<P>;			///< Return handle of the current entity
			auto mutex_ptr() noexcept		-> std::atomic<uint32_t>*;	///< Return poiter to the mutex of this entity
			auto has_value() noexcept		-> bool;					///< Is currently pointint to a valid entity
			
			auto operator*() noexcept		-> reference;							///< Access the data
			auto operator++() noexcept		-> VecsIteratorBaseClass<P, ETL,CTL>&;	///< Increase by 1
			auto operator++(int) noexcept	-> VecsIteratorBaseClass<P, ETL,CTL>&;	///< Increase by 1
			auto size() noexcept			-> size_t;								///< Number of valid entities
	};


	/**
	* \brief Copy-constructor of class VecsIteratorbaseClass
	*
	* \param[in] v Other Iterator that should be copied
	*/
	template<typename P, typename ETL, typename CTL>
	inline VecsIteratorBaseClass<P, ETL, CTL>::VecsIteratorBaseClass(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept 
		: VecsIteratorBaseClass<P, ETL, CTL>(v.m_is_end) {

		//std::cout << "VecsIteratorBaseClass<ETL,CTL>(const VecsIteratorBaseClass<ETL,CTL>& v): " << typeid(ETL).name() << std::endl << std::endl;

		m_current_iterator = v.m_current_iterator;
		m_current_index = v.m_current_index;
		m_current_sizeE_ptr = v.m_current_sizeE_ptr;
		m_current_sizeE = v.m_current_sizeE;
		m_current_handle_ptr = v.m_current_handle_ptr;
		m_current_mutex_ptr = v.m_current_mutex_ptr;
		m_size = v.m_size;
		if (m_is_end) return;
	};

	/**
	* \brief Determines whether the iterator currently points to avalid entity.
	* The call is dispatched to the respective sub-iterator that is currently used.
	*
	* \returns true if the iterator points to a valid entity.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::has_value() noexcept	-> bool {
		if (m_is_end || m_current_handle_ptr == nullptr) return false;
		return VecsRegistryBaseClass<P>::m_registry.has_value(*m_current_handle_ptr);
	}

	/**
	* \brief Retrieve the handle of the current entity.
	* \returns the handle of the current entity.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::handle() noexcept	-> VecsHandleTemplate<P> {
		if (m_is_end || m_current_handle_ptr == nullptr) return {};
		return *m_current_handle_ptr;
	}

	/**
	* \brief Retrieve the handle of the current entity.
	* \returns the handle of the current entity.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::mutex_ptr() noexcept	-> std::atomic<uint32_t>* {
		return m_current_mutex_ptr;
	}

	/**
	* \brief Copy assignment operator.
	*
	* \param[in] v The source iterator for the assignment.
	* \returns *this.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::operator=(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept	-> VecsIteratorBaseClass<P, ETL, CTL> {
		m_current_iterator = v.m_current_iterator;
		m_current_index = v.m_current_index;
		m_current_sizeE_ptr = v.m_current_sizeE_ptr;
		m_current_sizeE = v.m_current_sizeE;
		m_current_handle_ptr = v.m_current_handle_ptr;
		m_current_mutex_ptr = v.m_current_mutex_ptr;
		m_size = v.m_size;
		return *this;
	}

	/**
	* \brief Get the values of the entity that the iterator is currently pointing to.
	*
	* \returns a tuple holding the components Ts of the entity the iterator is currently pointing to.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::operator*() noexcept	-> reference {
		return *(*m_dispatch[m_current_iterator]);
	};

	/**
	* \brief Goto next entity.
	*
	* \returns *this.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::operator++() noexcept		-> VecsIteratorBaseClass<P, ETL, CTL>& {
		if (m_is_end) return *this;
		++m_current_index;
		if (m_current_index >= m_current_sizeE) m_current_sizeE = m_current_sizeE_ptr->load();

		if ( m_current_index >= m_current_sizeE ) {
			size_t size = 0;
			while( size == 0 && m_current_iterator.value < m_dispatch.size() - 1) {
				++m_current_iterator;
				m_current_index = 0;
				m_dispatch[m_current_iterator]->init();
				size = m_current_sizeE;
			 } 
		}
		else {
			m_dispatch[m_current_iterator]->data();
		}
		return *this;
	};

	/**
	* \brief Goto next entity.
	*
	* \returns *this.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::operator++(int) noexcept	-> VecsIteratorBaseClass<P, ETL, CTL>& {
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
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::operator+=(size_t N) noexcept -> VecsIteratorBaseClass<P, ETL, CTL> {
		if (m_is_end) return *this;
		size_t left = N;
		while (left > 0) {
			size_t num = std::max((size_t)(m_current_sizeE - m_current_index), (size_t)0); ///< So much can we consume
			num = std::min(num, left);	///< That is what we consume

			left -= num;	///< Consume these numbers
			m_current_index += static_cast<decltype(m_current_index.value)>(num); ///< Increase index by this

			if (m_current_index >= m_current_sizeE) { ///< Are we at the end of the subiterator?
				size_t size = 0;
				while (size == 0 && m_current_iterator.value < m_dispatch.size() - 1) { ///< Yes, find next with non zero numbers
					++m_current_iterator;					///< Goto next
					m_current_index = 0;					///< Start at beginning
					m_dispatch[m_current_iterator]->init();	///< Get the data
					size = m_current_sizeE;		///< Do we have entities?
				}
				if(m_current_sizeE == 0) {	///< Nothing left to iterate over
					left = 0;
				}
			}
		}
		m_dispatch[m_current_iterator]->data();
		return *this;
	}

	/**
	* \brief Create an iterator that is N positions further
	*
	* \param[in] N Number of positions to jump.
	* \returns The new iterator.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::operator+(size_t N) noexcept	-> VecsIteratorBaseClass<P, ETL, CTL> {
		VecsIteratorBaseClass<P, ETL, CTL> temp{ *this };
		temp += N;
		return temp;
	}

	/**
	* \brief Test for unequality
	*
	* \returns true if the iterators are not equal.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::operator!=(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept	-> bool {
		return !(*this == v);
	}

	/**
	* \brief Test for equality.
	*
	* \returns true if the iterators are equal.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL,CTL>::operator==(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept	-> bool {
		return	v.m_current_iterator == m_current_iterator && v.m_current_index == m_current_index;
	}

	/**
	* \brief Get the total number of all entities that are covered by this iterator.
	* This is simply the sum of the entities covered by the subiterators.
	* This number is set when the iterator is created.
	*
	* \returns the total number of entities that have all components Ts.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::size() noexcept	-> size_t {
		return m_size;
	}


	//----------------------------------------------------------------------------------------------
	//VecsIterator - derived from VecsIteratorBaseClass


	/**
	* \brief Iterator for given list of entity types. Entity types are taken as is, and NOT extended by possible tags from the tags map.
	*/
	template<typename P, typename ETL>
	using it_CTL_entity_list = vtll::remove_types< vtll::intersection< ETL >, VecsEntityTagList<P> >;

	template<typename P, typename ETL>
	requires (is_entity_type_list<P, ETL>::value)
	class VecsIteratorTemplate<P, ETL> : public VecsIteratorBaseClass< P, ETL, it_CTL_entity_list<P, ETL> > {
	public:
		using component_types = it_CTL_entity_list<P, ETL> ;
		VecsIteratorTemplate(bool end = false) noexcept : VecsIteratorBaseClass< P, ETL, it_CTL_entity_list<P, ETL> >(end) {};
	};

	/**
	* \brief Iterator for given entity types.
	* 
	* Entity types are the given entity types, but also their extensions using the tag map.
	* Component types are the intersection of the entity types.
	*/
	template<typename P, typename... Es>
	using it_CTL_entity_types = vtll::remove_types< vtll::intersection< vtll::tl<Es...> >, VecsEntityTagList<P> >;

	template<typename P, typename... Es>
	requires (sizeof...(Es) > 0 && are_entity_types<P, Es...>)
	class VecsIteratorTemplate<P, Es...> : public VecsIteratorBaseClass< P, expand_tags< P, vtll::tl<Es...> >, it_CTL_entity_types<P, Es...> > {
	public:
		using component_types = it_CTL_entity_types<P, Es...>;
		VecsIteratorTemplate(bool end = false) noexcept : VecsIteratorBaseClass< P, expand_tags< P, vtll::tl<Es...> >, it_CTL_entity_types<P, Es...> >(end) {};
	};

	/**
	* \brief Iterator for given component types.
	*/
	template<typename P, typename... Cs>
	using it_CTL_types = vtll::remove_types< vtll::tl<Cs...>, VecsEntityTagList<P> >;

	template<typename P, typename... Cs>
	requires (sizeof...(Cs) > 0 && are_component_types<P, Cs...>)
	class VecsIteratorTemplate<P, Cs...> : public VecsIteratorBaseClass< P, vtll::filter_have_all_types< VecsEntityTypeList<P>, vtll::tl<Cs...> >, it_CTL_types<P, Cs...> > {
	public:
		using component_types = it_CTL_types<P, Cs...>;
		VecsIteratorTemplate(bool end = false) noexcept : VecsIteratorBaseClass< vtll::filter_have_all_types< VecsEntityTypeList<P>, vtll::tl<Cs...> >, it_CTL_types<P, Cs...> >(end) {};
	};

	/**
	* \brief Iterator for given entity type that has all tags.
	*/
	template<typename P, typename E, typename... Ts>
	using it_CTL_entity_tags = vtll::remove_types< E, VecsEntityTagList<P> >;

	template<typename P, typename E, typename... Ts>
	requires (is_entity_type<P, E> && (sizeof...(Ts) > 0) && are_entity_tags<P, Ts...>)
	class VecsIteratorTemplate<P, E,Ts...> : public VecsIteratorBaseClass< P, vtll::filter_have_all_types< expand_tags<P, vtll::tl<E>>, vtll::tl<Ts...> >, it_CTL_entity_tags<P, E> > {
	public:
		using component_types = it_CTL_entity_tags<P, E>;
		VecsIteratorTemplate(bool end = false) noexcept : VecsIteratorBaseClass< P, vtll::filter_have_all_types< expand_tags<P, vtll::tl<E>>, vtll::tl<Ts...> >, it_CTL_entity_tags<P, E> >(end) {};
	};

	/**
	* \brief Iterator for all entities.
	*/
	template<typename P>
	using it_CTL_all_entities = vtll::remove_types< vtll::intersection< VecsEntityTypeList<P> >, VecsEntityTagList<P> >;

	template<typename P>
	class VecsIteratorTemplate<P> : public VecsIteratorBaseClass<P, VecsEntityTypeList<P>, it_CTL_all_entities<P> > {
	public:
		using component_types = it_CTL_all_entities<P>;
		VecsIteratorTemplate(bool end = false) noexcept :  VecsIteratorBaseClass<P, VecsEntityTypeList<P>, it_CTL_all_entities<P> >(end) {};
	};


	//-------------------------------------------------------------------------
	//VecsIteratorEntityBaseClass

	/**
	* \brief Base class for Iterators that iterate over a VecsComponentTables.
	*/
	template<typename P, typename ETL, typename CTL>
	class VecsIteratorEntityBaseClass {
		friend class VecsIteratorBaseClass<P, ETL, CTL>;

	protected:
		VecsIteratorBaseClass<P, ETL, CTL>*	m_iter_ptr{nullptr};
		std::atomic<size_t>*				m_sizeE_ptr;			///< Size of THIS subiterator

	public:
		VecsIteratorEntityBaseClass(VecsIteratorBaseClass<P, ETL, CTL>* iter_ptr, bool is_end = false) noexcept : m_iter_ptr{ iter_ptr } {};

		virtual auto init() noexcept		-> void = 0;
		virtual auto data() noexcept		-> void = 0;
		virtual auto operator*() noexcept	-> typename VecsIteratorBaseClass<P, ETL, CTL>::reference = 0;		
		auto size() noexcept				-> size_t;	///< Total number of valid and invalid entities in the component table for type E

	};


	/**
	* \brief Return the total number of valid and invalid entities in the component table for type E.
	* \returns the total number of valid and invalid entities in the component table for type E.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorEntityBaseClass<P, ETL, CTL>::size() noexcept -> size_t {
		return m_sizeE_ptr->load();
	}


	//-------------------------------------------------------------------------
	//VecsIteratorEntity

	/**
	* \brief Iterator that iterates over a VecsComponentTable of type E and that is intested into components Ts
	*/

	template<typename P, typename E, typename ETL, typename CTL>
	class VecsIteratorEntity;

	template<typename P, typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	class VecsIteratorEntity<P, E, ETL, CTL<Cs...>> : public VecsIteratorEntityBaseClass<P, ETL, CTL<Cs...>> {

		using reference = VecsIteratorBaseClass<P, ETL, CTL<Cs...>>::reference;

		static inline VecsComponentTable<P, E> m_component_table;

	public:
		VecsIteratorEntity(VecsIteratorBaseClass<P, ETL, CTL<Cs...>>* iter_ptr, bool is_end = false) noexcept;

		auto init() noexcept		-> void;
		auto data() noexcept		-> void;
		auto operator*() noexcept	-> reference;
	};

	/**
	* \brief Constructor of class VecsIteratorEntity<P, E, Ts...>. Calls empty constructor of base class.
	*
	* \param[in] is_end If true, then the iterator belongs to an end-iterator.
	*/
	template<typename P, typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline VecsIteratorEntity<P, E, ETL, CTL<Cs...>>::VecsIteratorEntity(VecsIteratorBaseClass<P, ETL, CTL<Cs...>>* iter_ptr, bool is_end) noexcept
		: VecsIteratorEntityBaseClass<P, ETL, CTL<Cs...>>( iter_ptr, is_end) {

		this->m_sizeE_ptr = &m_component_table.m_data.m_size; ///< iterate over ALL entries, also the invalid ones!
	};

	template<typename P, typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline auto VecsIteratorEntity<P, E, ETL, CTL<Cs...>>::init() noexcept		-> void {
		this->m_iter_ptr->m_current_handle_ptr = m_component_table.handle_ptr(this->m_iter_ptr->m_current_index);
		this->m_iter_ptr->m_current_mutex_ptr = m_component_table.mutex_ptr(this->m_iter_ptr->m_current_index);
		this->m_iter_ptr->m_current_sizeE_ptr = this->m_sizeE_ptr;
		this->m_iter_ptr->m_current_sizeE = this->m_sizeE_ptr->load();
	}

	template<typename P, typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline auto VecsIteratorEntity<P, E, ETL, CTL<Cs...>>::data() noexcept		-> void {
		this->m_iter_ptr->m_current_handle_ptr = m_component_table.handle_ptr(this->m_iter_ptr->m_current_index);
		this->m_iter_ptr->m_current_mutex_ptr = m_component_table.mutex_ptr(this->m_iter_ptr->m_current_index);
	}

	/**
	* \brief Access operator retrieves all relevant components Ts from the entity it points to.
	* \returns all components Ts from the entity the iterator points to.
	*/
	template<typename P, typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline auto VecsIteratorEntity<P, E, ETL, CTL<Cs...>>::operator*() noexcept	-> reference {
		return std::forward_as_tuple(
			this->m_iter_ptr->m_current_handle_ptr == nullptr ? VecsHandleTemplate<P>{} : *this->m_iter_ptr->m_current_handle_ptr
			, m_component_table.component<Cs>(this->m_iter_ptr->m_current_index)...);
	};




	//-------------------------------------------------------------------------
	//Functor for for_each looping

	/**
	* \brief General functor type that can hold any function, and depends on a number of component types.
	*
	* Used in for_each to iterate over entities and call the functor for each entity.
	*/
	template<typename P, typename C>
	struct Functor;

	template<typename P, template<typename...> typename Seq, typename... Cs>
	struct Functor<P, Seq<Cs...>> {
		using type = void(VecsHandleTemplate<P>, Cs&...);	///< Arguments for the functor
	};


	//-------------------------------------------------------------------------
	//VecsRangeBaseClass

	/**
	* \brief Range for iterating over all entities that contain the given component types.
	*/
	template<typename P, typename ETL, typename CTL>
	class VecsRangeBaseClass {
		VecsIteratorBaseClass<P, ETL,CTL> m_begin;
		VecsIteratorBaseClass<P, ETL,CTL> m_end;

		public:
			VecsRangeBaseClass() noexcept : m_begin{ false }, m_end{ true } {
				//std::cout << "VecsRangeBaseClass<P, ETL,CTL>: " << typeid(ETL).name() << std::endl << std::endl;
			};
			VecsRangeBaseClass(VecsIteratorBaseClass<P, ETL, CTL>& begin, VecsIteratorBaseClass<P, ETL, CTL>& end) noexcept : m_begin{ begin }, m_end{ end } {};
			VecsRangeBaseClass(const VecsRangeBaseClass<P, ETL, CTL>& rhs) noexcept : m_begin{ rhs.m_begin }, m_end{ rhs.m_end } {};

			auto operator=(const VecsRangeBaseClass<P, ETL, CTL>& rhs) noexcept -> VecsIteratorBaseClass<P, ETL, CTL> {
				m_begin = rhs.m_begin;
				m_end = rhs.m_end;
				return *this;
			}

			auto split(size_t N) noexcept {
				std::pmr::vector<VecsRangeBaseClass<P,ETL,CTL>> result;	///< Result vector
				result.reserve(N);							///< Need exactly N slots
				size_t remain = m_begin.size();				///< Remaining entities
				size_t num = remain / N;					///< Put the same number in each slot
				if (num * N < remain) ++num;				///< We might need one more per slot
				auto b = m_begin;							///< Begin iterator
				while (remain > 0 && b != m_end) {			///< While there are remaining entities
					if (remain > num) {						///< At least two slots left
						size_t delta = (remain > num ? num : remain);
						VecsIteratorBaseClass<P,ETL,CTL> e = b + delta;
						result.emplace_back(VecsRangeBaseClass<P,ETL,CTL>(b, e));
						remain -= delta;
						b = e + 1;
					}
					else {													///< One last slot left
						result.emplace_back(VecsRangeBaseClass<P,ETL,CTL>(b, m_end));	///< Range lasts to the endf
						remain = 0;
					}
				};
				return result;
			}

			inline auto for_each(std::function<typename Functor<P, CTL>::type> f, bool sync = true) -> void {
				auto b = begin();
				auto e = end();
				for (; b != e; ++b) {
					if( sync ) VecsWriteLock lock(b.mutex_ptr());		///< Write lock
					if (!b.has_value()) continue;
					std::apply(f, *b);					///< Run the function on the references
				}
			}

			auto begin() noexcept	->	VecsIteratorBaseClass<P, ETL,CTL> {
				return m_begin;
			}

			auto end() noexcept		->	VecsIteratorBaseClass<P, ETL,CTL> {
				return m_end;
			}
	};


	/**
	* \brief Range over a list of entity types. In this case, entity types are taken as is, and are NOT expanded with the tag map.
	*/
	template<typename P, typename ETL>
	requires (is_entity_type_list<P, ETL>::value)
	class VecsRangeTemplate<P, ETL> : public VecsRangeBaseClass< P, ETL, it_CTL_entity_list<P, ETL> > {};

	/**
	* \brief Range over a set of entity types. Entity types are expanded using the tag map.
	*/
	template<typename P, typename... Es>
	requires (sizeof...(Es) > 0 && are_entity_types<P, Es...>)
	class VecsRangeTemplate<P, Es...> : public VecsRangeBaseClass< P, expand_tags< P, vtll::tl<Es...> >, it_CTL_entity_types<P, Es...> > {};

	/**
	* \brief Range over a set of entities that contain all given component types.
	*/
	template<typename P, typename... Cs>
	requires (sizeof...(Cs) > 0 && are_component_types<P, Cs...>)
	class VecsRangeTemplate<P, Cs...> : public VecsRangeBaseClass< P, vtll::filter_have_all_types< VecsEntityTypeList<P>, vtll::tl<Cs...> >, it_CTL_types<P, Cs...> > {};

	/**
	* \brief Iterator for given entity tape that has all tags.
	*/
	template<typename P, typename E, typename... Ts>
	requires (is_entity_type<P, E> && (sizeof...(Ts) > 0) && are_entity_tags<P, Ts...>)
	class VecsRangeTemplate<P, E, Ts...> : public VecsRangeBaseClass< P, vtll::filter_have_all_types< expand_tags<P, vtll::tl<E>>, vtll::tl<Ts...> >, it_CTL_entity_tags<P, E> > {};

	/**
	* \brief Range over all entity types in VECS. This includes all tag variants.
	*/
	template<typename P>
	class VecsRangeTemplate<P> : public VecsRangeBaseClass < P, VecsEntityTypeList<P>, it_CTL_all_entities<P> > {};


	//-------------------------------------------------------------------------
	//VecsRangeBaseClass - left over

	/**
	* \brief Constructor of class VecsIteratorBaseClass. If its not an end iterator then it also
	* creates subiterators for all entity types that have all component types.
	* 
	* \param[in] is_end If true then this iterator is an end iterator.
	*/
	template<typename P, typename ETL, typename CTL>
	inline VecsIteratorBaseClass<P, ETL, CTL>::VecsIteratorBaseClass(bool is_end) noexcept : m_is_end{ is_end } {

		//std::cout << "VecsIteratorBaseClass<P, ETL,CTL>(): " << typeid(ETL).name() << std::endl << std::endl;

		vtll::static_for<size_t, 0, vtll::size<ETL>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<ETL, i>;
				m_dispatch[i] = std::make_unique<VecsIteratorEntity<P, type, ETL, CTL>>(this, is_end);
				auto size = m_dispatch[i]->size();
				m_size += size;
				if (!is_end && m_current_iterator == i && size == 0 && i + 1 < vtll::size<ETL>::value) {
					m_current_iterator++;
				}
			}
		);

		if (is_end) {
			m_current_iterator = static_cast<decltype(m_current_iterator)>(m_dispatch.size() - 1);
			m_current_index = static_cast<decltype(m_current_index)>(m_dispatch[m_current_iterator]->size());
		}
		else {
			m_dispatch[m_current_iterator]->init();
		}
	};

}


#endif

