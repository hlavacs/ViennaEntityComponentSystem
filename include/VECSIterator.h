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
			
			size_t					m_size{ 0 };			///< Number of entities covered by the iterator (can change due to multithreading)
			bool					m_is_end{ false };		///< True if this is an end iterator (for stopping the loop)

		public:
			using value_type		= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandleT<P>>, CTL > >;
			using reference			= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandleT<P>&>, vtll::to_ref<CTL> > >;
			using pointer			= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandleT<P>*>, vtll::to_ptr<CTL> > >;
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

			inline auto handle() noexcept		-> VecsHandleT<P>&;			///< Return handle ref of the current entity
			inline auto mutex_ptr() noexcept	-> std::atomic<uint32_t>*;	///< Return pointer to the mutex of this entity
			inline auto is_valid() noexcept		-> bool;					///< Is currently pointing to a valid entity
			
			inline auto operator*() noexcept		-> reference;							///< Access the data
			inline auto operator++() noexcept		-> VecsIteratorBaseClass<P, ETL,CTL>&;	///< Increase by 1
			inline auto operator++(int) noexcept	-> VecsIteratorBaseClass<P, ETL,CTL>&;	///< Increase by 1
			inline auto size() noexcept				-> size_t;								///< Number of valid entities
			inline auto sizeE() noexcept			-> size_t;
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
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::is_valid() noexcept	-> bool {
		return handle().is_valid();
	}

	/**
	* \brief Retrieve the handle of the current entity.
	* \returns the handle of the current entity.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::handle() noexcept	-> VecsHandleT<P>& {
		assert(!m_is_end);
		return m_dispatch[m_current_iterator]->handle();
	}

	/**
	* \brief Retrieve the handle of the current entity.
	* \returns the handle of the current entity.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::mutex_ptr() noexcept	-> std::atomic<uint32_t>* {
		assert(!m_is_end);
		return m_dispatch[m_current_iterator]->mutex_ptr();
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
		++(*m_dispatch[m_current_iterator]);

		if ( m_current_index >= sizeE()) {
			size_t size = 0;
			while( size == 0 && m_current_iterator.value < m_dispatch.size() - 1) {
				++m_current_iterator;
				m_current_index = 0;
				size = sizeE();
			 } 
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
			size_t num = std::max((size_t)(sizeE() - m_current_index), (size_t)0); ///< So much can we consume
			num = std::min(num, left);	///< That is what we consume

			left -= num;	///< Consume these numbers
			m_current_index += static_cast<decltype(m_current_index.value)>(num); ///< Increase index by this

			if (m_current_index >= sizeE()) { ///< Are we at the end of the subiterator?
				size_t size = 0;
				while (size == 0 && m_current_iterator.value < m_dispatch.size() - 1) { ///< Yes, find next with non zero numbers
					++m_current_iterator;	///< Goto next
					m_current_index = 0;	///< Start at beginning
					size = sizeE();			///< Do we have entities?
				}
				if(sizeE() == 0) {	///< Nothing left to iterate over
					left = 0;
				}
			}
		}
		m_dispatch[m_current_iterator]->m_current_index = m_current_index;
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
		return	m_current_iterator == v.m_current_iterator && m_current_index == v.m_current_index;
	}

	/**
	* \brief Get the total number of all entities that are covered by this iterator.
	* This is simply the sum of the entities covered by the subiterators.
	* This number is set when the iterator is created.
	*
	* \returns the total number of entities that have all components Ts.
	*/
	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::size() noexcept		-> size_t {
		return m_size;
	}

	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorBaseClass<P, ETL, CTL>::sizeE() noexcept	-> size_t {
		return m_dispatch[m_current_iterator]->sizeE();
	}


	//----------------------------------------------------------------------------------------------
	//VecsIterator - derived from VecsIteratorBaseClass


	/**
	* \brief Iterator for given list of entity types. Entity types are taken as is, and NOT extended by possible tags from the tags map.
	*/
	template<typename P, typename ETL>
	using it_CTL_entity_list = vtll::remove_types< vtll::intersection< ETL >, typename VecsRegistryBaseClass<P>::entity_tag_list >;

	template<typename P, typename ETL>
	requires (is_entity_type_list<P, ETL>::value)
	class VecsIteratorT<P, ETL> : public VecsIteratorBaseClass< P, ETL, it_CTL_entity_list<P, ETL> > {
	public:
		using component_types = it_CTL_entity_list<P, ETL> ;
		VecsIteratorT(bool end = false) noexcept : VecsIteratorBaseClass< P, ETL, it_CTL_entity_list<P, ETL> >(end) {};
	};

	/**
	* \brief Iterator for given entity types.
	* 
	* Entity types are the given entity types, but also their extensions using the tag map.
	* Component types are the intersection of the entity types.
	*/
	template<typename P, typename... Es>
	using it_ETL_entity_types = expand_tags< typename VecsRegistryBaseClass<P>::entity_tag_map, vtll::tl<Es...> >;

	template<typename P, typename... Es>
	using it_CTL_entity_types = vtll::remove_types< vtll::intersection< vtll::tl<Es...> >, typename VecsRegistryBaseClass<P>::entity_tag_list >;

	template<typename P, typename... Es>
	requires (sizeof...(Es) > 0 && are_entity_types<P, Es...>)
	class VecsIteratorT<P, Es...> : public VecsIteratorBaseClass< P, it_ETL_entity_types<P, Es...>, it_CTL_entity_types<P, Es...> > {
	public:
		using component_types = it_CTL_entity_types<P, Es...>;
		VecsIteratorT(bool end = false) noexcept : VecsIteratorBaseClass< P, it_ETL_entity_types<P, Es...>, it_CTL_entity_types<P, Es...> >(end) {};
	};

	/**
	* \brief Iterator for given component types.
	*/
	template<typename P, typename... Cs>
	using it_ETL_types = vtll::filter_have_all_types< typename VecsRegistryBaseClass<P>::entity_type_list, vtll::tl<Cs...> >;

	template<typename P, typename... Cs>
	using it_CTL_types = vtll::remove_types< vtll::tl<Cs...>, typename VecsRegistryBaseClass<P>::entity_tag_list >;

	template<typename P, typename... Cs>
	requires (sizeof...(Cs) > 0 && are_component_types<P, Cs...>)
	class VecsIteratorT<P, Cs...> : public VecsIteratorBaseClass< P, it_ETL_types<P, Cs...>, it_CTL_types<P, Cs...> > {
	public:
		using component_types = it_CTL_types<P, Cs...>;
		VecsIteratorT(bool end = false) noexcept : VecsIteratorBaseClass< P, it_ETL_types<P, Cs...>, it_CTL_types<P, Cs...> >(end) {};
	};

	/**
	* \brief Iterator for given entity type that has all tags.
	*/
	template<typename P, typename E, typename... Ts>
	using it_ETL_entity_tags = vtll::filter_have_all_types< expand_tags<typename VecsRegistryBaseClass<P>::entity_tag_map, vtll::tl<E>>, vtll::tl<Ts...> >;

	template<typename P, typename E, typename... Ts>
	using it_CTL_entity_tags = vtll::remove_types< E, typename VecsRegistryBaseClass<P>::entity_tag_list >;

	template<typename P, typename E, typename... Ts>
	requires (is_entity_type<P, E> && (sizeof...(Ts) > 0) && are_entity_tags<P, Ts...>)
	class VecsIteratorT<P, E,Ts...> : public VecsIteratorBaseClass< P, it_ETL_entity_tags<P, E, Ts...>, it_CTL_entity_tags<P, E, Ts...> > {
	public:
		using component_types = it_CTL_entity_tags<P, E, Ts...>;
		VecsIteratorT(bool end = false) noexcept : VecsIteratorBaseClass< P, it_ETL_entity_tags<P, E, Ts...>, it_CTL_entity_tags<P, E, Ts...> >(end) {};
	};

	/**
	* \brief Iterator for all entities.
	*/
	template<typename P>
	using it_CTL_all_entities = vtll::remove_types< vtll::intersection< typename VecsRegistryBaseClass<P>::entity_type_list >, typename VecsRegistryBaseClass<P>::entity_tag_list >;

	template<typename P>
	class VecsIteratorT<P> : public VecsIteratorBaseClass<P, typename VecsRegistryBaseClass<P>::entity_type_list, it_CTL_all_entities<P> > {
	public:
		using component_types = it_CTL_all_entities<P>;
		VecsIteratorT(bool end = false) noexcept :  VecsIteratorBaseClass<P, typename VecsRegistryBaseClass<P>::entity_type_list, it_CTL_all_entities<P> >(end) {};
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
		std::atomic<size_t>*	m_sizeE_ptr{ nullptr };
		size_t					m_sizeE{0};
		table_index_t			m_current_index{0};
		VecsHandleT<P>*			m_current_handle_ptr{ nullptr };	///< Current handle the subiterator points to
		std::atomic<uint32_t>*	m_current_mutex_ptr{ nullptr };		///< Current mutex the subiterator points to

	public:
		VecsIteratorEntityBaseClass(bool is_end = false) noexcept {};

		virtual auto sizeE_ptr() noexcept	-> std::atomic<size_t>* { return m_sizeE_ptr; };	///< Total number of valid and invalid entities in the component table for type E
		virtual auto sizeE() noexcept		-> size_t { return m_sizeE; };						///< Total number of valid and invalid entities in the component table for type E
		auto handle() noexcept				-> VecsHandleT<P>&;
		virtual auto handle_ptr() noexcept	-> VecsHandleT<P>* = 0;
		virtual auto mutex_ptr() noexcept	-> std::atomic<uint32_t>* = 0;
		virtual auto operator++() noexcept	-> void = 0;
		virtual auto operator*() noexcept	-> typename VecsIteratorBaseClass<P, ETL, CTL>::reference = 0;
	};

	template<typename P, typename ETL, typename CTL>
	inline auto VecsIteratorEntityBaseClass<P, ETL, CTL>::handle() noexcept -> VecsHandleT<P>& {
		if (this->m_current_handle_ptr != nullptr) return *m_current_handle_ptr;
		return *handle_ptr();
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
		using pointer = VecsIteratorBaseClass<P, ETL, CTL<Cs...>>::pointer;

		static const size_t c_segment_size = VecsComponentTable<P,E>::c_segment_size;
		using layout_type_t = typename VecsComponentTable<P, E>::layout_type_t;

		pointer m_value_ptr;

		static inline VecsComponentTable<P, E> m_component_table;

	public:
		VecsIteratorEntity(bool is_end = false) noexcept;

		auto handle_ptr() noexcept	-> VecsHandleT<P>*;
		auto mutex_ptr() noexcept	-> std::atomic<uint32_t>*;
		auto operator++() noexcept	-> void;
		auto operator*() noexcept	-> reference;
	};

	/**
	* \brief Constructor of class VecsIteratorEntity<P, E, Ts...>. Calls empty constructor of base class.
	*
	* \param[in] is_end If true, then the iterator belongs to an end-iterator.
	*/
	template<typename P, typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline VecsIteratorEntity<P, E, ETL, CTL<Cs...>>::VecsIteratorEntity(bool is_end) noexcept
		: m_value_ptr{}, VecsIteratorEntityBaseClass<P, ETL, CTL<Cs...>>(is_end) {

		this->m_sizeE_ptr = &m_component_table.m_data.m_size; ///< iterate over ALL entries, also the invalid ones!
		this->m_sizeE = this->m_sizeE_ptr->load();
	};

	template<typename P, typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline auto VecsIteratorEntity<P, E, ETL, CTL<Cs...>>::handle_ptr() noexcept -> VecsHandleT<P>* {
		if(this->m_current_handle_ptr == nullptr) this->m_current_handle_ptr = m_component_table.handle_ptr(this->m_current_index);
		return this->m_current_handle_ptr;
	}

	template<typename P, typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline auto VecsIteratorEntity<P, E, ETL, CTL<Cs...>>::mutex_ptr() noexcept -> std::atomic<uint32_t>* {
		if(this->m_current_mutex_ptr == nullptr) this->m_current_mutex_ptr = m_component_table.mutex_ptr(this->m_current_index);
		return this->m_current_mutex_ptr;
	}

	template<typename P, typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline auto VecsIteratorEntity<P, E, ETL, CTL<Cs...>>::operator++() noexcept	-> void {
		this->m_current_handle_ptr = nullptr;
		this->m_current_mutex_ptr = nullptr;
		++(this->m_current_index);
	}

	/**
	* \brief Access operator retrieves all relevant components Ts from the entity it points to.
	* \returns all components Ts from the entity the iterator points to.
	*/
	template<typename P, typename E, typename ETL, template<typename...> typename CTL, typename... Cs>
	inline auto VecsIteratorEntity<P, E, ETL, CTL<Cs...>>::operator*() noexcept	-> reference {
		if constexpr (layout_type_t::value == VECS_LAYOUT_COLUMN::value) {

		}
		m_value_ptr = std::forward_as_tuple(handle_ptr(), &m_component_table.component<Cs>(this->m_current_index)...);
		return vtll::ptr_to_ref_tuple(m_value_ptr);
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
		using type = void(VecsHandleT<P>&, Cs&...);	///< Arguments for the functor
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
					if( sync ) VecsWriteLock::lock(b.mutex_ptr());		///< Write lock
					if (!b.is_valid()) {
						if (sync) VecsWriteLock::unlock(b.mutex_ptr());	///< unlock
						continue;
					}
					std::apply(f, *b);			///< Run the function on the references
					if (sync) VecsWriteLock::unlock(b.mutex_ptr());		///< unlock
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
	class VecsRangeT<P, ETL> : public VecsRangeBaseClass< P, ETL, it_CTL_entity_list<P, ETL> > {};

	/**
	* \brief Range over a set of entity types. Entity types are expanded using the tag map.
	*/
	template<typename P, typename... Es>
	requires (sizeof...(Es) > 0 && are_entity_types<P, Es...>)
	class VecsRangeT<P, Es...> : public VecsRangeBaseClass< P, it_ETL_entity_types<P, Es...>, it_CTL_entity_types<P, Es...> > {};

	/**
	* \brief Range over a set of entities that contain all given component types.
	*/
	template<typename P, typename... Cs>
	requires (sizeof...(Cs) > 0 && are_component_types<P, Cs...>)
	class VecsRangeT<P, Cs...> : public VecsRangeBaseClass< P, it_ETL_types<P, Cs...>, it_CTL_types<P, Cs...> > {};

	/**
	* \brief Iterator for given entity type that has all tags.
	*/
	template<typename P, typename E, typename... Ts>
	requires (is_entity_type<P, E> && (sizeof...(Ts) > 0) && are_entity_tags<P, Ts...>)
	class VecsRangeT<P, E, Ts...> : public VecsRangeBaseClass< P, it_ETL_entity_tags<P, E, Ts...>, it_CTL_entity_tags<P, E, Ts...> > {};

	/**
	* \brief Range over all entity types in VECS. This includes all tag variants.
	*/
	template<typename P>
	class VecsRangeT<P> : public VecsRangeBaseClass < P, typename VecsRegistryBaseClass<P>::entity_type_list, it_CTL_all_entities<P> > {};


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
				m_dispatch[i] = std::make_unique<VecsIteratorEntity<P, type, ETL, CTL>>(is_end);
				auto size = m_dispatch[i]->sizeE();
				m_size += size;
				if (!is_end && m_current_iterator == i && size == 0 && i + 1 < vtll::size<ETL>::value) {
					m_current_iterator++;
				}
			}
		);

		if (is_end) {
			m_current_iterator = static_cast<decltype(m_current_iterator)>(m_dispatch.size() - 1);
			m_current_index = static_cast<decltype(m_current_index)>(m_dispatch[m_current_iterator]->sizeE());
		}

	};

}


#endif

