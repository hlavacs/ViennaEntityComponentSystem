#ifndef VECSITERATOR_H
#define VECSITERATOR_H


namespace vecs {

	//-------------------------------------------------------------------------
	//iterator

	/**
	* \brief Base class for an iterator that iterates over a VecsComponentTable of any type
	* and that is intested into components Cs
	*
	* The iterator can be used to loop through the entities. By providing component types Cs..., only
	* entities are included that contain ALL component types.
	*/

	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		class VecsIterator {

		protected:
			using entity_types = typename std::conditional_t<are_component_types<Ts...>
				, vtll::filter_have_all_types< VecsEntityTypeList, vtll::type_list<Ts...> >
				, vtll::type_list<Ts...>
			>;

			using last_type = vtll::back<entity_types>;	///< last type for end iterator

			using array_type = typename std::conditional_t<are_component_types<Ts...>
				, std::array<std::unique_ptr<VecsIteratorEntityBaseClass<Ts...>>, vtll::size<entity_types>::value>
				, std::array<std::unique_ptr<VecsIteratorEntityBaseClass<>>, vtll::size<entity_types>::value>
			>;

			array_type m_dispatch;				///< Subiterators for each entity type E

			index_t m_current_iterator{ 0 };	///< Current iterator of type E that is used
			index_t m_current_index{ 0 };		///< Current index in the VecsComponentTable<E>
			bool	m_is_end{ false };			///< True if this is an end iterator (for stopping the loop)
			size_t	m_size{ 0 };				///< Number of entities max covered by the iterator

			VecsIterator(std::nullopt_t n) noexcept {};			///< Needed for derived iterator to call

		public:
			using value_type = std::tuple<VecsHandle, Ts...>; ///< Tuple containing all component values
			using ref_type = std::tuple<VecsHandle, Ts&...>;  ///< Tuple containing component references

			VecsIterator(bool is_end = false) noexcept;		///< Constructor that should be called always from outside
			VecsIterator(const VecsIterator& v) noexcept;		///< Copy constructor

			auto operator=(const VecsIterator& v) noexcept			-> VecsIterator<Ts...>;	///< Copy
			auto operator+=(size_t N) noexcept						-> VecsIterator<Ts...>;	///< Increase and set
			auto operator+(size_t N) noexcept						-> VecsIterator<Ts...>;	///< Increase
			auto operator!=(const VecsIterator<Ts...>& v) noexcept	-> bool;	///< Unqequal
			auto operator==(const VecsIterator<Ts...>& v) noexcept	-> bool;	///< Equal

			virtual auto handle() noexcept			-> VecsHandle;				///< Return handle of the current entity
			virtual auto mutex() noexcept			-> std::atomic<uint32_t>*;	///< Return poiter to the mutex of this entity
			virtual auto has_value() noexcept		-> bool;					///< Is currently pointint to a valid entity
			virtual	auto operator*() noexcept		-> ref_type;				///< Access the data
			virtual auto operator++() noexcept		-> VecsIterator<Ts...>&;	///< Increase by 1
			virtual auto operator++(int) noexcept	-> VecsIterator<Ts...>&;	///< Increase by 1
			virtual auto is_vector_end() noexcept	-> bool;					///< Is currently at the end of any sub iterator
			virtual auto size() noexcept			-> size_t;					///< Number of valid entities
	};


	//----------------------------------------------------------------------------------------------

	/**
	* \brief Iterator that iterates over a VecsComponentTable of type E
	* and that is intested into components Cs
	*/
	template<typename... Cs>
	class VecsIteratorEntityBaseClass {
		template<typename... Ts> requires (are_component_types<Ts...> || are_entity_types<Ts...>) friend class VecsIterator;

	protected:
		index_t m_current_index{ 0 };		///< Current index in the VecsComponentTable<E>
		bool	m_is_end{ false };			///< True if this is an end iterator (for stopping the loop)
		size_t	m_sizeE{ 0 };				///< Number of entities of type E

	public:
		VecsIteratorEntityBaseClass(bool is_end = false) noexcept : m_is_end(is_end) {};
		virtual auto handle() noexcept		-> VecsHandle = 0;
		virtual auto mutex() noexcept		-> std::atomic<uint32_t>* = 0;
		virtual auto has_value() noexcept	-> bool = 0;
		virtual auto operator*() noexcept	-> typename VecsIterator<Cs...>::ref_type = 0;

		auto operator++() noexcept			-> void;
		auto operator++(int) noexcept		-> void;
		auto is_vector_end() noexcept		-> bool;
		auto size() noexcept				-> size_t;
	};


	//----------------------------------------------------------------------------------------------


