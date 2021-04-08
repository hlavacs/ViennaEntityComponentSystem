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
	* \brief VecsTable is a data container similar to std::vector, but with additional properties
	* 
	* VecsTable has the following properties:
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
		using tuple_rvref_t = vtll::to_rvref_tuple<DATA>;	///< Tuple holding prvalue references to the entries

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

		/**
		* \brief Get a reference to a particular component with index I.
		* \param[in] n Index to the entry.
		* \returns a reference to the Ith component of entry n.
		*/
		template<size_t I, typename C = vtll::Nth_type<DATA,I>>
		inline auto comp_ref_idx(index_t n) noexcept -> C& { 
			if constexpr (ROW) {
				return std::get<I>((*m_segment[n >> L])[n & BIT_MASK]);
			}
			else {
				return std::get<I>(*m_segment[n >> L])[n & BIT_MASK];
			}
		};

		/**
		* \brief Get a reference to a particular component with type C. 
		* The first component found having this type is used.
		* 
		* \param[in] n Index to the entry.
		* \returns a reference to component with type C of entry n.
		*/
		template<typename C>
		inline auto comp_ref_type(index_t n) noexcept -> C& { 
			if constexpr (ROW) {
				return std::get<vtll::index_of<DATA, C>::value>((*m_segment[n >> L])[n & BIT_MASK]);
			}
			else {
				return std::get<vtll::index_of<DATA, C>::value>(*m_segment[n >> L])[n & BIT_MASK];
			}
		};

		/**
		* \brief Get a tuple with references to all components of an entry.
		* \param[in] n Index to the entry.
		* \returns a tuple with references to all components of entry n.
		*/
		inline auto tuple_ref(index_t n) noexcept -> tuple_ref_t {
			auto f = [&]<size_t... Is>(std::index_sequence<Is...>) {
				if constexpr (ROW) {
					return std::tie(std::get<Is>((*m_segment[n >> L])[n & BIT_MASK])...);
				}
				else {
					return std::tie(std::get<Is>(*m_segment[n >> L])[n & BIT_MASK]...);
				}
			};
			return f(std::make_index_sequence<vtll::size<DATA>::value>{});
		};

		/**
		* \brief Get a tuple with prvalue references to all components of an entry.
		* \param[in] n Index to the entry.
		* \returns a tuple with prvalue references to all components of entry n.
		*/
		inline auto tuple_rvref(index_t n) noexcept -> tuple_rvref_t {
			auto f = [&]<size_t... Is>(std::index_sequence<Is...>) {
				if constexpr (ROW) {
					return std::make_tuple(std::move(std::get<Is>((*m_segment[n >> L])[n & BIT_MASK]))...);
				}
				else {
					return std::make_tuple(std::move(std::get<Is>(*m_segment[n >> L])[n & BIT_MASK])...);
				}
			};
			return f(std::make_index_sequence<vtll::size<DATA>::value>{});
		};

		/**
		* \brief Get a tuple with valuesof all components of an entry.
		* \param[in] n Index to the entry.
		* \returns a tuple with values of all components of entry n.
		*/
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

		/**
		* \brief Create a new entry at the end of the table.
		* Must be externally synchronized!
		* \returns the index of the new entry.
		*/
		inline auto push_back() -> index_t {
			auto idx = m_size.fetch_add(1);		///< Increase table size and get old size.
			if (!reserve(idx+1)) {				///< Make sure there is enough space in the table
				m_size--;
				return index_t{};				///< If not return an invalid index.
			}
			return index_t{ static_cast<decltype(index_t::value)>(idx)}; ///< Return index of new entry
		}

		/**
		* \brief Create a new entry at end of table and fill it with data.
		* Must be externally synchronized!
		* \param[in] data Universal references to the new data.
		* \returns index of new entry.
		*/
		template<typename TDATA>
		requires std::is_same_v<TDATA, tuple_value_t>
		inline auto push_back(TDATA&& data) -> index_t {
			auto idx = m_size + 1;		///< Increase table size and get old size.
			if (!reserve(idx + 1)) {	///< Make sure there is enough space in the table
				m_size--;
				return index_t{};		///< If not return an invalid index.
			}
			decltype(auto) ref = tuple_ref(index_t{ idx });						///< Get references to components
			vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) { 
				std::get<i>(ref) = std::get<i>(data);							///< Copy/move data over components
			});
			m_size++;															///< Publish new entry
			return index_t{ static_cast<decltype(index_t::value)>(idx) };		///< Return new index
		}

		/**
		* \brief Remove the last entry from the table.
		*/
		inline auto pop_back() -> void {
			m_size--;
		}

		/**
		* \brief Update the component with index I of an entry.
		* \param[in] n Index of entry holding the component.
		* \param[in] C Universal reference to the component holding the data.
		* \returns true if the operation was successful.
		*/
		template<size_t I, typename C = vtll::Nth_type<DATA, I>>
		inline auto update(index_t n, C&& data) -> bool {
			if (n >= m_size) return false;
			comp_ref_idx<I>(n) = data;
			return true;
		}

		/**
		* \brief Update the component with type C of an entry. The first component with this type is used.
		* \param[in] n Index of entry holding the component.
		* \param[in] C Universal reference to the component holding the data.
		* \returns true if the operation was successful.
		*/
		template<typename C>
		requires vtll::has_type<DATA,std::decay_t<C>>::value
		inline auto update(index_t n, C&& data) -> bool {
			if (n >= m_size) return false;
			comp_ref_type<std::decay_t<C>>(n) = data;
			return true;
		}

		/**
		* \brief Update all components of an entry.
		* \param[in] n Index of entry holding the component.
		* \param[in] C Universal reference to tuple holding the components with the data.
		* \returns true if the operation was successful.
		*/
		template<template<typename... Cs> typename T, typename... Cs>
		requires std::is_same_v<vtll::to_tuple<DATA>, std::tuple<std::decay_t<Cs>...>>
		inline auto update(index_t n, T<Cs...>&& data ) -> bool {
			if (n >= m_size) return false;
			decltype(auto) ref = tuple_ref(n);
			vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) { 
				std::get<i>(ref) = std::get<i>(data); 
			} );
			return true;
		}

		/**
		* \brief Move one row to another row.
		* \param[in] idst Index of destination row.
		* \param[in] isrc Index of source row.
		* \returns true if the operation was successful.
		*/
		inline auto move(index_t idst, index_t isrc) -> bool {
			if (idst >= m_size || isrc >= m_size) return false;
			decltype(auto) src = tuple_ref(isrc);
			decltype(auto) dst = tuple_rvref(idst);
			vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) { 
				std::get<i>(dst) = std::move(std::get<i>(src)); 
			});
			return true;
		}

		/**
		* \brief Swap the values of two rows.
		* \param[in] n1 Index of first row.
		* \param[in] n2 Index of second row.
		* \returns true if the operation was successful.
		*/
		inline auto swap(index_t n1, index_t n2) -> bool {
			if (n1 >= m_size || n2 >= m_size) return false;
			tuple_value_t  tmp;
			decltype(auto) t1 = tuple_rvref(n1);

			vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) {
				std::get<i>(tmp) = std::move(std::get<i>(t1));
			});

			move(n1, n2);
			update(n2, std::move(tmp));
			return true;
		}


		/**
		* \brief Allocate segements to make sure enough memory is available.
		* \param[in] r Number of entries to be stored in the table.
		* \returns true if the operation was successful.
		*/
		auto reserve(size_t r) noexcept -> bool {
			if (r > m_seg_max * N) return false;
			while (m_segment.size() * N < r) { 
				m_segment.push_back(std::make_unique<array_tuple_t>()); ///< Create new segment
			}
			m_seg_allocated = m_segment.size();	///< Publish new size
			return true;
		}

		/**
		* \brief Set capacity of the segment table. This might reallocate the whole vector.
		* No parallel processing is allowed when calling this function!
		* \param[in] r Max number of entities to be allowed in the table.
		*/
		auto max_capacity(size_t r) noexcept -> void {
			if (r == 0) return;
			auto segs = (r-1) / N + 1;				///< Number of segments necessary
			if (segs > m_segment.capacity()) {		///< Is it larger than the one we have?
				m_segment.reserve(segs);			///< If yes, reallocate the vector.
			}
			m_seg_max = m_segment.capacity();		///< Publish the new capacity.
		}

		/**
		* \brief Deallocate segments that are currently not used. 
		* No parallel processing allowed when calling this function.
		*/
		auto compress() noexcept -> void {
			while (m_segment.size() > 1 && m_size + N <= m_segment.size() * N) {
				m_segment.pop_back();
			}
			m_seg_allocated = m_segment.size();
		}
	};

}


#endif

