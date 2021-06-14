#ifndef VECSITERATOR_H
#define VECSITERATOR_H

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

namespace vecs {

	//-------------------------------------------------------------------------
	//iterator

	template<typename P, typename CTL>
	class VecsIteratorT {

		template<typename P, typename ETL, typename CTL> friend class VecsRangeBaseClass;

	public:
		using value_type = vtll::to_tuple< vtll::cat< vtll::tl<std::atomic<uint32_t>*, VecsHandleT<P>>, CTL > >;					///< Value type
		using reference = vtll::to_tuple< vtll::cat< vtll::tl<std::atomic<uint32_t>*&, VecsHandleT<P>&>, vtll::to_ref<CTL> > >;	///< Reference type
		using pointer = vtll::to_tuple< vtll::cat< vtll::tl<std::atomic<uint32_t>**, VecsHandleT<P>*>, vtll::to_ptr<CTL> > >;	///< Pointer type
		using iterator_category = std::forward_iterator_tag;				///< Forward iterator
		using difference_type = int64_t;									///< Difference type
		using accessor = std::function<pointer(table_index_t)>;

	protected:
		type_index_t	m_type{};				///< entity type that this iterator iterates over
		size_t			m_size{ 0 };			///< Size of the components, m_current must be smaller than this
		size_t			m_bit_mask{ 0 };		///< Size of table segments - 1 , use as bit mask 
		size_t			m_row_size{ 0 };		///< Size of a row for row layout, or zero for column layout
		table_index_t	m_current{ 0 };			///< Current index in the VecsComponentTable<E>
		pointer			m_pointers;				///< Pointers to the components
		accessor		m_accessor;				///< Get pointers to the components

	public:
		VecsIteratorT() noexcept = default;									///< Empty constructor is required

		template<typename E>
		VecsIteratorT(const VecsComponentTable<P, E>& tab, size_t current) noexcept;		///< Constructor
		VecsIteratorT(const VecsIteratorT<P, CTL>& v) noexcept = default;		///< Copy constructor
		VecsIteratorT(VecsIteratorT<P, CTL>&& v) noexcept = default;			///< Move constructor

		auto operator=(const VecsIteratorT<P, CTL>& v) noexcept	-> VecsIteratorT<P, CTL> & = default;	///< Copy
		auto operator=(VecsIteratorT<P, CTL>&& v) noexcept		-> VecsIteratorT<P, CTL> & = default;	///< Move

		auto operator*() noexcept		-> reference;				///< Access the data
		auto operator*() const noexcept	-> reference;				///< Access the const data
		auto operator->() noexcept { return operator*(); };			///< Access
		auto operator->() const noexcept { return operator*(); };	///< Access

		auto operator++() noexcept		-> VecsIteratorT<P, CTL>&;				///< Increase by 1
		auto operator++(int) noexcept	-> VecsIteratorT<P, CTL>;				///< Increase by 1
		auto operator+=(difference_type N) noexcept	-> VecsIteratorT<P, CTL>&;	///< Increase by N
		auto operator+(difference_type N) noexcept	-> VecsIteratorT<P, CTL>;	///< Increase by N

		bool operator!=(const VecsIteratorT<P, CTL>& v) noexcept;	///< Unequal

		friend auto operator==(const VecsIteratorT<P, CTL>& v1, const VecsIteratorT<P, CTL>& v2) noexcept -> bool {	///< Equal
			return v1.m_type == v2.m_type && v1.m_current == v2.m_current;
		}
	};


	///< Constructor
	template<typename P, typename CTL>
	template<typename E>
	inline VecsIteratorT<P, CTL>::VecsIteratorT(const VecsComponentTable<P, E>& tab, size_t current) noexcept
		: m_type{ type_index_t{ vtll::index_of<typename VecsRegistryBaseClass<P>::entity_type_list, E>::value } }
		, m_size{ tab.size() }
		, m_bit_mask{ VecsComponentTable<P, E>::c_segment_size - 1 }
		, m_row_size{ VecsComponentTable<P, E>::c_row_size }
		, m_current{ current }
		, m_accessor{ std::bind(VecsComponentTable<P, E>::template accessor<CTL>, std::placeholders::_1) } {

		m_pointers = m_current.value < m_size ? m_accessor(m_current) : pointer{};
	};

