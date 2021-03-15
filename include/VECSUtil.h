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

	/**
	* \brief Base class for CRTP, provides easy access to underlying derived class.
	*/
	template <typename T, template<typename...> class crtpType>
	struct VecsCRTP {
		T& underlying() { return static_cast<T&>(*this); }
		T const& underlying() const { return static_cast<T const&>(*this); }

	protected:
		VecsCRTP() {};
		friend crtpType<T>;
	};


	/**
	* \brief Base class for all mono state classes, provides functionality for initialization.
	*/
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


	class VecsReadLock {
	protected:
	public:
		std::atomic<uint32_t>* m_mutex;
		static const uint32_t WRITE = 1 << 24;

		static void lock(std::atomic<uint32_t>* mutex) {
			if (mutex == nullptr) return;
			uint32_t val = mutex->fetch_add(1);
			while (val >= WRITE) {
				val = mutex->fetch_sub(1);
				size_t cnt = 0;
				do {
					if (++cnt > 1000) { 
						cnt = 0;
						std::this_thread::sleep_for(1ns); 
					}
					val = mutex->load();
				} while(val >= WRITE);
				val = mutex->fetch_add(1);
			}
		}

		static void unlock(std::atomic<uint32_t>* mutex) {
			if (mutex == nullptr) return;
			mutex->fetch_sub(1);
		}

		VecsReadLock(std::atomic<uint32_t>* mutex) : m_mutex(mutex) {
			lock(mutex);
		}

		~VecsReadLock() {
			unlock(m_mutex);
		}
	};


	class VecsWriteLock {
	protected:
	public:

		std::atomic<uint32_t>* m_mutex;
		static const uint32_t WRITE = 1 << 24;

		static void lock(std::atomic<uint32_t>* mutex) {
			if (mutex == nullptr) return;
			uint32_t val = mutex->fetch_add(WRITE);
			while (val != 0) {
				val = mutex->fetch_sub(WRITE);
				size_t cnt = 0;
				do {
					if (++cnt > 1000) {
						cnt = 0;
						std::this_thread::sleep_for(1ns);
					}
					val = mutex->load();
				} while (val != 0);
				val = mutex->fetch_add(WRITE);
			}
		}

		static void unlock(std::atomic<uint32_t>* mutex) {
			if (mutex == nullptr) return;
			mutex->fetch_sub(WRITE);
		}

		VecsWriteLock(std::atomic<uint32_t>* mutex) : m_mutex(mutex) {
			lock(mutex);
		}

		~VecsWriteLock() {
			unlock(m_mutex);
		}
	};


};


#endif

