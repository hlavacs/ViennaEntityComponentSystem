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

	using counter16_t = int_type<uint16_t, struct P4, std::numeric_limits<uint16_t>::max()>;
	using counter32_t = int_type<uint32_t, struct P5, std::numeric_limits<uint32_t>::max()>;
	using counter_t = counter32_t;

	using map_index_t = int_type<uint32_t, struct P6, std::numeric_limits<uint32_t>::max()>;
	using table_index_t = int_type<uint32_t, struct P7, std::numeric_limits<uint32_t>::max()>;
	using type_index_t = int_type<uint32_t, struct P8, std::numeric_limits<uint32_t>::max()>;


	//-------------------------------------------------------------------------
	//Table segment layout

	using VECS_LAYOUT_ROW = std::integral_constant<bool, true>;		///< Layout is row wise. Good if all components of an entity are needed at once.
	using VECS_LAYOUT_COLUMN = std::integral_constant<bool, false>;	///< Layout is column wise. Good is only single components of entities are needed.
	using VECS_LAYOUT_DEFAULT = VECS_LAYOUT_COLUMN;					///< Default is column wise


	//https://www.fluentcpp.com/2017/05/19/crtp-helper/

	/**
	* \brief Base class for CRTP, provides easy access to underlying derived class.
	*/
	template <typename T, template<typename...> class crtpType>
	struct VecsCRTP {
		T& underlying() { return static_cast<T&>(*this); }						///< \brief Access to underlying type
		T const& underlying() const { return static_cast<T const&>(*this); }	///< \brief Access to underlying type

	protected:
		VecsCRTP() {};		///< Constructor
		friend crtpType<T>;
	};


	/**
	* \brief Base class for all mono state classes, provides functionality for initialization.
	*/
	template<typename T>
	class VecsMonostate : VecsCRTP<T,VecsMonostate> {
	protected:
		static inline std::atomic_flag m_init = ATOMIC_FLAG_INIT;

		/** 
		* \brief Initialize only once.
		*/
		bool init() {
			if (m_init.test()) [[likely]] return false;
			auto init = m_init.test_and_set();
			if (init) return false;
			return true;
		};

	};

	/**
	* \brief Lock for read access that is guarded by an std::atomic<uint32_t>. Reads can happen in parallel.
	*/
	class VecsReadLock {
	protected:
	public:
		std::atomic<uint32_t>* m_mutex;			///< Pointer to the guard
		static const uint32_t WRITE = 1 << 24;	///< Bit offset for storing write access

		/**
		* \brief Create the read lock
		*/
		static void lock(std::atomic<uint32_t>* mutex) {
			if (mutex == nullptr) return;					///< Is the guard valid?
			uint32_t val = mutex->load();
			do {											///< Enter the loop
				size_t cnt = 0;
				while (val >= WRITE) {						///< if writer active, stay in loop
					if (++cnt > 10) {						///< sleep if too often in loop
						cnt = 0;
						mutex->wait(val);
					}
					val = mutex->load();					///< still a writer?
				};	
			} while ( !mutex->compare_exchange_weak( val, val + 1 ) );	///< While a writer is active
		}

		/**
		* \brief Remove the read lock
		*/
		static void unlock(std::atomic<uint32_t>* mutex) {
			if (mutex == nullptr) return;
			--(*mutex);
			mutex->notify_one();	///< Only writers are waiting, so notify one writer.
		}

		/**
		* \brief Constructor
		*/
		VecsReadLock(std::atomic<uint32_t>* mutex) : m_mutex(mutex) {
			lock(mutex);
		}

		/**
		* \brief Destructor
		*/
		~VecsReadLock() {
			unlock(m_mutex);
		}
	};


	/**
	* \brief Lock for write access that is guarded by an std::atomic<uint32_t>. Write blocks all other accesses.
	*/
	class VecsWriteLock {
	protected:
	public:

		std::atomic<uint32_t>* m_mutex;			///< Pointer to the guard
		static const uint32_t WRITE = 1 << 24;	///< Bit offset for storing write access

		/**
		* \brief Create the write lock
		*/
		static void lock(std::atomic<uint32_t>* mutex) {
			if (mutex == nullptr) return;				///< Check if guard ok
			uint32_t val{0};
			do {										///< Have there been others?
				size_t cnt = 0;
				do {
					val = mutex->load();				///< Are there still others?
					if (val != 0 && ++cnt > 10) {
						cnt = 0;
						mutex->wait(val);
					}
				} while (val != 0);						///< If yes stay in loop
			} while ( !mutex->compare_exchange_weak( val, WRITE ) );
		}

		/**
		* \brief Remove the write lock
		*/
		static void unlock(std::atomic<uint32_t>* mutex) {
			if (mutex == nullptr) return;
			mutex->operator-=(WRITE);
			mutex->notify_all();	///< Many readers may wait, notify all of them
		}

		/**
		* \brief Constructor
		*/
		VecsWriteLock(std::atomic<uint32_t>* mutex) : m_mutex(mutex) {
			lock(mutex);
		}

		/**
		* \brief Destructor
		*/
		~VecsWriteLock() {
			unlock(m_mutex);
		}
	};


};


#endif

