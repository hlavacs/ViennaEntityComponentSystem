#ifndef VECSTABLE_H
#define VECSTABLE_H

#include <assert.h>
#include <memory_resource>
#include <shared_mutex>
#include <optional>
#include <array>
#include <concepts>
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
		using segment_t  = std::conditional_t<ROW, array_tuple_t1, array_tuple_t2>;		///< Memory layout of the table

		using segment_ptr_t = std::shared_ptr<segment_t>;
		using seg_vector_t = std::pmr::vector<std::atomic<segment_ptr_t>>; ///< A seg_vector_t is a vector holding shared pointers to segments

		struct slot_size_t {
			uint32_t m_next_slot{ 0 };
			uint32_t m_size{ 0 };
		};

		std::pmr::memory_resource*									m_mr;
		std::pmr::polymorphic_allocator<seg_vector_t>				m_allocator;			///< use this allocator
		std::atomic<std::shared_ptr<seg_vector_t>>					m_seg_vector;			///< Vector of shared ptrs to the segments
		std::atomic<size_t>											m_size = 0;				///< Number of rows in the table
		std::atomic<slot_size_t>									m_size_cnt{ {0,0} };	///< Next slot and size as atomic
		std::atomic<std::shared_ptr<seg_vector_t>>					m_next_seg_vector{};	///< Vector of shared ptrs to the segments

		inline auto size2() noexcept -> size_t;

	public:
		VecsTable(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept;
		~VecsTable() noexcept;

		inline auto size() noexcept -> size_t ; ///< \returns the current numbers of rows in the table
		
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
		inline auto update(table_index_t n, C&& data) noexcept		-> bool;	///< Update a component  for a given row
		
		template<typename... Cs>
		requires vtll::has_all_types<DATA, vtll::tl<std::decay_t<Cs>...>>::value
		inline auto update(table_index_t n, Cs&&... data) noexcept	-> bool;

		//-------------------------------------------------------------------------------------------
		//move and remove data

		inline auto pop_back( vtll::to_tuple<DATA>& tup )	noexcept -> bool;				///< Remove the last row
		inline auto remove_back()	noexcept -> bool;			///< Remove the last row
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
	inline auto VecsTable<P, DATA, N0, ROW>::size2() noexcept -> size_t {
		auto size = m_size_cnt.load();
		return std::max(size.m_next_slot, size.m_size);
	};

	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::size() noexcept -> size_t {
		auto size = m_size_cnt.load();
		return std::min( size.m_next_slot, size.m_size );
	};

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
		assert(n.value < size2());
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
		slot_size_t size = m_size_cnt.load();
		while (size.m_next_slot < size.m_size || !m_size_cnt.compare_exchange_weak(size, slot_size_t{ size.m_next_slot + 1, size.m_size })) {
			if (size.m_next_slot < size.m_size) {
				size = m_size_cnt.load();
			}
		};

		auto vector_ptr{ m_seg_vector.load() };					///< Shared pointer to current segment ptr vector, can be nullptr
		size_t num_seg = vector_ptr ? vector_ptr->size() : 0;	///< Current number of segments
		if (size.m_next_slot >= N * num_seg) {					///< Do we have enough?
			auto new_vector_ptr = std::make_shared<seg_vector_t>( std::max( num_seg * 2, 16ULL  ), m_mr);		///< Reallocate
			for (size_t i = 0; i < num_seg; ++i) { (*new_vector_ptr)[i].store( (*vector_ptr)[i].load() );  };	///< Copy segment pointers
			if (m_seg_vector.compare_exchange_strong(vector_ptr, new_vector_ptr)) {	///< Try to exchange old segment vector with new
				vector_ptr = new_vector_ptr;						///< Remember for later
			}
		}
			
		auto seg_num = size.m_next_slot >> L;					///< Index of segment we need
		auto seg_ptr = (*vector_ptr)[seg_num].load();			///< Does the segment exist yet?
		if (!seg_ptr) {											///< If not, create one
			auto new_seg_ptr = std::make_shared<segment_t>();	///< Create a new segment
			(*vector_ptr)[seg_num].compare_exchange_strong( seg_ptr, new_seg_ptr);	///< Try to put it into seg vector, someone might beat us here 
		}

		if constexpr (sizeof...(Cs) > 0) {						///< copy/move the data
			(update<vtll::index_of<DATA, std::decay_t<Cs>>::value>(table_index_t{ size.m_next_slot }, std::forward<Cs>(data)), ...);
		}

		slot_size_t new_size = m_size_cnt.load();
		do {
			new_size.m_size = size.m_next_slot;
		} while (!m_size_cnt.compare_exchange_weak(new_size, slot_size_t{ new_size.m_next_slot, new_size.m_size + 1 }));

		return table_index_t{ size.m_next_slot }; ///< Return index of new entry
	}


	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::pop_back(vtll::to_tuple<DATA>& tup) noexcept -> bool {
		slot_size_t size = m_size_cnt.load();
		if (size.m_next_slot == 0) return false;

		while (size.m_next_slot > size.m_size || !m_size_cnt.compare_exchange_weak(size, slot_size_t{ size.m_next_slot - 1, size.m_size })) {
			if (size.m_next_slot > size.m_size) { size = m_size_cnt.load(); }
			if (size.m_next_slot == 0) return false;
		};

		vtll::static_for<size_t, 0, vtll::size<DATA>::value >(			///< Loop over all components
			[&](auto i) {
				using type = vtll::Nth_type<DATA, i>;
				if constexpr (std::is_move_assignable_v<type>) {
					std::get<i>(tup) = std::move(component_ptr<i>(table_index_t{ size.m_next_slot }));
				}
				else if constexpr (std::is_copy_assignable_v<type>) {
					std::get<i>(tup) = component_ptr<i>(table_index_t{ size.m_next_slot });

					if constexpr (std::is_destructible_v<type> && !std::is_trivially_destructible_v<type>) {
						component_ptr<i>(table_index_t{ size.m_next_slot })->~type();	///< Call destructor
					}
				}
			}
		);

		slot_size_t new_size = m_size_cnt.load();
		do {
			new_size.m_size = size.m_next_slot;
		} while (!m_size_cnt.compare_exchange_weak(new_size, slot_size_t{ new_size.m_next_slot, new_size.m_size - 1 }));

		return true;
	}


	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::remove_back() noexcept -> bool {
		slot_size_t size = m_size_cnt.load();
		if (size.m_next_slot == 0) return false;

		while (size.m_next_slot > size.m_size || !m_size_cnt.compare_exchange_weak(size, slot_size_t{ size.m_next_slot - 1, size.m_size })) {
			if (size.m_next_slot > size.m_size) { size = m_size_cnt.load(); }
			if (size.m_next_slot == 0) return false;
		};

		slot_size_t new_size = m_size_cnt.load();
		do {
			new_size.m_size = size.m_next_slot;
		} while (!m_size_cnt.compare_exchange_weak(new_size, slot_size_t{ new_size.m_next_slot, new_size.m_size - 1 }));

		return true;
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
		assert(n.value < size2());
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
		if (idst >= size() || isrc >= size()) return false;
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
		if (idst >= size() || isrc >= size()) return false;
		auto src = tuple_ptr(isrc);
		auto dst = tuple_ptr(idst);
		vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) {
			using type = vtll::Nth_type<DATA, i>;
			if constexpr (std::is_move_assignable_v<type> && std::is_move_constructible_v<type>) {
				std::swap(*std::get<i>(dst), *std::get<i>(src));
			}
			else if constexpr (std::is_copy_assignable_v<type> && std::is_copy_constructible_v<type>) {
				type tmp{ *std::get<i>(src) };
				*std::get<i>(src) = *std::get<i>(dst);
				*std::get<i>(dst) = tmp;
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
		return !vector_ptr ? 0 : vector_ptr->size() * N;
	}

	/**
	* \brief Deallocate segments that are currently not used.
	* No parallel processing allowed when calling this function.
	*/
	template<typename P, typename DATA, size_t N0, bool ROW>
	inline auto VecsTable<P, DATA, N0, ROW>::compress() noexcept -> void {
		auto vector_ptr{ m_seg_vector.load() };
		if (!vector_ptr) return;
		for( size_t i = (size() >> L) + 1; i< vector_ptr->size(); ++i ) {
			if ((*vector_ptr)[i].load()) {
				(*vector_ptr)[i] = nullptr;
			}
		}
	}

}


#endif

