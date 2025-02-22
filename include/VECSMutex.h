#pragma once

namespace vecs {

	//----------------------------------------------------------------------------------------------
	//Mutexes and Locks

	const int LOCKGUARDTYPE_SEQUENTIAL = 0;
	const int LOCKGUARDTYPE_PARALLEL = 1;

	/// @brief An exclusive lock guard for a mutex, meaning that only one thread can lock the mutex at a time.
	/// A LockGuard is used to lock and unlock a mutex in a RAII manner.
	/// In case of two simultaneous locks, the mutexes are locked in the correct order to avoid deadlocks.
	/// @tparam LTYPE Type of the lock guard.
	template<int LTYPE>
		requires (LTYPE == LOCKGUARDTYPE_SEQUENTIAL || LTYPE == LOCKGUARDTYPE_PARALLEL)
	struct LockGuard {

		/// @brief Constructor for a single mutex.
		/// @param mutex Pointer to the mutex.
		LockGuard(mutex_t* mutex) : m_mutex{mutex}, m_other{nullptr} { 
			if constexpr (LTYPE == LOCKGUARDTYPE_PARALLEL) { 
				if(mutex) m_mutex->lock(); 
			}
		}

		/// @brief Constructor for two mutexes. This is necessary if an entity must be moved from one archetype to another, because
		/// the entity components change. In this case, two mutexes must be locked.
		/// @param mutex Pointer to the mutex.
		/// @param other Pointer to the other mutex.
		LockGuard(mutex_t* mutex, mutex_t* other) : m_mutex{mutex}, m_other{other} { 
			if constexpr (LTYPE == LOCKGUARDTYPE_PARALLEL) {
				if(mutex && other) { 
					std::min(m_mutex, m_other)->lock();	///lock the mutexes in the correct order
					std::max(m_mutex, m_other)->lock();				
				} else if(m_mutex) m_mutex->lock();
			}
		}

		/// @brief Destructor, unlocks the mutexes.
		~LockGuard() { 
			if constexpr (LTYPE == LOCKGUARDTYPE_PARALLEL) {
				if(m_mutex && m_other) { 
					std::max(m_mutex, m_other)->unlock();
					std::min(m_mutex, m_other)->unlock();	///lock the mutexes in the correct order
				} else if(m_mutex) m_mutex->unlock();
			}
		}

		mutex_t* m_mutex{nullptr};
		mutex_t* m_other{nullptr};
	};

	/// @brief A lock guard for a shared mutex in RAII manner. Several threads can lock the mutex in shared mode at the same time.
	/// This is used to make sure that data structures are not modified while they are read.
	/// @tparam LTYPE Type of the lock guard.
	template<int LTYPE>
		requires (LTYPE == LOCKGUARDTYPE_SEQUENTIAL || LTYPE == LOCKGUARDTYPE_PARALLEL)
	struct LockGuardShared {

		/// @brief Constructor for a single mutex, locks the mutex.
		LockGuardShared(mutex_t* mutex) : m_mutex{mutex} { 
			if constexpr (LTYPE == LOCKGUARDTYPE_PARALLEL) { m_mutex->lock_shared(); }
		}

		/// @brief Destructor, unlocks the mutex.
		~LockGuardShared() { 
			if constexpr (LTYPE == LOCKGUARDTYPE_PARALLEL) { m_mutex->unlock_shared(); }
		}

		mutex_t* m_mutex{nullptr}; ///< Pointer to the mutex.
	};

	template<int LTYPE>
		requires (LTYPE == LOCKGUARDTYPE_SEQUENTIAL || LTYPE == LOCKGUARDTYPE_PARALLEL)
	struct UnlockGuardShared {

		/// @brief Constructor for a single mutex, locks the mutex.
		template<typename T>
		UnlockGuardShared(T* ptr) { 
			if constexpr (LTYPE == LOCKGUARDTYPE_PARALLEL) { 
				if(ptr!=nullptr) {
					m_mutex = &ptr->GetMutex();
					m_mutex->unlock_shared(); 
				}
			}
		}

		/// @brief Destructor, unlocks the mutex.
		~UnlockGuardShared() { 
			if constexpr (LTYPE == LOCKGUARDTYPE_PARALLEL) { 
				if(m_mutex!=nullptr) m_mutex->lock_shared(); 
			}
		}

		mutex_t* m_mutex{nullptr}; ///< Pointer to the mutex.
	};


}
