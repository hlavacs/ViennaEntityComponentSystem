#ifndef VECSUTIL_H
#define VECSUTIL_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <variant>
#include <chrono>
#include <thread>

#include <IntType.h>
#include <VECSMutex.h>

using namespace std::chrono_literals;

namespace vecs {

	using index16_t = int_type<uint16_t, struct P1, std::numeric_limits<uint16_t>::max()>;
	using index32_t = int_type<uint32_t, struct P2, std::numeric_limits<uint32_t>::max()>;
	using index64_t = int_type<uint64_t, struct P3, std::numeric_limits<uint64_t>::max()>;
	using index_t = index32_t;

	using counter16_t = int_type<uint16_t, struct P4, std::numeric_limits<uint16_t>::max()>;
	using counter32_t = int_type<uint32_t, struct P5, std::numeric_limits<uint32_t>::max()>;
	using counter_t = counter32_t;


	//https://www.fluentcpp.com/2017/05/19/crtp-helper/

	template <typename T, template<typename...> class crtpType>
	struct VecsCRTP {
		T& underlying() { return static_cast<T&>(*this); }
		T const& underlying() const { return static_cast<T const&>(*this); }

	protected:
		VecsCRTP() {};
		friend crtpType<T>;
	};


	template<typename T>
	class VecsMonostate : VecsCRTP<T,VecsMonostate> {
	protected:
		static inline std::atomic<uint32_t> m_init_counter = 0;

		bool init() {
			if (m_init_counter > 0) return false;
			auto cnt = m_init_counter.fetch_add(1);
			if (cnt > 0) return false;
			return true;
		};

	};




};


#endif

