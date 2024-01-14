#pragma once

#include <assert.h>
#include <algorithm>
#include <memory_resource>
#include <shared_mutex>
#include <optional>
#include <array>
#include <stack>
#include <concepts>
#include <algorithm>
#include <type_traits>
#include <vector>
#include <queue>
#include <optional>
#include <thread>
#include <latch>
#include <numeric>
#include <string>
#include <cstdlib>
#include <random>
#include <functional>

#include "VTLL.h"
#include "VSTY.h"

//todo: partition table indices into state/counter, turn spinlock into lockless with state/counter, 
//align atomics, block allocation on demand not all alloc when constructing, also pay constr/destr costs,
//read-write locking blocks: push - dont have to do anything (even if other are read/writing last block), 
//pop - write lock the last block

namespace vllt {

	/// <summary>
	/// VlltCache is a simple cache for objects of type T. It is a single linked list of objects.
	/// When an object is requested, then the first object in the list is returned. 
	/// </summary>
	template<typename T, size_t N = 256, size_t NUMBITS1 = 40>
		requires std::is_default_constructible_v<T> && std::is_move_assignable_v<T>
	class VlltCache {
	public:
		VlltCache() noexcept;
		~VlltCache() noexcept {};
		inline auto get() noexcept -> std::optional<T>;

		template<typename A>
			requires std::is_same_v<std::decay_t<A>, T>
		inline auto push(A&& v) noexcept -> bool;

	private:
		using cache_index_t = vsty::strong_type_t<int64_t, vsty::counter<>, std::integral_constant<int64_t, -1L>>;
		using generation_t = vsty::strong_type_t<size_t, vsty::counter<>>;

		using key_t = vsty::strong_type_t<uint64_t, vsty::counter<>>; //bits 0..NUMBITS1-1: index of first item, bits NUMBITS1..63: generation
		cache_index_t first(key_t key) { return cache_index_t{ key.get_bits_signed(0,NUMBITS1) }; }
		generation_t generation(key_t key) { return generation_t{ key.get_bits(NUMBITS1) }; }		

		struct cache_t {
			T m_value;
			cache_index_t m_next;
		};

		std::array<cache_t, N> m_cache;

		inline auto get(std::atomic<key_t>& stack) noexcept -> cache_index_t; //can be empty
		inline auto push(cache_index_t index, std::atomic<key_t>& stack) noexcept -> void;

		alignas(64) std::atomic<key_t> m_head{ key_t{cache_index_t{ }, 0, NUMBITS1} }; //index of first item in the cache, -1 if no item
		alignas(64) std::atomic<key_t> m_free{ key_t{cache_index_t{0}, 0, NUMBITS1} }; //index of first free slot in the cache, -1 if no free slot
	};
	
	
	template<typename T, size_t N, size_t NUMBITS1>
		requires std::is_default_constructible_v<T> && std::is_move_assignable_v<T>	
	VlltCache<T, N, NUMBITS1>::VlltCache() noexcept {
		int32_t i = 1;
		for( auto& c : m_cache ) { c.m_next = cache_index_t{i++}; }
		m_cache[N - 1].m_next = cache_index_t{};
	};


	/// @brief Get an object from the cache.
	/// @return Returns an object from the cache, or std::nullopt if the cache is empty.
	template<typename T, size_t N, size_t NUMBITS1>
		requires std::is_default_constructible_v<T> && std::is_move_assignable_v<T>
	inline auto VlltCache<T, N, NUMBITS1>::get() noexcept -> std::optional<T> {
		auto idx = get(m_head);
		if( !idx.has_value() ) return std::nullopt;
		T value = std::move(m_cache[idx].m_value);
		push(idx, m_free);
		return value; //implicit move through RVO
	}


	/// @brief Push an object into the cache.
	/// @return Returns true if there was space left, else false.
	template<typename T, size_t N, size_t NUMBITS1>
		requires std::is_default_constructible_v<T> && std::is_move_assignable_v<T>
	template<typename A>
			requires std::is_same_v<std::decay_t<A>, T>
	inline auto VlltCache<T, N, NUMBITS1>::push(A&& v) noexcept -> bool {
		auto idx = get(m_free);
		if( !idx.has_value() ) return false;
		m_cache[idx].m_value = v;
		push(idx, m_head);
		return true;
	}


	template<typename T, size_t N, size_t NUMBITS1>
		requires std::is_default_constructible_v<T> && std::is_move_assignable_v<T>
	inline auto VlltCache<T, N, NUMBITS1>::get(std::atomic<key_t>& stack) noexcept -> cache_index_t {
		key_t key = stack.load();
		while( cache_index_t{ key.get_bits_signed(0,NUMBITS1) }.has_value() ) {
			if( stack.compare_exchange_weak(key, key_t{ m_cache[first(key)].m_next, generation(key) + 1, NUMBITS1}) ) {
				return cache_index_t{ first(key) };
			}
		}
		return cache_index_t{};
	}


