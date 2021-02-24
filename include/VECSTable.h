#ifndef VECSTABLE_H
#define VECSTABLE_H

#include <assert.h>
#include <memory_resource>
#include <shared_mutex>
#include <optional>
#include <array>
#include "VECSUtil.h"

namespace vecs {

	template<typename DATA, size_t L = 10>
	struct VecsTable {
		static const size_t N = 1 << L;
		static const uint64_t BIT_MASK = N - 1;

		using array_tuple_t		= vtll::to_tuple<vtll::transform_size_t<DATA,std::array,N>>;
		using data_tuple_t		= vtll::to_tuple<DATA>;
		using ref_tuple_t		= vtll::to_ref_tuple<DATA>;

		using seg_ptr = std::unique_ptr<array_tuple_t>;

		std::pmr::vector<seg_ptr>	m_segment;
		std::atomic<size_t>			m_size = 0;
		std::atomic<size_t>			m_seg_allocated = 0;
		size_t						m_seg_max = 0;
		std::mutex					m_mutex;

		VecsTable(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept
			: m_segment{ mr }  { max_capacity(r); };

		size_t size() { return m_size.load(); };

		//Externally synchronized
		template<size_t I>
		inline auto comp_ref_idx(index_t n) noexcept -> vtll::Nth_type<DATA,I>& { return std::get<I>(*m_segment[n.value >> L])[n.value & BIT_MASK]; };

		template<typename C>
		inline auto comp_ref_type(index_t n) noexcept -> C& { return std::get<vtll::index_of<DATA,C>::value>(*m_segment[n.value >> L])[n.value & BIT_MASK]; };

		//Externally synchronized
		inline auto tuple_ref(index_t n) noexcept -> ref_tuple_t {
			auto f = [&]<size_t... Is>(std::index_sequence<Is...> is) {
				return std::make_tuple(std::ref(std::get<is>(*m_segment[n.value >> L])[n.value & BIT_MASK])... );
			};
			return f(std::make_index_sequence<vtll::size<DATA>>{});
		};

		//Externally synchronized
		inline auto tuple_value(index_t n) noexcept -> data_tuple_t {
			auto f = [&]<size_t... Is>(std::index_sequence<Is...> is) {
				return std::make_tuple( std::get<is>(*m_segment[n.value >> L])[n.value & BIT_MASK]...);
			};
			return f(std::make_index_sequence<vtll::size<DATA>>{});
		};

		//Internally synchronized
		inline auto allocate_one() -> index_t {
			auto idx = m_size.fetch_add(1);
			if (!reserve(idx+1)) {
				m_size--;
				return index_t{};
			}
			return index_t{ static_cast<decltype(index_t::value)>(idx)};
		}

		//Externally synchronized
		template<typename TDATA>
		requires std::is_same_v<TDATA, data_tuple_t>
		inline auto update(index_t index, TDATA&& data ) -> bool {
			if (index.value >= m_size) return false;
			auto ref = tuple_value(index);
			vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) { std::get<i>(ref) = std::get<i>(data); } );
			return true;
		}

