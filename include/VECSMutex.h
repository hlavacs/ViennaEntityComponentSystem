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

	class VecsSpinLockWrite {
	protected:
		static const uint32_t m_max_cnt = 1 << 10;
		std::atomic<uint32_t>& m_read;
		std::atomic<uint32_t>& m_write;

	public:
		VecsSpinLockWrite(std::atomic<uint32_t>& read, std::atomic<uint32_t>& write) : m_read(read), m_write(write) {
			uint32_t cnt = 0;
			uint32_t w;

			do {
				w = m_write.fetch_add(1);			//prohibit other readers and writers
				if (w == 0) {						//I have the ticket?
					break;
				}

				m_write--;									//undo and try again
				if (++cnt > m_max_cnt) {					//spin or sleep?
					cnt = 0;
					std::this_thread::sleep_for(100ns);		//might sleep a little to take stress from CPU
				}
			} while (true);	//go back and try again

			//a new reader might have slipped into here, or old readers exist
			cnt = 0;
			while (m_read.load() > 0) {		//wait for existing readers to finish
				if (++cnt > m_max_cnt) {
					cnt = 0;
					std::this_thread::sleep_for(100ns);
				}
			}
		}

		~VecsSpinLockWrite() {
			m_write--;
		}
	};


	class VecsSpinLockRead {
	protected:
		static const uint32_t m_max_cnt = 1 << 10;
		std::atomic<uint32_t>& m_read;
		std::atomic<uint32_t>& m_write;

	public:
		VecsSpinLockRead(std::atomic<uint32_t>& read, std::atomic<uint32_t>& write) : m_read(read), m_write(write) {
			uint32_t cnt = 0;

			do {
				while (m_write.load() > 0) {	//wait for writers to finish
					if (++cnt > m_max_cnt) {
						cnt = 0;
						std::this_thread::sleep_for(100ns);//might sleep a little to take stress from CPU
					}
				}
				//writer might have joined until here
				m_read++;	//announce yourself as reader

				if (m_write.load() == 0) { //still no writer?
					break;
				}
				m_read--; //undo reading and try again
			} while (true);
		}

		~VecsSpinLockRead() {
			m_read--;
		}
	};


}




#endif