	template<typename T, size_t N, size_t NUMBITS1>
		requires std::is_default_constructible_v<T> && std::is_move_assignable_v<T>
	inline auto VlltCache<T, N, NUMBITS1>::push(cache_index_t index, std::atomic<key_t>& stack) noexcept -> void {
		key_t key = stack.load();
		while( true ) {
			m_cache[index].m_next = first(key);
			if( stack.compare_exchange_weak(key, key_t{index, generation(key) + 1, NUMBITS1}) ) {
				return;
			}
		}
	}



	//---------------------------------------------------------------------------------------------------



	/// <summary>
	/// VlltTable is the base class for some classes, enabling management of tables that can be
	// appended in parallel.
	/// </summary>
	/// <typeparam name="DATA">List of component types. All types must be default constructible.</typeparam>
	/// <typeparam name="N0">Number of slots per block. Is rounded to the next power of 2.</typeparam>
	/// <typeparam name="ROW">If true, then data is stored rowwise, else columnwise.</typeparam>
	/// <typeparam name="MINSLOTS">Miniumum number of available slots in the block map.</typeparam>
	/// <returns>Pointer to the component.</returns>
	template<typename DATA, size_t N0 = 1 << 10, bool ROW = true, size_t MINSLOTS = 16>
	class VlltTable {
	protected:
		static_assert(std::is_default_constructible_v<DATA>, "Your components are not default constructible!");

		using table_index_t = vsty::strong_type_t<size_t, vsty::counter<>, 
			std::integral_constant<size_t, std::numeric_limits<size_t>::max()>>;///< Strong integer type for indexing rows, 0 to number rows - 1
		using block_idx_t = vsty::strong_type_t<size_t, vsty::counter<>>; ///< Strong integer type for indexing blocks, 0 to size map - 1

		static const size_t N = vtll::smallest_pow2_leq_value< N0 >::value;	///< Force N to be power of 2
		static const size_t L = vtll::index_largest_bit< std::integral_constant<size_t, N> >::value - 1; ///< Index of largest bit in N
		static const size_t BIT_MASK = N - 1;	///< Bit mask to mask off lower bits to get index inside block

		using tuple_value_t = vtll::to_tuple<DATA>;	///< Tuple holding the entries as value
		using tuple_ref_t = vtll::to_ref_tuple<DATA>; ///< Tuple holding refs to the entries

		using array_tuple_t1 = std::array<tuple_value_t, N>;///< ROW: an array of tuples
		using array_tuple_t2 = vtll::to_tuple<vtll::transform_size_t<DATA, std::array, N>>;	///< COLUMN: a tuple of arrays
		using block_t = std::conditional_t<ROW, array_tuple_t1, array_tuple_t2>; ///< Memory layout of the table

		using block_ptr_t = std::shared_ptr<block_t>; ///< Shared pointer to a block
		struct block_map_t {
			std::pmr::vector<block_ptr_t> m_blocks;	///< Vector of shared pointers to the blocks
			block_idx_t m_block_offset = block_idx_t{0}; ///< Segment offset for FIFO queue (offsets blocks NOT rows)
		};

	public:
		VlltTable(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept
			: m_mr{ mr }, m_allocator{ mr }, m_block_map{ nullptr } {};
		~VlltTable() noexcept {};

		template<size_t I, typename C = vtll::Nth_type<DATA, I>>
		inline auto get_component_ptr(table_index_t n, std::shared_ptr<block_map_t>& map_ptr) noexcept -> C*;

		template<typename... Cs>
			requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
		auto insert(table_index_t n, std::atomic<table_index_t>* first_slot, Cs&&... data) -> void;

	protected:

		static auto block(table_index_t n, size_t offset) -> block_idx_t { return block_idx_t{ (n >> L) - offset }; }
		inline auto transfer_local_cache( auto& map_ptr ) -> void;	
		inline auto get_global_cache() -> block_ptr_t;

		inline auto resize(table_index_t slot, std::atomic<table_index_t>* first_slot = nullptr) -> std::shared_ptr<block_map_t>;

		std::pmr::memory_resource* m_mr; ///< Memory resource for allocating blocks
		std::pmr::polymorphic_allocator<block_map_t> m_allocator;  ///< use this allocator
		alignas(64) std::atomic<std::shared_ptr<block_map_t>> m_block_map;///< Atomic shared ptr to the map of blocks

		VlltCache<block_ptr_t, 64> m_block_global_cache;
		static inline thread_local std::stack<block_ptr_t> m_block_local_cache;
	};



	/// <summary>
	/// Return a pointer to a component.
	/// </summary>
	/// <typeparam name="C">Type of component.</typeparam>
	/// <typeparam name="I">Index in the component list.</typeparam>
	/// <param name="n">Slot number in the table.</param>
	/// <param name="map_ptr">Pointer to the table block holding the row.</param>
	/// <returns>Pointer to the component.</returns>
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS>
	template<size_t I, typename C>
	inline auto VlltTable<DATA,N0,ROW,MINSLOTS>::get_component_ptr(table_index_t n, std::shared_ptr<block_map_t>& map_ptr) noexcept -> C* { ///< \returns a pointer to a component
		auto idx = block(n, map_ptr->m_block_offset);
		auto block_ptr = (map_ptr->m_blocks[idx]); ///< Access the block holding the slot
		if constexpr (ROW) { return &std::get<I>((*block_ptr)[n & BIT_MASK]); }
		else { return &std::get<I>(*block_ptr)[n & BIT_MASK]; }
	}