	/**
	* \brief Copy-constructor of class VecsIterator
	*
	* \param[in] v Other Iterator that should be copied
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline VecsIterator<Ts...>::VecsIterator(const VecsIterator& v) noexcept : VecsIterator(v.m_is_end) {
		m_current_iterator = v.m_current_iterator;
		m_current_index = v.m_current_index;
		m_size = v.m_size;
		if (m_is_end) return;
		for (int i = 0; i < m_dispatch.size(); ++i) {
			m_dispatch[i]->m_current_index = v.m_dispatch[i]->m_current_index;
		}
	};

	/**
	* \brief Determines whether the iterator currently points to avalid entity.
	* The call is dispatched to the respective sub-iterator that is currently used.
	*
	* \returns true if the iterator points to a valid entity.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::has_value() noexcept	-> bool {
		if (m_is_end || is_vector_end()) return false;
		return m_dispatch[m_current_iterator]->has_value();
	}

	/**
	* \brief Retrieve the handle of the current entity.
	* \returns the handle of the current entity.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::handle() noexcept	-> VecsHandle {
		return m_dispatch[m_current_iterator]->handle();
	}

	/**
	* \brief Retrieve the handle of the current entity.
	* \returns the handle of the current entity.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::mutex() noexcept	-> std::atomic<uint32_t>* {
		return m_dispatch[m_current_iterator]->mutex();
	}

	/**
	* \brief Copy assignment operator.
	*
	* \param[in] v The source iterator for the assignment.
	* \returns *this.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::operator=(const VecsIterator& v) noexcept	-> VecsIterator<Ts...> {
		m_current_iterator = v.m_current_iterator;
		m_current_index = v.m_current_index;
		m_size = v.m_size;
		for (int i = 0; i < m_dispatch.size(); ++i) {
			m_dispatch[i]->m_current_index = v.m_dispatch[i]->m_current_index;
		}
		return *this;
	}

	/**
	* \brief Get the values of the entity that the iterator is currently pointing to.
	*
	* \returns a tuple holding the components Cs of the entity the iterator is currently pointing to.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::operator*() noexcept	-> ref_type {
		return *(*m_dispatch[m_current_iterator]);
	};

	/**
	* \brief Goto next entity.
	*
	* \returns *this.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::operator++() noexcept		-> VecsIterator<Ts...>& {
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
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::operator++(int) noexcept	-> VecsIterator<Ts...>& {
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
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::operator+=(size_t N) noexcept -> VecsIterator<Ts...> {
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
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::operator+(size_t N) noexcept	-> VecsIterator<Ts...> {
		VecsIterator<Ts...> temp{ *this };
		temp += N;
		return temp;
	}

	/**
	* \brief Test for unequality
	*
	* \returns true if the iterators are not equal.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::operator!=(const VecsIterator<Ts...>& v) noexcept	-> bool {
		return !(*this == v);
	}

	/**
	* \brief Test for equality.
	*
	* \returns true if the iterators are equal.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::operator==(const VecsIterator<Ts...>& v) noexcept	-> bool {
		return	v.m_current_iterator == m_current_iterator && v.m_current_index == m_current_index;
	}

	/**
	* \brief Test whether the curerntly used sub-iterator is at its end point.
	* The call is dispatched to the current subiterator.
	*
	* \returns true if the current subiterator is at its end.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::is_vector_end() noexcept		-> bool {
		return m_dispatch[m_current_iterator]->is_vector_end();
	}

	/**
	* \brief Get the total number of all entities that are covered by this iterator.
	* This is simply the sum of the entities covered by the subiterators.
	* This number is set when the iterator is created.
	*
	* \returns the total number of entities that have all components Cs.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline auto VecsIterator<Ts...>::size() noexcept	-> size_t {
		return m_size;
	}


	//-------------------------------------------------------------------------
	//VecsIteratorEntityBaseClass

	/**
	* \brief Point to the next entity.
	*/
	template<typename... Cs>
	inline auto VecsIteratorEntityBaseClass<Cs...>::operator++() noexcept		-> void {
		if (!is_vector_end()) [[likely]] ++this->m_current_index;
	};

	/**
	* \brief Point to the next entity.
	*/
	template<typename... Cs>
	inline auto VecsIteratorEntityBaseClass<Cs...>::operator++(int) noexcept	-> void {
		if (!is_vector_end()) [[likely]] ++this->m_current_index;
	};

	/**
	* \brief Determine whether the iterator points beyond the last entity of its type E.
	* This means it has reached its end.
	* \returns true if the iterator points beyond its last entity.
	*/
	template<typename... Cs>
	inline auto VecsIteratorEntityBaseClass<Cs...>::is_vector_end() noexcept	-> bool {
		return this->m_current_index >= this->m_sizeE;
	};

	/**
	* \brief Return the number of entities covered by this iterator.
	* \returns the number of entities this iterator covers.
	*/
	template<typename... Cs>
	inline auto VecsIteratorEntityBaseClass<Cs...>::size() noexcept			-> size_t {
		return this->m_sizeE;
	}


	//-------------------------------------------------------------------------
	//VecsIteratorEntity

	/**
	* \brief Iterator that iterates over a VecsComponentTable of type E and that is intested into components Cs
	*/
	template<typename E, typename... Cs>
	class VecsIteratorEntity : public VecsIteratorEntityBaseClass<Cs...> {
	public:
		VecsIteratorEntity(bool is_end = false) noexcept;
		auto handle() noexcept			-> VecsHandle;
		auto mutex() noexcept			-> std::atomic<uint32_t>*;
		auto has_value() noexcept		-> bool;
		auto operator*() noexcept		-> typename VecsIterator<Cs...>::ref_type;
	};


