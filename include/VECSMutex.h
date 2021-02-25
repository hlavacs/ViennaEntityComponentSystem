#ifndef VECSMUTEX_H
#define VECSMUTEX_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <variant>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace vecs {

	class VecsSpinLockRead;
	class VecsSpinLockWrite;

	class VecsReadWriteMutex {
		friend class VecsSpinLockRead;
		friend class VecsSpinLockWrite;

	protected:
		std::atomic<uint32_t> m_read = 0;
		std::atomic<uint32_t> m_write = 0;
	public:
		VecsReadWriteMutex() = default;
	};


	class VecsSpinLockWrite {
	protected:
		static const uint32_t m_max_cnt = 1 << 10;
		VecsReadWriteMutex& m_spin_mutex;

	public:
		VecsSpinLockWrite(VecsReadWriteMutex& spin_mutex) : m_spin_mutex(spin_mutex) {
			uint32_t cnt = 0;
			uint32_t w;

			do {
				w = m_spin_mutex.m_write.fetch_add(1);			//prohibit other readers and writers
				if (w == 0) {									//I have the ticket?
					break;
				}

				m_spin_mutex.m_write--;						//undo and try again
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

		~VecsSpinLockWrite() {
			m_spin_mutex.m_write--;
		}
	};


	class VecsSpinLockRead {
	protected:
		static const uint32_t m_max_cnt = 1 << 10;
		VecsReadWriteMutex& m_spin_mutex;

	public:
		VecsSpinLockRead(VecsReadWriteMutex& spin_mutex) : m_spin_mutex(spin_mutex) {
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
				m_spin_mutex.m_read--; //undo reading and try again
			} while (true);
		}

		~VecsSpinLockRead() {
			m_spin_mutex.m_read--;
		}
	};


}




#endif

