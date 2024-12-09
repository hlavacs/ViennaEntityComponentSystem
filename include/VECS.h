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
#include <VTLL.h>


using namespace std::chrono_literals;


namespace vecs {


	//----------------------------------------------------------------------------------------------
	//Conveneience functions


	template <typename> struct is_tuple : std::false_type {};
	template <typename ...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};

	/// @brief Compute the hash of a list of hashes. If stored in a vector, make sure that hashes are sorted.
	/// @tparam T Container type of the hashes.
	/// @param hashes Reference to the container of the hashes.
	/// @return Overall hash made from the hashes.
	template <typename T>
	inline size_t Hash( T& hashes ) {
		std::size_t seed = 0;
		if constexpr (std::is_same_v<std::decay_t<T>, const std::vector<size_t>&> ) 
			std::sort(hashes.begin(), hashes.end());
		for( auto& v : hashes ) {
			seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
		}
		return seed;
	}

	template <typename T>
	inline size_t Hash( T&& hashes ) {
		std::size_t seed = 0;
		if constexpr (std::is_same_v<std::decay_t<T>, const std::vector<size_t>&> ) 
			std::sort(hashes.begin(), hashes.end());
		for( auto& v : hashes ) {
			seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
		}
		return seed;
	}


	template<typename T>
	inline auto Type() -> std::size_t {
		return std::type_index(typeid(T)).hash_code();
	}


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
			if constexpr (LTYPE == LOCKGUARDTYPE_PARALLEL) { m_mutex->lock(); }
		}

		/// @brief Constructor for two mutexes. This is necessary if an entity must be moved from one archetype to another, because
		/// the entity components change. In this case, two mutexes must be locked.
		/// @param mutex Pointer to the mutex.
		/// @param other Pointer to the other mutex.
		LockGuard(mutex_t* mutex, mutex_t* other) : m_mutex{mutex}, m_other{other} { 
			if constexpr (LTYPE == LOCKGUARDTYPE_PARALLEL) {
				std::min(m_mutex, m_other)->lock();	///lock the mutexes in the correct order
				std::max(m_mutex, m_other)->lock();
			}
		}

		/// @brief Destructor, unlocks the mutexes.
		~LockGuard() { 
			if constexpr (LTYPE == LOCKGUARDTYPE_PARALLEL) {
				if(m_other) { 
					std::max(m_mutex, m_other)->unlock();
					std::min(m_mutex, m_other)->unlock();	///lock the mutexes in the correct order
					return;
				}
				m_mutex->unlock();
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


	//----------------------------------------------------------------------------------------------
	//Segmented Stack

	/// @brief A stack that stores elements in segments to avoid reallocations. The size of a segment is 2^segmentBits.
	template<typename T>
	class Stack {

		using segment_t = std::unique_ptr<std::vector<T>>;
		using stack_t = std::vector<segment_t>;

		public:

			/// @brief Constructor, creates the stack.
			/// @param segmentBits The number of bits for the segment size.
			Stack(size_t segmentBits = 10) : m_segmentBits(segmentBits), m_segmentSize{1ul<<segmentBits} {
				assert(segmentBits > 0);
				m_segments.emplace_back( std::make_unique<std::vector<T>>(m_segmentSize) );
			}

			~Stack() = default;

			/// @brief Push a value to the back of the stack.
			/// @param value The value to push.
			template<typename U>
				requires std::is_convertible_v<U, T>
			void push_back(U&& value) {
				if( Segment(m_size) == m_segments.size() ) {
					m_segments.emplace_back( std::make_unique<std::vector<T>>(m_segmentSize) );
				}
				++m_size;
				operator[](size()-1) = std::forward<U>(value);
			}

			/// @brief Pop the last value from the stack.
			void pop_back() {
				assert(m_size > 0);
				--m_size;
				if(	Offset(m_size) == 0 && m_segments.size() > 1 ) {
					m_segments.pop_back();
				}
			}

			/// @brief Get the value at an index.
			/// @param index The index of the value.
			auto operator[](size_t index) -> T& {
				assert(index < m_size);
				return (*m_segments[Segment(index)])[Offset(index)];
			}

			/// @brief Get the value at an index.
			auto size() -> size_t { return m_size; }

			/// @brief Clear the stack. Make sure that one segment is always available.
			void clear() {
				m_size = 0;
				m_segments.clear();
				m_segments.emplace_back( std::make_unique<std::vector<T>>(m_segmentSize) );
			}

		private:

			/// @brief Compute the segment index of an entity index.
			/// @param index Entity index.
			/// @return Index of the segment.
			inline size_t Segment(size_t index) { return index >> m_segmentBits; }

			/// @brief Compute the offset of an entity index in a segment.
			/// @param index Entity index.
			/// @return Offset in the segment.
			inline size_t Offset(size_t index) { return index & (m_segmentSize-1ul); }

			size_t m_size{0};	///< Size of the stack.
			size_t m_segmentBits{10};	///< Number of bits for the segment size.
			size_t m_segmentSize{1<<m_segmentBits}; ///< Size of a segment.
			stack_t m_segments;	///< Vector holding unique pointers to the segments.
	};



	//----------------------------------------------------------------------------------------------
	//Handles

	/// @brief A handle for an entity or a component. 
	struct Handle {
		uint32_t m_index{std::numeric_limits<uint32_t>::max()}; ///< Index of the entity in the slot map.
		uint32_t m_version{0}; ///< Version of the entity in the slot map. If the version is different from the slot version, the entity is also invalid.

		bool IsValid() const {
			return m_index != std::numeric_limits<uint32_t>::max();
		}

		bool operator<(const Handle& other) const {
			return m_index < other.m_index;
		}
	};

	inline bool IsValid(const Handle& handle) {
		return handle.IsValid();
	}

	inline std::ostream& operator<<(std::ostream& os, const vecs::Handle& handle) {
    	return os << "{" <<  handle.m_index << ", " << handle.m_version << "}"; 
	}


	//----------------------------------------------------------------------------------------------
	//Slot Maps


	/// @brief A slot map for storing a map from handle to archetype and index in the archetype.
	/// A slot map can never shrink. If an entity is erased, the slot is added to the free list. A handle holds an index
	/// to the slot map and a version counter. If the version counter of the slot is different from the version counter of the handle,
	/// the slot is invalid.
	/// @tparam T The value type of the slot map.
	template<typename T>
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
		SlotMap(int64_t size = 1<<10) {
			m_firstFree = 0;
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
			return { Handle{ (uint32_t)index, (uint32_t)slot->m_version}, *slot};			
		}

		/// @brief Erase a value from the slot map.
		/// @param handle The handle of the value to erase.
		void Erase(Handle handle) {
			auto& slot = m_slots[handle.m_index];
			++slot.m_version;	//increment the version to invalidate the slot
			slot.m_nextFree = m_firstFree;	
			m_firstFree = handle.m_index; //add the slot to the free list
			--m_size;
		}

		/// @brief Get a value from the slot map.
		/// @param handle The handle of the value to get.
		/// @return Reference to the value.
		auto operator[](Handle handle) -> Slot& {
			return m_slots[handle.m_index];
		}

		/// @brief Get the size of the slot map.
		/// @return The size of the slot map.
		auto Size() -> size_t {
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
		size_t m_size{0}; ///< Size of the slot map. This is the size of the Stack minus the free slots.
		int64_t m_firstFree{-1}; ///< Index of the first free slot. If -1 then there are no free slots.
		Stack<Slot> m_slots; ///< Container of slots.
	};


	//----------------------------------------------------------------------------------------------
	//Registry concepts and types

	template<typename... Ts>
	concept VecsArchetype = (vtll::unique<vtll::tl<Ts...>>::value && (sizeof...(Ts) > 0) && (!std::is_same_v<Handle, std::decay_t<Ts>> && ...));

	template<typename... Ts>
	concept VecsView = ((vtll::unique<vtll::tl<Ts...>>::value) && (sizeof...(Ts) > 0) && (!std::is_same_v<Handle&, Ts> && ...));

	template<typename... Ts>
	concept VecsIterator = (vtll::unique<vtll::tl<Ts...>>::value);

	template<typename U>
	concept VecsRef = (!std::is_reference_v<U>);

	template<typename... Ts> requires VecsIterator<Ts...> class Iterator;
	template<typename... Ts> requires VecsView<Ts...> class View;
	template<typename U> requires VecsRef<U> class Ref;


	//----------------------------------------------------------------------------------------------
	//Registry 

	using RegistryType = int;
	const int REGISTRYTYPE_SEQUENTIAL = 0;
	const int REGISTRYTYPE_PARALLEL = 1;

	/// @brief A registry for entities and components.
	template <RegistryType RTYPE>
		requires (RTYPE == REGISTRYTYPE_SEQUENTIAL || RTYPE == REGISTRYTYPE_PARALLEL)
	class Registry {

		struct TypeSetAndHash {
			std::set<size_t> m_types;	//set of types that have been searched for
			size_t m_hash;				//hash of the set
		};

	public:
	
		template<typename U> requires VecsRef<U> friend class Ref;

	private:

		//----------------------------------------------------------------------------------------------
		//ComponentMapBase

		/// @brief Base class for component maps. They hold the components of the same type.
		/// The component maps are stored in the archetype. An entity can have multiple components.
		/// The components of the same entity have the same index in all component maps
		/// The base class is used to allow access to the component maps without knowing the type.
		/// This is useful when e.g. moving entities between archetypes.
		class ComponentMapBase {

		public:
			ComponentMapBase() = default; //constructor
			virtual ~ComponentMapBase() = default; //destructor
			
			/// @brief Insert a new component value.
			/// @param v The component value.
			/// @return The index of the new component value.
			size_t Insert(auto&& v) {
				using T = std::decay_t<decltype(v)>;
				return static_cast<ComponentMap<T>*>(this)->Insert(std::forward<T>(v));
			}

			virtual auto Insert() -> size_t = 0;
			virtual auto Erase(size_t index) -> size_t = 0;
			virtual void Move(ComponentMapBase* other, size_t from) = 0;
			virtual auto Size() -> size_t= 0;
			virtual auto Clone() -> std::unique_ptr<ComponentMapBase> = 0;
			virtual void Clear() = 0;
			virtual void print() = 0;
		}; //end of ComponentMapBase

		//----------------------------------------------------------------------------------------------
		//ComponentMap

		/// @brief A map of components of the same type.
		/// @tparam U The value type for this comoonent map. Note that the type is decayed.
		/// If you want to store a pointer or reference, use a struct to wrap it.
		template<typename U>
		class ComponentMap : public ComponentMapBase {

			using T = std::decay_t<U>; //remove pointer or reference

		public:

			ComponentMap() = default; //constructor

			/// @brief Insert a new component value.
			/// @param v The component value.
			/// @return The index of the new component value.
			[[nodiscard]] size_t Insert(auto&& v) {
				m_data.push_back( std::forward<T>(v) );
				return m_data.size() - 1;
			};

			/// @brief Insert a new empty component value.
			/// @return The index of the new component value.
			[[nodiscard]] size_t Insert() override{
				m_data.push_back( T{} );
				return m_data.size() - 1;
			};

			/// @brief Get reference to the component of an entity.
			/// @param index The index of the component value in the map.
			/// @return Reference to the component value.
			auto Get(std::size_t index) -> T& {
				assert(index < m_data.size());
				return m_data[index];
			};

			/// @brief Get reference to the data container of the components.
			/// @return Reference to the data container.
			[[nodiscard]] auto& Data() {
				return m_data;
			};

			/// @brief Erase a component from the container.
			/// @param index The index of the component value in the map.
			/// @return The index of the last element in the vector.
			[[nodiscard]] auto Erase(size_t index)  -> size_t override {
				size_t last = m_data.size() - 1;
				assert(index <= last);
				if( index < last ) {
					m_data[index] = std::move( m_data[last] ); //move the last entity to the erased one
				}
				m_data.pop_back(); //erase the last entity
				return last; //if index < last then last element was moved -> correct mapping 
			};

			/// @brief Move a component from another map to this map.
			/// @param other The other map.
			/// @param from The index of the component in the other map.
			void Move(ComponentMapBase* other, size_t from) override {
				m_data.push_back( static_cast<ComponentMap<std::decay_t<T>>*>(other)->Get(from) );
			};

			/// @brief Get the size of the map.
			/// @return The size of the map.
			[[nodiscard]] auto Size() -> size_t override {
				return m_data.size();
			}

			/// @brief Clone a new map from this map.
			/// @return A unique pointer to the new map.
			[[nodiscard]] auto Clone() -> std::unique_ptr<ComponentMapBase> override {
				return std::make_unique<ComponentMap<T>>();
			};

			/// @brief Clear the map.
			void Clear() override {
				m_data.clear();
			}

			void print() override {
				std::cout << "Name: " << typeid(T).name() << " ID: " << Type<T>();
			}

		private:
			Stack<T> m_data; //vector of component values
		}; //end of ComponentMap


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

			template <RegistryType> requires (RTYPE == REGISTRYTYPE_SEQUENTIAL || RTYPE == REGISTRYTYPE_PARALLEL)
			friend class Registry;

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
				assert( m_types.size() == sizeof...(Ts) + 1 );
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
				return Map<T>()->Get(archIndex);
			}

			/// @brief Get component values of an entity.
			/// @tparam ...Ts Types of the components to get.
			/// @param handle Handle of the entity.
			/// @return A tuple of the component values.
			template<typename... Ts>
				requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value))
			[[nodiscard]] auto Get(size_t archIndex) -> std::tuple<Ts&...> {
				return std::tuple<Ts&...>{ Map<Ts>()->Get(archIndex)... };
			}

			/// @brief Erase an entity
			/// @param index The index of the entity in the archetype.
			/// @return The handle of the entity that was moved over the erased one so its index can be changed.
			void Erase(size_t index, auto& slotmap) {
				Erase2(index, slotmap);
			}

			/// @brief Move components from another archetype to this one.
			size_t Move( auto& types, size_t other_index, Archetype& other, auto& slotmap) {			
				for( auto& ti : types ) { //go through all maps
					assert( m_maps.contains(ti) );
					m_maps[ti]->Move(other.Map(ti), other_index); //insert the new value
				}
				other.Erase2(other_index, slotmap); //erase from old component map
				++other.m_changeCounter;
				++m_changeCounter;
				return m_maps[Type<Handle>()]->Size() - 1; //return the index of the new entity
			}

			void Clone(Archetype& other, auto& types) {
				for( auto& ti : types ) { //go through all maps
					m_types.insert(ti); //add the type to the list
					m_maps[ti] = other.Map(ti)->Clone(); //make a component map like this one
				}
			}

			/// @brief Get the number of entites in this archetype.
			/// @return The number of entities.
			size_t Size() {
				return m_maps[Type<Handle>()]->Size();
			}

			/// @brief Clear the archetype.
			void Clear() {
				for( auto& map : m_maps ) {
					map.second->Clear();
				}
				++m_changeCounter;
			}

			/// @brief Print the archetype.
			void Print() {
				std::cout << "Hash: " << Hash(m_types) << std::endl;
				for( auto ti : m_types ) {
					std::cout << "Type: " << ti << " ";
				}
				std::cout << std::endl;
				for( auto& map : m_maps ) {
					std::cout << "Map: ";
					map.second->print();
					std::cout << std::endl;
				}
				std::cout << std::endl;
			}

			/// @brief Validate the archetype. Make sure all maps have the same size.
			void Validate() {
				for( auto& map : m_maps ) {
					assert( map.second->Size() == m_maps[Type<Handle>()]->Size() );
				}
			}
		
			/// @brief Get the change counter of the archetype. It is increased when a change occurs
			/// that might invalidate a Ref object, e.g. when an entity is moved to another archetype, or erased.
			size_t GetChangeCounter() {
				return m_changeCounter;
			}

			/// @brief Get the mutex of the archetype.
			/// @return Reference to the mutex.
			[[nodiscard]] auto GetMutex() -> mutex_t& {
				return m_mutex;
			}

		private:

			/// @brief Add a new component to the archetype.
			/// @tparam T The type of the component.
			template<typename U>
			void AddComponent() {
				using T = std::decay_t<U>; //remove pointer or reference
				size_t ti = Type<T>();
				assert( !m_types.contains(ti) );
				m_types.insert(ti);	//add the type to the list
				m_maps[ti] = std::make_unique<ComponentMap<T>>(); //create the component map
			};

			/// @brief Add a new component value to the archetype.
			/// @param v The component value.
			/// @return The index of the component value.
			template<typename U>
			auto AddValue( U&& v ) -> size_t {
				using T = std::decay_t<U>;
				return m_maps[Type<T>()]->Insert(std::forward<T>(v));	//insert the component value
			};

			auto AddEmptyValue( size_t ti ) -> size_t {
				return m_maps[ti]->Insert();	//insert the component value
			};

			/// @brief Erase an entity. To ensure thet consistency of the entity indices, the last entity is moved to the erased one.
			/// This might result in a reindexing of the moved entity in the slot map. Thus we need a ref to the slot map
			/// @param index The index of the entity in the archetype.
			/// @param slotmap Reference to the slot map of the registry.
			void Erase2(size_t index, auto& slotmap) {
				size_t last{index};
				for( auto& it : m_maps ) {
					last = it.second->Erase(index); //Erase from the component map
				}
				if( index < last ) {
					auto& lastHandle = static_cast<ComponentMap<Handle>*>(m_maps[Type<Handle>()].get())->Get(index);
					slotmap[lastHandle].m_value.m_archIndex = index;
				}
				++m_changeCounter;
			}
			
			/// @brief Get the map of the components.
			/// @tparam T The type of the component.
			/// @return Pointer to the component map.
			template<typename T>
			auto Map() -> ComponentMap<T>* {
				auto it = m_maps.find(Type<T>());
				assert(it != m_maps.end());
				return static_cast<ComponentMap<T>*>(it->second.get());
			}

			/// @brief Get the data of the components.
			/// @param ti Type index of the component.
			/// @return Pointer to the component map base class.
			auto Map(size_t ti) -> ComponentMapBase* {
				auto it = m_maps.find(ti);
				assert(it != m_maps.end());
				return it->second.get();
			}

			using Size_t = std::conditional_t<RTYPE == REGISTRYTYPE_SEQUENTIAL, std::size_t, std::atomic<std::size_t>>;

			mutex_t m_mutex; //mutex for thread safety
			Size_t m_changeCounter{0}; //changes invalidate references
			std::set<size_t> m_types; //types of components
			std::unordered_map<size_t, std::unique_ptr<ComponentMapBase>> m_maps; //map from type index to component data
		}; //end of Archetype

	public:

		//----------------------------------------------------------------------------------------------
		
		/// @brief A samrt reference to an entity component. After a change in the entity, components might be moved.
		/// This reference checks if the entity has changed and updates the reference if necessary.
		/// This class is meant for use in SEQUENTIAL mode or in PARALLEL mode when iterating through range based for loop.
		/// When iterating through a range based for loop in PARALLEL mode, because the achetype is locked in shared mode, components 
		/// cannot be moved. Thus, the reference is always valid in these cases.
		/// However this is not true if you hold a reference to any other component not from the current archetype. In these cases make sure
		/// to use Get and Put only.
		/// @tparam U The type of the component.
		template<typename U>
			requires VecsRef<U>
		class Ref {

			using T = std::decay_t<U>;

		public:

			/// @brief Constructor, creates a reference to an entity component.
			/// @param system The registry system.
			/// @param handle Handle to the entity
			/// @param arch Archetype of the entity.
			/// @param valueRef Reference to the component value.
			Ref(Registry<RTYPE>& system, Handle handle, Archetype *arch, T& valueRef) 
				: m_system{system}, m_handle{handle}, m_archetype{arch}, m_valueRef{valueRef}, m_changeCounter{arch->GetChangeCounter()} {}

			/// @brief Get the component value. Check if the entity has changed before returning the value.
			auto operator()() -> T {
				return Get();
			}

			/// @brief Set the component value. Check if the entity has changed before setting the value.
			void operator=(T&& value) {
				CheckChangeCounter();
				m_valueRef = std::forward<T>(value);
			}

			/// @brief Get the component value. Check if the entity has changed before returning the value.
			auto Get() -> T {
				return CheckChangeCounter();
			}

			operator T () { return Get(); }; ///< Conversion operator.

		private:

			/// @brief Check if the entity has changed and update the reference if necessary.
			/// @return Reference to the component value.
			auto CheckChangeCounter() -> T& {
				auto cc = m_archetype->GetChangeCounter();
				if(cc != m_changeCounter ) {
					auto& value = m_system.m_entities[m_handle].m_value;
					m_archetype = value.m_archetypePtr;
					m_valueRef = m_archetype->template Map<T>()->Get(value.m_archIndex);
					m_changeCounter = m_archetype->GetChangeCounter();
				}
				return  m_valueRef;
			}

			Registry<RTYPE>& 	m_system; 			///< Reference to the registry system.
			Handle 				m_handle;  			///< Handle of the entity.
			Archetype* 			m_archetype;		///< Archetype of the entity. Might have changed.
			T& 					m_valueRef; 		///< Reference to the component value.
			size_t 				m_changeCounter; 	///< Change counter of the archetype.
		};
		

		//----------------------------------------------------------------------------------------------

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
			Iterator( Registry<RTYPE>& system, std::vector<Archetype*>& arch, size_t archidx) 
				: m_system{system}, m_archetypes{arch}, m_archidx{archidx} {
					
				system.increaseIterators(); //Tell the system that an iterator is created
				if( m_archetypes.size() > 0 ) { m_size = m_archetypes[0]->Size(); }
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
				assert( m_archetype && m_entidx < m_archetype->Size() );
				Handle handle = m_mapHandle->Get(m_entidx);
				auto tup = std::make_tuple( Get<Ts>(m_system, handle, m_archetype)... );
				if constexpr (sizeof...(Ts) == 1) { return std::get<0>(tup); }
				else return tup;
			}

			/// @brief Compare two iterators.
			auto operator!=(const Iterator& other) -> bool {
				return (m_archidx != other.m_archidx) || (m_entidx != other.m_entidx);
			}

		private:

			/// @brief Get a component reference from the archetype.
			template<typename U>
				requires (!std::is_same_v<U, Handle&> && std::is_reference_v<U>)
			[[nodiscard]] auto Get(auto &system, Handle handle, Archetype *arch) {
				using T = std::decay_t<U>;
				return Ref<T>{system, handle, arch, arch->template Map<T>()->Get(m_entidx)};
			}

			/// @brief Get a component value from the archetype.
			template<typename U>
				requires (!std::is_same_v<U, Handle&> && !std::is_reference_v<U>)
			[[nodiscard]] auto Get(auto &system, Handle handle, Archetype *arch) {
				using T = std::decay_t<U>;
				return arch->template Map<T>()->Get(m_entidx);
			}

			/// @brief Go to the next entity. If this means changing the archetype, unlock the current archetype and lock the new one.
			void Next() {
				while( m_archidx < m_archetypes.size() && m_entidx >= std::min(m_size, m_archetypes[m_archidx]->Size()) ) {
					if( m_isLocked ) UnlockShared();
					++m_archidx;
					m_entidx = 0;
				} 
				if( m_archidx < m_archetypes.size() ) {
					m_archetype = m_archetypes[m_archidx];
					m_size = m_archetype->Size();
					m_mapHandle = m_archetype->template Map<Handle>();
					LockShared();
				} else {
					m_archetype = nullptr;
				}
			}

			/// @brief Lock the archetype in shared mode.
			void LockShared() {
				if constexpr (RTYPE == REGISTRYTYPE_PARALLEL) {
					if( !m_archetype ) return;
					if( m_isLocked ) return;
					m_archetype->GetMutex().lock_shared();
					m_isLocked = true;
				}
			}

			/// @brief Unlock the archetype.
			void UnlockShared() {
				if constexpr (RTYPE == REGISTRYTYPE_PARALLEL) {
					if( !m_archetype ) return;
					if( !m_isLocked ) return;
					m_archetype->GetMutex().unlock_shared();
					m_isLocked = false;
				}
			}

			Registry<RTYPE>& 		m_system; ///< Reference to the registry system.
			Archetype*				m_archetype{nullptr}; ///< Pointer to the current archetype.
			ComponentMap<Handle>*	m_mapHandle{nullptr}; ///< Pointer to the comp map holding the handle of the current archetype.
			std::vector<Archetype*>& m_archetypes; ///< List of archetypes.
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
			View(Registry<RTYPE>& system) : m_system{system} {} ///< Constructor.

			/// @brief Get an iterator to the first entity. 
			/// The archetype is locked in shared mode to prevent changes. 
			/// @return Iterator to the first entity.
			auto begin() {
				m_archetypes.clear();
				auto types = std::vector<size_t>({Type<Ts>()...});
				auto hs = Hash(types);
				{
					LockGuardShared<RTYPE> lock(&m_system.GetMutex()); //lock the system
					if(FindAndCopy(hs) ) return Iterator<Ts...>{m_system, m_archetypes, 0};
				}

				LockGuard<RTYPE> lock(&m_system.GetMutex()); //lock the system
				if(FindAndCopy(hs) ) return Iterator<Ts...>{m_system, m_archetypes, 0};

				auto& archetypes = m_system.m_searchCacheMap[hs]; //create empty set
				assert(archetypes.size() == 0);
				for( auto& it : m_system.m_archetypes ) {
					auto arch = it.second.get();
					auto func = [&]<typename T>() {
						if( arch->Types().contains(Type<T>())) return true;
						return false;
					};
					if( (func.template operator()<Ts>() && ...) ) archetypes.insert(arch);
				}
				m_system.m_searchCacheSet.emplace_back(TypeSetAndHash{{}, hs});
				( m_system.m_searchCacheSet.back().m_types.insert(Type<Ts>()), ... );
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
			[[nodiscard]]
			inline auto FindAndCopy(size_t hs) -> bool {
				if( m_system.m_searchCacheMap.contains(hs) ) {
					for( auto& arch : m_system.m_searchCacheMap[hs] ) {
						m_archetypes.emplace_back( arch );
					}
					return true;
				}
				return false;
			}

			Registry<RTYPE>& 		m_system;	///< Reference to the registry system.
			std::vector<Archetype*> m_archetypes;	///< List of archetypes.
		}; //end of View


		//----------------------------------------------------------------------------------------------

		Registry() = default; ///< Constructor.
		~Registry() = default;	///< Destructor.

		/// @brief Get the number of entities in the system.
		/// @return The number of entities.
		size_t Size() {
			return m_entities.Size();
		}

		/// @brief Create an entity with components.
		/// @tparam ...Ts The types of the components.
		/// @param ...component The new values.
		/// @return Handle of new entity.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		[[nodiscard]] auto Insert( Ts&&... component ) -> Handle {
			assert(CheckDelay());

			LockGuard<RTYPE> lock(&GetMutex()); //lock the mutex
			auto [handle, slot] = m_entities.Insert( {nullptr, 0} ); //get a slot for the entity

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
				archIndex = arch->Insert( handle, std::forward<Ts>(component)... );
				slot.m_value = { arch, archIndex };
			}

			return handle;
		}

		/// @brief Test if an entity exists.
		/// @param handle The handle of the entity.
		/// @return true if the entity exists, else false.
		bool Exists(Handle handle) {
			assert(CheckDelay());
			LockGuardShared<RTYPE> lock(&GetMutex()); //lock the mutex
			auto& slot = m_entities[handle];
			return slot.m_version == handle.m_version;
		}

		/// @brief Test if an entity has a component.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return true if the entity has the component, else false.
		template<typename T>
		bool Has(Handle handle) {
			assert(CheckDelay());
			assert(Exists(handle));
			LockGuardShared<RTYPE> lock(&GetMutex()); //lock the system
			auto arch = m_entities[handle].m_value.m_archetypePtr;
			return arch->Has(Type<T>());
		}

		/// @brief Get the types of the components of an entity.
		/// @param handle The handle of the entity.
		/// @return A vector of type indices of the components.
		const auto& Types(Handle handle) {
			assert(CheckDelay());
			assert(Exists(handle));
			LockGuardShared<RTYPE> lock(&GetMutex()); //lock the system
			auto arch = m_entities[handle].m_value.m_archetypePtr;
			return arch->Types();
		}

		/// @brief Get component value of an entity. If the component does not exist, it will be created.
		/// This might result in moving the entity to another archetype.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.	
		/// @return The component value.
		template<typename T>
			requires (!std::is_same_v<T, Handle&> && !std::is_reference_v<T>)
		[[nodiscard]] auto Get(Handle handle) -> T {
			assert(CheckDelay());
			return std::get<0>(Get3<T>(handle));
		}

		/// @brief Get component reference of an entity.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return Ref objects to the component value.
		template<typename T>
			requires (!std::is_same_v<T, Handle&> && std::is_reference_v<T>)
		[[nodiscard]] auto Get(Handle handle) -> Ref<std::decay_t<T>> {
			assert(CheckDelay());
			return std::get<0>(Get3<T>(handle));
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		[[nodiscard]] auto Get(Handle handle) {
			assert(CheckDelay());
			return Get3<Ts...>(handle);			
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
			Put2(handle, std::forward<Ts>(std::get<Ts>(v))...);
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
		void Put(Handle handle, Ts&&... vs) {
			Put2(handle, std::forward<Ts>(vs)...);
		}
		
		/// @brief Erase components from an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.		
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Erase(Handle handle) {
			assert(CheckDelay());
			assert( (Has<Ts>(handle) && ...) );

			LockGuard<RTYPE> lock(&GetMutex()); //lock the mutex
			auto value = &m_entities[handle].m_value;
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
			assert(CheckDelay());
			assert(Exists(handle));
			LockGuard<RTYPE> lock(&GetMutex()); //lock the mutex
			auto& value = m_entities[handle].m_value;
			LockGuard<RTYPE> lock2(&value.m_archetypePtr->GetMutex()); //lock the archetype
			value.m_archetypePtr->Erase(value.m_archIndex, m_entities); //erase the entity from the archetype (do both locked)
			m_entities.Erase(handle); //erase the entity from the entity list
		}

		/// @brief Clear the registry by removing all entities.
		void Clear() {
			assert(CheckDelay());
			LockGuard<RTYPE> lock(&GetMutex()); //lock the mutex
			for( auto& arch : m_archetypes ) {
				arch.second->Clear();
			}
			m_entities.Clear();
		}

		/// @brief Get a view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		/// @return A view of the entity components
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto GetView() -> View<Ts...>{
			return {*this};
		}

		/// @brief Print the registry.
		/// Print the number of entities and the archetypes.
		void Print() {
			std::cout << "Entities: " << m_entities.Size() << std::endl;
			for( auto& it : m_archetypes ) {
				it.second->print();
			}
			std::cout << std::endl << std::endl;
		}

		/// @brief Validate the registry.
		/// Make sure all archetypes have the same size in all component maps.
		void Validate() {
			assert(CheckDelay());
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

		/// @brief Check if a transaction must be delayed.
		/// @return true if the transaction must be delayed, else false.
		bool CheckDelay() {
			if constexpr( RTYPE == REGISTRYTYPE_PARALLEL ) {
				if( m_numIterators > 0 ) {
					return false;
				}
			}
			return true;
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
		[[nodiscard]] inline auto GetMutex() -> mutex_t& {
			return m_mutex;
		}

	private:

		/// @brief Change the component values of an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
		void Put2(Handle handle, Ts&&... vs) {
			auto func = [&]<typename T>(Archetype* arch, size_t archIndex, T&& v){ arch->template Get<T>(archIndex) = std::forward<T>(v); };

			std::vector<size_t> newTypes;
			{
				LockGuardShared<RTYPE> lock(&GetMutex()); //lock the mutex
				auto value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
				//value->m_archetypePtr->Print();	
				if(newTypes.empty()) {
					( func.template operator()<Ts>(value->m_archetypePtr, value->m_archIndex, std::forward<Ts>(vs)), ... );
					return;
				};
			}

			LockGuard<RTYPE> lock1(&GetMutex()); //lock the mutex
			auto value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
			auto arch = value->m_archetypePtr;
			if(newTypes.empty()) {
				( func.template operator()<Ts>(arch, value->m_archIndex, std::forward<Ts>(vs)), ... );
				return;
			}
			auto newArch = CreateArchetype<std::decay_t<Ts>...>(handle, newTypes);
			
			value->m_archetypePtr = newArch;
			LockGuard<RTYPE> lock2(&arch->GetMutex()); //lock old archetype, new one cannot be seen yet
			value->m_archIndex = newArch->Move(arch->Types(), value->m_archIndex, *arch, m_entities); //move values
			( func.template operator()<Ts>(newArch, value->m_archIndex, std::forward<Ts>(vs)), ... );
			return;
		}
		
		/// @brief Get a Ref object for he cmopoent of an entity.
		/// @tparam U The type of the component.
		/// @param handle The handle of the entity.
		/// @param arch The archetype of the entity.
		/// @param archIndex The index of the entity in the archetype.
		/// @return A Ref object.
		template<typename U>
			requires (std::is_reference_v<U>)
		[[nodiscard]] auto Get2( Handle handle, Archetype* arch, size_t archIndex) {
			using T = std::decay_t<U>;
			return Ref<T>{*this, handle, arch, arch->template Map<T>()->Get(archIndex)};
		}

		/// @brief Get the component value of an entity.
		/// @tparam U The type of the component.
		/// @param handle The handle of the entity.
		/// @param arch The archetype of the entity.
		/// @param archIndex The index of the entity in the archetype.
		/// @return the component value.
		template<typename U>
			requires (!std::is_reference_v<U>)
		[[nodiscard]] auto Get2( Handle handle, Archetype* arch, size_t archIndex) {
			using T = std::decay_t<U>;
			return arch->template Map<T>()->Get(archIndex);
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
		[[nodiscard]] auto Get3(Handle handle)  {
			std::vector<size_t> newTypes;
			{
				LockGuardShared<RTYPE> lock(&m_mutex); //lock the mutex
				auto value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
				//value->m_archetypePtr->Print();	
				if(newTypes.empty()) return std::make_tuple(Get2<Ts>(handle, value->m_archetypePtr, value->m_archIndex)...);
			}

			LockGuard<RTYPE> lock1(&m_mutex); //lock the mutex
			auto value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
			auto arch = value->m_archetypePtr;
			if(newTypes.empty()) return std::make_tuple(Get2<Ts>(handle, arch, value->m_archIndex)...);
			auto newArch = CreateArchetype<std::decay_t<Ts>...>(handle, newTypes);
			
			value->m_archetypePtr = newArch;
			LockGuard<RTYPE> lock2(&arch->GetMutex()); //lock old archetype, new one cannot be seen yet
			value->m_archIndex = newArch->Move(arch->Types(), value->m_archIndex, *arch, m_entities); //move values
			return std::make_tuple(Get2<Ts>(handle, newArch, value->m_archIndex)...);
		}

		/// @brief Get the archetype/index of an entity and the types of the components that are currently not contained.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param newTypes A vector to store the types of the components that are not contained.
		/// @return Pointer to the archetype/index of the entity.
		template<typename... Ts>
		inline auto GetArchetype(Handle handle, std::vector<size_t>& newTypes) -> ArchetypeAndIndex* {		
			newTypes.clear();
			newTypes.reserve(sizeof...(Ts));
			auto value = &m_entities[handle].m_value;
			
			auto func = [&]<typename T>(){ if(!value->m_archetypePtr->Has(Type<T>())) newTypes.emplace_back(Type<T>()); };
			( func.template operator()<std::decay_t<Ts>>(), ...  );
			return value;
		}

		/// @brief Create a new archetype with the types of the components that are not contained.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param newTypes A vector with the types of the components that are not contained.
		/// @return Pointer to the new archetype.
		template<typename... Ts>
		inline auto CreateArchetype(Handle handle, std::vector<size_t>& newTypes) -> Archetype* {
			auto value = GetArchetype<Ts...>(handle, newTypes);
			if( newTypes.empty() ) return value->m_archetypePtr;
			auto arch = value->m_archetypePtr;
			std::vector<size_t> allTypes;
			allTypes.reserve(arch->Types().size() + newTypes.size());
			std::copy( arch->Types().begin(), arch->Types().end(), std::back_inserter(allTypes) );
			std::copy( newTypes.begin(), newTypes.end(), std::back_inserter(allTypes) );
			size_t hs = Hash(allTypes);
			Archetype *newArch=nullptr;
			if( !m_archetypes.contains(hs) ) {
				auto newArchUnique = std::make_unique<Archetype>();
				newArch = newArchUnique.get();
				newArch->Clone(*arch, arch->Types());
				
				auto func = [&]<typename U>(){ 
					using T = std::decay_t<U>;
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

		void UpdateSearchCache(Archetype* arch) {
			auto types = arch->Types();
			for( auto& ts : m_searchCacheSet ) {
				if( std::includes(types.begin(), types.end(), ts.m_types.begin(), ts.m_types.end()) ) {
					m_searchCacheMap[ts.m_hash].insert(arch);
				}
			}
		}

		mutex_t 					m_mutex; //mutex for thread safety
		SlotMap<ArchetypeAndIndex> 	m_entities;
		std::unordered_map<size_t, std::unique_ptr<Archetype>> 	m_archetypes; //Mapping vector of type set hash to archetype 1:1
		std::unordered_map<size_t, std::set<Archetype*>> 		m_searchCacheMap; //Mapping vector of hash to archetype, 1:N
		std::vector<TypeSetAndHash> 							m_searchCacheSet; //These type combinations have been searched for
		inline static thread_local size_t 								m_numIterators{0}; //thread local variable for locking
		inline static thread_local std::vector<std::function<void()>> 	m_delayedTransactions; //thread local variable for locking
	};

	/// @brief Output operator for a Ref object.
	template<typename T>
	inline std::ostream& operator<<(std::ostream& os, Registry<REGISTRYTYPE_SEQUENTIAL>::Ref<T>& ref) {
    	return os <<  ref(); 
	}

	/// @brief Output operator for a Ref object.
	template<typename T>
	inline std::ostream& operator<<(std::ostream& os, Registry<REGISTRYTYPE_PARALLEL>::Ref<T>& ref) {
    	return os <<  ref(); 
	}

} //end of namespace vecs



