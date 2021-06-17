#ifndef VECSTABLE_H
#define VECSTABLE_H

#include <assert.h>
#include <memory_resource>
#include <shared_mutex>
#include <optional>
#include <array>
#include <algorithm>
#include "VTLL.h"
#include "VECSUtil.h"

namespace vecs {

	/**
	* \brief VecsTable is a data container similar to std::vector, but with additional properties
	* 
	* VecsTable has the following properties:
	* 1) It stores tuples of data, thus the result is a table.
	* 2) The memory layout can be row-oriented or column-oriented.
	* 3) It can grow even when used with multiple threads. This is achieved by storing data in segments,
	* which are accessed over via a std::vector of unique_ptr. New segments can simply be added to the 
	* std::vector, and no reallocation occurs if the std::vector has a large enough capacity.
	* If more segments are needed then multithreaded operations must be stopped to reallocate this std::vector. 
	* 
	* The number of items S per segment must be a power of 2 : N = 2^L. This way, random access to row K is esily achieved
	* by first right shift K >> L to get the index of the segment pointer, then use K & (N-1) to get the index within 
	* the segment.
	* 
	*/
	template<typename P, typename DATA, size_t N0 = 1<<10, bool ROW = true>
	class VecsTable {

		//friends
		template<typename P, typename E> friend class VecsRegistryTemplate;
		template<typename P, typename E> friend class VecsComponentTable;
		template<typename P, typename E, size_t I> friend class VecsComponentTableDerived;
		template<typename P, typename E, typename ETL, typename CTL> friend class VecsIteratorEntity;

		static_assert(std::is_default_constructible_v<DATA>, "Your components are not default constructible!");

		static const size_t N = vtll::smallest_pow2_leq_value< N0 >::value;								///< Force N to be power of 2
		static const size_t L = vtll::index_largest_bit< std::integral_constant<size_t,N> >::value - 1;	///< Index of largest bit in N
		static const uint64_t BIT_MASK = N - 1;		///< Bit mask to mask off lower bits to get index inside segment

		using tuple_value_t = vtll::to_tuple<DATA>;		///< Tuple holding the entries as value
		using tuple_ref_t	= vtll::to_ref_tuple<DATA>;	///< Tuple holding the entries as value
		using tuple_ptr_t	= vtll::to_ptr_tuple<DATA>;	///< Tuple holding references to the entries

		using array_tuple_t1 = std::array<tuple_value_t, N>;								///< ROW: an array of tuples
		using array_tuple_t2 = vtll::to_tuple<vtll::transform_size_t<DATA,std::array,N>>;	///< COLUMN: a tuple of arrays
		using array_tuple_t  = std::conditional_t<ROW, array_tuple_t1, array_tuple_t2>;		///< Memory layout of the table

		using array_ptr = std::shared_ptr<array_tuple_t>;
		using seg_vector_t = std::pmr::vector<std::atomic<array_ptr>>; ///< A seg_vector_t is a vector holding shared pointers to segments

		std::pmr::memory_resource*								m_mr;
		std::pmr::polymorphic_allocator<seg_vector_t>			m_allocator;		///< use this allocator
		std::atomic<std::shared_ptr<seg_vector_t>>				m_seg_vector;		///< Vector of shared ptrs to the segments
		std::atomic<size_t>										m_num_segments{0};	///< Current number of segments
		inline static thread_local std::pmr::vector<array_ptr>	m_reuse_segments; ///< Might allocate too much, so remember that
		std::atomic<size_t>										m_size = 0;			///< Number of rows in the table
		std::shared_mutex										m_mutex;			///< Mutex for reallocating the vector