	///< Access the data
	template<typename P, typename CTL>
	inline auto VecsIteratorT<P, CTL>::operator*() noexcept	-> reference {
		return ptr_to_ref_tuple(m_pointers);
	}

	///< Access the const data
	template<typename P, typename CTL>
	inline auto VecsIteratorT<P, CTL>::operator*() const noexcept ->reference {
		return ptr_to_ref_tuple(m_pointers);
	}

	///< Increase by 1
	template<typename P, typename CTL>
	inline auto VecsIteratorT<P, CTL>::operator++() noexcept	-> VecsIteratorT<P, CTL>& {
		m_current.value++;
		if ((m_current.value & m_bit_mask) == 0) {	//is at the start of a segment, so load pointers from table
			m_pointers = m_current.value < m_size ? m_accessor(m_current) : pointer{};
		}
		else {
			if (m_row_size > 0) {											///< If row layout
				vtll::static_for<size_t, 0, std::tuple_size_v<pointer> >(	///< Loop over all components
					[&](auto i) {
						using type = std::tuple_element_t<i, pointer>;
						auto& ptr = std::get<i>(m_pointers);
						ptr = (type)((char*)ptr + m_row_size);
					}
				);
			}
			else {
				vtll::static_for<size_t, 0, std::tuple_size_v<pointer> >(	///< Column layout, loop over all components
					[&](auto i) {
						std::get<i>(m_pointers)++;
					}
				);
			}
		}
		return *this;
	}

	///< Increase by 1
	template<typename P, typename CTL>
	inline auto VecsIteratorT<P, CTL>::operator++(int) noexcept	-> VecsIteratorT<P, CTL> {
		auto tmp = *this;
		operator++();
		return *this;
	}

	///< Increase by N
	template<typename P, typename CTL>
	inline auto VecsIteratorT<P, CTL>::operator+=(difference_type N) noexcept -> VecsIteratorT<P, CTL>& {
		m_current += N;
		m_pointers = m_current.value < m_size ? m_accessor(m_current) : pointer{};
		return *this;
	}

	///< Increase by N
	template<typename P, typename CTL>
	inline auto VecsIteratorT<P, CTL>::operator+(difference_type N) noexcept	-> VecsIteratorT<P, CTL> {
		m_current += N;
		m_pointers = m_current.value < m_size ? m_accessor(m_current) : pointer{};
		return *this;
	}

	template<typename P, typename CTL>
	inline auto VecsIteratorT<P, CTL>::operator!=(const VecsIteratorT<P, CTL>& v) noexcept -> bool {
		return m_type != v.m_type || m_current != v.m_current;
	}


	/**
	* \brief General functor type that can hold any function, and depends on a number of component types.
	*
	* Used in for_each to iterate over entities and call the functor for each entity.
	*/
	template<typename P, typename C>
	struct Functor;

	/**
	* \brief Specialization of primary Functor to get the types.
	*/
	template<typename P, template<typename...> typename Seq, typename... Cs>
	struct Functor<P, Seq<Cs...>> {
		using type = void(std::atomic<uint32_t>*&, VecsHandleT<P>&, Cs&...);	///< Arguments for the functor
	};


	template<typename P, typename ETL, typename CTL>
	class VecsRangeBaseClass {

		struct range_t {
			VecsIteratorT<P, CTL>	m_begin{};
			VecsIteratorT<P, CTL>	m_end{};
			size_t					m_size{0};