	/// <summary>
	/// Insert a new row at the end of the table. Make sure that there are enough blocks to store the new data.
	/// If not allocate a new map to hold the segements, and allocate new blocks.
	/// </summary>
	/// <param name="n">Row number in the table.</param>
	/// <param name="map_ptr">Shared pointer to the block map.</param>
	/// <param name="first_seg">Index of first block that currently holds information.</param>
	/// <param name="last_seg">Index of the last block that currently holds informaton.</param>
	/// <param name="data">The data for the new row.</param>
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS>
	template<typename... Cs>
		requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
	inline auto VlltTable<DATA,N0,ROW,MINSLOTS>::insert(table_index_t n, std::atomic<table_index_t>* first_slot, Cs&&... data) -> void {
		auto map_ptr = resize(n, first_slot); //if need be, grow the map of blocks
		//copy or move the data to the new slot, using a recursive templated lambda
		auto f = [&]<size_t I, typename T, typename... Ts>(auto && fun, T && dat, Ts&&... dats) {
			if constexpr (vtll::is_atomic<T>::value) get_component_ptr<I>(n, map_ptr)->store(dat); //copy value for atomic
			else *get_component_ptr<I>(n, map_ptr) = std::forward<T>(dat); //move or copy
			if constexpr (sizeof...(dats) > 0) { fun.template operator() < I + 1 > (fun, std::forward<Ts>(dats)...); } //recurse
		};
		f.template operator() < 0 > (f, std::forward<Cs>(data)...);
	}


	/// <summary>
	/// Transfer blocks that have been unnessearily allocated to a global cache.
	/// <param name="map_ptr">Slot.</param>
	/// </summary>
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS>
	inline auto VlltTable<DATA,N0,ROW,MINSLOTS>::transfer_local_cache( auto& map_ptr ) -> void {
		while (m_block_local_cache.size()) {
			m_block_global_cache.push(m_block_local_cache.top()); //if full the block will be deallocated
			m_block_local_cache.pop();
		}
	}

	/// <summary>
	/// Get a new block - either from the global cache or allocate a new one.
	/// </summary>
	/// <returns></returns>
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS>
	inline auto VlltTable<DATA,N0,ROW,MINSLOTS>::get_global_cache() -> block_ptr_t {
		auto ptr = m_block_global_cache.get();
		if(ptr.has_value()) return ptr.value();
		return std::make_shared<block_t>();
	}


