#ifndef VECSTABLE_H
#define VECSTABLE_H

#include <assert.h>
#include <memory_resource>
#include <shared_mutex>
#include <optional>
#include <array>
#include "VECSUtil.h"

namespace vecs {

	template<typename E>
	class VecsComponentTable;

	template<typename INFO, typename E, size_t L = 8>
	class VecsTable {

		template<typename E>
		friend class VecsComponentTable;

	public:
		static const size_t N = 1 << L;
		const uint64_t BIT_MASK = N - 1;
		using array_tuple_t		= vtll::to_tuple<vtll::transform_size_t<E,std::array,N>>;
		using data_tuple_t		= vtll::to_tuple<E>;
		using ref_tuple_t		= vtll::to_ref_tuple<E>;
		using entry_tuple_t		= std::tuple< ID, index_t, data_tuple_t >;
		using entry_ref_tuple_t	= std::tuple< ID&, index_t&, ref_tuple_t >;

	protected:

		struct segment_t {
			std::array<INFO, N>		m_info;		//entry information (handle, mutex, ...)
			std::array<index_t, N>	m_next;		//next free entry index or use as free index
			array_tuple_t			m_data;		//the entry data organized as arrays
		};

		using seg_ptr = std::unique_ptr<segment_t>;

		std::pmr::memory_resource*	m_mr = nullptr;
		std::pmr::vector<seg_ptr>	m_segment;
		std::atomic<size_t>			m_size = 0;
		index_t						m_first_free{};
		VecsReadWriteMutex			m_delete_mutex;

		/*T* address(size_t) noexcept;
		size_t add2(T&&) noexcept;
		size_t push_back3(T&&) noexcept;*/

	public:
		VecsTable(std::pmr::memory_resource* mr = std::pmr::new_delete_resource())  noexcept
			: m_mr{ mr }, m_segment{ mr }  {};

		template<typename TINFO, typename TNEXT, TDATA>
		requires std::is_same_v<TINFO,INFO> && std::is_same_v<TNEXT,index_t> && std::is_same_v<TDATA,data_tuple_t>
		auto insert(TINFO&& info, TINDEX&& index, TDATA&& data) noexcept -> index_t;

		template<typename T>
		requires std::is_same_v<T, TINFO>
		auto update(index_t index, T&& info) noexcept -> void;

		template<typename T>
		requires std::is_same_v<T, index_t>
		auto update(index_t index, T&& next) noexcept -> void;

		template<typename T>
		requires std::is_same_v<T, data_tuple_t>
		auto update(index_t index, T&& data) noexcept -> void;

		auto info(index_t index) noexcept	 -> INFO&;
		auto next(index_t index) noexcept	 -> index_t&;
		auto data(index_t index) noexcept	 -> ref_tuple_t&;
		auto erase(index_t index) noexcept	 -> void;
		auto size() const noexcept			 -> size_t { return m_size; };
		auto reserve(index_t index) noexcept -> void;
		auto compress_one()					 -> std::tuple<INFO&, index_t>;
	};






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