			range_t() = default;
			range_t(const VecsIteratorT<P, CTL>& b, const VecsIteratorT<P, CTL>& e) noexcept : m_begin{ b }, m_end{ e }, m_size{ e.m_current - b.m_current }  {};
			range_t(VecsIteratorT<P, CTL>&& b, VecsIteratorT<P, CTL>&& e) noexcept : m_begin{ b }, m_end{ e }, m_size{ e.m_current - b.m_current } {};
			range_t(const range_t&) = default;
			range_t(range_t&&) = default;

			auto begin() noexcept { return m_begin; };
			auto end() noexcept { return m_end; };
			auto size() const noexcept { return m_size; };
		};

		using view_type = decltype(std::views::join(std::declval<std::vector<range_t>&>()));
		view_type				m_view;
		size_t					m_size{0};
		std::vector<range_t>	m_ranges;

	public:
		VecsRangeBaseClass() noexcept = default;
		VecsRangeBaseClass(const VecsRangeBaseClass&) noexcept = default;
		VecsRangeBaseClass(VecsRangeBaseClass&&) noexcept = default;

		auto operator=(const VecsRangeBaseClass<P, ETL, CTL>& v) noexcept -> VecsRangeBaseClass<P, ETL, CTL> & = default;
		auto operator=(VecsRangeBaseClass<P, ETL, CTL>&& v) noexcept -> VecsRangeBaseClass<P, ETL, CTL> & = default;

		void compute_ranges() noexcept {
			m_ranges.reserve(vtll::size<ETL>::value);
			m_size = 0;
			vtll::static_for<size_t, 0, vtll::size<ETL>::value >(			///< Loop over all components
				[&](auto i) {
					using E = vtll::Nth_type<ETL, i>;
					VecsComponentTable<P, E> tab;
					auto range = range_t{ VecsIteratorT<P, CTL>(tab, 0), VecsIteratorT<P, CTL>(tab, tab.size()) };
					m_ranges.push_back(range);
					m_size += m_ranges.back().size();
				}
			);
		}

		auto begin() noexcept {
			if (m_ranges.size() == 0) { compute_ranges(); }
			m_view = std::views::join(m_ranges);
			return m_view.begin();
		}

		auto end() noexcept { 
			return m_view.end();
		}

		auto size() const noexcept { return m_size; }

		auto split(size_t N) noexcept {
			std::vector<VecsRangeBaseClass<P, ETL, CTL>> result;
			if (m_ranges.size() == 0) { compute_ranges(); }
			if (m_ranges.size() == 0) return result;

			VecsRangeBaseClass<P, ETL, CTL> new_vecs_range;

			size_t remain = m_size;					///< Remaining entities
			size_t num = remain / N;				///< Put the same number in each slot (except maybe last slot)
			if (num * N < remain) ++num;			///< We might need one more per slot
			size_t need = num;						///< Still need that many entities to complete the next slot

			for( auto& range : m_ranges ) {
				size_t available = range.size();			///< Still available in the current range
				size_t current = range.m_begin.m_current;	///< Index of current entity
				size_t take = 0;							///< Number of entities to take from the current range
				while (available > 0) {						///< Are there entities available in the current range?
					take = (need <= available) ? need : available;

					auto b = range.m_begin;			///< Copy begin and end iterators from range
					b.m_current = current;			///< Adjust begin and end indices
					auto e = range.m_end;
					e.m_current = current + take;
					new_vecs_range.m_ranges.push_back(range_t{ b, e });
					new_vecs_range.m_size += take;

					current += take;
					available -= take;
					need -= take;
					remain -= take;

					if (need == 0) {							///< Next slot is satisfied
						result.push_back(new_vecs_range);		///< Add range to the result
						if (remain > 0) {
							new_vecs_range = VecsRangeBaseClass<P, ETL, CTL>{}; ///< Clear the range
							need = result.size() < N - 1 ? num : remain;			///< Reset need for next slot
						}
					}
				}
			}
			return result;
		}

