#pragma once

#include <VECSMutex.h>

namespace vecs {

	//----------------------------------------------------------------------------------------------
	//Hash Map

	const int HASHMAPTYPE_SEQUENTIAL = 0;
	const int HASHMAPTYPE_PARALLEL = 1;

	/// @brief A hash map for storing a map from key to value.
	/// The hash map is implemented as a vector of buckets. Each bucket is a list of key-value pairs.
	/// Inserting and reading from the hash map is thread-safe, i.e., internally sysnchronized.
		
	template<typename T, int HTYPE = HASHMAPTYPE_SEQUENTIAL>
	class HashMap {

		using mutex_t = std::shared_mutex; ///< Shared mutex type

		/// @brief A key-ptr pair in the hash map.
		struct Pair {
			std::pair<size_t, T> m_keyValue; ///< The key-value pair.
			std::unique_ptr<Pair> m_next{}; ///< Pointer to the next pair.
			size_t& Key() { return m_keyValue.first; }
			T& Value() { return m_keyValue.second; };
		};

		/// @brief A bucket in the hash map.
		struct Bucket {
			std::unique_ptr<Pair> m_first{}; ///< Pointer to the first pair
			mutex_t m_mutex; ///< Mutex for adding something to the bucket.

			Bucket() : m_first{} {};
			Bucket(const Bucket& other) : m_first{} {};
		};

		using Map_t = std::vector<Bucket>; ///< Type of the map.

		/// @brief Iterator for the hash map.
		class Iterator {
		public:
			/// @brief Constructor for the iterator.
			/// @param map The hash map.
			/// @param bucketIdx Index of the bucket.
			Iterator(HashMap& map, size_t bucketIdx) : m_hashMap{map}, m_bucketIdx{bucketIdx}, m_pair{nullptr} {
				if( m_bucketIdx >= m_hashMap.m_buckets.size() ) return;
				m_pair = &m_hashMap.m_buckets[m_bucketIdx].m_first;
				if( !*m_pair ) Next();
			}	

			~Iterator() = default; ///< Destructor.
			auto operator++() {  /// @brief Increment the iterator.
				if( *m_pair ) { m_pair = &(*m_pair)->m_next;  if( *m_pair ) { return *this; } }
				Next(); 
				return *this; 
			}

			auto& operator*() { return (*m_pair)->m_keyValue; } ///< Dereference the iterator.
			auto operator!=(const Iterator& other) -> bool { return m_bucketIdx != other.m_bucketIdx; } ///< Compare the iterator.

			private:

			/// @brief Move to the next bucket.
			void Next() {
				do {++m_bucketIdx; }
				while( m_bucketIdx < m_hashMap.m_buckets.size() && !m_hashMap.m_buckets[m_bucketIdx].m_first );
				if( m_bucketIdx < m_hashMap.m_buckets.size() ) { m_pair = &m_hashMap.m_buckets[m_bucketIdx].m_first; } 
			}

			HashMap& m_hashMap;	///< Reference to the hash map.
			std::unique_ptr<Pair>* m_pair;	///< Pointer to the pair.
			size_t m_bucketIdx;	///< Index of the current bucket.
		};

		public:

		/// @brief Constructor, creates the hash map.
		HashMap(size_t bits = 10) : m_buckets{} {
			size_t size = (1ull << bits);
			for( int i = 0; i < size; ++i ) {
				m_buckets.emplace_back();
			}
		}

		/// @brief Destructor, destroys the hash map.
		~HashMap() = default;

		/// @brief Find a pair in the bucket. If not found, create a new pair with the given key.	
		/// @param key Find this key
		/// @return Pointer to the value.
		T& operator[](size_t key) {
			auto& bucket = m_buckets[key & (m_buckets.size()-1)];
			std::unique_ptr<Pair>* pair;
			{
				LockGuardShared<HTYPE> lock(&bucket.m_mutex);
				pair = Find(&bucket.m_first, key);
				if( (*pair) != nullptr ) { return (*pair)->Value(); }
			}

			LockGuard<HTYPE> lock(&bucket.m_mutex);
			pair = Find(pair, key);
			if( (*pair) != nullptr ) { 	return (*pair)->Value(); }
			*pair = std::make_unique<Pair>( std::make_pair(key, T{}) );
			m_size++;
			return (*pair)->Value();
		}

		/// @brief Get a value from the hash map.
		/// @param key The key of the value.
		/// @return Pointer to the value.
		T* get(size_t key) {
			auto& bucket = m_buckets[key & (m_buckets.size()-1)];
			LockGuardShared<HTYPE> lock(&bucket.m_mutex);
			std::unique_ptr<Pair>* pair = Find(&bucket.m_first, key);
			if( (*pair) != nullptr ) { return (*pair)->Value(); }
			return nullptr;
		}

		/// @brief Test whether the hash map contains a key.
		/// @param key The key to test.
		/// @return true if the key is in the hash map, else false.
		bool contains(size_t key) {
			auto& bucket = m_buckets[key & (m_buckets.size()-1)];
			LockGuardShared<HTYPE> lock(&bucket.m_mutex);
			std::unique_ptr<Pair>* pair = Find(&bucket.m_first, key);
			return (*pair) != nullptr;
		}

		size_t size() { return m_size; } ///< Get the number of items in the hash map.

		auto begin() -> Iterator { return Iterator(*this, 0); }
		auto end() -> Iterator { return Iterator(*this, m_buckets.size()); }

	private:

		/// @brief Find a pair in the bucket.
		/// @param pair Start of the list of pairs.
		/// @param key Find this key
		/// @return Pointer to the pair with the key and value, points to the unqiue_ptr of the pair.
		auto Find(std::unique_ptr<Pair>* pair, size_t key) -> std::unique_ptr<Pair>* {
			while( (*pair) != nullptr && (*pair)->Key() != key ) { pair = &(*pair)->m_next; }
			return pair;
		}

		Map_t m_buckets; ///< The map of buckets.
		size_t m_size{0}; ///< The size of the hash map.
	};

}


