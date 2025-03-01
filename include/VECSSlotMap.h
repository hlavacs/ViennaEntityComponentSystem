#pragma once

namespace vecs {
	
	//----------------------------------------------------------------------------------------------
	//Slot Maps

	/// @brief A slot map for storing a map from handle to archetype and index in the archetype.
	/// A slot map can never shrink. If an entity is erased, the slot is added to the free list. A handle holds an index
	/// to the slot map and a version counter. If the version counter of the slot is different from the version counter of the handle,
	/// the slot is invalid.
	/// @tparam T The value type of the slot map.
	template<VecsPOD T>
	class SlotMap {

	public:
		/// @brief A slot in the slot map.
		struct Slot {
			int64_t m_nextFree; //index of the next free slot in the free list. 
			size_t m_version;	//version of the slot
			T 	   m_value{};	//value of the slot

			Slot() = default;

			/// @brief Constructor, creates a slot.
			/// @param next Next free slot in the free list.
			/// @param version Version counter of the slot to avoid accessing erased slots.
			/// @param value Value stored in the slot.
			Slot(const int64_t& next, const size_t& version, T& value) : m_value{value} {
				m_nextFree = next;
				m_version = version;
			}

			Slot(const int64_t&& next, const size_t&& version, T&& value) : m_value{std::forward<T>(value)} {
				m_nextFree = next;
				m_version = version;
			}

			/// @brief Copy operator.
			/// @param other The slot to copy.
			Slot& operator=( const Slot& other ) {
				m_nextFree = other.m_nextFree;
				m_version = other.m_version;
				m_value = other.m_value;
				return *this;
			}
		};

	public:

		/// @brief Constructor, creates the slot map and prefills it with an empty list.
		/// @param size Size of the initial slot map.
		SlotMap(uint32_t storageIndex, int64_t bits) : m_storageIndex{storageIndex} {
			m_firstFree = 0;
			int64_t size = ((int64_t)1l << bits);
			for( int64_t i = 1; i <= size-1; ++i ) { //create the free list
				m_slots.push_back( Slot(int64_t{i}, size_t{0}, T{} ) );
			}
			m_slots.push_back( Slot(int64_t{-1}, size_t{0}, T{}) ); //last slot
		}

		SlotMap( const SlotMap& other ) : m_storageIndex{other.m_storageIndex} {
			m_firstFree = 0;
			int64_t size = other.m_slots.size();
			for( int64_t i = 1; i <= size-1; ++i ) { //create the free list
				m_slots.push_back( Slot( int64_t{i}, size_t{0}, T{} ) );
			}
			m_slots.push_back( Slot(int64_t{-1}, size_t{0}, T{}) ); //last slot
		}
		
		~SlotMap() = default; ///< Destructor.
		
		/// @brief Insert a value to the slot map.
		/// @param value The value to insert.
		/// @return A pair of the handle and reference to the slot.
		auto Insert(T& value) -> std::pair<Handle, Slot&> {
			auto [handle, slot] = Insert2(std::forward<T>(value));
			slot.m_value = value;
			return {handle, slot};		
		}

		auto Insert(T&& value) -> std::pair<Handle, Slot&> {
			auto [handle, slot] = Insert2(std::forward<T>(value));
			slot.m_value = std::forward<T>(value);
			return {handle, slot};				
		}

		/// @brief Erase a value from the slot map.
		/// @param handle The handle of the value to erase.
		void Erase(Handle handle) {
			auto& slot = m_slots[handle.GetIndex()];
			++slot.m_version;	//increment the version to invalidate the slot
			slot.m_nextFree = m_firstFree;	
			m_firstFree = handle.GetIndex(); //add the slot to the free list
			--m_size;
		}

		/// @brief Get a value from the slot map. Do not assert versions here, could be used for writing!
		/// @param handle The handle of the value to get.
		/// @return Reference to the value.
		auto operator[](Handle handle) -> Slot& {
			return m_slots[handle.GetIndex()];
		}

		/// @brief Get the size of the slot map.
		/// @return The size of the slot map.
		auto Size() const -> size_t {
			return m_size;
		}

		/// @brief Clear the slot map. This puts all slots in the free list.
		void Clear() {
			m_firstFree = 0;
			m_size = 0;
			size_t size = m_slots.size();
			for( size_t i = 1; i <= size-1; ++i ) { 
				m_slots[i-1].m_nextFree = i;
				m_slots[i-1].m_version++;
			}
			m_slots[size-1].m_nextFree = -1;
			m_slots[size-1].m_version++;
		}

	private:
		auto Insert2(const T&& value) -> std::pair<Handle, Slot&> {
			int64_t index = m_firstFree;
			Slot* slot = nullptr;
			if( index > -1 ) { 
				slot = &m_slots[index];
				m_firstFree = slot->m_nextFree;
				slot->m_nextFree = -1;
			} else {
				m_slots.push_back( Slot{ int64_t{-1}, size_t{0}, {} } );
				index = m_slots.size() - 1; //index of the new slot
				slot = &m_slots[index];
			}
			++m_size;
			return { Handle{ (uint32_t)index, (uint32_t)slot->m_version, m_storageIndex}, *slot};	
		}

		size_t m_storageIndex{0}; ///< Index of the storage.
		size_t m_size{0}; ///< Size of the slot map. This is the size of the Vector minus the free slots.
		int64_t m_firstFree{-1}; ///< Index of the first free slot. If -1 then there are no free slots.
		Vector<Slot> m_slots; ///< Container of slots.
	};



}