		inline auto for_each(std::function<typename Functor<P, CTL>::type> f, bool sync = true) noexcept -> void {
			auto b = begin();
			auto e = end();
			for (; b != e; ++b) {
				auto tuple = *b;
				auto& mutex_ptr = std::get<0>(tuple);
				if (sync) VecsWriteLock::lock(mutex_ptr);		///< Write lock
				if (!std::get<1>(tuple).is_valid()) {
					if (sync) VecsWriteLock::unlock(mutex_ptr);	///< unlock
					continue;
				}
				std::apply(f, tuple);			///< Run the function on the references
				if (sync) VecsWriteLock::unlock(mutex_ptr);		///< unlock
			}
		}

	};


	/**
	* \brief Iterator for given list of entity types. Entity types are taken as is, and NOT extended by possible tags from the tags map.
	*/
	template<typename P, typename ETL>
	using it_CTL_entity_list = vtll::remove_types< vtll::intersection< ETL >, typename VecsRegistryBaseClass<P>::entity_tag_list >;

	/**
	* \brief Range over a list of entity types. In this case, entity types are taken as is, and are NOT expanded with the tag map.
	*/
	template<typename P, typename ETL>
	requires (is_entity_type_list<P, ETL>::value)
		class VecsRangeT<P, ETL> : public VecsRangeBaseClass< P, ETL, it_CTL_entity_list<P, ETL> > {};


	/**
	* \brief Iterator for given entity types.
	*/
	template<typename P, typename... Es>
	using it_ETL_entity_types = expand_tags< typename VecsRegistryBaseClass<P>::entity_tag_map, vtll::tl<Es...> >;

	template<typename P, typename... Es>
	using it_CTL_entity_types = vtll::remove_types< vtll::intersection< vtll::tl<Es...> >, typename VecsRegistryBaseClass<P>::entity_tag_list >;

	/**
	* \brief Range over a set of entity types. Entity types are expanded using the tag map.
	*/
	template<typename P, typename... Es>
	requires (sizeof...(Es) > 0 && are_entity_types<P, Es...>)
		class VecsRangeT<P, Es...> : public VecsRangeBaseClass< P, it_ETL_entity_types<P, Es...>, it_CTL_entity_types<P, Es...> > {};


	/**
	* \brief Iterator for given component types.
	*/
	template<typename P, typename... Cs>
	using it_ETL_types = vtll::filter_have_all_types< typename VecsRegistryBaseClass<P>::entity_type_list, vtll::tl<Cs...> >;

	template<typename P, typename... Cs>
	using it_CTL_types = vtll::remove_types< vtll::tl<Cs...>, typename VecsRegistryBaseClass<P>::entity_tag_list >;

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
	using it_ETL_entity_tags = vtll::filter_have_all_types< expand_tags<typename VecsRegistryBaseClass<P>::entity_tag_map, vtll::tl<E>>, vtll::tl<Ts...> >;

	template<typename P, typename E, typename... Ts>
	using it_CTL_entity_tags = vtll::remove_types< E, typename VecsRegistryBaseClass<P>::entity_tag_list >;

	/**
	* \brief Iterator for given entity type that has all tags.
	*/
	template<typename P, typename E, typename... Ts>
	requires (is_entity_type<P, E> && (sizeof...(Ts) > 0) && are_entity_tags<P, Ts...>)
		class VecsRangeT<P, E, Ts...> : public VecsRangeBaseClass< P, it_ETL_entity_tags<P, E, Ts...>, it_CTL_entity_tags<P, E, Ts...> > {};


	/**
	* \brief Iterator for all entities.
	*/
	template<typename P>
	using it_CTL_all_entities = vtll::remove_types< vtll::intersection< typename VecsRegistryBaseClass<P>::entity_type_list >, typename VecsRegistryBaseClass<P>::entity_tag_list >;

	/**
	* \brief Range over all entity types in VECS. This includes all tag variants.
	*/
	template<typename P>
	class VecsRangeT<P> : public VecsRangeBaseClass < P, typename VecsRegistryBaseClass<P>::entity_type_list, it_CTL_all_entities<P> > {};

}


#endif