	/// <summary>
	/// If the map of blocks is too small, allocate a larger one and copy the previous block pointers into it.
	/// Then make one CAS attempt. If the attempt succeeds, then remember the new block map.
	/// If the CAS fails because another thread beat us, then CAS will copy the new pointer so we can use it.
	/// </summary>
	/// <param name="slot">Slot number in the table.</param>
	/// <param name="map_ptr">Shared pointer to the block map.</param>
	/// <param name="first_seg">Index of first block that currently holds information.</param>
	/// <returns>Pointer to the block map.</returns>
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS>
	inline auto VlltTable<DATA,N0,ROW,MINSLOTS>::resize(table_index_t slot, std::atomic<table_index_t>* first_slot) -> std::shared_ptr<block_map_t> {

		//Get a pointer to the block map. If there is none, then allocate a new one.
		auto map_ptr{ m_block_map.load() };
		if (!map_ptr) {
			auto new_map_ptr = std::make_shared<block_map_t>( //map has always as many MINSLOTS as its capacity is -> size==capacity
				block_map_t{ std::pmr::vector<block_ptr_t>{MINSLOTS, m_mr}, block_idx_t{ 0 } } //increase existing one
			);
			for (auto& ptr : new_map_ptr->m_blocks) {
				ptr = std::make_shared<block_t>();
			}
			if (m_block_map.compare_exchange_strong(map_ptr, new_map_ptr)) {	///< Try to exchange old block map with new
				map_ptr = new_map_ptr; ///< If success, remember for later
			} //Note: if we were beaten by other thread, then compare_exchange_strong itself puts the new value into map_ptr
		}

		//Threads should not try to allocate a new block map all at once. Randomize the allocation.
		auto sz = map_ptr->m_blocks.size() / 16;
		double rnd = (rand() % 1000) / 1000.0;
		//if (rnd > 0.2 && block_cache.size() < map_ptr->m_blocks.size()) { put_cache(std::make_shared<block_t>(), map_ptr); }
		auto f = sz* rnd - sz;

		//Make sure that there is enough space in the block map so that blocks are there to hold the new slot.
		//Because other threads might also do this, we need to run in a loop until we are sure that the new slot is covered.
		while ( slot >= N * (map_ptr->m_blocks.size() + map_ptr->m_block_offset + f)) {
			f = 0.0;

			//index of the first block that currently holds information - used in a queue
			block_idx_t first_seg{ 0 };
			auto fs = first_slot ? first_slot->load() : table_index_t{0};
			if (fs.has_value()) {
				first_seg = block(fs, map_ptr->m_block_offset);
			}

			//calculate new size of the map, if necessary
			//The map might grow or shrin. If it grows, then we need to copy the old block pointers into the new map.
			size_t num_blocks = map_ptr->m_blocks.size();
			size_t new_offset = map_ptr->m_block_offset + first_seg;
			size_t min_size = block(slot, new_offset);
			size_t smaller_size = std::max((num_blocks >> 2), MINSLOTS);
			size_t medium_size = std::max((num_blocks >> 1), MINSLOTS);
			size_t new_size = num_blocks + (num_blocks >> 1);
			while (min_size > new_size) { new_size *= 2; }
			if (first_seg > num_blocks * 0.85 && min_size < smaller_size) { new_size = smaller_size; }
			else if (first_seg > num_blocks * 0.65 && min_size < medium_size) { new_size = medium_size; }
			else if (first_seg > (num_blocks >> 1) && min_size < num_blocks) new_size = num_blocks;
			
			//Test if we have been beaten by another thread. If so, then restart the loop.
			auto map_ptr2 = m_block_map.load();
			if (map_ptr != map_ptr2) {
				map_ptr = map_ptr2;
				continue;
			}

			//Allocate a new block map and populate it with empty semgement pointers.
			auto new_map_ptr = std::make_shared<block_map_t>( //map has always as many slots as its capacity is -> size==capacity
				block_map_t{ std::pmr::vector<block_ptr_t>{new_size, m_mr}, block_idx_t{ new_offset } } //increase existing one
			);

			//Copy the old block pointers into the new map. Create also empty blocks for the new slots (or get the from the cache).
			size_t idx = 0;
			size_t remain = num_blocks - first_seg;
			std::ranges::for_each(new_map_ptr->m_blocks.begin(), new_map_ptr->m_blocks.end(), [&](auto& ptr) {
				if (first_seg + idx < num_blocks) { ptr = map_ptr->m_blocks[first_seg + idx]; } //copy
				else {
					size_t i1 = idx - remain;
					if (i1 < first_seg) { ptr = map_ptr->m_blocks[i1]; } //copy
					else { 
						ptr = get_global_cache();  //get from cache or create new block
						m_block_local_cache.push(ptr); //put into temp cache
					}
				}
				++idx;
			});

			//Try to exchange the old block map with the new one. If we are beaten, then restart the loop.
			if (m_block_map.compare_exchange_strong(map_ptr, new_map_ptr)) {	///< Try to exchange old block map with new
				//also reuse blocks that we did not reuse because map was shrunk
				auto reused = (int64_t)new_map_ptr->m_blocks.size() - (int64_t)remain;
				auto unused = (int64_t)map_ptr->m_blocks.size() - (int64_t)new_map_ptr->m_blocks.size();
				for (int64_t i = 0; i < unused; ++i) {
					m_block_local_cache.push(map_ptr->m_blocks[i + reused]);
				}
				map_ptr = new_map_ptr; ///< If success, remember for later
				while (m_block_local_cache.size()) m_block_local_cache.pop();  //clear local cache, we used those for the new block
			} //Note: if we were beaten by other thread, then compare_exchange_strong itself puts the new value into map_ptr
			else {
				transfer_local_cache(map_ptr); //we were beaten, so save the new blocks in the global cache
			}

		}
		return map_ptr;
	}



	//---------------------------------------------------------------------------------------------------


	using stack_index_t = vsty::strong_type_t<uint64_t, vsty::counter<>, std::integral_constant<uint32_t, std::numeric_limits<uint32_t>::max()>>;
	using stack_diff_t = vsty::strong_type_t<int64_t, vsty::counter<>, std::integral_constant<int32_t, std::numeric_limits<int32_t>::max()>>;


	/////
	// \brief VlltStack is a data container similar to std::deque, but with additional properties
	//
	// VlltStack has the following properties:
	// 1) It stores tuples of data
	// 2) The memory layout is cache-friendly and can be row-oriented or column-oriented.
	// 3) Lockless multithreaded access. It can grow - by calling push_back() - even when
	// used with multiple threads. This is achieved by storing data in blocks,
	// which are accessed over via a std::vector of shared_ptr. New blocks can simply be added to the
	// std::vector. Also the std::vector can seamlessly grow using CAS.
	// Is can also shrink when using multithreading by calling pop_back(). Again, no locks are used!
	//
	// The number of items S per block must be a power of 2 : N = 2^L. This way, random access to row K is esily achieved
	// by first right shift K >> L to get the index of the block pointer, then use K & (N-1) to get the index within
	// the block.
	//
	///
	template<typename DATA, size_t N0 = 1 << 10, bool ROW = true, size_t MINSLOTS = 16, size_t NUMBITS1 = 40>
	class VlltStack : public VlltTable<DATA, N0, ROW, MINSLOTS> {

