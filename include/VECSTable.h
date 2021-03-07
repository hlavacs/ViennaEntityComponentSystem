#ifndef VECSTABLE_H
#define VECSTABLE_H

#include <assert.h>
#include <memory_resource>
#include <shared_mutex>
#include <optional>
#include <array>
#include "VECSUtil.h"

namespace vecs {

	template<typename DATA, size_t L = 10, bool ROW = false>
	class VecsTable {
		template<typename E> friend class VecsRegistry;
		template<typename E> friend class VecsComponentTable;
		template<typename E, size_t I> friend class VecsComponentTableDerived;

		static const size_t N = 1 << L;
		static const uint64_t BIT_MASK = N - 1;

		using tuple_value_t = vtll::to_tuple<DATA>;
		using tuple_ref_t = vtll::to_ref_tuple<DATA>;

		using array_tuple_t1 = std::array<tuple_value_t, N>;
		using array_tuple_t2 = vtll::to_tuple<vtll::transform_size_t<DATA,std::array,N>>;
		using array_tuple_t  = std::conditional_t<ROW, array_tuple_t1, array_tuple_t2>;

		using seg_ptr = std::unique_ptr<array_tuple_t>;

		std::pmr::vector<seg_ptr>	m_segment;
		std::atomic<size_t>			m_size = 0;
		std::atomic<size_t>			m_seg_allocated = 0;
		size_t						m_seg_max = 0;

	public:
		VecsTable(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept
			: m_segment{ mr }  { max_capacity(r); };

		size_t size() { return m_size.load(); };
		void clear() { m_size = 0; };

		//Externally synchronized
		template<size_t I, typename C = vtll::Nth_type<DATA,I>>
		inline auto comp_ref_idx(index_t n) noexcept -> C& { 
			if constexpr (ROW) {
				return std::get<I>((*m_segment[n.value >> L])[n.value & BIT_MASK]);
			}
			else {
				return std::get<I>(*m_segment[n.value >> L])[n.value & BIT_MASK];
			}
		};

		template<typename C>
		inline auto comp_ref_type(index_t n) noexcept -> C& { 
			if constexpr (ROW) {
				return std::get<vtll::index_of<DATA, C>::value>((*m_segment[n.value >> L])[n.value & BIT_MASK]);
			}
			else {
				return std::get<vtll::index_of<DATA, C>::value>(*m_segment[n.value >> L])[n.value & BIT_MASK];
			}
		};

		//Externally synchronized
		inline auto tuple_ref(index_t n) noexcept -> tuple_ref_t {
			auto f = [&]<size_t... Is>(std::index_sequence<Is...>) {
				if constexpr (ROW) {
					return std::make_tuple(std::ref(std::get<Is>((*m_segment[n.value >> L])[n.value & BIT_MASK]))...);
				}
				else {
					return std::make_tuple(std::ref(std::get<Is>(*m_segment[n.value >> L])[n.value & BIT_MASK])...);
				}
			};
			return f(std::make_index_sequence<vtll::size<DATA>::value>{});
		};

		//Externally synchronized
		inline auto tuple_value(index_t n) noexcept -> tuple_value_t {
			auto f = [&]<size_t... Is>(std::index_sequence<Is...>) {
				if constexpr (ROW) {
					return std::make_tuple(std::get<Is>((*m_segment[n.value >> L])[n.value & BIT_MASK])...);
				}
				else {
					return std::make_tuple(std::get<Is>(*m_segment[n.value >> L])[n.value & BIT_MASK]...);
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
			if (m_seg_allocated.load() * N < r) {
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