	public:
		VecsTable(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept;
		~VecsTable() noexcept;

		inline size_t size() noexcept { return m_size.load(); };	///< \returns the current numbers of rows in the table

		//-------------------------------------------------------------------------------------------
		//read data

		template<size_t I, typename C = vtll::Nth_type<DATA, I>>
		inline auto component(table_index_t n) noexcept		-> C&;		///< \returns a pointer to a component

		template<size_t I, typename C = vtll::Nth_type<DATA,I>>
		inline auto component_ptr(table_index_t n) noexcept	-> C*;		///< \returns a pointer to a component

		inline auto tuple(table_index_t n) noexcept	-> tuple_ref_t;		///< \returns a tuple with copies of all components
		inline auto tuple_ptr(table_index_t n) noexcept	-> tuple_ptr_t;	///< \returns a tuple with pointers to all components
		
		//-------------------------------------------------------------------------------------------
		//add data

		template<typename... Cs>
		requires vtll::has_all_types<DATA, vtll::tl<std::decay_t<Cs>...>>::value
		inline auto push_back(Cs&&... data) noexcept			-> table_index_t;	///< Push new component data to the end of the table

		//-------------------------------------------------------------------------------------------
		//update data

		template<size_t I, typename C = vtll::Nth_type<DATA, I>>
		requires vtll::has_type<DATA, std::decay_t<C>>::value
		inline auto update(table_index_t n, C&& data) noexcept			-> bool;	///< Update a component  for a given row
		
		template<typename... Cs>
		requires vtll::has_all_types<DATA, vtll::tl<std::decay_t<Cs>...>>::value
		inline auto update(table_index_t n, Cs&&... data) noexcept -> bool;

		//-------------------------------------------------------------------------------------------
		//move and remove data

		inline auto pop_back()	noexcept									-> void { m_size--; }	///< Remove the last row
		inline auto clear() noexcept -> void { m_size = 0; };	///< Set the number if rows to zero - effectively clear the table
		inline auto move(table_index_t idst, table_index_t isrc) noexcept	-> bool;	///< Move contents of a row to another row
		inline auto swap(table_index_t n1, table_index_t n2) noexcept		-> bool;	///< Swap contents of two rows

		//-------------------------------------------------------------------------------------------
		//memory management

		inline auto reserve(size_t r) noexcept	-> bool;		///< Reserve enough memory ny allocating segments
		inline auto capacity(size_t r) noexcept	-> size_t;		///< Set new max capacity -> might reallocate the segment vector!
		inline auto compress() noexcept			-> void;		///< Deallocate unsused segements
	};


