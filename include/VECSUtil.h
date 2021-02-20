#ifndef VECSUTIL_H
#define VECSUTIL_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <variant>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace vecs {

	template<typename T, typename P, auto D = -1>
	struct int_type {
		using type_name = T;
		static const T null = static_cast<T>(D);

		T value{};
		int_type() {
			static_assert(!(std::is_unsigned_v<T> && std::is_signed_v<decltype(D)> && static_cast<int>(D) < 0));
			value = static_cast<T>(D);
		};
		explicit int_type(const T& t) noexcept : value{ t } {};
		int_type(const int_type<T, P, D>& t) noexcept : value{ t.value } {};
		int_type(int_type<T, P, D>&& t) noexcept : value{ std::move(t.value) } {};
		void operator=(const int_type<T, P, D>& rhs) noexcept { value = rhs.value; };
		void operator=(int_type<T, P, D>&& rhs) noexcept { value = std::move(rhs.value); };
		auto operator<=>(const int_type<T, P, D>& v) const = default;
		auto operator<=>(const T& v) noexcept { return value <=> v; };

		struct hash {
			std::size_t operator()(const int_type<T, P, D>& tg) const { return std::hash<T>()(tg.value); };
		};

		struct equal_to {
			constexpr bool operator()(const T& lhs, const T& rhs) const { return lhs == rhs; };
		};

		bool is_null() {
			return value == null;
		}
	};

	using index_t = int_type<uint32_t, struct P0, std::numeric_limits<uint32_t>::max()>;
	using index16_t = int_type<uint16_t, struct P1, std::numeric_limits<uint16_t>::max()>;

	using counter_t = int_type<uint32_t, struct P2, std::numeric_limits<uint32_t>::max()>;
	using counter16_t = int_type<uint16_t, struct P3, std::numeric_limits<uint16_t>::max()>;


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


	class VeSpinLockRead;
	class VeSpinLockWrite;

	class VeReadWriteMutex {
		friend class VeSpinLockRead;
		friend class VeSpinLockWrite;

	protected:
		std::atomic<uint32_t> m_read = 0;
		std::atomic<uint32_t> m_write = 0;
	public:
		VeReadWriteMutex() = default;
	};


	class VeSpinLockWrite {
	protected:
		static const uint32_t m_max_cnt = 1 << 10;
		VeReadWriteMutex& m_spin_mutex;

	public:
		VeSpinLockWrite(VeReadWriteMutex& spin_mutex) : m_spin_mutex(spin_mutex) {
			uint32_t cnt = 0;
			uint32_t w;

			do {
				w = m_spin_mutex.m_write.fetch_add(1);			//prohibit other readers and writers
				if (w > 1) {									//someone has the write lock?
					m_spin_mutex.m_write--;						//undo and try again
				}
				else
					break;	//you got the ticket, so go on. new readers and writers are blocked now

				if (++cnt > m_max_cnt) {					//spin or sleep?
					cnt = 0;
					std::this_thread::sleep_for(100ns);		//might sleep a little to take stress from CPU
				}
			} while (true);	//go back and try again

			//a new reader might have slipped into here, or old readers exist
			cnt = 0;
			while (m_spin_mutex.m_read.load() > 0) {		//wait for existing readers to finish
				if (++cnt > m_max_cnt) {
					cnt = 0;
					std::this_thread::sleep_for(100ns);
				}
			}
		}

		~VeSpinLockWrite() {
			m_spin_mutex.m_write--;
		}
	};


	class VeSpinLockRead {
	protected:
		static const uint32_t m_max_cnt = 1 << 10;
		VeReadWriteMutex& m_spin_mutex;

	public:
		VeSpinLockRead(VeReadWriteMutex& spin_mutex) : m_spin_mutex(spin_mutex) {
			uint32_t cnt = 0;

			do {
				while (m_spin_mutex.m_write.load() > 0) {	//wait for writers to finish
					if (++cnt > m_max_cnt) {
						cnt = 0;
						std::this_thread::sleep_for(100ns);//might sleep a little to take stress from CPU
					}
				}
				//writer might have joined until here
				m_spin_mutex.m_read++;	//announce yourself as reader

				if (m_spin_mutex.m_write.load() == 0) { //still no writer?
					break;
				}
				else {
					m_spin_mutex.m_read--; //undo reading and try again
					if (++cnt > m_max_cnt) {
						cnt = 0;
						std::this_thread::sleep_for(100ns);//might sleep a little to take stress from CPU
					}
				}
			} while (true);
		}

		~VeSpinLockRead() {
			m_spin_mutex.m_read--;
		}
	};


};


#endif

