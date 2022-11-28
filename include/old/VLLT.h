#ifndef VlltTABLE_H
#define VlltTABLE_H

#include <assert.h>
#include <memory_resource>
#include <shared_mutex>
#include <optional>
#include <array>
#include <concepts>
#include <algorithm>
#include <type_traits>
#include "VTLL.h"

namespace vllt {

	template<typename T>
	struct is_atomic : std::false_type {};

	template<typename T>
	struct is_atomic<std::atomic<T>> : std::true_type {};


	/**
	* \brief VlltTable is a data container similar to std::vector, but with additional properties
	*
	* VlltTable has the following properties:
	* 1) It stores tuples of data, thus the result is a table.
	* 2) The memory layout is cache-friendly and can be row-oriented or column-oriented.
	* 3) Lockless multithreaded access. It can grow - by calling push_back() - even when
	* used with multiple threads. This is achieved by storing data in segments,
	* which are accessed over via a std::vector of shared_ptr. New segments can simply be added to the
	* std::vector. Also the std::vector can seamlessly grow using CAS.
	* Is can also shrink when using multithreading by calling pop_back(). Again, no locks are used!
	*
	* The number of items S per segment must be a power of 2 : N = 2^L. This way, random access to row K is esily achieved
	* by first right shift K >> L to get the index of the segment pointer, then use K & (N-1) to get the index within
	* the segment.
	*
	*/
	template<typename DATA, size_t N0 = 1<<10, bool ROW = true, typename table_index_t = uint32_t>
	class VlltTable {

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

		std::pmr::memory_resource*						m_mr;					///< Memory resource for allocating segments
		std::pmr::polymorphic_allocator<seg_vector_t>	m_allocator;			///< use this allocator
		std::atomic<std::shared_ptr<seg_vector_t>>		m_seg_vector;			///< Vector of shared ptrs to the segments
		std::atomic<slot_size_t>						m_size_cnt{ {0,0} };	///< Next slot and size as atomic

		inline auto size2() noexcept -> size_t;

	public:
		VlltTable(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept;
		~VlltTable() noexcept;

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
		requires (sizeof...(Cs) > 1 && vtll::has_all_types<DATA, vtll::tl<std::decay_t<Cs>...>>::value)
		inline auto update(table_index_t n, Cs&&... data) noexcept	-> bool;

		//-------------------------------------------------------------------------------------------
		//move and remove data

		inline auto pop_back( vtll::to_tuple<DATA>* tup = nullptr, bool del = true ) noexcept -> bool;	///< Remove the last row, call destructor on components
		inline auto clear() noexcept			-> size_t;			///< Set the number if rows to zero - effectively clear the table, call destructors
		inline auto remove_back(vtll::to_tuple<DATA>* tup = nullptr)	noexcept	-> bool;			///< Remove the last row - no destructor called
		inline auto remove_all()	noexcept	-> size_t;			///< Remove all rows - no destructor called
		inline auto move(table_index_t idst, table_index_t isrc) noexcept	-> bool;	///< Move contents of a row to another row
		inline auto swap(table_index_t n1, table_index_t n2) noexcept		-> bool;	///< Swap contents of two rows
		inline auto compress() noexcept			-> void;		///< Deallocate unsused segements
	};