	public:
		using VlltTable<DATA, N0, ROW, MINSLOTS>::N;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::L;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::m_mr;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::m_block_map;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::get_component_ptr;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::insert;

		using tuple_opt_t = std::optional< vtll::to_tuple< vtll::remove_atomic<DATA> > >;
		using typename VlltTable<DATA, N0, ROW, MINSLOTS>::table_index_t;
		using typename VlltTable<DATA, N0, ROW, MINSLOTS>::tuple_ref_t;
		using typename VlltTable<DATA, N0, ROW, MINSLOTS>::block_t;
		using typename VlltTable<DATA, N0, ROW, MINSLOTS>::block_ptr_t;
		using typename VlltTable<DATA, N0, ROW, MINSLOTS>::block_map_t;

		using push_callback_t = std::optional<std::function<void(const stack_index_t)>>; ///< Callback function that is called when a new row is pushed

		VlltStack(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept;

		//-------------------------------------------------------------------------------------------
		//read data

		inline auto size() noexcept -> size_t; ///< \returns the current numbers of rows in the table

		template<size_t I>
		inline auto get(stack_index_t n) noexcept -> std::optional<std::reference_wrapper<vtll::Nth_type<DATA, I>>>;		///< \returns a ref to a component

		template<typename C>									///< \returns a ref to a component
		inline auto get(stack_index_t n) noexcept -> std::optional<std::reference_wrapper<C>> requires vtll::unique<DATA>::value;

		inline auto get_tuple(stack_index_t n) noexcept	-> std::optional<tuple_ref_t>;	///< \returns a tuple with refs to all components

		//-------------------------------------------------------------------------------------------
		//add data

		template<typename... Cs>
			requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
		inline auto push_callback(push_callback_t f, Cs&&... data) noexcept -> stack_index_t;	///< Push new component data to the end of the table

		template<typename... Cs>
			requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
		inline auto push(Cs&&... data) noexcept -> stack_index_t;	///< Push new component data to the end of the table

		//-------------------------------------------------------------------------------------------
		//remove and swap data

		inline auto pop() noexcept		-> tuple_opt_t; ///< Remove the last row, call destructor on components
		inline auto clear() noexcept	-> size_t; ///< Set the number if rows to zero - effectively clear the table, call destructors
		inline auto swap(stack_index_t n1, stack_index_t n2) noexcept -> void;	///< Swap contents of two rows
		inline auto erase(stack_index_t n1) -> tuple_opt_t; ///< Remove a row, call destructor on components

	protected:

		using slot_size_t = vsty::strong_type_t<uint64_t, vsty::counter<>>;
		stack_index_t stack_size(slot_size_t size) { return stack_index_t{ size.get_bits(0, NUMBITS1) }; }	
		stack_diff_t  stack_diff(slot_size_t size) { return stack_diff_t{ size.get_bits_signed(NUMBITS1) }; }

		alignas(64) std::atomic<slot_size_t> m_size_cnt{ slot_size_t{ stack_index_t{ 0 }, stack_diff_t{0}, NUMBITS1 } };	///< Next slot and size as atomic

		inline auto max_size() noexcept -> size_t;
	};

	/////
	// \brief Constructor of class VlltStack.
	// \param[in] r Max number of rows that can be stored in the table.
	// \param[in] mr Memory allocator.
	///
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	inline VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::VlltStack(size_t r, std::pmr::memory_resource* mr) noexcept : VlltTable<DATA, N0, ROW, MINSLOTS>(r, mr) {};

	/////
	// \brief Return number of rows when growing including new rows not yet established.
	// \returns number of rows when growing including new rows not yet established.
	///
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	inline auto VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::max_size() noexcept -> size_t {
		auto size = m_size_cnt.load();
		return std::max(static_cast<decltype(stack_size(size))>(stack_size(size) + stack_diff(size)), stack_size(size));
	};

	/////
	// \brief Return number of valid rows.
	// \returns number of valid rows.
	///
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	inline auto VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::size() noexcept -> size_t {
		auto size = m_size_cnt.load();
		return std::min(static_cast<decltype(stack_size(size))>(stack_size(size) + stack_diff(size)), stack_size(size));
	};

	/////
	// \brief Get a pointer to a particular component with index I.
	// \param[in] n Index to the entry.
	// \returns a pointer to the Ith component of entry n.
	///
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	template<size_t I>
	inline auto VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::get(stack_index_t n) noexcept -> std::optional<std::reference_wrapper<vtll::Nth_type<DATA, I>>> {
		if (n >= size()) return std::nullopt;
		auto map_ptr = m_block_map.load();
		return { *(this->template get_component_ptr<I>(table_index_t{n}, map_ptr)) };
	};

