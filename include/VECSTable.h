#ifndef VECSTABLE_H
#define VECSTABLE_H

#include <assert.h>
#include <memory_resource>
#include <shared_mutex>
#include <optional>
#include <array>
#include "VECSUtil.h"

namespace vecs {

	/**
	* VecsTable is a data container similar to std::vector, but with three additional properties
	* 1) It stores tuples of data, thus the result is a table.
	* 2) The memory layout can be row-oriented or column-oriented.
	* 3) It can grow even when used with multiple threads. This is achieved by storing data in segements,
	* which are accessed over via a std::vector of unique_ptr. New segments can simply be added to the 
	* std::vector, and no reallocation occurs if the std::vector has a large enough capacity.
	* If more segments are needed then multithreaded operations must be stopped to reallocate this std::vector. 
	* 
	* The number of items S per segment must be a power of 2 : N = 2^L. This way, random access to row K is esily achieved
	* by first right shift K >> L to get the index of the segment pointer, then use K & (N-1) to get the index within 
	* the segment.
	* 
	*/
	template<typename DATA, size_t L = 10, bool ROW = true>
	class VecsTable {
		template<typename E> friend class VecsRegistry;
		template<typename E> friend class VecsComponentTable;
		template<typename E, size_t I> friend class VecsComponentTableDerived;

		static const size_t N = 1 << L;				///< Number if entries per segment
		static const uint64_t BIT_MASK = N - 1;		///< Bit mask to mask off lower bits to get index inside segment

		using tuple_value_t = vtll::to_tuple<DATA>;		///< Tuple holding the entries as value
		using tuple_ref_t = vtll::to_ref_tuple<DATA>;	///< Tuple holding references to the entries

		using array_tuple_t1 = std::array<tuple_value_t, N>;								///< ROW: an array of tuples
		using array_tuple_t2 = vtll::to_tuple<vtll::transform_size_t<DATA,std::array,N>>;	///< COLUMN: a tuple of arrays
		using array_tuple_t  = std::conditional_t<ROW, array_tuple_t1, array_tuple_t2>;		///< Memory layout of the table

		using seg_ptr = std::unique_ptr<array_tuple_t>;		///< Unique ptr to a segment

		std::pmr::vector<seg_ptr>	m_segment;				///< Vector of unique ptrs to the segments
		std::atomic<size_t>			m_size = 0;				///< Number of rows in the table
		std::atomic<size_t>			m_seg_allocated = 0;	///< Thread safe number of segments in the table
		size_t						m_seg_max = 0;			///< Current max number of segments

	public:
		VecsTable(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept
			: m_segment{ mr }  { max_capacity(r); };

		size_t size() { return m_size.load(); };	///< \returns the current numbers of rows in the table
		void clear() { m_size = 0; };				///< Set the number if rows to zero - effectively clear the table

		//Externally synchronized
		template<size_t I, typename C = vtll::Nth_type<DATA,I>>
		inline auto comp_ref_idx(index_t n) noexcept -> C& { 
			if constexpr (ROW) {
				return std::get<I>((*m_segment[n >> L])[n & BIT_MASK]);
			}
			else {
				return std::get<I>(*m_segment[n >> L])[n & BIT_MASK];
			}
		};

		template<typename C>
		inline auto comp_ref_type(index_t n) noexcept -> C& { 
			if constexpr (ROW) {
				return std::get<vtll::index_of<DATA, C>::value>((*m_segment[n >> L])[n & BIT_MASK]);
			}
			else {
				return std::get<vtll::index_of<DATA, C>::value>(*m_segment[n >> L])[n & BIT_MASK];
			}
		};

		//Externally synchronized
		inline auto tuple_ref(index_t n) noexcept -> tuple_ref_t {
			auto f = [&]<size_t... Is>(std::index_sequence<Is...>) {
				if constexpr (ROW) {
					return std::make_tuple(std::ref(std::get<Is>((*m_segment[n >> L])[n & BIT_MASK]))...);
				}
				else {
					return std::make_tuple(std::ref(std::get<Is>(*m_segment[n >> L])[n & BIT_MASK])...);
				}
			};
			return f(std::make_index_sequence<vtll::size<DATA>::value>{});
		};

		//Externally synchronized
		inline auto tuple_value(index_t n) noexcept -> tuple_value_t {
			auto f = [&]<size_t... Is>(std::index_sequence<Is...>) {
				if constexpr (ROW) {
					return std::make_tuple(std::get<Is>((*m_segment[n >> L])[n & BIT_MASK])...);
				}
				else {
					return std::make_tuple(std::get<Is>(*m_segment[n >> L])[n & BIT_MASK]...);
				}
			};
			return f(std::make_index_sequence<vtll::size<DATA>::value>{});
		};

		//Externally synchronized
		inline auto push_back() -> index_t {
			auto idx = m_size.fetch_add(1);
			if (!reserve(idx+1)) {
				m_size--;
				return index_t{};
			}
			return index_t{ static_cast<decltype(index_t::value)>(idx)};
		}

		//Externally synchronized
		template<typename TDATA>
		requires std::is_same_v<TDATA, tuple_value_t>
		inline auto push_back(TDATA&& data) -> index_t {
			auto idx = m_size.fetch_add(1);
			if (!reserve(idx + 1)) {
				m_size--;
				return index_t{};
			}
			decltype(auto) ref = tuple_ref(index_t{ idx });
			vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) { 
				std::get<i>(ref) = std::get<i>(data); 
			});
			return index_t{ static_cast<decltype(index_t::value)>(idx) };
		}

		//Externally synchronized
		inline auto pop_back() -> void {
			m_size--;
		}

		//Externally synchronized
		template<size_t I>
		inline auto update(index_t index, const vtll::Nth_type<DATA,I>& data) -> bool {
			if (index.value >= m_size) return false;
			comp_ref_idx<I>(index) = data;
			return true;
		}

		//Externally synchronized
		template<typename C>
		requires vtll::has_type<DATA,std::decay_t<C>>::value
		inline auto update(index_t index, C&& data) -> bool {
			if (index.value >= m_size) return false;
			comp_ref_type<std::decay_t<C>>(index) = data;
			return true;
		}

		//Externally synchronized
		template<typename T>
		requires std::is_same_v<vtll::to_tuple<DATA>, std::decay_t<T>>
		inline auto update(index_t index, T&& data ) -> bool {
			if (index.value >= m_size) return false;
			decltype(auto) ref = tuple_ref(index);
			vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) { std::get<i>(ref) = std::get<i>(data); } );
			return true;
		}

		//Externally synchronized
		auto reserve(size_t r) noexcept -> bool {
			if (r == 0 || r > m_seg_max * N) return false;
			while (m_segment.size() * N < r) { 
				m_segment.push_back(std::make_unique<array_tuple_t>()); 
			}
			m_seg_allocated = m_segment.size();
			return true;
		}

		//Externally synchronized
		auto max_capacity(size_t r) noexcept -> void {
			if (r == 0) return;
			auto segs = (r-1) / N + 1;
			if (segs > m_segment.capacity()) {
				m_segment.reserve(segs);
			}
			m_seg_max = m_segment.capacity();
		}

		//Externally synchronized
		auto compress() noexcept -> void {
			while (m_segment.size() > 1 && m_size + N <= m_segment.size() * N) {
				m_segment.pop_back();
			}
			m_seg_allocated = m_segment.size();
		}
	};

}


#endif