	/**
	* \brief Constructor of class VlltTable.
	* \param[in] r Max number of rows that can be stored in the table.
	* \param[in] mr Memory allocator.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline VlltTable<DATA, N0, ROW, table_index_t>::VlltTable(size_t r, std::pmr::memory_resource* mr) noexcept
		: m_mr{ mr }, m_allocator{ mr }, m_seg_vector{ nullptr } {};

	/**
	* \brief Destructor of class VlltTable.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline VlltTable<DATA, N0, ROW, table_index_t>::~VlltTable() noexcept { clear(); };

	/**
	* \brief Return number of rows when growing including new rows not yet established.
	* \returns number of rows when growing including new rows not yet established.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::size2() noexcept -> size_t {
		auto size = m_size_cnt.load();
		return std::max(size.m_next_slot, size.m_size);
	};

	/**
	* \brief Return number of valid rows.
	* \returns number of valid rows.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::size() noexcept -> size_t {
		auto size = m_size_cnt.load();
		return std::min( size.m_next_slot, size.m_size );
	};

	/**
	* \brief Return reference to a component with index I.
	* \param[in] n Index to the entry.
	* \returns reference to a component with index I.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	template<size_t I, typename C>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::component(table_index_t n) noexcept -> C& {
		return *component_ptr<I>(n);
	}

	/**
	* \brief Get a pointer to a particular component with index I.
	* \param[in] n Index to the entry.
	* \returns a pointer to the Ith component of entry n.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	template<size_t I, typename C>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::component_ptr(table_index_t n) noexcept -> C* {
		assert(n < size2());
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
	* \brief Get a tuple with references to all components of a row.
	* \param[in] n Index to the entry.
	* \returns a tuple with references to all components of a row.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::tuple(table_index_t n) noexcept -> tuple_ref_t {
		return vtll::ptr_to_ref_tuple(tuple_ptr(n));
	};

	/**
	* \brief Get a tuple with pointers to all components of an entry.
	* \param[in] n Index to the entry.
	* \returns a tuple with pointers to all components of entry n.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::tuple_ptr(table_index_t n) noexcept -> tuple_ptr_t {
		assert(n < size());
		auto f = [&]<size_t... Is>(std::index_sequence<Is...>) {
			return std::make_tuple(component_ptr<Is>(n)...);
		};
		return f(std::make_index_sequence<vtll::size<DATA>::value>{});
	};

	/**
	* \brief Create a new entry at the end of the table.
	* \param[in] data References to values to be moved or copied to the new row.
	* \returns the index of the new entry.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	template<typename... Cs>
	requires vtll::has_all_types<DATA, vtll::tl<std::decay_t<Cs>...>>::value
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::push_back(Cs&&... data) noexcept -> table_index_t {
		slot_size_t size = m_size_cnt.load();	///< Make sure that no other thread is popping currently
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
				vector_ptr = new_vector_ptr;					///< Remember for later
			}
		}

		auto seg_num = size.m_next_slot >> L;					///< Index of segment we need
		auto seg_ptr = (*vector_ptr)[seg_num].load();			///< Does the segment exist yet? If yes, increases use count.
		if (!seg_ptr) {											///< If not, create one
			auto new_seg_ptr = std::make_shared<segment_t>();	///< Create a new segment
			(*vector_ptr)[seg_num].compare_exchange_strong( seg_ptr, new_seg_ptr);	///< Try to put it into seg vector, someone might beat us here
		}

		if constexpr (sizeof...(Cs) > 0) {						///< copy/move the data
			(update<vtll::index_of<DATA, std::decay_t<Cs>>::value>(table_index_t{ size.m_next_slot }, std::forward<Cs>(data)), ...);
		}

		slot_size_t new_size = m_size_cnt.load();	///< Increase size to validate the new row
		do {
			new_size.m_size = size.m_next_slot;
		} while (!m_size_cnt.compare_exchange_weak(new_size, slot_size_t{ new_size.m_next_slot, new_size.m_size + 1 }));

		return table_index_t{ size.m_next_slot };	///< Return index of new entry
	}

	/**
	* \brief Pop the last row if there is one.
	* \param[in] tup Pointer to tuple to move the row data into.
	* \returns true if a row was popped.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::pop_back(vtll::to_tuple<DATA>* tup, bool del) noexcept -> bool {
		slot_size_t size = m_size_cnt.load();
		if (size.m_next_slot == 0) return false;	///< Is there a row to pop off?

		/// Make sure that no other thread is currently pushing a new row
		while (size.m_next_slot > size.m_size || !m_size_cnt.compare_exchange_weak(size, slot_size_t{ size.m_next_slot - 1, size.m_size })) {
			if (size.m_next_slot > size.m_size) { size = m_size_cnt.load(); }
			if (size.m_next_slot == 0) return false;	///< Is there a row to pop off?
		};

		vtll::static_for<size_t, 0, vtll::size<DATA>::value >(	///< Loop over all components
			[&](auto i) {
				using type = vtll::Nth_type<DATA, i>;
				if constexpr (std::is_move_assignable_v<type>) {
					if (tup != nullptr) { std::get<i>(*tup) = std::move(*component_ptr<i>(table_index_t{ size.m_next_slot - 1 })); }
				}
				else if constexpr (std::is_copy_assignable_v<type>) {
					if (tup != nullptr) { std::get<i>(*tup) = *component_ptr<i>(table_index_t{ size.m_next_slot - 1 }); }
				}
				else if constexpr (is_atomic< std::decay_t<type>>::value) {
					if (tup != nullptr) { std::get<i>(*tup).store( component_ptr<i>(table_index_t{ size.m_next_slot - 1 })->load() ); }
				}
				if constexpr (std::is_destructible_v<type> && !std::is_trivially_destructible_v<type>) {
					if (del) { component_ptr<i>(table_index_t{ size.m_next_slot - 1 })->~type(); }	///< Call destructor
				}
			}
		);

		slot_size_t new_size = m_size_cnt.load();	///< Commit the popping of the row
		do {
			new_size.m_size = size.m_next_slot;
		} while (!m_size_cnt.compare_exchange_weak(new_size, slot_size_t{ new_size.m_next_slot, new_size.m_size - 1 }));

		return true;
	}


	/**
	* \brief Remove the last row if there is one.
	* \returns true if a row was popped.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::remove_back(vtll::to_tuple<DATA>* tup) noexcept -> bool {
		return pop_back(tup, false);
	}

	/**
	* \brief Remove all rows.
	* \returns number of removed rows.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::remove_all() noexcept -> size_t {
		size_t num = 0;
		while (remove_back()) { ++num; }
		return num;
	}

	/**
	* \brief Pop all rows.
	* \returns number of popped rows.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::clear() noexcept -> size_t {
		size_t num = 0;
		while (pop_back()) { ++num; }
		return num;
	}

	/**
	* \brief Update the component with index I of an entry.
	* \param[in] n Index of entry holding the component.
	* \param[in] C Universal reference to the component holding the data.
	* \returns true if the operation was successful.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	template<size_t I, typename C>
	requires vtll::has_type<DATA, std::decay_t<C>>::value
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::update(table_index_t n, C&& data) noexcept -> bool {
		assert(n < size2());
		if constexpr (std::is_move_assignable_v<std::decay_t<C>> && std::is_rvalue_reference_v<decltype(data)>) {
			component<I>(n) = std::move(data);
		}
		else if constexpr (std::is_copy_assignable_v<std::decay_t<C>>) {
			component<I>(n) = data;
		}
		else if constexpr (is_atomic< std::decay_t<C>>::value) {
			component<I>(n).store( data.load() );
		}

		return true;
	}

	/**
	* \brief Update components of an entry.
	* \param[in] n Index of entry holding the component.
	* \param[in] C Universal reference to tuple holding the components with the data.
	* \returns true if the operation was successful.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	template<typename... Cs>
	requires (sizeof...(Cs)>1 && vtll::has_all_types<DATA, vtll::tl<std::decay_t<Cs>...>>::value)
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::update(table_index_t n, Cs&&... data) noexcept -> bool {
		return (update<vtll::index_of<DATA,std::decay_t<Cs>>::value>(n, std::forward<Cs>(data)) && ... && true);
	}

	/**
	* \brief Move one row to another row.
	* \param[in] idst Index of destination row.
	* \param[in] isrc Index of source row.
	* \returns true if the operation was successful.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::move(table_index_t idst, table_index_t isrc) noexcept -> bool {
		if (idst >= size() || isrc >= size()) return false;
		auto src = tuple_ptr(isrc);
		auto dst = tuple_ptr(idst);
		vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) {
			using type = vtll::Nth_type<DATA, i>;
			if constexpr (std::is_move_assignable_v<type>) {
				*std::get<i>(dst) = std::move(*std::get<i>(src));
			}
			else if constexpr (std::is_copy_assignable_v<type>) {
				*std::get<i>(dst) = *std::get<i>(src);
			}
			else if constexpr (is_atomic<type>::value) {
				std::get<i>(dst).store(std::get<i>(src).load());
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
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::swap(table_index_t idst, table_index_t isrc) noexcept -> bool {
		if (idst >= size() || isrc >= size()) return false;
		auto src = tuple_ptr(isrc);
		auto dst = tuple_ptr(idst);
		vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) {
			using type = vtll::Nth_type<DATA, i>;
			//std::cout << typeid(type).name() << "\n";
			if constexpr (std::is_move_assignable_v<type> && std::is_move_constructible_v<type>) {
				std::swap(*std::get<i>(dst), *std::get<i>(src));
			}
			else if constexpr (std::is_copy_assignable_v<type> && std::is_copy_constructible_v<type>) {
				type tmp{ *std::get<i>(src) };
				*std::get<i>(src) = *std::get<i>(dst);
				*std::get<i>(dst) = tmp;
			}
			else if constexpr (is_atomic<type>::value) {
				type tmp{ std::get<i>(src)->load() };
				std::get<i>(src)->store(std::get<i>(dst)->load());
				std::get<i>(dst)->store( tmp->load() );
			}
		});
		return true;
	}

	/**
	* \brief Deallocate segments that are currently not used.
	* No parallel processing allowed when calling this function.
	*/
	template<typename DATA, size_t N0, bool ROW, typename table_index_t>
	inline auto VlltTable<DATA, N0, ROW, table_index_t>::compress() noexcept -> void {
		auto vector_ptr{ m_seg_vector.load() };
		if (!vector_ptr) return;
		for (size_t i = vector_ptr->size() - 1; i > (size2() >> L); --i) {
			auto seg_ptr = (*vector_ptr)[i].load();
			if (seg_ptr && seg_ptr.use_count() == 2) {
				std::shared_ptr<segment_t> new_seg_ptr;
				(*vector_ptr)[i].compare_exchange_strong(seg_ptr, new_seg_ptr);	///< Try to put it into seg vector, someone might beat us here
			}
		}
	}

}


#endif