	/////
	// \brief Get a pointer to a particular component with index I.
	// \param[in] n Index to the entry.
	// \returns a pointer to the Ith component of entry n.
	///
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	template<typename C>
	inline auto VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::get(stack_index_t n) noexcept -> std::optional<std::reference_wrapper<C>> requires vtll::unique<DATA>::value {
		if (n >= size()) return std::nullopt;
		auto map_ptr = m_block_map.load();
		return { *(this->template get_component_ptr<vtll::index_of<DATA, C>::value>(table_index_t{n}, map_ptr)) };
	};

	/////
	// \brief Get a tuple with pointers to all components of an entry.
	// \param[in] n Index to the entry.
	// \returns a tuple with pointers to all components of entry n.
	///
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	inline auto VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::get_tuple(stack_index_t n) noexcept -> std::optional<tuple_ref_t> {
		if (n >= size()) return std::nullopt;
		auto map_ptr = m_block_map.load();
		return { [&] <size_t... Is>(std::index_sequence<Is...>) { return std::tie(* (this->template get_component_ptr<Is>(table_index_t{n}, map_ptr))...); }(std::make_index_sequence<vtll::size<DATA>::value>{}) };
	};

	/////
	// \brief Push a new element to the end of the stack.
	// \param[in] data References to the components to be added.
	// \returns the index of the new entry.
	///
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	template<typename... Cs>
		requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
	inline auto VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::push_callback(push_callback_t f, Cs&&... data) noexcept -> stack_index_t {
		//increase size.m_diff to announce your demand for a new slot -> slot is now reserved for you
		slot_size_t size = m_size_cnt.load();	///< Make sure that no other thread is popping currently
		while (stack_diff(size) < 0 || !m_size_cnt.compare_exchange_weak(size, slot_size_t{ stack_size(size), stack_diff(size) + 1, NUMBITS1 } )) {
			if ( stack_diff(size)  < 0 ) { //here compare_exchange_weak was NOT called to copy manually
				size = m_size_cnt.load();
			}
			//call wait function here
		};

		//make sure there is enough space in the block VECTOR - if not then change the old map to a larger map
		insert(table_index_t{stack_size(size) + stack_diff(size)}, nullptr, std::forward<Cs>(data)...); ///< Make sure there are enough MINSLOTS for blocks

		if(f.has_value()) f.value()( stack_index_t{ stack_size(size) + stack_diff(size) } ); ///< Call callback function

		slot_size_t new_size = slot_size_t{ stack_size(size), stack_diff(size) + 1, NUMBITS1 };	///< Increase size to validate the new row
		while (!m_size_cnt.compare_exchange_weak(new_size, slot_size_t{ stack_size(new_size) + 1, stack_diff(new_size) - 1, NUMBITS1 } ));

		return stack_index_t{ stack_size(size) + stack_diff(size) };	///< Return index of new entry
	}

	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	template<typename... Cs>
		requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
	inline auto VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::push(Cs&&... data) noexcept -> stack_index_t {
		return push_callback( {}, std::forward<Cs>(data)...);
	}

	/////
	// \brief Pop the last row if there is one.
	// \param[in] tup Pointer to tuple to move the row data into.
	// \param[in] del If true, then call desctructor on the removed slot.
	// \returns true if a row was popped.
	///
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	inline auto VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::pop() noexcept -> tuple_opt_t {
		vtll::to_tuple<vtll::remove_atomic<DATA>> ret{};

		slot_size_t size = m_size_cnt.load();
		if (stack_size(size) + stack_diff(size) == 0) return std::nullopt;	///< Is there a row to pop off?

		/// Make sure that no other thread is currently pushing a new row
		while (stack_diff(size) > 0 || !m_size_cnt.compare_exchange_weak(size, slot_size_t{ stack_size(size), stack_diff(size) - 1, NUMBITS1 })) {
			if (stack_diff(size) > 0) { size = m_size_cnt.load(); }
			if (stack_size(size) + stack_diff(size) == 0) return std::nullopt;	///< Is there a row to pop off?
		};

		auto map_ptr{ m_block_map.load() };						///< Access the block map

		auto idx = stack_size(size) + stack_diff(size) - 1;
		vtll::static_for<size_t, 0, vtll::size<DATA>::value >(	///< Loop over all components
			[&](auto i) {
				using type = vtll::Nth_type<DATA, i>;
				if		constexpr (std::is_move_assignable_v<type>) { std::get<i>(ret) = std::move(* (this->template get_component_ptr<i>(table_index_t{ idx }, map_ptr)) ); }	//move
				else if constexpr (std::is_copy_assignable_v<type>) { std::get<i>(ret) = *(this->template get_component_ptr<i>(table_index_t{ idx }, map_ptr)); }				//copy
				else if constexpr (vtll::is_atomic<type>::value) { std::get<i>(ret) = this->template get_component_ptr<i>(table_index_t{ idx }, map_ptr)->load(); } 		//atomic

				if constexpr (std::is_destructible_v<type> && !std::is_trivially_destructible_v<type>) { this->template get_component_ptr<i>(table_index_t{ idx }, map_ptr)->~type(); }	///< Call destructor
			}
		);

		slot_size_t new_size = slot_size_t{ stack_size(size), stack_diff(size) - 1, NUMBITS1 };	///< Commit the popping of the row
		while (!m_size_cnt.compare_exchange_weak(new_size, slot_size_t{ stack_size(new_size) - 1, stack_diff(new_size) + 1, NUMBITS1 }));

		return ret; //RVO?
	}

