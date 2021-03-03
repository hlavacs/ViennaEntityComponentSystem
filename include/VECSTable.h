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
	class VecsTable {
		template<typename E> friend class VecsRegistry;
		template<typename E> friend class VecsComponentTable;
		template<typename E, typename C> friend class VecsComponentTableDerived;

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

	public:
		VecsTable(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept
			: m_segment{ mr }  { max_capacity(r); };

		size_t size() { return m_size.load(); };

		//Externally synchronized
		template<size_t I, typename C = vtll::Nth_type<DATA,I>>
		inline auto comp_ref_idx(index_t n) noexcept -> C& { 
			return std::get<I>(*m_segment[n.value >> L])[n.value & BIT_MASK]; 
		};

		template<typename C>
		inline auto comp_ref_type(index_t n) noexcept -> C& { 
			return std::get<vtll::index_of<DATA,C>::value>(*m_segment[n.value >> L])[n.value & BIT_MASK]; 
		};

		//Externally synchronized
		inline auto tuple_ref(index_t n) noexcept -> ref_tuple_t {
			auto f = [&]<size_t... Is>(std::index_sequence<Is...>) {
				return std::make_tuple(std::ref(std::get<Is>(*m_segment[n.value >> L])[n.value & BIT_MASK])... );
			};
			return f(std::make_index_sequence<vtll::size<DATA>::value>{});
		};

		//Externally synchronized
		inline auto tuple_value(index_t n) noexcept -> data_tuple_t {
			auto f = [&]<size_t... Is>(std::index_sequence<Is...>) {
				return std::make_tuple( std::get<Is>(*m_segment[n.value >> L])[n.value & BIT_MASK]... );
			};
			return f(std::make_index_sequence<vtll::size<DATA>::value>{});
		};

		//Internally synchronized
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
		requires std::is_same_v<TDATA, data_tuple_t>
		inline auto push_back(TDATA&& data) -> index_t {
			auto idx = m_size.fetch_add(1);
			if (!reserve(idx + 1)) {
				m_size--;
				return index_t{};
			}
			decltype(auto) ref = tuple_ref(index_t{ idx });
			vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) { std::get<i>(ref) = std::get<i>(data); });
			return index_t{ static_cast<decltype(index_t::value)>(idx) };
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
		template<typename TDATA>
		requires std::is_same_v<TDATA, data_tuple_t>
		inline auto update(index_t index, TDATA&& data ) -> bool {
			if (index.value >= m_size) return false;
			decltype(auto) ref = tuple_ref(index);
			vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) { std::get<i>(ref) = std::get<i>(data); } );
			return true;
		}

		//Internally synchronized
		auto reserve(size_t r) noexcept -> bool {
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

}


#endif