	/**
	* \brief Constructor of class VecsIteratorEntity<E, Cs...>. Calls empty constructor of base class.
	*
	* \param[in] is_end If true, then the iterator belongs to an end-iterator.
	*/
	template<typename E, typename... Cs>
	inline VecsIteratorEntity<E, Cs...>::VecsIteratorEntity(bool is_end) noexcept : VecsIteratorEntityBaseClass<Cs...>() {
		this->m_sizeE = VecsComponentTable<E>().size(); ///< iterate over ALL entries, also the invalid ones!
		if (is_end) {
			this->m_current_index = static_cast<decltype(this->m_current_index)>(this->m_sizeE);
		}
	};

	/**
	* \brief Test whether the iterator points to a valid entity.
	* \returns true if the iterator points to a valid entity.
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorEntity<E, Cs...>::has_value() noexcept		-> bool {
		return VecsComponentTable<E>().handle(this->m_current_index).has_value();
	}

	/**
	* \returns the handle of the current entity.
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorEntity<E, Cs...>::handle() noexcept		-> VecsHandle {
		return VecsComponentTable<E>().handle(this->m_current_index);
	}

	/**
	* \returns the mutex of the current entity.
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorEntity<E, Cs...>::mutex() noexcept		-> std::atomic<uint32_t>* {
		return VecsComponentTable<E>().mutex(this->m_current_index);
	}

	/**
	* \brief Access operator retrieves all relevant components Cs from the entity it points to.
	* \returns all components Cs from the entity the iterator points to.
	*/
	template<typename E, typename... Cs>
	inline auto VecsIteratorEntity<E, Cs...>::operator*() noexcept		-> typename VecsIterator<Cs...>::ref_type {
		return std::forward_as_tuple(VecsComponentTable<E>().handle(this->m_current_index), VecsComponentTable<E>().component<Cs>(this->m_current_index)...);
	};


	//-------------------------------------------------------------------------
	//VecsRange

	/**
	* \brief Range for iterating over all entities that contain the given component types.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		class VecsRange {
		VecsIterator<Ts...> m_begin;
		VecsIterator<Ts...> m_end;

		public:
			VecsRange() noexcept : m_begin{ VecsIterator<Ts...>(false) }, m_end{ VecsIterator<Ts...>(true) } {};
			VecsRange(VecsIterator<Ts...>& begin, VecsIterator<Ts...>& end) noexcept : m_begin{ begin }, m_end{ end } {};
			VecsRange(const VecsRange<Ts...>& rhs) noexcept : m_begin{ rhs.m_begin }, m_end{ rhs.m_end } {};

			auto operator=(const VecsRange<Ts...>& rhs) noexcept -> VecsIterator<Ts...> {
				m_begin = rhs.m_begin;
				m_end = rhs.m_end;
				return *this;
			}

			auto split(size_t N) noexcept {
				std::pmr::vector<VecsRange<Ts...>> result;
				result.reserve(N);
				size_t remain = m_begin.size();
				size_t num = remain / N;
				if (num * N < remain) ++num;
				auto b = m_begin;
				while (remain > 0 && b != m_end) {
					if (remain > num) {
						size_t delta = (remain > num ? num : remain) - 1;
						VecsIterator<Ts...> e = b + delta;
						result.emplace_back(VecsRange(b, e));
						remain -= (delta + 1);
						b = e + 1;
					}
					else {
						result.emplace_back(VecsRange(b, m_end));
						remain = 0;
					}
				};
				return result;
			}

			auto begin() noexcept	->	VecsIterator<Ts...> {
				return m_begin;
			}

			auto end() noexcept		->	VecsIterator<Ts...> {
				return m_end;
			}
	};


	/**
	* \brief Constructor of class VecsIterator. If its not an end iterator then it also
	* creates subiterators for all entity types that have all component types.
	* \param[in] is_end If true then this iterator is an end iterator.
	*/
	template<typename... Ts>
	requires (are_component_types<Ts...> || are_entity_types<Ts...>)
		inline VecsIterator<Ts...>::VecsIterator(bool is_end) noexcept : m_is_end{ is_end } {
		if (is_end) {
			m_current_iterator = static_cast<decltype(m_current_iterator)>(m_dispatch.size() - 1);
			m_current_index = static_cast<decltype(m_current_index)>(VecsRegistry<last_type>().size());
		}

		vtll::static_for<size_t, 0, vtll::size<entity_types>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<entity_types, i>;
				m_dispatch[i] = std::make_unique<VecsIteratorEntity<type, Ts...>>(is_end);
				auto size = m_dispatch[i]->size();
				m_size += size;
				if (size == 0 && i + 1 < vtll::size<entity_types>::value) m_current_iterator++;
			}
		);
	};

}


#endif

