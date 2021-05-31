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

	using map_index_t = int_type<uint32_t, struct P6, std::numeric_limits<uint32_t>::max()>;
	using table_index_t = int_type<uint32_t, struct P7, std::numeric_limits<uint32_t>::max()>;
	using type_index_t = int_type<uint32_t, struct P8, std::numeric_limits<uint32_t>::max()>;


	/**
	* \brief Get the upper 32 bits of a uint64_t value.
	* 
	* \param[in] num The uint64_t value.
	* \returns the upper 32 bits of the input value.
	*/
	uint32_t get_upper( uint64_t num ) {
		return num >> 32;
	}

	/**
	* \brief Get the lower 32 bits of a uint64_t value.
	*
	* \param[in] num The uint64_t value.
	* \returns the lower 32 bits of the input value.
	*/
	uint32_t get_lower(uint64_t num) {
		static const uint64_t BIT_MASK = ((1ULL << 32) - 1ULL);
		return num & BIT_MASK;
	}

	/**
	* \brief Set the upper 32 bits of a uint64_t value.
	*
	* \param[in] num The uint64_t value.
	* \param[in] upper The new 32-bit upper value.
	* \returns the new uint64_t value.
	*/
	uint64_t set_upper(uint64_t num, uint64_t upper) {
		return (upper << 32) + get_lower(num);
	}

	/**
	* \brief Set the lower 32 bits of a uint64_t value.
	*
	* \param[in] num The uint64_t value.
	* \param[in] lower The new 32-bit lower value.
	* \returns the new uint64_t value.
	*/
	uint64_t set_lower(uint64_t num, uint64_t lower) {
		static const uint64_t BIT_MASK = ((1ULL << 32) - 1) << 32ULL;
		return (num & BIT_MASK) | lower;
	}

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
			uint32_t val = mutex->fetch_add(1);				///< fetch old value and add 1 atomically
			while (val >= WRITE) {							///< Is there a writer active?
				val = mutex->fetch_sub(1);					///< Yes - remove own value
				size_t cnt = 0;
				do {
					val = mutex->load();					///< still a writer?
					if (val >= WRITE && ++cnt > 10) {		///< sleep if too often in loop
						cnt = 0;
						mutex->wait(val);
						//std::this_thread::yield();			///< yield the thread
					}
				} while(val >= WRITE);						///< if yes, stay in loop
				val = mutex->fetch_add(1);					///< if no, again try to add 1 to signal reader
			}
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
			uint32_t val = mutex->fetch_add(WRITE);		///< Announce yourself as writer by adding WRITE
			while (val != 0) {							///< Have there been others?
				val = mutex->fetch_sub(WRITE);			///< If yes, remove announcement
				size_t cnt = 0;
				do {
					val = mutex->load();				///< Are there still others?
					if (val != 0 && ++cnt > 10) {
						cnt = 0;
						mutex->wait(val);
						//std::this_thread::yield();		///< Yield the thread
					}
				} while (val != 0);						///< If yes stay in loop
				val = mutex->fetch_add(WRITE);			///< Again get old value and add WRITE
			}
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

