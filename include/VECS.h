#pragma once

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <variant>
#include <array>
#include <deque>
#include <memory>
#include <optional>
#include <functional>
#include <chrono>
#include <ranges>
#include <map>
#include <set>
#include <any>
#include <cassert>
#include <bitset>
#include <algorithm>    // std::sort
#include <shared_mutex>
#include <thread>
#include <VTLL.h>
#include <VSTY.h>


using namespace std::chrono_literals;


namespace vecs {

	//----------------------------------------------------------------------------------------------
	//Handles

	/// @brief A handle for an entity or a component.
	template<size_t INDEX_BITS=32, size_t VERSION_BITS=24, size_t STORAGE_BITS=8>
	struct HandleT {
		using type_t = typename vsty::strong_type_t<size_t, vsty::counter<>, 
							std::integral_constant<size_t, std::numeric_limits<size_t>::max()>>; ///< Strong type for the handle type.

	public:
		HandleT() = default; ///< Default constructor.

		HandleT(size_t index, size_t version, size_t storageIndex=0) : 
			m_value{std::numeric_limits<size_t>::max()} { 
			m_value.set_bits(index, 0, INDEX_BITS);
			m_value.set_bits(version, INDEX_BITS, VERSION_BITS);
			m_value.set_bits(storageIndex, INDEX_BITS + VERSION_BITS, STORAGE_BITS);
		};

		size_t GetIndex() const { return m_value.get_bits(0, INDEX_BITS); }
		size_t GetVersion() const { return m_value.get_bits(INDEX_BITS, VERSION_BITS); }
		size_t GetStorageIndex() const { return m_value.get_bits(INDEX_BITS + VERSION_BITS); }
		size_t GetVersionedIndex() const { return (GetVersion() << VERSION_BITS) + GetIndex(); }
		bool IsValid() const { return m_value != std::numeric_limits<size_t>::max(); }
		HandleT& operator=(const HandleT& other) { m_value = other.m_value; return *this; }
		bool operator==(const HandleT& other) const { return GetIndex() == other.GetIndex() && GetVersion() == other.GetVersion(); }
		bool operator!=(const HandleT& other) const { return !(*this == other); }
		bool operator<(const HandleT& other) const { return GetIndex() < other.GetIndex(); }

	private:
		type_t m_value; ///< Strong type for the handle.
	};

	using Handle = HandleT<32,24,8>; ///< Type of the handle.

	inline bool IsValid(const Handle& handle) {
		return handle.IsValid();
	}

	inline std::ostream& operator<<(std::ostream& os, const vecs::Handle& handle) {
    	return os << "{" <<  handle.GetIndex() << ", " << handle.GetVersion() << ", " << handle.GetStorageIndex() << "}"; 
	}
	

	//----------------------------------------------------------------------------------------------
	//Registry concepts and types

	template<typename U>
	concept VecsPOD = (!std::is_reference_v<U> && !std::is_pointer_v<U>);
	
	template<typename... Ts>
	concept VecsArchetype = (vtll::unique<vtll::tl<Ts...>>::value && (sizeof...(Ts) > 0) && (!std::is_same_v<Handle, std::decay_t<Ts>> && ...));

	template<typename... Ts>
	concept VecsView = ((vtll::unique<vtll::tl<Ts...>>::value) && (sizeof...(Ts) > 0) && (!std::is_same_v<Handle&, Ts> && ...));

	template<typename... Ts>
	concept VecsIterator = (vtll::unique<vtll::tl<Ts...>>::value);

	template<typename... Ts> requires VecsIterator<Ts...> class Iterator;
	template<typename... Ts> requires VecsView<Ts...> class View;

	//----------------------------------------------------------------------------------------------
	//Convenience functions

	template<typename>
	struct is_std_vector : std::false_type {};

	template<typename T, typename A>
	struct is_std_vector<std::vector<T,A>> : std::true_type {};