	/**
	* \brief Constructor of class VecsTable.
	* \param[in] r Max number of rows that can be stored in the table.
	* \param[in] mr Memory allocator.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline VecsTable<P, DATA, N0, ROW>::VecsTable(size_t r, std::pmr::memory_resource* mr) noexcept 
		: m_mr{ mr }, m_allocator{ mr }, m_seg_vector{ nullptr } {};

	/**
	* \brief Destructor of class VecsTable.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline VecsTable<P, DATA, N0, ROW>::~VecsTable() noexcept { clear(); };

	template<typename P, typename DATA, size_t N0, bool ROW>
	template<size_t I, typename C>
	inline auto VecsTable<P, DATA, N0, ROW>::component(table_index_t n) noexcept -> C& {
		return *component_ptr<I>(n);
	}

	/**
	* \brief Get a pointger to a particular component with index I.
	* \param[in] n Index to the entry.
	* \returns a pointer to the Ith component of entry n.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	template<size_t I, typename C>
	inline auto VecsTable<P, DATA, N0, ROW>::component_ptr(table_index_t n) noexcept -> C* {
		assert(n.value < size());
		auto vector_ptr{ m_seg_vector.load() };
		auto segment_ptr = ((*vector_ptr)[n >> L]).load();
		if constexpr (ROW) {
			return &std::get<I>((*segment_ptr)[n & BIT_MASK]);
		}
		else {
			return &std::get<I>(*segment_ptr) [n & BIT_MASK];
		}
	};

	/**
	* \brief Get a tuple with valuesof all components of an entry.
	* \param[in] n Index to the entry.
	* \returns a tuple with values of all components of entry n.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::tuple(table_index_t n) noexcept -> tuple_ref_t {
		return vtll::ptr_to_ref_tuple(tuple_ptr(n));
	};

	/**
	* \brief Get a tuple with references to all components of an entry.
	* \param[in] n Index to the entry.
	* \returns a tuple with references to all components of entry n.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::tuple_ptr(table_index_t n) noexcept -> tuple_ptr_t {
		assert(n.value < size());
		auto f = [&]<size_t... Is>(std::index_sequence<Is...>) {
			return std::make_tuple(component_ptr<Is>(n)...);
		};
		return f(std::make_index_sequence<vtll::size<DATA>::value>{});
	};

	/**
	* \brief Create a new entry at the end of the table.
	* Must be externally synchronized!
	* \returns the index of the new entry.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	template<typename... Cs>
	requires vtll::has_all_types<DATA, vtll::tl<std::decay_t<Cs>...>>::value
	inline auto VecsTable<P, DATA, N0, ROW>::push_back(Cs&&... data) noexcept -> table_index_t {
		auto idx = m_size.fetch_add(1);			///< Increase table size and get old size.
		if (!reserve(idx + 1)) {				///< Make sure there is enough space in the table
			m_size--;
			return table_index_t{};				///< If not return an invalid index.
		}
		if constexpr (sizeof...(Cs)>0) {
			(update<vtll::index_of<DATA,std::decay_t<Cs>>::value>(table_index_t{ idx }, std::forward<Cs>(data)), ...);
		}
		return table_index_t{ idx }; ///< Return index of new entry
	}

	/**
	* \brief Update the component with index I of an entry.
	* \param[in] n Index of entry holding the component.
	* \param[in] C Universal reference to the component holding the data.
	* \returns true if the operation was successful.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	template<size_t I, typename C>
	requires vtll::has_type<DATA, std::decay_t<C>>::value
	inline auto VecsTable<P, DATA, N0, ROW>::update(table_index_t n, C&& data) noexcept -> bool {
		if (n >= m_size) return false;
		if constexpr (std::is_move_assignable_v<C> && std::is_rvalue_reference_v<decltype(data)>) {
			component<I>(n) = std::move(data);
		} else if constexpr (std::is_copy_assignable_v<C>) {
			component<I>(n) = data;
		}

		return true;
	}

	/**
	* \brief Update all components of an entry.
	* \param[in] n Index of entry holding the component.
	* \param[in] C Universal reference to tuple holding the components with the data.
	* \returns true if the operation was successful.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	template<typename... Cs>
	requires vtll::has_all_types<DATA, vtll::tl<std::decay_t<Cs>...>>::value
	inline auto VecsTable<P, DATA, N0, ROW>::update(table_index_t n, Cs&&... data) noexcept -> bool {
		return (update<vtll::index_of<DATA,std::decay_t<Cs>>>(n, std::forward<Cs>(data)) && ... && true);
	}

	/**
	* \brief Move one row to another row.
	* \param[in] idst Index of destination row.
	* \param[in] isrc Index of source row.
	* \returns true if the operation was successful.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::move(table_index_t idst, table_index_t isrc) noexcept -> bool {
		if (idst >= m_size || isrc >= m_size) return false;
		auto src = tuple_ptr(isrc);
		auto dst = tuple_ptr(idst);
		vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) {
			using type = vtll::Nth_type<DATA,i>;
			if constexpr (std::is_move_assignable_v<type>) {
				*std::get<i>(dst) = std::move(*std::get<i>(src));
			}
			else if constexpr (std::is_copy_assignable_v<type>) {
				*std::get<i>(dst) = *std::get<i>(src);
			}
		});
		return true;
	}

	/**
	* \brief Swap the values of two rows.
	* \param[in] n1 Index of first row.
	* \param[in] n2 Index of second row.
	* \returns true if the operation was successful.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::swap(table_index_t idst, table_index_t isrc) noexcept -> bool {
		if (idst >= m_size || isrc >= m_size) return false;
		auto src = tuple_ptr(isrc);
		auto dst = tuple_ptr(idst);
		vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) {
			using type = vtll::Nth_type<DATA,i>;
			if constexpr (std::is_copy_assignable_v<type> || std::is_move_assignable_v<type>) {
				std::swap(*std::get<i>(dst), *std::get<i>(src));
			}
		});
		return true;
	}

	/**
	* \brief Allocate segements to make sure enough memory is available.
	* \param[in] r Number of entries to be stored in the table.
	* \returns true if the operation was successful.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::reserve(size_t r) noexcept -> bool {
		capacity(r);

		std::shared_lock lock(m_mutex);			///< Shared lock - other threads can enter this section as well

		auto vector_ptr{ m_seg_vector.load() };	///< Shared pointer to current segement ptr vector

		auto old_num = m_num_segments.load();
		auto new_num = ((r - 1ULL) >> L) + 1ULL;
		if (new_num <= old_num) return true;	///< are there already segments that we can use?

		array_ptr segment_ptr{ nullptr };
		array_ptr expected{ nullptr };
		m_reuse_segments.reserve(m_reuse_segments.size() + new_num - old_num);

		for (size_t i = old_num; i < new_num; ++i) {
			auto& current_ptr = (*vector_ptr)[i];			///< Previous pointer, if not nullptr then another thread beat us
			if (!current_ptr.load() && !segment_ptr) {		///< Still nullptr, and we do not have a leftover from prev loop -> get a segment
				if (m_reuse_segments.size() > 0) {			///< Is there a leftover in thread's reuse list?
					segment_ptr = m_reuse_segments.back();	///< Yes, then take it out
					m_reuse_segments.pop_back();
				}
				else {
					segment_ptr = std::make_shared<array_tuple_t>();	///< No - create a new segment
				}
			}

			if (current_ptr.compare_exchange_weak(expected, segment_ptr)) { ///< Only possible if old value is nullptr
				segment_ptr = nullptr;
			}
		}

		for (size_t i = new_num; segment_ptr && i < vector_ptr->size(); ++i ) { ///< Do we have a leftover from loop?
			auto& current_ptr = (*vector_ptr)[i];								///< 
			if (current_ptr.compare_exchange_weak(expected, segment_ptr)) {		///< Try to get rid of it in the remaining slots
				segment_ptr = nullptr;
			}
		}

		if (segment_ptr)  {		///< if there is still a leftover, save it in the thread's reuse list
			m_reuse_segments.push_back(segment_ptr);
		}

		bool flag{ false };
		do {
			flag = m_num_segments.compare_exchange_weak(old_num, new_num); ///< try to store new value
			if (!flag) {								///< If it did not work then another thread has beaten us
				old_num = m_num_segments.load();		///< Can we raise the value still?
				if (old_num >= new_num) flag = true;	///< If the other thread's value is larger -> quit
			}
		} while (!flag);
			
		return true;
	}

	/**
	* \brief Set capacity of the segment table. This might reallocate the whole vector.
	* No parallel processing is allowed when calling this function!
	* \param[in] r Max number of entities to be allowed in the table.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::capacity(size_t r) noexcept -> size_t {
		auto vector_ptr{ m_seg_vector.load() };

		if (r == 0) return !vector_ptr ? 0 : vector_ptr->size() * N;

		if (!vector_ptr || r > vector_ptr->size() * N) {	///< is there enough space in the table?

			std::lock_guard lock(m_mutex);					///< Stop all other accesses to the vector of pointers

			vector_ptr = m_seg_vector.load();				///< Reload, since another thread could have beaten us to here

			if (!vector_ptr || r > vector_ptr->size() * N) {				///< Retest 
				auto old = vector_ptr;										///< Remember old vector
				
				auto num_segs = std::max( ((r - 1ULL) >> L) + 1ULL, 16ULL);			///< Number of segments necessary, at least 16
				num_segs = vector_ptr ? std::max( num_segs, vector_ptr->size() << 1ULL ) : num_segs;	///< At least double size

				vector_ptr = std::make_shared<seg_vector_t>( num_segs, m_mr );		///< Create new segment ptr
				for (int i = 0; old && i < m_num_segments.load(); ++i) {
					(*vector_ptr)[i].store((*old)[i].load()); 	///< Copy old shared pointers to the new vector
				}
				m_seg_vector.store(vector_ptr);		///< Copy new to old -> now all threads use the new vector
			}
		}
		return vector_ptr->size() * N;

		//vector_ptr = m_allocator.allocate(1);            ///< allocate the object
		//m_allocator.construct(vector_ptr, m_mr);
	}

	/**
	* \brief Deallocate segments that are currently not used.
	* No parallel processing allowed when calling this function.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::compress() noexcept -> void {
		std::lock_guard lock(m_mutex);					///< Stop all other accesses to the vector of pointers

		auto vector_ptr{ m_seg_vector.load() };
		if (!vector_ptr) return;
		while (m_num_segments > 1 && m_size + N <= m_num_segments * N) {
			(*vector_ptr)[m_num_segments.load() - 1] = nullptr;
			--m_num_segments;
		}
		m_reuse_segments.clear();
	}

}


#endif