	/////
	// \brief Pop all rows and call the destructors.
	// \returns number of popped rows.
	///
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	inline auto VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::clear() noexcept -> size_t {
		size_t num = 0;
		while (pop().has_value()) { ++num; }
		return num;
	}

	/////
	// \brief Swap the values of two rows.
	// \param[in] n1 Index of first row.
	// \param[in] n2 Index of second row.
	// \returns true if the operation was successful.
	///
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	inline auto VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::swap(stack_index_t idst, stack_index_t isrc) noexcept -> void {
		assert(idst < size() && isrc < size());
		auto src = get_tuple(isrc);
		if (!src.has_value()) return;
		auto dst = get_tuple(idst);
		if (!dst.has_value()) return;
		vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) {
			using type = vtll::Nth_type<DATA, i>;
			//std::cout << typeid(type).name() << "\n";
			if constexpr (std::is_move_assignable_v<type> && std::is_move_constructible_v<type>) {
				std::swap(std::get<i>(dst.value()), std::get<i>(src.value()));
			}
			else if constexpr (std::is_copy_assignable_v<type> && std::is_copy_constructible_v<type>) {
				auto& tmp{ std::get<i>(src.value()) };
				std::get<i>(src.value()) = std::get<i>(dst.value());
				std::get<i>(dst.value()) = tmp;
			}
			else if constexpr (vtll::is_atomic<type>::value) {
				type tmp{ std::get<i>(src.value()).load() };
				std::get<i>(src.value()).store(std::get<i>(dst.value()).load());
				std::get<i>(dst.value()).store(tmp.load());
			}
		});
		return;
	}

	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS, size_t NUMBITS1>
	inline auto VlltStack<DATA, N0, ROW, MINSLOTS, NUMBITS1>::erase(stack_index_t n1) -> tuple_opt_t {
		auto n2 = size() - 1;
		if (n1 == n2) return pop();
		swap(n1, stack_index_t{ n2 });
		return pop();
	}


	//----------------------------------------------------------------------------------------------------


	///
	// \brief VlltFIFOQueue is a FIFO queue that can be ued by multiple threads in parallel
	//
	// It has the following properties:
	// 1) It stores tuples of data
	// 2) Lockless multithreaded access.
	//
	// The FIFO queue is a stack thet keeps block pointers in a map.
	// Segments that are empty are recycled to the end of the blocks map.
	// An offset is maintained that is subtracted from a table index.
	//
	//

	template<typename DATA, size_t N0 = 1 << 10, bool ROW = true, size_t MINSLOTS = 16>
	class VlltFIFOQueue : public VlltTable<DATA, N0, ROW, MINSLOTS> {

	public:
		using VlltTable<DATA, N0, ROW, MINSLOTS>::N;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::L;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::m_block_map;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::m_mr;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::get_component_ptr;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::insert;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::resize;
		using VlltTable<DATA, N0, ROW, MINSLOTS>::block;

		using tuple_opt_t = std::optional< vtll::to_tuple< vtll::remove_atomic<DATA> > >;
		using typename VlltTable<DATA, N0, ROW, MINSLOTS>::table_index_t;
		using typename VlltTable<DATA, N0, ROW, MINSLOTS>::block_t;
		using typename VlltTable<DATA, N0, ROW, MINSLOTS>::block_ptr_t;
		using typename VlltTable<DATA, N0, ROW, MINSLOTS>::block_map_t;

		VlltFIFOQueue() {};

		template<typename... Cs>
			requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
		inline auto push(Cs&&... data) noexcept -> table_index_t;	///< Push new component data to the end of the table

		inline auto pop() noexcept		-> tuple_opt_t;	///< Remove the last row, call destructor on components
		inline auto size() noexcept		-> size_t;				///< Number of elements in the queue
		inline auto clear() noexcept	-> size_t;			///< Set the number if rows to zero - effectively clear the table, call destructors

	protected:
		alignas(64) std::atomic<table_index_t>	m_next{ table_index_t{0} };	//next element to be taken out of the queue
		alignas(64) std::atomic<table_index_t>	m_consumed;		//last element that was taken out and fully read and destroyed
		alignas(64) std::atomic<table_index_t>	m_next_free_slot{ table_index_t{0} };		//next element to write over
		alignas(64) std::atomic<table_index_t>	m_last;			//last element that has been produced and fully constructed
	};


