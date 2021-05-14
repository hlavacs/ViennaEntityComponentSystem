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

		using seg_vector_t = std::pmr::vector<std::shared_ptr<array_tuple_t>>; ///< A seg_vector_t is a vector holding shared pointers to segments

		std::atomic<seg_vector_t*>		m_segment;		///< Vector of shared ptrs to the segments
		std::unique_ptr<seg_vector_t>	m_old_segment;	///< Late deallocation of old segment
		std::atomic<size_t>				m_size = 0;		///< Number of rows in the table
		std::mutex						m_mutex;		///< Needed when reallocating the vector

	public:
		VecsTable(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept;
		~VecsTable() noexcept;

		inline size_t size() noexcept { return m_size.load(); };	///< \returns the current numbers of rows in the table

		//-------------------------------------------------------------------------------------------
		//read data

		template<size_t I, typename C = vtll::Nth_type<DATA, I>>
		inline auto component_ref(table_index_t n) noexcept	-> C&;		///< \returns a reference to a component

		template<size_t I, typename C = vtll::Nth_type<DATA,I>>
		inline auto component_ptr(table_index_t n) noexcept	-> C*;		///< \returns a pointer to a component

		inline auto tuple_ref(table_index_t n) noexcept	-> tuple_ref_t;	///< \returns a tuple with copies of all components
		inline auto tuple_ptr(table_index_t n) noexcept	-> tuple_ptr_t;		///< \returns a tuple with pointers to all components
		
		//-------------------------------------------------------------------------------------------
		//add data

		inline auto push_back() noexcept						-> table_index_t;	///< Create an empty row at end of the table

		template<template<typename... Cs> typename T, typename... Cs>
		requires std::is_same_v<vtll::to_tuple<DATA>, std::tuple<std::decay_t<Cs>...>>
		inline auto push_back(T<Cs...>&& data) noexcept			-> table_index_t;	///< Push new component data to the end of the table

		//-------------------------------------------------------------------------------------------
		//update data

		template<size_t I, typename C = vtll::Nth_type<DATA, I>>
		inline auto update(table_index_t n, C&& data) noexcept			-> bool;	///< Update a component  for a given row

		template<template<typename... Cs> typename T, typename... Cs>
		requires std::is_same_v<vtll::to_tuple<DATA>, std::tuple<std::decay_t<Cs>...>>
		inline auto update(table_index_t n, T<Cs...>&& data) noexcept	-> bool;	///< Update components for a given row
		
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
	inline VecsTable<P, DATA, N0, ROW>::VecsTable(size_t r, std::pmr::memory_resource* mr) noexcept : m_segment{ nullptr } {};

	/**
	* \brief Destructor of class VecsTable.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline VecsTable<P, DATA, N0, ROW>::~VecsTable() noexcept {
		if (m_segment.load()) delete m_segment.load();
	};

	/**
	* \brief Get a reference to a particular component with index I.
	* \param[in] n Index to the entry.
	* \returns a reference to the Ith component of entry n.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	template<size_t I, typename C>
	inline auto VecsTable<P, DATA, N0, ROW>::component_ref(table_index_t n) noexcept -> C& {
		assert(n.value < size());
		if constexpr (ROW) {
			return std::get<I>( (* (*m_segment.load()) [n >> L] )[n & BIT_MASK]);
		}
		else {
			return std::get<I>(* (*m_segment.load()) [n >> L])[n & BIT_MASK];
		}
	};

	/**
	* \brief Get a pointger to a particular component with index I.
	* \param[in] n Index to the entry.
	* \returns a pointer to the Ith component of entry n.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	template<size_t I, typename C>
	inline auto VecsTable<P, DATA, N0, ROW>::component_ptr(table_index_t n) noexcept -> C* {
		assert(n.value < size());
		if constexpr (ROW) {
			return &std::get<I>((*(*m_segment.load())[n >> L])[n & BIT_MASK]);
		}
		else {
			return &std::get<I>(*(*m_segment.load())[n >> L])[n & BIT_MASK];
		}
	};

	/**
	* \brief Get a tuple with valuesof all components of an entry.
	* \param[in] n Index to the entry.
	* \returns a tuple with values of all components of entry n.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::tuple_ref(table_index_t n) noexcept -> tuple_ref_t {
		assert(n.value < size());
		auto f = [&]<size_t... Is>(std::index_sequence<Is...>) {
			if constexpr (ROW) {
				return std::tie(std::get<Is>((* (*m_segment.load()) [n >> L])[n & BIT_MASK])...);
			}
			else {
				return std::tie(std::get<Is>(* (*m_segment.load()) [n >> L])[n & BIT_MASK]...);
			}
		};
		return f(std::make_index_sequence<vtll::size<DATA>::value>{});
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
			if constexpr (ROW) {
				return std::make_tuple(&std::get<Is>((* (*m_segment.load()) [n >> L])[n & BIT_MASK])...);
			}
			else {
				return std::make_tuple(&std::get<Is>(* (*m_segment.load()) [n >> L])[n & BIT_MASK]...);
			}
		};
		return f(std::make_index_sequence<vtll::size<DATA>::value>{});
	};

	/**
	* \brief Create a new entry at the end of the table.
	* Must be externally synchronized!
	* \returns the index of the new entry.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::push_back() noexcept -> table_index_t {
		auto idx = m_size.fetch_add(1);		///< Increase table size and get old size.
		if (!reserve(idx + 1)) {				///< Make sure there is enough space in the table
			m_size--;
			return table_index_t{};				///< If not return an invalid index.
		}
		return table_index_t{ static_cast<decltype(table_index_t::value)>(idx) }; ///< Return index of new entry
	}

	/**
	* \brief Create a new entry at end of table and fill it with data.
	* Must be externally synchronized!
	* \param[in] data Universal references to the new data.
	* \returns index of new entry.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	template<template<typename... Cs> typename T, typename... Cs>
	requires std::is_same_v<vtll::to_tuple<DATA>, std::tuple<std::decay_t<Cs>...>>
	inline auto VecsTable<P, DATA, N0, ROW>::push_back(T<Cs...>&& data) noexcept -> table_index_t {
		auto idx = m_size.fetch_add(1);		///< Increase table size and get old size.
		if (!reserve(idx + 1)) {			///< Make sure there is enough space in the table
			m_size--;
			return table_index_t{};			///< If not return an invalid index.
		}
		auto ptr = tuple_ptr(table_index_t{ idx });				///< Get references to components
		vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) {
			*std::get<i>(ptr) = std::get<i>(data);							///< Copy/move data over components
			});
		return table_index_t{ static_cast<decltype(table_index_t::value)>(idx) };		///< Return new index
	}

	/**
	* \brief Update the component with index I of an entry.
	* \param[in] n Index of entry holding the component.
	* \param[in] C Universal reference to the component holding the data.
	* \returns true if the operation was successful.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	template<size_t I, typename C>
	inline auto VecsTable<P, DATA, N0, ROW>::update(table_index_t n, C&& data) noexcept -> bool {
		if (n >= m_size) return false;
		if constexpr (std::is_copy_assignable_v<C>) {
			*component_ptr<I>(n) = data;
		}
		else if constexpr (std::is_move_assignable_v<C>) {
			*component_ptr<I>(n) = std::move(data);
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
	template<template<typename... Cs> typename T, typename... Cs>
	requires std::is_same_v<vtll::to_tuple<DATA>, std::tuple<std::decay_t<Cs>...>>
	inline auto VecsTable<P, DATA, N0, ROW>::update(table_index_t n, T<Cs...>&& data) noexcept -> bool {
		if (n >= m_size) return false;
		auto ptr = tuple_ptr(n);
		vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) {
			*std::get<i>(ptr) = std::get<i>(data);
		});
		return true;
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
			if constexpr (std::is_move_assignable_v<type>) {
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
		seg_vector_t* segment_ptr{ m_segment.load() };
		if (segment_ptr && r <= segment_ptr->size() * N) return true;	///< is there enough space in the table?

		std::lock_guard<std::mutex> lock(m_mutex);
		segment_ptr = m_segment.load();									///< Current seg pointer
		if (segment_ptr && r <= segment_ptr->size() * N) return true;	///< Retest, since another thread could have beaten us to here

		auto old{ segment_ptr };									///< Remember old vector
		auto old_segs = segment_ptr ? segment_ptr->capacity() : 0;	///< Old seg capacity
		auto num_segs = std::max((r - 1) / N + 1, 16ULL);			///< Number of segments necessary, min 16
		if (!segment_ptr || num_segs > old_segs) {					///< Is it larger than what we have?
			segment_ptr = new seg_vector_t;		///< Create new segment ptr vector
			num_segs = std::max(num_segs, old_segs << 1);			/// Grow at least 100%
			segment_ptr->reserve(num_segs);		///< Make vector large enough
			if (old) { *segment_ptr = *old; }	///< Copy old shared pointers to the new vector
		}

		while (segment_ptr->size() * N < r) {							///< Allocate enough segments
			segment_ptr->emplace_back(std::make_shared<array_tuple_t>());	///< Create new segment
		}
		if (segment_ptr != old) {				///< If the vector was reallocated
			m_segment.store(segment_ptr);		///< Copy new to old -> now all threads use the new vector
			if (old) {
				m_old_segment = std::unique_ptr<seg_vector_t>(old);	///< Remember the old vector, deallocate the previous old vector
			}
		}
		return true;
	}

	/**
	* \brief Set capacity of the segment table. This might reallocate the whole vector.
	* No parallel processing is allowed when calling this function!
	* \param[in] r Max number of entities to be allowed in the table.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::capacity(size_t r) noexcept -> size_t {
		seg_vector_t* segment_ptr{ m_segment.load() };

		if (!segment_ptr || r > segment_ptr->capacity() * N) {	///< is there enough space in the table?

			std::lock_guard<std::mutex> lock(m_mutex);
			segment_ptr = m_segment.load();				///< Reaload, since another thread could have beaten us to here

			if (!segment_ptr || r > segment_ptr->capacity() * N) {	///< Retest 
				auto old = segment_ptr;								///< Remember old vector
				auto num_segs = std::max((r - 1) / N + 1, 16);		///< Number of segments necessary, at least 16
				segment_ptr = new seg_vector_t;						///< Create new segment ptr
				segment_ptr->reserve(num_segs);						///< Make vector large enough
				if (old) {
					*segment_ptr = *old;			///< Copy old shared pointers to the new vector
					m_old_segment = std::unique_ptr<seg_vector_t>(old);	///< Remember the old vector, deallocate the previous segment
				}
				m_segment.store(segment_ptr);		///< Copy new to old -> now all threads use the new vector
			}
		}
		return segment_ptr->capacity() * N;
	}

	/**
	* \brief Deallocate segments that are currently not used.
	* No parallel processing allowed when calling this function.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::compress() noexcept -> void {
		if (!m_segment.load()) return;
		while (m_segment.load()->size() > 1 && m_size + N <= m_segment.load()->size() * N) {
			m_segment.load()->pop_back();
		}
		m_old_segment.reset();
	}

}


#endif