	template <typename> struct is_tuple : std::false_type {};
	template <typename ...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};

	/// @brief Compute the hash of a list of hashes. If stored in a vector, make sure that hashes are sorted.
	/// @tparam T Container type of the hashes.
	/// @param hashes Reference to the container of the hashes.
	/// @return Overall hash made from the hashes.
	template <typename T>
	inline size_t Hash( T& hashes ) {
		std::size_t seed = 0;
		if constexpr ( is_std_vector<std::decay_t<T>>::value ) {
			std::ranges::sort(hashes);
		}
		for( auto& v : hashes ) {
			seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
		}
		return seed;
	}

	template <typename T>
	inline size_t Hash( T&& hashes ) {
		std::size_t seed = 0;
		if constexpr ( is_std_vector<std::decay_t<T>>::value ) {
			std::ranges::sort(hashes);
		}
		for( auto& v : hashes ) {
			seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
		}
		return seed;
	}

	template<typename T>
	inline auto Type() -> std::size_t {
		return std::type_index(typeid(T)).hash_code();
	}


	template<typename... Ts>
	struct Yes {};

	template<typename... Ts>
	struct No {};

	//----------------------------------------------------------------------------------------------
	//Mutexes and Locks

	using mutex_t = std::shared_mutex; ///< Shared mutex type

	using LockGuardType = int; ///< Type of the lock guard.
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


	//----------------------------------------------------------------------------------------------
	//Segmented Vector

	template<typename T>
		requires (!std::is_reference_v<T> && !std::is_pointer_v<T>)
	class Vector;

	class VectorBase {

		/// @brief Iterator for the vector.
		class Iterator {
		public:
			Iterator(VectorBase& data, size_t index) : m_data{data}, m_index{index} {}
			Iterator& operator++() { ++m_index; return *this; }
			bool operator!=(const Iterator& other) const { return m_index != other.m_index; }
			void* operator*() { return &m_data; }	
		private:
			VectorBase& m_data;
			size_t m_index{0};
		};

		public:
		VectorBase() = default; //constructor
		virtual ~VectorBase() = default; //destructor
			
		/// @brief Insert a new component value.
		/// @param v The component value.
		/// @return The index of the new component value.
		template<typename T>
		auto push_back(T&& v) -> size_t {
			return static_cast<Vector<T>*>(this)->push_back(std::forward<T>(v));
		}

		virtual auto push_back() -> size_t = 0;
		virtual auto pop_back() -> void = 0; 
		virtual auto erase(size_t index) -> size_t = 0;
		virtual void copy(VectorBase* other, size_t from) = 0;
		virtual void swap(size_t index1, size_t index2) = 0;
		virtual auto size() const -> size_t = 0;
		virtual auto clone() -> std::unique_ptr<VectorBase> = 0;
		virtual void clear() = 0;
		virtual void print() = 0;

		auto begin() -> Iterator { return Iterator{*this, 0}; }
		auto end() -> Iterator { return Iterator{*this, size()}; }
	}; //end of VectorBase


	/// @brief A vector that stores elements in segments to avoid reallocations. The size of a segment is 2^segmentBits.
	template<typename T>
		requires (!std::is_reference_v<T> && !std::is_pointer_v<T>)
	class Vector : public VectorBase {

		using Segment_t = std::shared_ptr<std::vector<T>>;
		using Vector_t = std::vector<Segment_t>;

		public:

			/// @brief Iterator for the vector.
			class Iterator {
				public:
				Iterator(Vector<T>& data, size_t index) : m_data{data}, m_index{index} {}
				Iterator& operator++() { ++m_index; return *this; }
				bool operator!=(const Iterator& other) const { return m_index != other.m_index; }
				T& operator*() { return m_data[m_index]; }	

				private:
				Vector<T>& m_data;
				size_t m_index{0};
			};

			/// @brief Constructor, creates the vector.
			/// @param segmentBits The number of bits for the segment size.
			Vector(size_t segmentBits = 6) : m_size{0}, m_segmentBits(segmentBits), m_segmentSize{1ul<<segmentBits}, m_segments{} {
				assert(segmentBits > 0);
				m_segments.emplace_back( std::make_shared<std::vector<T>>(m_segmentSize) );
			}

			~Vector() = default;

			Vector( const Vector& other) : m_size{other.m_size}, m_segmentBits(other.m_segmentBits), m_segmentSize{other.m_segmentSize}, m_segments{} {
				m_segments.emplace_back( std::make_shared<std::vector<T>>(m_segmentSize) );
			}

			/// @brief Push a value to the back of the vector.
			/// @param value The value to push.
			template<typename U>
			auto push_back(U&& value) -> size_t {
				while( Segment(m_size) >= m_segments.size() ) {
					m_segments.emplace_back( std::make_shared<std::vector<T>>(m_segmentSize) );
				}
				++m_size;
				(*this)[m_size - 1] = std::forward<T>(value);
				return m_size - 1;
			}

			auto push_back() -> size_t override {
				return push_back(T{});
			}

			/// @brief Pop the last value from the vector.
			void pop_back() override {
				assert(m_size > 0);
				--m_size;
				if(	Offset(m_size) == 0 && m_segments.size() > 1 ) {
					m_segments.pop_back();
				}
			}

			/// @brief Get the value at an index.
			/// @param index The index of the value.
			auto operator[](size_t index) const -> T& {
				assert(index < m_size);
				auto seg = Segment(index);
				auto off = Offset(index);
				return (*m_segments[Segment(index)])[Offset(index)];
			}

			/// @brief Get the value at an index.
			auto size() const -> size_t override { return m_size; }

			/// @brief Clear the vector. Make sure that one segment is always available.
			void clear() override {
				m_size = 0;
				m_segments.clear();
				m_segments.emplace_back( std::make_shared<std::vector<T>>(m_segmentSize) );
			}

			/// @brief Erase an entity from the vector.
			auto erase(size_t index) -> size_t override {
				size_t last = size() - 1;
				assert(index <= last);
				if( index < last ) {
					(*this)[index] = std::move( (*this)[last] ); //move the last entity to the erased one
				}
				pop_back(); //erase the last entity
				return last; //if index < last then last element was moved -> correct mapping 
			}

			/// @brief Copy an entity from another vector to this.
			void copy(VectorBase* other, size_t from) override {
				push_back( (static_cast<Vector<T>*>(other))->operator[](from) );
			}

			/// @brief Swap two entities in the vector.
			void swap(size_t index1, size_t index2) override {
				std::swap( (*this)[index1], (*this)[index2] );
			}

			/// @brief Clone the vector.
			auto clone() -> std::unique_ptr<VectorBase> override {
				return std::make_unique<Vector<T>>();
			}

			/// @brief Print the vector.
			void print() override {
				std::cout << "Name: " << typeid(T).name() << " ID: " << Type<T>();
			}

			auto begin() -> Iterator { return Iterator{*this, 0}; }
			auto end() -> Iterator { return Iterator{*this, m_size}; }

		private:

			/// @brief Compute the segment index of an entity index.
			/// @param index Entity index.
			/// @return Index of the segment.
			inline size_t Segment(size_t index) const { return index >> m_segmentBits; }

			/// @brief Compute the offset of an entity index in a segment.
			/// @param index Entity index.
			/// @return Offset in the segment.
			inline size_t Offset(size_t index) const { return index & (m_segmentSize-1ul); }

			size_t m_size{0};	///< Size of the vector.
			size_t m_segmentBits;	///< Number of bits for the segment size.
			size_t m_segmentSize; ///< Size of a segment.
			Vector_t m_segments{10};	///< Vector holding unique pointers to the segments.
	}; //end of Vector


	//----------------------------------------------------------------------------------------------
	//Slot Maps


	/// @brief A slot map for storing a map from handle to archetype and index in the archetype.
	/// A slot map can never shrink. If an entity is erased, the slot is added to the free list. A handle holds an index
	/// to the slot map and a version counter. If the version counter of the slot is different from the version counter of the handle,
	/// the slot is invalid.
	/// @tparam T The value type of the slot map.
	template<VecsPOD T>
	class SlotMap {

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
			int64_t size = 1 << bits;
			for( int64_t i = 1; i <= size-1; ++i ) { //create the free list
				m_slots.push_back( Slot( int64_t{i}, size_t{0uL}, T{} ) );
			}
			m_slots.push_back( Slot(int64_t{-1}, size_t{0}, T{}) ); //last slot
		}

		SlotMap( const SlotMap& other ) : m_storageIndex{other.m_storageIndex} {
			m_firstFree = 0;
			int64_t size = other.m_slots.size();
			for( int64_t i = 1; i <= size-1; ++i ) { //create the free list
				m_slots.push_back( Slot( int64_t{i}, size_t{0uL}, T{} ) );
			}
			m_slots.push_back( Slot(int64_t{-1}, size_t{0}, T{}) ); //last slot
		}
		
		~SlotMap() = default; ///< Destructor.
		
		/// @brief Insert a value to the slot map.
		/// @param value The value to insert.
		/// @return A pair of the handle and reference to the slot.
		auto Insert(T&& value) -> std::pair<Handle, Slot&> {
			int64_t index = m_firstFree;
			Slot* slot = nullptr;
			if( index > -1 ) { 
				slot = &m_slots[index];
				m_firstFree = slot->m_nextFree;
				slot->m_nextFree = -1;
			} else {
				m_slots.push_back( Slot{ int64_t{-1}, size_t{0}, std::forward<T>(value) } );
				index = m_slots.size() - 1; //index of the new slot
				slot = &m_slots[index];
			}
			slot->m_value = std::forward<T>(value);
			++m_size;
			return { Handle{ (uint32_t)index, (uint32_t)slot->m_version, m_storageIndex}, *slot};			
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

		/// @brief Get a value from the slot map.
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
		size_t m_storageIndex{0}; ///< Index of the storage.
		size_t m_size{0}; ///< Size of the slot map. This is the size of the Vector minus the free slots.
		int64_t m_firstFree{-1}; ///< Index of the first free slot. If -1 then there are no free slots.
		Vector<Slot> m_slots; ///< Container of slots.
	};

	//----------------------------------------------------------------------------------------------
	//Registry 

	const int REGISTRYTYPE_SEQUENTIAL = 0;
	const int REGISTRYTYPE_PARALLEL = 1;

	template<int T>
	concept RegistryType = (T == REGISTRYTYPE_SEQUENTIAL || T == REGISTRYTYPE_PARALLEL);

	/// @brief A registry for entities and components.
	template <int RTYPE>
		requires RegistryType<RTYPE>
	class Registry {

		/// @brief Entry for the seach cache
		struct TypeSetAndHash {
			std::set<size_t> m_types;	//set of types that have been searched for
			size_t m_hash;				//hash of the set
		};

		template<VecsPOD T>
		struct SlotMapAndMutex {
			SlotMap<T> m_slotMap;
			mutex_t m_mutex;
			SlotMapAndMutex( uint32_t storageIndex, uint32_t bits ) : m_slotMap{storageIndex, bits}, m_mutex{} {};
			SlotMapAndMutex( const SlotMapAndMutex& other ) : m_slotMap{other.m_slotMap}, m_mutex{} {};
		};

		using NUMBER_SLOTMAPS = std::conditional_t< RTYPE == REGISTRYTYPE_SEQUENTIAL, 
			std::integral_constant<int, 1>, std::integral_constant<int, 16>>;
	
		//----------------------------------------------------------------------------------------------
		//Archetype

		/// @brief An archetype of entities with the same components. 
		/// All entities that have the same components are stored in the same archetype. 
		/// The components are stored in the component maps. Note that the archetype class is not templated,
		/// but some methods including a constructor are templated. Thus the class knows only type indices
		/// of its components, not the types themselves.
		class Archetype;

		/// @brief A pair of an archetype and an index. This is stored in the slot map.
		struct ArchetypeAndIndex {
			Archetype* m_archetypePtr;	//pointer to the archetype
			size_t m_archIndex;			//index of the entity in the archetype
		};

		/// @brief An archetype of entities with the same components. 
		/// All entities that have the same components are stored in the same archetype.
		class Archetype {

			template <int T> requires RegistryType<T> friend class Registry;

		public:

			Archetype() = default; //default constructor

			/// @brief Constructor, called if a new entity should be created with components, and the archetype does not exist yet.
			/// @tparam ...Ts 
			/// @param handle The handle of the entity.
			/// @param ...values The values of the components.
			template<typename... Ts>
				requires VecsArchetype<Ts...>
			Archetype(Handle handle, size_t& archIndex, Ts&& ...values ) {
				(AddComponent<Ts>(), ...); //insert component types
				(AddValue( std::forward<Ts>(values) ), ...); //insert all components, get index of the handle
				AddComponent<Handle>(); //insert the handle
				archIndex = AddValue( handle ); //insert the handle
			}

			/// @brief Insert a new entity with components to the archetype.
			/// @tparam ...Ts Value types of the components.
			/// @param handle The handle of the entity.
			/// @param ...values The values of the components.
			/// @return The index of the entity in the archetype.
			template<typename... Ts>
				requires VecsArchetype<Ts...>
			size_t Insert(Handle handle, Ts&& ...values ) {
				assert( m_maps.size() == sizeof...(Ts) + 1 );
				assert( (m_maps.contains(Type<Ts>()) && ...) );
				(AddValue( std::forward<Ts>(values) ), ...); //insert all components, get index of the handle
				return AddValue( handle ); //insert the handle
			}

			/// @brief Get referece to the types of the components.
			/// @return A reference to the container of the types.
			[[nodiscard]] const auto&  Types() const {
				return m_types;
			}

			/// @brief Test if the archetype has a component.
			/// @param ti Hash of the type index of the component.
			/// @return true if the archetype has the component, else false.
			bool Has(const size_t ti) {
				return m_types.contains(ti);
			}

			/// @brief Get component value of an entity. 
			/// @tparam T The type of the component.
			/// @param archIndex The index of the entity in the archetype.
			/// @return The component value.
			template<typename T>
			[[nodiscard]] auto Get(size_t archIndex) -> T& {
				return (*Map<T>())[archIndex];
			}

			/// @brief Get component values of an entity.
			/// @tparam ...Ts Types of the components to get.
			/// @param handle Handle of the entity.
			/// @return A tuple of the component values.
			template<typename... Ts>
				requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value))
			[[nodiscard]] auto Get(size_t archIndex) -> std::tuple<Ts&...> {
				return std::tuple<std::decay_t<Ts>&...>{ Map<std::decay_t<Ts>>()->Get(archIndex)... };
			}

			/// @brief Erase an entity
			/// @param index The index of the entity in the archetype.
			/// @param slotmaps The slot maps vector of the registry.
			void Erase(size_t index, auto& slotmaps) {
				Erase2(index, slotmaps);
			}

			/// @brief Move components from another archetype to this one.
			size_t Move( auto& types, size_t other_index, Archetype& other, auto& slotmaps) {			
				for( auto& ti : types ) { //go through all maps
					if( m_maps.contains(ti) ) m_maps[ti]->copy(other.Map(ti), other_index); //insert the new value
				}
				other.Erase2(other_index, slotmaps); //erase from old component map
				++other.m_changeCounter;
				++m_changeCounter;
				return m_maps[Type<Handle>()]->size() - 1; //return the index of the new entity
			}

			/// @brief Swap two entities in the archetype.
			void Swap(ArchetypeAndIndex& slot1, ArchetypeAndIndex& slot2) {
				assert( slot1.m_value.m_archetypePtr == slot2.m_value.m_archetypePtr );
				for( auto& map : m_maps ) {
					map.second->swap(slot1.m_value.m_archindex, slot2.m_value.m_archindex);
				}
				std::swap(slot1.m_value.m_archindex, slot2.m_value.m_archindex);
				++m_changeCounter;
			}

			/// @brief Clone the archetype.
			/// @param other The archetype to clone.
			/// @param types The types of the components to clone.
			void Clone(Archetype& other, const auto& types) {
				for( auto& ti : types ) { //go through all maps
					m_types.insert(ti); //add the type to the list
					if( other.m_maps.contains(ti) ) m_maps[ti] = other.Map(ti)->clone(); //make a component map like this one
				}
			}

			/// @brief Get the number of entites in this archetype.
			/// @return The number of entities.
			size_t Size() {
				return m_maps[Type<Handle>()]->size();
			}

			/// @brief Clear the archetype.
			void Clear() {
				for( auto& map : m_maps ) {
					map.second->clear();
				}
				++m_changeCounter;
			}

			/// @brief Print the archetype.
			void Print() {
				std::cout << "Archetype: " << Hash(m_types) << std::endl;
				for( auto ti : m_types ) {
					std::cout << "Type: " << ti << " ";
				}
				std::cout << std::endl;
				for( auto& map : m_maps ) {
					std::cout << "Map: ";
					map.second->print();
					std::cout << std::endl;
				}
				std::cout << "Entities: ";
				auto map = Map<Handle>();
				for( auto handle : *map ) {
					std::cout << handle << " ";
				}
				std::cout << std::endl << std::endl;
			}

			/// @brief Validate the archetype. Make sure all maps have the same size.
			void Validate() {
				for( auto& map : m_maps ) {
					assert( map.second->size() == m_maps[Type<Handle>()]->size() );
				}
			}
		
			/// @brief Get the change counter of the archetype. It is increased when a change occurs
			/// that might invalidate a Ref object, e.g. when an entity is moved to another archetype, or erased.
			auto GetChangeCounter() -> size_t {
				return m_changeCounter;
			}

			/// @brief Get the mutex of the archetype.
			/// @return Reference to the mutex.
			[[nodiscard]] auto GetMutex() -> mutex_t& {
				return m_mutex;
			}

			void AddType(size_t ti) {
				assert( !m_types.contains(ti) );
				m_types.insert(ti);	//add the type to the list
			};


		private:

			/// @brief Add a new component to the archetype.
			/// @tparam T The type of the component.
			template<typename U>
			void AddComponent() {
				using T = std::decay_t<U>; //remove pointer or reference
				size_t ti = Type<T>();
				assert( !m_types.contains(ti) );
				m_types.insert(ti);	//add the type to the list
				m_maps[ti] = std::make_unique<Vector<T>>(); //create the component map
			};

			/// @brief Add a new component value to the archetype.
			/// @param v The component value.
			/// @return The index of the component value.
			template<typename U>
			auto AddValue( U&& v ) -> size_t {
				using T = std::decay_t<U>;
				return m_maps[Type<T>()]->push_back(std::forward<T>(v));	//insert the component value
			};

			auto AddEmptyValue( size_t ti ) -> size_t {
				return m_maps[ti]->push_back();	//insert the component value
			};

			/// @brief Erase an entity. To ensure thet consistency of the entity indices, the last entity is moved to the erased one.
			/// This might result in a reindexing of the moved entity in the slot map. Thus we need a ref to the slot map
			/// @param index The index of the entity in the archetype.
			/// @param slotmaps Reference to the slot maps vector of the registry.
			void Erase2(size_t index, auto& slotmaps) {
				size_t last{index};
				for( auto& it : m_maps ) {
					last = it.second->erase(index); //Erase from the component map
				}
				if( index < last ) {
					auto& lastHandle = static_cast<Vector<Handle>*>(m_maps[Type<Handle>()].get())->operator[](index);
					slotmaps[lastHandle.GetStorageIndex()].m_slotMap[lastHandle].m_value.m_archIndex = index;
				}
				++m_changeCounter;
			}
			
			/// @brief Get the map of the components.
			/// @tparam T The type of the component.
			/// @return Pointer to the component map.
			template<typename U>
			auto Map() -> Vector<std::decay_t<U>>* {
				auto it = m_maps.find(Type<std::decay_t<U>>());
				assert(it != m_maps.end());
				return static_cast<Vector<std::decay_t<U>>*>(it->second.get());
			}

			/// @brief Get the data of the components.
			/// @param ti Type index of the component.
			/// @return Pointer to the component map base class.
			auto Map(size_t ti) -> VectorBase* {
				auto it = m_maps.find(ti);
				assert(it != m_maps.end());
				return it->second.get();
			}

			using Size_t = std::conditional_t<RTYPE == REGISTRYTYPE_SEQUENTIAL, std::size_t, std::atomic<std::size_t>>;
			using Map_t = std::unordered_map<size_t, std::unique_ptr<VectorBase>>;

			mutex_t 			m_mutex; //mutex for thread safety
			Size_t 				m_changeCounter{0}; //changes invalidate references
			std::set<size_t> 	m_types; //types of components
			Map_t 				m_maps; //map from type index to component data
		}; //end of Archetype

	public:	
		
		//----------------------------------------------------------------------------------------------
		//Hash Map

		/// @brief A hash map for storing a map from key to value.
		/// The hash map is implemented as a vector of buckets. Each bucket is a list of key-value pairs.
		/// Inserting and reading from the hash map is thread-safe, i.e., internally sysnchronized.
		
		template<typename T>
		class HashMap {

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
				size_t size = 1<<bits;
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
					LockGuardShared<RTYPE> lock(&bucket.m_mutex);
					pair = Find(&bucket.m_first, key);
					if( (*pair) != nullptr ) { return (*pair)->Value(); }
				}

				LockGuard<RTYPE> lock(&bucket.m_mutex);
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
				LockGuardShared<RTYPE> lock(&bucket.m_mutex);
				std::unique_ptr<Pair>* pair = Find(&bucket.m_first, key);
				if( (*pair) != nullptr ) { return (*pair)->Value(); }
				return nullptr;
			}

			/// @brief Test whether the hash map contains a key.
			/// @param key The key to test.
			/// @return true if the key is in the hash map, else false.
			bool contains(size_t key) {
				auto& bucket = m_buckets[key & (m_buckets.size()-1)];
				LockGuardShared<RTYPE> lock(&bucket.m_mutex);
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


		//----------------------------------------------------------------------------------------------

		/// @brief A structure holding a pointer to an archetype and the current size of the archetype.
		struct ArchetypeAndSize {
			Archetype* 	m_archetype;	//pointer to the archetype
			size_t 		m_size;			//size of the archetype
			ArchetypeAndSize(Archetype* arch, size_t size) : m_archetype{arch}, m_size{size} {}
		};

		/// @brief Used for iterating over entity components. Iterators are created by a view. 
		/// When iterating over an archetype, the archetype is locked in shared mode to prevent changes.
		/// The iterator unlocks the archetype when it changes to another archetype or when it is destroyed.
		/// Because of the shared lock, the iterator can be used in parallel for different archetypes, but 
		/// there are opertions that must be delayed until the shared lock is freed.
		/// @tparam ...Ts Choose the types of the components you want the entities to have.
		template<typename... Ts>
			requires VecsIterator<Ts...>
		class Iterator {

		public:
			/// @brief Iterator constructor saving a list of archetypes and the current index.
			/// @param arch List of archetypes. 
			/// @param archidx First archetype index.
			Iterator( Registry<RTYPE>& system, std::vector<ArchetypeAndSize>& arch, size_t archidx) 
				: m_system(system), m_archetypes{arch}, m_archidx{archidx} {
					
				system.increaseIterators(); //Tell the system that an iterator is created
				if( m_archetypes.size() > 0 ) { 
					m_size = std::min(m_archetypes.front().m_size, m_archetypes.front().m_archetype->Size());
				}
				Next(); //go to the first entity
			}

			/// @brief Copy constructor.
			Iterator(const Iterator& other) : m_system{other.m_system}, m_archetypes{other.m_archetypes}, 
				m_archidx{other.m_archidx}, m_size{other.m_size}, m_entidx{other.m_entidx}, m_isLocked{false} {
				m_system.increaseIterators();
			}

			/// @brief Destructor, unlocks the archetype.
			~Iterator() {
				UnlockShared();
				if( m_system.decreaseIterators() == 0 ) m_system.RunDelayedTransactions();
			}

			/// @brief Prefix increment operator.
			auto operator++() {
				++m_entidx;
				Next();				
				return *this;
			}

			/// @brief Access the content the iterator points to.
			auto operator*() {
				assert( m_archetype && m_entidx < m_size );
				Handle handle = (*m_mapHandle)[m_entidx];
				auto tup = std::make_tuple( Get<Ts>(m_system, handle, m_archetype)... );
				if constexpr (sizeof...(Ts) == 1) { return std::get<0>(tup); }
				else return tup;
			}

			/// @brief Compare two iterators.
			auto operator!=(const Iterator& other) -> bool {
				return (m_archidx != other.m_archidx) || (m_entidx != other.m_entidx);
			}

		private:

			/// @brief Get a component value from the archetype.
			template<typename U>
				requires (!std::is_same_v<U, Handle&>) // && !std::is_reference_v<U>)
			[[nodiscard]] auto Get(auto &system, Handle handle, Archetype *arch) {
				using T = std::decay_t<U>;
				return (*arch->template Map<T>())[m_entidx];
			}

			/// @brief Go to the next entity. If this means changing the archetype, unlock the current archetype and lock the new one.
			void Next() {
				while( m_archidx < m_archetypes.size() && m_entidx >= std::min(m_archetypes[m_archidx].m_size, m_archetypes[m_archidx].m_archetype->Size()) ) {
					if( m_isLocked ) UnlockShared();
					++m_archidx;
					m_entidx = 0;
				} 
				if( m_archidx < m_archetypes.size() ) {
					m_archetype = m_archetypes[m_archidx].m_archetype;
					m_size = std::min(m_archetypes[m_archidx].m_size, m_archetypes[m_archidx].m_archetype->Size());
					m_mapHandle = m_archetype->template Map<Handle>();
					LockShared();
				} else {
					m_archetype = nullptr;
				}
			}

			/// @brief Lock the archetype in shared mode.
			void LockShared() {
				if constexpr (RTYPE == REGISTRYTYPE_PARALLEL) {
					if( !m_archetype || m_isLocked ) return;
					m_archetype->GetMutex().lock_shared();
					m_system.m_currentArchetype = m_archetype;
					m_isLocked = true;
				}
			}

			/// @brief Unlock the archetype.
			void UnlockShared() {
				if constexpr (RTYPE == REGISTRYTYPE_PARALLEL) {
					if( !m_archetype || !m_isLocked ) return;
					m_archetype->GetMutex().unlock_shared();
					m_system.m_currentArchetype = nullptr;
					m_isLocked = false;
				}
			}

			Registry<RTYPE>& 		m_system; ///< Reference to the registry system.
			Archetype*		 		m_archetype{nullptr}; ///< Pointer to the current archetype.
			Vector<Handle>*	m_mapHandle{nullptr}; ///< Pointer to the comp map holding the handle of the current archetype.
			std::vector<ArchetypeAndSize>& m_archetypes; ///< List of archetypes.
			size_t 	m_archidx{0};	///< Index of the current archetype.
			size_t 	m_size{0};		///< Size of the current archetype.
			size_t 	m_entidx{0};		///< Index of the current entity.
			bool 	m_isLocked{false};	///< Flag if the archetype is locked.
		}; //end of Iterator


		//----------------------------------------------------------------------------------------------


		/// @brief A view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		template<typename... Ts>
			requires VecsView<Ts...>
		class View {

		public:
			template<typename U>
			View(Registry<RTYPE>& system, U&& tagsYes, U&& tagsNo ) : 
				m_system{system}, m_tagsYes{std::forward<U>(tagsYes)}, m_tagsNo{std::forward<U>(tagsNo)} {
			} ///< Constructor.

			/// @brief Get an iterator to the first entity. 
			/// The archetype is locked in shared mode to prevent changes. 
			/// @return Iterator to the first entity.
			auto begin() {
				m_archetypes.clear();
				auto types = std::vector<size_t>({Type<std::decay_t<Ts>>()...});
				auto hs = Hash(types);
				{
					LockGuardShared<RTYPE> lock(&m_system.GetMutex()); //lock the cache
					if(FindAndCopy(hs) ) return Iterator<Ts...>{m_system, m_archetypes, 0};
				}

				LockGuard<RTYPE> lock(&m_system.GetMutex()); //lock the cache
				if(FindAndCopy(hs) ) return Iterator<Ts...>{m_system, m_archetypes, 0};

				auto& archetypes = m_system.m_searchCacheMap[hs]; //create empty set
				assert(archetypes.size() == 0);
				for( auto& it : m_system.m_archetypes ) {
					auto arch = it.second.get();
					auto func = [&]<typename T>() {
						if( arch->Types().contains(Type<std::decay_t<T>>())) return true;
						return false;
					};
					if( (func.template operator()<Ts>() && ...) ) archetypes.insert(arch);
				}
				m_system.m_searchCacheSet.emplace_back(TypeSetAndHash{{}, hs});
				( m_system.m_searchCacheSet.back().m_types.insert(Type<std::decay_t<Ts>>()), ... );
				FindAndCopy(hs);
				return Iterator<Ts...>{m_system, m_archetypes, 0};
			}

			/// @brief Get an iterator to the end of the view.
			auto end() {
				return Iterator<Ts...>{m_system, m_archetypes, m_archetypes.size()};
			}

		private:

			/// @brief Find archetypes with the same components in the search cache and copy them to the list.
			/// @param hs Hash of the types of the components.
			/// @return true if the archetypes were found in the cache, else false.
			inline auto FindAndCopy2(size_t hs) -> bool {
				if( m_system.m_searchCacheMap.contains(hs) ) {
					for( auto& arch : m_system.m_searchCacheMap[hs] ) {
						m_archetypes.emplace_back( arch, arch->Size() );
					}
					return true;
				}
				return false;
			}

			inline auto FindAndCopy(size_t hs) -> bool {
				for( auto& arch : m_system.m_archetypes ) {
					bool found = (arch.second->Types().contains(Type<std::decay_t<Ts>>()) && ...) && 
		 						 (std::ranges::all_of( m_tagsYes, [&](size_t ti){ return arch.second->Types().contains(ti); } )) &&
								 (std::ranges::none_of( m_tagsNo, [&](size_t ti){ return arch.second->Types().contains(ti); } ) );
					if(found) m_archetypes.emplace_back( arch.second.get(), arch.second->Size() );
				}
				return true;
			}

			Registry<RTYPE>& 				m_system;	///< Reference to the registry system.
			std::vector<size_t> 			m_tagsYes;	///< List of tags that must be present.
			std::vector<size_t> 			m_tagsNo;	///< List of tags that must not be present.
			std::vector<ArchetypeAndSize>  	m_archetypes;	///< List of archetypes.
		}; //end of View

		//----------------------------------------------------------------------------------------------


		template< template<typename...> typename Y, typename... Ts, template<typename...> typename N,  typename... Us >
			requires (VecsView<Ts...>)
		class View<Y<Ts...>, N<Us...>> {

		public:
			template<typename U>
			View(Registry<RTYPE>& system, U&& tagsYes, U&& tagsNo ) : 
				m_system{system}, m_tagsYes{std::forward<U>(tagsYes)}, m_tagsNo{std::forward<U>(tagsNo)} {} ///< Constructor.

			/// @brief Get an iterator to the first entity. 
			/// The archetype is locked in shared mode to prevent changes. 
			/// @return Iterator to the first entity.
			auto begin() {
				m_archetypes.clear();
				auto types = std::vector<size_t>({ Type<Yes<std::decay_t<Ts>...>>(), Type<No<std::decay_t<Us>...>>() } );
				auto hs = Hash(types);
				{
					LockGuardShared<RTYPE> lock(&m_system.GetMutex()); //lock the cache
					if(FindAndCopy(hs) ) return Iterator<Ts...>{m_system, m_archetypes, 0};
				}

				LockGuard<RTYPE> lock(&m_system.GetMutex()); //lock the cache
				if(FindAndCopy(hs) ) return Iterator<Ts...>{m_system, m_archetypes, 0};

				auto& archetypes = m_system.m_searchCacheMap[hs]; //create empty set
				assert(archetypes.size() == 0);
				
				for( auto& it : m_system.m_archetypes ) {

					auto arch = it.second.get();
					bool found = false;
					auto func = [&]<typename T>() {
						if( arch->Types().contains(Type<std::decay_t<T>>())) return true;
						return false;
					};

					found = (func.template operator()<Ts>() && ...) && (!func.template operator()<Us>() && ...);
					if(found) archetypes.insert(arch);
				}
				m_system.m_searchCacheSet.emplace_back(TypeSetAndHash{{}, hs});
				( m_system.m_searchCacheSet.back().m_types.insert(Type<std::decay_t<Ts>>()), ... );
				FindAndCopy(hs);
				return Iterator<Ts...>{m_system, m_archetypes, 0};
			}

			/// @brief Get an iterator to the end of the view.
			auto end() {
				return Iterator<Ts...>{m_system, m_archetypes, m_archetypes.size()};
			}

		private:

			/// @brief Find archetypes with the same components in the search cache and copy them to the list.
			/// @param hs Hash of the types of the components.
			/// @return true if the archetypes were found in the cache, else false.
			inline auto FindAndCopy2(size_t hs) -> bool {
				if( m_system.m_searchCacheMap.contains(hs) ) {
					for( auto& arch : m_system.m_searchCacheMap[hs] ) {
						m_archetypes.emplace_back( arch, arch->Size() );
					}
					return true;
				}
				return false;
			}

			inline auto FindAndCopy(size_t hs) -> bool {
				for( auto& arch : m_system.m_archetypes ) {
					bool found = (arch.second->Types().contains(Type<std::decay_t<Ts>>()) && ...) && 
								 (!arch.second->Types().contains(Type<std::decay_t<Us>>()) && ...) &&
		 						 (std::ranges::all_of( m_tagsYes, [&](size_t ti){ return arch.second->Types().contains(ti); } )) &&
								 (std::ranges::none_of( m_tagsNo, [&](size_t ti){ return arch.second->Types().contains(ti); } ) );

					if(found) m_archetypes.emplace_back( arch.second.get(), arch.second->Size() );
				}
				return true;
			}

			Registry<RTYPE>& 				m_system;	///< Reference to the registry system.
			std::vector<size_t> 			m_tagsYes;	///< List of tags that must be present.
			std::vector<size_t> 			m_tagsNo;	///< List of tags that must not be present.
			std::vector<ArchetypeAndSize>  	m_archetypes;	///< List of archetypes.

		};


		//----------------------------------------------------------------------------------------------

		Registry() { ///< Constructor.
			//m_entities.reserve(NUMBER_SLOTMAPS::value); //resize the slot storage
			for( uint32_t i = 0; i < NUMBER_SLOTMAPS::value; ++i ) {
				m_entities.emplace_back( SlotMapAndMutex<ArchetypeAndIndex>{ i, 6  } ); //add the first slot
			}
		};

		~Registry() = default;	///< Destructor.

		/// @brief Get the number of entities in the system.
		/// @return The number of entities.
		size_t Size() {
			int size = 0;
			for( auto& slot : m_entities ) {
				size += slot.m_slotMap.Size();
			}
			return size;
		}

		/// @brief Create an entity with components.
		/// @tparam ...Ts The types of the components.
		/// @param ...component The new values.
		/// @return Handle of new entity.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		[[nodiscard]] auto Insert( Ts&&... component ) -> Handle {
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype

			size_t slotMapIndex = GetSlotmapIndex();
			LockGuard<RTYPE> lock(&GetMutex(slotMapIndex)); //lock the mutex
			auto [handle, slot] = m_entities[slotMapIndex].m_slotMap.Insert( {nullptr, 0} ); //get a slot for the entity

			size_t archIndex;
			std::vector<size_t> types = {Type<Handle>(), Type<Ts>()...};
			size_t hs = Hash(types);
			if( !m_archetypes.contains( hs ) ) { //not found
				auto arch = std::make_unique<Archetype>( handle, archIndex, std::forward<Ts>(component)... );
				UpdateSearchCache(arch.get());
				slot.m_value = { arch.get(), archIndex };
				m_archetypes[hs] = std::move(arch);
			} else {
				auto arch = m_archetypes[hs].get();
				LockGuard<RTYPE> lock(&arch->GetMutex()); //lock the mutex
				slot.m_value = { arch, arch->Insert( handle, std::forward<Ts>(component)... ) };
			}
			return handle;
		}

		/// @brief Test if an entity exists.
		/// @param handle The handle of the entity.
		/// @return true if the entity exists, else false.
		bool Exists(Handle handle) {
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			auto& slot = m_entities[handle.GetStorageIndex()].m_slotMap[handle];
			return slot.m_version == handle.GetVersion();
		}

		/// @brief Test if an entity has a component.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return true if the entity has the component, else false.
		template<typename T>
		bool Has(Handle handle) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the system
			auto arch = m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value.m_archetypePtr;
			return arch->Has(Type<T>());
		}

		/// @brief Test if an entity has a tag.
		/// @param handle The handle of the entity.
		/// @param tag The tag to test.
		/// @return true if the entity has the tag, else false.
		bool Has(Handle handle, size_t ti) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the system
			auto arch = m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value.m_archetypePtr;
			return arch->Has(ti);
		}

		/// @brief Get the types of the components of an entity.
		/// @param handle The handle of the entity.
		/// @return A vector of type indices of the components.
		const auto& Types(Handle handle) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the system
			auto arch = m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value.m_archetypePtr;
			return arch->Types();
		}

		/// @brief Get a component value of an entity.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return The component value or reference to it.
		template<typename T>
		[[nodiscard]] auto Get(Handle handle) -> T {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			return Get2<T>(handle);
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires (sizeof...(Ts)>1 && vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle&>::value)
		[[nodiscard]] auto Get(Handle handle) -> std::tuple<Ts...> {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			return Get2<Ts...>(handle);
		}

		/// @brief Put a new component value to an entity. If the entity does not have the component, it will be created.
		/// This might result in moving the entity to another archetype.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @param v The new value.
		template<typename U>
			requires (!is_tuple<U>::value && !std::is_same_v<std::decay_t<U>, Handle>)
		void Put(Handle handle, U&& v) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			using T = std::decay_t<U>;
			Put2(handle, std::forward<T>(v));
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param v The new values in a tuple
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
		void Put(Handle handle, std::tuple<Ts...>& v) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			Put2(handle, std::forward<Ts>(std::get<Ts>(v))...);
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
		void Put(Handle handle, Ts&&... vs) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			Put2(handle, std::forward<Ts>(vs)...);
		}

		/// @brief Add tags to an entity.
		template<typename... Ts>
			requires (sizeof...(Ts) > 0 && (std::is_convertible_v<Ts, size_t> && ...) )
		void AddTags(Handle handle, Ts... tags) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			AddTags2(handle, tags...);
		}

		/// @brief Add tags to an entity.
		template<typename... Ts>
			requires (sizeof...(Ts) > 0 && (std::is_convertible_v<Ts, size_t> && ...) )
		void EraseTags(Handle handle, Ts... tags) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			EraseTags2(handle, tags...);
		}
		
		/// @brief Erase components from an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.		
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Erase(Handle handle) {
			assert( (Has<Ts>(handle) && ...) );
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype

			LockGuard<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			auto value = &m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value;
			auto oldArch = value->m_archetypePtr;
			std::set<size_t> types = oldArch->Types();
			(types.erase(Type<Ts>()), ... );
			size_t hs = Hash(types);
			Archetype *arch;
			if( !m_archetypes.contains(hs) ) {
				auto archPtr = std::make_unique<Archetype>();
				arch = archPtr.get();
				arch->Clone(*oldArch, types);
				UpdateSearchCache(arch);
				m_archetypes[hs] = std::move(archPtr);
			} else { arch = m_archetypes[hs].get(); }
			LockGuard<RTYPE> lock2(&arch->GetMutex(), &oldArch->GetMutex());
			value->m_archetypePtr = arch;
			value->m_archIndex = arch->Move(types, value->m_archIndex, *oldArch, m_entities);
		}

		/// @brief Erase an entity from the registry.
		/// @param handle The handle of the entity.
		void Erase(Handle handle) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuard<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			auto& value = m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value;
			LockGuard<RTYPE> lock2(&value.m_archetypePtr->GetMutex()); //lock the archetype
			value.m_archetypePtr->Erase(value.m_archIndex, m_entities); //erase the entity from the archetype (do both locked)
			m_entities[handle.GetStorageIndex()].m_slotMap.Erase(handle); //erase the entity from the entity list
		}

		/// @brief Clear the registry by removing all entities.
		void Clear() {
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype

			for( auto& arch : m_archetypes ) {
				LockGuard<RTYPE> lock(&arch.second->GetMutex()); //lock the mutex
				arch.second->Clear();
			}
			for( auto& slotmap : m_entities ) {
				LockGuard<RTYPE> lock(&slotmap.m_mutex); //lock the mutex
				slotmap.m_slotMap.Clear();
			}
			LockGuard<RTYPE> lock(&GetMutex()); //lock the mutex
			m_searchCacheMap.clear();
			m_searchCacheSet.clear();
		}

		/// @brief Get a view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		/// @return A view of the entity components
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto GetView() -> View<Ts...> {
			return {*this, std::vector<size_t>{}, std::vector<size_t>{}};
		}

		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto GetView(std::vector<size_t>&& yes) -> View<Ts...> {
			return {*this, std::forward<std::vector<size_t>>(yes), std::vector<size_t>{}};
		}

		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto GetView(std::vector<size_t>&& yes, std::vector<size_t>&& no) -> View<Ts...> {
			return {*this, std::forward<std::vector<size_t>>(yes), std::forward<std::vector<size_t>>(no),};
		}

		/// @brief Print the registry.
		/// Print the number of entities and the archetypes.
		void Print() {
			std::cout << "-----------------------------------------------------------------------------------------------" << std::endl;
			std::cout << "Entities: " << Size() << std::endl;
			for( auto& it : m_archetypes ) {
				std::cout << "Archetype Hash: " << it.first << std::endl;
				it.second->Print();
			}
			std::cout << "Cache Map " << m_searchCacheMap.size() << " Set: " << m_searchCacheSet.size() << std::endl;
			for( auto it : m_searchCacheMap ) {
				std::cout << "Hash: " << it.first << " Archetypes: " << it.second.size() << std::endl;
			}
			std::cout << std::endl << std::endl;
		}

		/// @brief Validate the registry.
		/// Make sure all archetypes have the same size in all component maps.
		void Validate() {
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuardShared<RTYPE> lock(&GetMutex());
			for( auto& it : m_archetypes ) {
				auto arch = it.second.get();
				LockGuardShared<RTYPE> lock(&arch->GetMutex());
				arch->Validate();
			}
		}

		/// @brief Increase the number of iterators.
		/// @return number of iterators.
		size_t increaseIterators() {
			return ++m_numIterators;
		}	

		/// @brief Decrease the number of iterators.
		/// @return number of iterators.
		size_t decreaseIterators() {
			assert(m_numIterators > 0);
			return --m_numIterators;
		}

		inline bool CheckUnlockArchetype() {
			if constexpr( RTYPE == REGISTRYTYPE_PARALLEL ) {
				if( m_currentArchetype ) {
					m_currentArchetype->GetMutex().unlock_shared();
					return true;
				}
			}
			return false;
		}

		inline bool CheckLockArchetype() {
			if constexpr( RTYPE == REGISTRYTYPE_PARALLEL ) {
				if( m_currentArchetype ) {
					m_currentArchetype->GetMutex().lock_shared();
					return true;
				}
			}
			return false;
		}

		/// @brief Delay a transaction until all iterators are destroyed.
		bool DelayTransaction( auto&& func ) {
			if constexpr( RTYPE == REGISTRYTYPE_PARALLEL ) {
				if( m_numIterators > 0 ) { 
					m_delayedTransactions.emplace_back(func); 
					return true; 
				}
			}
			func();
			return false;
		}

		/// @brief Run all delayed transactions. Since each thread has its own list of delayed transactions,
		/// this function must be called by each thread, and does not need synchronization.
		void RunDelayedTransactions() {
			for( auto& func : m_delayedTransactions ) {
				func();
			}
			m_delayedTransactions.clear();
		}

		/// @brief Get the mutex of the archetype.
		/// @return Reference to the mutex.
		[[nodiscard]] inline auto GetMutex(uint32_t index) -> mutex_t& {
			return m_entities[index].m_mutex;
		}

		/// @brief Get the mutex of the archetype.
		/// @return Reference to the mutex.
		[[nodiscard]] inline auto GetMutex() -> mutex_t& {
			return m_mutex; 
		}

		/// @brief Get the index of the entity in the archetype
		auto GetArchetypeIndex( Handle handle ) {
			LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the system
			return m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value.m_archIndex;
		}

		/// @brief Swap two entities.
		/// @param h1 The handle of the first entity.
		/// @param h2 The handle of the second entity.
		/// @return true if the entities were swapped, else false.
		bool Swap( Handle h1, Handle h2 ) {
			assert(Exists(h1) && Exists(h2));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			size_t index1 = h1.GetStorageIndex();
			size_t index2 = h2.GetStorageIndex();
			mutex_t* m1 = &GetMutex(index1);
			mutex_t* m2 = &GetMutex(index2);
			if( index1 == index2 ) { m2 = nullptr; } //same slotmap -> only lock once
			LockGuard<RTYPE> lock(m1, m2); //lock the slotmap(s)
			auto& slot1 = m_entities[index1].m_slotMap[h1];
			auto& slot2 = m_entities[index2].m_slotMap[h2];
			if( slot1.m_archetypePtr != slot2.m_archetypePtr ) return false;
			LockGuard<RTYPE> lock2(&slot1.m_archetypePtr->GetMutex()); //lock the archetype
			slot1.m_archetypePtr->Swap(slot1, slot2);
			return true;
		}

	private:

		/// @brief Get the index of the slotmap to use and the archetype.
		/// @param handle The handle of the entity.
		/// @return The index of the slotmap and the archetype.
		auto GetSlotAndArch(auto handle) {
			ArchetypeAndIndex* value = &m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value;
			return std::make_pair(value, value->m_archetypePtr);
		}

		/// @brief Clone an archetype with a subset of components.
		/// @param arch The archetype to clone.
		/// @param types The types of the components to clone.
		auto CloneArchetype(Archetype* arch, const std::vector<size_t>& types) {
			auto newArch = std::make_unique<Archetype>();
			newArch->Clone(*arch, types);
			UpdateSearchCache(newArch.get());
			m_archetypes[Hash(newArch->Types())] = std::move(newArch);
		}

		/// @brief Add tags to an entity.
		/// @tparam ...Ts The types of the tags must be size_t.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to add.
		template<typename... Ts>
		void AddTags2(Handle handle, Ts... tags) {
			LockGuard<RTYPE> lock1(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			auto [value, arch] = GetSlotAndArch(handle);
			assert(!arch->Types().contains(tags) && ...);
			std::vector<size_t> allTypes{ tags...};
			std::ranges::copy(arch->Types(), std::back_inserter(allTypes));
			size_t hs = Hash(allTypes);
			if( !m_archetypes.contains(hs) ) { CloneArchetype(arch, allTypes); } 
			auto newArch = m_archetypes[hs].get();
			LockGuard<RTYPE> lock2(&arch->GetMutex()); //lock the old archetype
			*value = { newArch, newArch->Move(arch->Types(), value->m_archIndex, *arch, m_entities) };
		}

		/// @brief Erase tags from an entity.
		/// @tparam ...Ts The types of the tags must be size_t.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to erase.
		template<typename... Ts>
		void EraseTags2(Handle handle, Ts... tags) {
			LockGuard<RTYPE> lock1(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			auto [value, arch] = GetSlotAndArch(handle);
			std::vector<size_t> delTypes{tags...};
			std::vector<size_t> allTypes;
			auto found = [&](size_t x) -> bool { return std::ranges::find_if( delTypes, [&](size_t y){ return x==y; } ) != delTypes.end(); };
			for( auto x : arch->Types() ) { if( !found(x) ) allTypes.push_back(x); }			
			size_t hs = Hash(allTypes);
			if( !m_archetypes.contains(hs) ) { CloneArchetype(arch, allTypes); } 
			auto newArch = m_archetypes[hs].get();
			LockGuard<RTYPE> lock2(&arch->GetMutex(), &newArch->GetMutex()); //lock the old archetype
			*value = { newArch, newArch->Move(allTypes, value->m_archIndex, *arch, m_entities) };
		}

		/// @brief Change the component values of an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
		void Put2(Handle handle, Ts&&... vs) {
			auto func = [&]<typename T>(Archetype* arch, size_t archIndex, T&& v) { 
				(*arch->template Map<T>())[archIndex] = std::forward<T>(v);
			};

			std::vector<size_t> newTypes;
			{
				LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
				ArchetypeAndIndex* value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
				//value->m_archetypePtr->Print();	
				if(newTypes.empty()) {
					( func.template operator()<Ts>(value->m_archetypePtr, value->m_archIndex, std::forward<Ts>(vs)), ... );
					return;
				};
			}

			LockGuard<RTYPE> lock1(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			ArchetypeAndIndex* value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
			auto arch = value->m_archetypePtr;
			if(newTypes.empty()) {
				( func.template operator()<Ts>(arch, value->m_archIndex, std::forward<Ts>(vs)), ... );
				return;
			}
			auto newArch = CreateArchetype<std::decay_t<Ts>...>(handle, value, newTypes);
			
			value->m_archetypePtr = newArch;
			LockGuard<RTYPE> lock2(&arch->GetMutex()); //lock old archetype, new one cannot be seen yet
			value->m_archIndex = newArch->Move(arch->Types(), value->m_archIndex, *arch, m_entities); //move values
			( func.template operator()<Ts>(newArch, value->m_archIndex, std::forward<Ts>(vs)), ... );
			UpdateSearchCache(newArch); //update the search cache
			return;
		}
		
		/// @brief Get component values of an entity.
		/// @tparam T The type of the components.
		/// @param handle The handle of the entity.
		/// @return Component values.
		template<typename U>
		[[nodiscard]] auto Get2(Handle handle) -> U& {
			using T = std::decay_t<U>;
			std::vector<size_t> newTypes;
			{
				LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
				ArchetypeAndIndex* value = GetArchetype<T>(handle, newTypes);
				auto arch = value->m_archetypePtr;
				if(newTypes.empty()) { 
					decltype(auto) v = arch->template Get<T>(value->m_archIndex); 
					return v;
				}
			}

			LockGuard<RTYPE> lock1(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			ArchetypeAndIndex* value = GetArchetype<T>(handle, newTypes);
			auto arch = value->m_archetypePtr;
			if(newTypes.empty()) { 
				decltype(auto) v = arch->template Get<T>(value->m_archIndex); 
				return v;
			}
			auto newArch = CreateArchetype<T>(handle, value, newTypes);
			value->m_archetypePtr = newArch;
			LockGuard<RTYPE> lock2(&arch->GetMutex()); //lock old archetype, new one cannot be seen yet
			value->m_archIndex = newArch->Move(arch->Types(), value->m_archIndex, *arch, m_entities); //move values		
			UpdateSearchCache(newArch); //update the search cache
			decltype(auto) v = newArch->template Get<T>(value->m_archIndex);
			return v;
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires (sizeof...(Ts)>1 && vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle&>::value)
		[[nodiscard]] auto Get2(Handle handle) -> std::tuple<Ts...> {
			using T = vtll::Nth_type<vtll::tl<Ts...>, 0>;

			std::vector<size_t> newTypes;
			{
				LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
				ArchetypeAndIndex* value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
				auto arch = value->m_archetypePtr;
				//value->m_archetypePtr->Print();	
				if(newTypes.empty()) { return std::tuple<Ts...>{ arch->template Get<Ts>(value->m_archIndex)... };  }
			}

			LockGuard<RTYPE> lock1(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			ArchetypeAndIndex* value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
			auto arch = value->m_archetypePtr;
			if(newTypes.empty()) { return std::tuple<Ts...>{ arch->template Get<Ts>(value->m_archIndex)... } ; } 

			auto newArch = CreateArchetype<std::decay_t<Ts>...>(handle, value, newTypes);		
			value->m_archetypePtr = newArch;
			LockGuard<RTYPE> lock2(&arch->GetMutex()); //lock old archetype, new one cannot be seen yet
			value->m_archIndex = newArch->Move(arch->Types(), value->m_archIndex, *arch, m_entities); //move values
			return std::tuple<Ts...>{ newArch->template Get<Ts>(value->m_archIndex)... };
		}

		/// @brief Get the archetype/index of an entity and the types of the components that are currently not contained.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param newTypes A vector to store the types of the components that are not contained and should be created.
		/// @return Slotmap value of the entity containing pointer to the archetype and index in the archetype. 
		template<typename... Ts>
		inline auto GetArchetype(Handle handle, std::vector<size_t>& newTypes) -> ArchetypeAndIndex* {		
			newTypes.clear();
			newTypes.reserve(sizeof...(Ts));
			ArchetypeAndIndex* value = &m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value;
			
			auto func = [&]<typename T>(){ if(!value->m_archetypePtr->Has(Type<T>())) newTypes.emplace_back(Type<T>()); };
			( func.template operator()<std::decay_t<Ts>>(), ...  );
			return value;
		}

		/// @brief Create a new archetype with the types of the components that are not contained.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param value Pointer to the archetype/index of the entity.
		/// @param newTypes A vector with the types of the components that are not contained and shhould be created.
		/// @return Pointer to the new archetype.
		template<typename... Ts>
		inline auto CreateArchetype(Handle handle, ArchetypeAndIndex* value, std::vector<size_t>& newTypes) -> Archetype* {
			auto arch = value->m_archetypePtr;
			std::vector<size_t> allTypes; //{newTypes};
			std::ranges::copy( newTypes, std::back_inserter(allTypes) );
			std::ranges::copy( arch->Types(), std::back_inserter(allTypes) );

			size_t hs = Hash(allTypes);
			Archetype *newArch=nullptr;
			if( !m_archetypes.contains(hs) ) {
				auto newArchUnique = std::make_unique<Archetype>();
				newArch = newArchUnique.get();
				newArch->Clone(*arch, arch->Types());
				
				auto func = [&]<typename T>(){ 
					if(!arch->Has(Type<T>())) { //need the types, type indices are not enough
						newArch->template AddComponent<T>();
						newArch->template AddValue(T{}); 
					}
				};
				( func.template operator()<std::decay_t<Ts>>(), ...  );
				UpdateSearchCache(newArch);

				m_archetypes[hs] = std::move(newArchUnique);
			} else { 
				newArch = m_archetypes[hs].get(); 
				for( auto ti : newTypes ) { newArch->AddEmptyValue(ti); }
			}

			return newArch;
		}

		/// @brief Update the search cache with a new archetype. m_searchCacheSet contains the types of 
		/// the components that have been searched for.	FInd those previous search results that include 
		/// all components that are searched for. Add them to the set of archetypes that yielded a result
		/// for this particular search.
		/// @param arch Pointer to the new archetype.
		void UpdateSearchCache(Archetype* arch) {
			LockGuard<RTYPE> lock(&arch->GetMutex()); //lock the cache
			auto types = arch->Types();
			for( auto& ts : m_searchCacheSet ) {
				if( std::includes(types.begin(), types.end(), ts.m_types.begin(), ts.m_types.end()) ) {
					m_searchCacheMap[ts.m_hash].insert(arch);
				}
			}
		}

		/// @brief Get a new index of the slotmap for the current thread.
		/// @return New index of the slotmap.
		size_t GetSlotmapIndex() {
			m_slotMapIndex = (m_slotMapIndex + 1) & (NUMBER_SLOTMAPS::value - 1);
			return m_slotMapIndex;
		}

		using SlotMaps_t = std::vector<SlotMapAndMutex<ArchetypeAndIndex>>;
		using HashMap_t = HashMap<std::unique_ptr<Archetype>>;
		using SearchCacheMap_t = std::unordered_map<size_t, std::set<Archetype*>>;
		using SearchCacheSet_t = std::vector<TypeSetAndHash>;

		inline static thread_local size_t m_slotMapIndex = NUMBER_SLOTMAPS::value - 1;

		SlotMaps_t m_entities;
		HashMap_t m_archetypes; //Mapping vector of type set hash to archetype 1:1. Internally synchronized!

		mutex_t m_mutex; //mutex for reading and writing search cache. Not needed for HashMaps.
		SearchCacheMap_t m_searchCacheMap; //Mapping vector of hash to archetype, 1:N
		SearchCacheSet_t m_searchCacheSet; //These type combinations have been searched for, for updating
		inline static thread_local size_t 		m_numIterators{0}; //thread local variable for locking
		inline static thread_local Archetype* 	m_currentArchetype{nullptr}; //is there an iterator now
		inline static thread_local std::vector<std::function<void()>> m_delayedTransactions; //thread local variable for locking
	};

} //end of namespace vecs