	///
	// \brief Push a new element to the end of the stack.
	// \param[in] data References to the components to be added.
	// \returns the index of the new entry.
	///
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS>
	template<typename... Cs>
		requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
	inline auto VlltFIFOQueue<DATA, N0, ROW, MINSLOTS>::push(Cs&&... data) noexcept -> table_index_t {
		auto next_free_slot = m_next_free_slot.load();
		while (!m_next_free_slot.compare_exchange_weak(next_free_slot, table_index_t{ next_free_slot + 1 })); ///< Slot number to put the new data into	

		insert(next_free_slot, &m_consumed, std::forward<Cs>(data)...);

		table_index_t expected = next_free_slot > 0 ? table_index_t{ next_free_slot - 1 } : table_index_t{};
		auto old_last{ expected };
		while (!m_last.compare_exchange_weak(old_last, next_free_slot)) { old_last = expected; };

		return next_free_slot;	///< Return index of new entry
	}

	/// <summary>
	/// Pop the next item from the queue.
	/// Move its content into the return value (tuple),
	/// then remove it from the queue.
	/// </summary>
	/// <typeparam name="DATA">Type list of data items.</typeparam>
	/// <typeparam name="N0">Number of items per block.</typeparam>
	/// <typeparam name="ROW">ROW or COLUMN layout.</typeparam>
	/// <typeparam name="MINSLOTS">Default number of MINSLOTS in the first block map.</typeparam>
	/// <returns>Tuple with values from the next item, or nullopt.</returns>
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS>
	inline auto VlltFIFOQueue<DATA, N0, ROW, MINSLOTS>::pop() noexcept -> tuple_opt_t {
		vtll::to_tuple<vtll::remove_atomic<DATA>> ret{};

		if (!m_last.load().has_value()) return std::nullopt;

		table_index_t last;
		auto next = m_next.load();
		do {
			last = m_last.load();
			if (!(next <= last)) return std::nullopt;
		} while (!m_next.compare_exchange_weak(next, table_index_t{ next + 1ul }));  ///< Slot number to put the new data into	
		
		auto map_ptr{ m_block_map.load() };						///< Access the block map

		vtll::static_for<size_t, 0, vtll::size<DATA>::value >(	///< Loop over all components
			[&](auto i) {
				using type = vtll::Nth_type<DATA, i>;
				if		constexpr (std::is_move_assignable_v<type>) { std::get<i>(ret) = std::move(*(this->template get_component_ptr<i>(next, map_ptr))); } //move
				else if constexpr (std::is_copy_assignable_v<type>) { std::get<i>(ret) = *(this->template get_component_ptr<i>(next, map_ptr)); } 			//copy
				else if constexpr (vtll::is_atomic<type>::value) { std::get<i>(ret) = this->template get_component_ptr<i>(next, map_ptr)->load(); } 	//atomic

				if constexpr (std::is_destructible_v<type> && !std::is_trivially_destructible_v<type>) { this->template get_component_ptr<i>(next, map_ptr)->~type(); }	///< Call destructor
			}
		);

		table_index_t expected = (next > 0 ? table_index_t{ next - 1ull } : table_index_t{});
		auto consumed{ expected };
		while (!m_consumed.compare_exchange_weak(consumed, next)) { consumed = expected; };

		return ret;		//RVO?
	}

	/// <summary>
	/// Get the number of items currently in the queue.
	/// </summary>
	/// <typeparam name="DATA">Type list of data items.</typeparam>
	/// <typeparam name="N0">Number of items per block.</typeparam>
	/// <typeparam name="ROW">ROW or COLUMN layout.</typeparam>
	/// <typeparam name="MINSLOTS">Default number of MINSLOTS in the first block map.</typeparam>
	/// <returns>Number of items currently in the queue.</returns>
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS>
	inline auto VlltFIFOQueue<DATA, N0, ROW, MINSLOTS>::size() noexcept -> size_t {
		auto last = m_last.load();
		auto consumed = m_consumed.load();
		size_t sz{0};
		if (last.has_value()) {	//have items been produced yet?
			sz += last;			//yes -> count them
			if (consumed.has_value()) sz -= consumed; //have items been consumed yet?
			else ++sz;	//no -> we start at zero, so increase by 1
		}

		return sz;
	}

	/// <summary>
	/// Remove all items from the queue.
	/// </summary>
	/// <typeparam name="DATA">Type list of data items.</typeparam>
	/// <typeparam name="N0">Number of items per block.</typeparam>
	/// <typeparam name="ROW">ROW or COLUMN layout.</typeparam>
	/// <typeparam name="MINSLOTS">Default number of MINSLOTS in the first block map.</typeparam>
	/// <returns>Number of items removed from the queue.</returns>
	template<typename DATA, size_t N0, bool ROW, size_t MINSLOTS>
	inline auto VlltFIFOQueue<DATA, N0, ROW, MINSLOTS>::clear() noexcept -> size_t {
		size_t num = 0;
		while (pop().has_value()) { ++num; }
		
		//auto next_free_slot = m_next_free_slot.load();
		//auto map_ptr{ m_block_map.load() };		///< Shared pointer to current block ptr map, can be nullptr
		//resize(next_free_slot, map_ptr, &m_consumed);

		return num;
	}

}