		//Internally synchronized
		auto reserve(size_t r, bool force = false) noexcept -> bool {
			if (force) max_capacity( r );
			if (r == 0 || r > m_seg_max * N) return false;
			if (m_seg_allocated.load() * N < r) {
				const std::lock_guard<std::mutex> lock(m_mutex);
				if (m_seg_allocated.load() * N < r) {
					while (m_segment.size() * N < r) { m_segment.push_back(std::make_unique<array_tuple_t>()); }
					m_seg_allocated = m_segment.size();
				}
			}
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



	/*
	template<typename DATA, size_t L> 
	template<typename TDATA>
	inline auto VecsTable<DATA, L>::insert(TDATA&& data) noexcept -> index_t {

		static_assert(	std::is_same_v<TINFO, INFO> &&  std::is_same_v<TIDX, index_t>
						&& std::is_same_v<TDATA, typename VecsTable<INFO, E, L>::data_tuple_t>);

		{
			const std::lock_guard<std::mutex> lock(m_mutex);

			if (!m_first_free.is_null()) {		//there is an empty slot in the table
				auto free = m_first_free;
				m_first_free = ref_next(free);
				ref(free) = data;
				m_size++;
				return free;
			}

			if (m_size < m_segment.size() * N) {	//append within last segment
				//return push_back3(value);
			}

		}

		if (m_size < m_segment.capacity() * N) { //append new segment, no reallocation
			m_segment.push_back({});
			//return push_back3(value);
		}

		m_segment.push_back({});	//reallocate vector
		//return push_back3(value);
	}

	template<typename DATA, size_t L> 
	inline auto VecsTable<DATA, L>::update(index_t index, DATA&& data) noexcept -> index_t {

		return index_t{};
	}

	template<typename DATA, size_t L> 
	template<typename C>
	inline auto VecsTable<DATA, L>::update(index_t index, C&& info) noexcept -> void {
		static_assert(	std::is_same_v<T, INFO> || std::is_same_v<T, index_t> 
						|| std::is_same_v<T, typename VecsTable<INFO, E, L>::data_tuple_t> );

	}

	template<typename INFO, typename E, size_t L>
	inline auto VecsTable<INFO, E, L>::info(index_t index) noexcept	-> INFO& {

	}

	template<typename INFO, typename E, size_t L>
	inline auto VecsTable<INFO, E, L>::prev(index_t index) noexcept	-> index_t& {

	}

	template<typename INFO, typename E, size_t L>
	inline auto VecsTable<INFO, E, L>::next(index_t index) noexcept	-> index_t& {

	}

	template<typename INFO, typename E, size_t L>
	inline auto VecsTable<INFO, E, L>::data(index_t index) noexcept	-> typename VecsTable<INFO, E, L>::ref_tuple_t& {

	}

	template<typename INFO, typename E, size_t L>
	inline auto VecsTable<INFO, E, L>::invalidate(index_t index)	-> bool {

	}

	template<typename INFO, typename E, size_t L>
	inline auto VecsTable<INFO, E, L>::erase_one()					-> std::tuple<INFO&, index_t> {

	}


	template<typename INFO, typename E, size_t L>
	inline auto VecsTable<INFO,E,L>::reserve(size_t r) noexcept		-> void {
		while (m_segment.size() * N < r) m_segment.push_back({});
	}
	*/


	/*

	template<typename T, size_t L = 8, int SYNC = 2, bool SHRINK = false>
	class VecsVector {
	protected:
		static const size_t N = 1 << L;
		const uint64_t BIT_MASK = N - 1;

		struct VecsTableSegment {
			std::array<T, N> m_entry;
		};

		using seg_ptr = std::unique_ptr<VecsTableSegment>;

		std::pmr::memory_resource*	m_mr = nullptr;
		std::pmr::vector<seg_ptr>	m_segment;
		std::atomic<size_t>			m_size = 0;
		std::shared_timed_mutex 	m_mutex;		//guard reads and writes
		std::mutex 					m_mutex_append;	//guard reads and writes

		T* address(size_t) noexcept;
		size_t push_back2(T&&) noexcept;
		size_t push_back3(T&&) noexcept;

	public:
		VecsVector(std::pmr::memory_resource* mr = std::pmr::new_delete_resource())  noexcept
			: m_mr{ mr }, m_segment{mr}  {};
		std::optional<T>	at(size_t n) noexcept;
		void				set(size_t n, T&& v) noexcept;
		size_t				size() const noexcept { return m_size; };
		size_t				push_back(T&&) noexcept;
		std::optional<T>	pop_back() noexcept;
		void				erase(size_t n) noexcept;
		void				swap(size_t n1, size_t n2) noexcept;
		void				reserve(size_t n) noexcept;
	};

	//---------------------------------------------------------------------------

	template<typename T, size_t L = 8, int SYNC = 2, bool SHRINK = false>
	class VecsTable {
	protected:
		static const size_t N = 1 << L;
		const uint64_t BIT_MASK = N - 1;

		struct VecsTableEntry {
			T		m_data;		//the entry data 
			index_t m_next{};	//use for list of free entries
		};

		struct VecsTableSegment {
			std::array<VecsTableEntry, N> m_entry;
			size_t						m_size = 0;
		};

		using seg_ptr = std::unique_ptr<VecsTableSegment>;

		std::pmr::memory_resource*  m_mr = nullptr;
		std::pmr::vector<seg_ptr>	m_segment;
		std::atomic<size_t>			m_size = 0;
		index_t						m_first_free{};
		std::shared_timed_mutex 	m_mutex;		//guard reads and writes
		std::mutex 					m_mutex_append;	//guard reads and writes

		T* address(size_t) noexcept;
		size_t add2(T&&) noexcept;
		size_t push_back3(T&&) noexcept;

	public:
		VecsTable(std::pmr::memory_resource* mr = std::pmr::new_delete_resource())  noexcept
			: m_mr{ mr }, m_segment{ mr }  {};
		std::optional<T>	at(size_t n) noexcept;
		void				set(size_t n, T&& v) noexcept;
		size_t				size() const noexcept { return m_size; };
		size_t				add(T&&) noexcept;
		void				erase(size_t n) noexcept;
		void				swap(size_t n1, size_t n2) noexcept;
		void				reserve(size_t n) noexcept;
	};

	//---------------------------------------------------------------------------

	template<typename T, typename ID, size_t L = 8, int SYNC = 2, bool SHRINK = false>
	class VecsSlotMap {
	protected:
		static const size_t N = 1 << L;
		const uint64_t BIT_MASK = N - 1;

		struct VecsMapEntry {
			index_t	m_entry_index;
			ID		m_id;
		};

		VecsVector<T, L, 0, SHRINK>			m_entry;
		VecsTable<VecsMapEntry, L, 0, SHRINK>	m_map;
		std::shared_timed_mutex 			m_mutex;		//guard reads and writes
		std::mutex 							m_mutex_append;	//guard appends

	public:
		VecsSlotMap(std::pmr::memory_resource* mr = std::pmr::new_delete_resource())  noexcept
			: m_entry{ mr }, m_map{ mr }  {};
		std::optional<T>	at(size_t n, ID& id) noexcept;
		void				set(size_t n, ID& id, T&& v) noexcept;
		size_t				size() const noexcept { return m_entry.size(); };
		size_t				add(ID&& id, T&& v) noexcept;
		void				erase(size_t n, ID& id) noexcept;
		void				swap(size_t n1, size_t n2) noexcept;
		void				reserve(size_t n) noexcept { m_entry.reserve(n); m_map.reserve(n); };
	};

	*/
}


#endif

