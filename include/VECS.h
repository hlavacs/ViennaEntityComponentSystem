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

	template <typename> struct is_tuple : std::false_type {};
	template <typename ...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};

	/// @brief Compute the hash of a list of hashes. If stored in a vector, make sure that hashes are sorted.
	/// @tparam T Container type of the hashes.
	/// @param hashes Reference to the container of the hashes.
	/// @return Overall hash made from the hashes.
	template <typename T>
	inline size_t Hash( const T& hashes ) {
		std::size_t seed = 0;
		if constexpr (std::is_same_v<T, const std::vector<size_t>&> ) 
			std::sort(hashes.begin(), hashes.end());
		for( auto& v : hashes ) {
			seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
		}
		return seed;
	} 

	using mutex_t = std::shared_mutex; ///< Shared mutex type.


	//----------------------------------------------------------------------------------------------

	using LockGuardType = int; ///< Type of the lock guard.
	const int LOCKGUARDTYPE_SEQUENTIAL = 0;
	const int LOCKGUARDTYPE_PARALLEL = 1;

	/// @brief A lock guard for a mutex. A LockGuard is used to lock and unlock a mutex in a RAII manner.
	/// In some instances, when the mutex is in read lock, and the same thread wants to create a new entity,
	/// the mutex must be unlocked and locked again. This is done by the LockGuard with two mutexes.
	/// @tparam LTYPE Type of the lock guard.
	template<int LTYPE = LOCKGUARDTYPE_SEQUENTIAL>
	struct LockGuard {

		/// @brief Constructor for a single mutex.
		/// @param mutex Pointer to the Archetype mutex.
		/// @param sharedMutex In case the thread has already locked the mutex in shared mode, this is the pointer to the shared mutex.
		/// If the pointers are the same, the thread already holds a read lock.
		LockGuard(mutex_t* mutex, mutex_t* sharedMutex=nullptr) 
			: m_mutex{mutex}, m_sharedMutex{sharedMutex}, m_other{nullptr}, m_sharedOther{nullptr} { 
			if constexpr (LTYPE == LOCKGUARDTYPE_SEQUENTIAL) return;
			if( mutex == m_sharedMutex ) { m_mutex->unlock_shared(); }
			m_mutex->lock(); 
		}

		/// @brief Constructor for two mutexes. This is necessary if an entity must be moved from one archetype to another, because
		/// the entity components change.
		/// @param mutex Pointer to the Archetype mutex.
		/// @param sharedMutex In case the thread has already locked the mutex in shared mode, this is the pointer to the shared mutex.
		/// If the pointers are the same, the thread already holds a read lock.
		/// @param other Pointer to the other Archetype mutex.
		LockGuard(mutex_t* mutex, mutex_t* sharedMutex, mutex_t* other, mutex_t* sharedOther) 
			: m_mutex{mutex}, m_sharedMutex{sharedMutex}, m_other{other}, m_sharedOther{sharedOther} { 
			if constexpr (LTYPE == LOCKGUARDTYPE_SEQUENTIAL) return;
			if( m_mutex == m_sharedMutex ) { m_mutex->unlock_shared(); } //check if the mutex is already locked
			if( m_other == m_sharedOther ) { m_other->unlock_shared(); }
			std::min(m_mutex, m_other)->lock();	///lock the mutexes in the correct order
			std::max(m_mutex, m_other)->lock();
		}

		/// @brief Destructor, unlocks the mutexes.
		~LockGuard() { 
			if constexpr (LTYPE == LOCKGUARDTYPE_SEQUENTIAL) return;
			if(m_other) { 
				std::max(m_mutex, m_other)->unlock();
				std::min(m_mutex, m_other)->unlock();	///lock the mutexes in the correct order
				if( m_mutex == m_sharedMutex && m_other == m_sharedOther ) { 
					std::min(m_mutex, m_other)->lock_shared();	///lock the mutexes in the correct order
					std::max(m_mutex, m_other)->lock_shared();
				} 
				else if( m_mutex == m_sharedMutex ) { m_mutex->lock_shared(); }
				else if( m_other == m_sharedOther ) { m_other->lock_shared(); }
				return;
			}
			m_mutex->unlock();
			if( m_mutex == m_sharedMutex ) { m_mutex->lock_shared(); } //get the shared lock back
		}

		mutex_t* m_mutex{nullptr};
		mutex_t* m_sharedMutex{nullptr};
		mutex_t* m_other{nullptr};
		mutex_t* m_sharedOther{nullptr};
	};

	/// @brief A lock guard for a shared mutex in RAII manner.
	/// @tparam LTYPE Type of the lock guard.
	template<int LTYPE = LOCKGUARDTYPE_SEQUENTIAL>
	struct LockGuardShared {

		/// @brief Constructor for a single mutex. Acquire only if the thread has not already locked the mutex in shared mode.
		LockGuardShared(mutex_t* mutex, mutex_t* sharedMutex=nullptr) : m_mutex{mutex}, m_sharedMutex{sharedMutex} { 
			if constexpr (LTYPE == LOCKGUARDTYPE_SEQUENTIAL) return;
			if( m_mutex != m_sharedMutex ) m_mutex->lock_shared();
		}

		/// @brief Destructor, unlocks the mutex.
		~LockGuardShared() { 
			if constexpr (LTYPE == LOCKGUARDTYPE_SEQUENTIAL) return;
			if( m_mutex != m_sharedMutex ) m_mutex->unlock_shared();
		}

		mutex_t* m_mutex{nullptr};		 ///< Pointer to the mutex.
		mutex_t* m_sharedMutex{nullptr}; ///< Pointer to the shared mutex.
	};


	//----------------------------------------------------------------------------------------------
	//Segmented Stack

	template<typename T>
	class Stack {

		using segment_t = std::unique_ptr<std::vector<T>>;
		using stack_t = std::vector<segment_t>;

		public:
			Stack(size_t segmentBits = 10) : m_segmentBits(segmentBits), m_segmentSize{1ul<<segmentBits} {
				assert(segmentBits > 0);
				m_stack.push_back( std::make_unique<std::vector<T>>(m_segmentSize) );
			}

			~Stack() = default;

			template<typename U>
				requires std::is_convertible_v<U, T>
			void push_back(U&& value) {
				if( size() >> m_segmentBits == m_stack.size() ) {
					m_stack.push_back( std::make_unique<std::vector<T>>(m_segmentSize) );
				}
				++m_size;
				operator[](size()-1) = std::forward<U>(value);
			}

			void pop_back() {
				assert(m_size > 0);
				--m_size;
				if(	(m_size & (m_segmentSize-1ul)) == 0 && m_stack.size() > 1 ) {
					m_stack.pop_back();
				}
			}

			auto operator[](size_t index) -> T& {
				assert(index < m_size);
				size_t segment = index >> m_segmentBits;
				size_t offset = index & (m_segmentSize-1ul);
				return (*m_stack[segment])[offset];
			}

			auto size() -> size_t {
				return m_size;
			}

			void clear() {
				m_size = 0;
				m_stack.clear();
				m_stack.push_back( std::make_unique<std::vector<T>>(m_segmentSize) );
			}

		private:
			size_t m_size{0};
			size_t m_segmentBits{10};
			size_t m_segmentSize{1<<m_segmentBits};
			stack_t m_stack;
	};



	//----------------------------------------------------------------------------------------------
	//Slot Maps

	struct Handle {
		uint32_t m_index;
		uint32_t m_version;

		bool operator<(const Handle& other) const {
			return m_index < other.m_index;
		}
	};

	/// @brief A slot map for storing a map from handle to archetpe and index.
	/// @tparam T The value type of the slot map.
	/// @tparam SIZE The minimum size of the slot map.
	template<typename T>
	class SlotMap {

		/// @brief Need atomics only for the parallel case
		using next_t = int64_t;
		using vers_t = size_t;

		/// @brief A slot in the slot map.
		struct Slot {
			next_t m_nextFree; 	//index of the next free slot
			vers_t m_version;	//version of the slot
			T 	   m_value{};	//value of the slot

			Slot() = default;

			Slot(const next_t&& next, const vers_t&& version, T&& value) : m_value{std::forward<T>(value)} {
				m_nextFree = next;
				m_version = version;
			}

			Slot& operator=( const Slot& other ) {
				m_nextFree = other.m_nextFree;
				m_version = other.m_version;
				m_value = other.m_value;
				return *this;
			}
		};

	public:

		/// @brief Constructor, creates the slot map.
		SlotMap(int64_t SIZE = 1<<20) {
			m_firstFree = 0;
			for( int64_t i = 1; i <= SIZE-1; ++i ) { //create the free list
				m_slots.push_back( Slot( next_t{i}, vers_t{0uL}, T{} ) );
			}
			m_slots.push_back( Slot(next_t{-1}, vers_t{0}, T{}) ); //last slot
		}
		
		~SlotMap() = default; ///< Destructor.
		
		auto Insert(T&& value) -> std::pair<Handle, Slot&> {
			{
				int64_t firstFree = m_firstFree;
				if( firstFree > -1 ) { 
					auto& slot = m_slots[firstFree];
					m_firstFree = slot.m_nextFree;
					slot.m_nextFree = -1;
					slot.m_value = std::forward<T>(value);
					++m_size;
					return {Handle{(uint32_t)firstFree, (uint32_t)slot.m_version}, slot};		
				}
			}

			m_slots.push_back( Slot{ next_t{-1}, vers_t{0}, std::forward<T>(value) } );
			size_t index = m_slots.size() - 1; //index of the new slot
			auto& slot = m_slots[index];
			slot.m_value = std::forward<T>(value);
			++m_size;
			return { Handle{ (uint32_t)index, (uint32_t)slot.m_version}, slot};			
		}

		/// @brief Erase a value from the slot map.
		/// @param index The index of the value to erase.
		void Erase(Handle handle) {
			assert(handle.m_index < m_slots.size());
			auto& slot = m_slots[handle.m_index];
			++slot.m_version;	//increment the version to invalidate the slot
			slot.m_nextFree = m_firstFree;	
			m_firstFree = handle.m_index; //add the slot to the free list
			--m_size;
		}

		auto operator[](Handle handle) -> Slot& {
			assert(handle.m_index < m_slots.size());
			auto& slot = m_slots[handle.m_index];
			assert(slot.m_version == handle.m_version);
			return slot;
		}

		auto Size() -> size_t {
			return m_size;
		}

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
		vers_t m_size{0}; ///< Size of the slot map.
		next_t m_firstFree{-1}; ///< Index of the first free slot.
		Stack<Slot> m_slots{15}; ///< Vector of slots.
	};


	//----------------------------------------------------------------------------------------------

	using RegistryType = int;
	const int REGISTRYTYPE_SEQUENTIAL = 0;
	const int REGISTRYTYPE_PARALLEL = 1;


	inline std::ostream& operator<<(std::ostream& os, const vecs::Handle& handle) {
    	return os << "{" <<  handle.m_index << ", " << handle.m_version << "}"; 
	}

	template<typename T>
	auto Type() -> std::size_t {
		return std::type_index(typeid(T)).hash_code();
	}

	template<typename... Ts>
	concept VecsArchetype = (vtll::unique<vtll::tl<Ts...>>::value && (sizeof...(Ts) > 0) && (!std::is_same_v<Handle, std::decay_t<Ts>> && ...));

	template<typename... Ts>
	concept VecsView = ((vtll::unique<vtll::tl<Ts...>>::value) && (sizeof...(Ts) > 0) && (!std::is_same_v<Handle&, Ts> && ...));

	template<typename... Ts>
	concept VecsIterator = (vtll::unique<vtll::tl<Ts...>>::value);

	template<typename... Ts>
		requires VecsIterator<Ts...>
	class Iterator;

	template<typename... Ts>
		requires VecsView<Ts...>
	class View;

	
	template<typename U> class Ref;

	/// @brief A registry for entities and components.
	template <RegistryType RTYPE = REGISTRYTYPE_SEQUENTIAL>
	class Registry {

	public:
	
		//----------------------------------------------------------------------------------------------

		template<typename U> friend class Ref;

	private:

		//----------------------------------------------------------------------------------------------

		/// @brief Base class for component maps.
		class ComponentMapBase {

			friend class Archetype; //for eaccessing the mutex
			template<typename... Ts> requires VecsIterator<Ts...>
			friend class Iterator;

		public:
			ComponentMapBase() = default;
			virtual ~ComponentMapBase() = default;
			
			size_t Insert(auto&& v) {
				using T = std::decay_t<decltype(v)>;
				return static_cast<ComponentMap<T>*>(this)->Insert(std::forward<T>(v));
			}

			virtual auto Erase(size_t index) -> size_t = 0;
			virtual void Move(ComponentMapBase* other, size_t from) = 0;
			virtual auto Size() -> size_t= 0;
			virtual auto Clone() -> std::unique_ptr<ComponentMapBase> = 0;
			virtual void Clear() = 0;
			virtual void print() = 0;
		}; //end of ComponentMapBase

		//----------------------------------------------------------------------------------------------

		/// @brief A map of components of the same type.
		/// @tparam U The value type for this comoonent map. Note that the type is decayed.
		/// If you want to store a pointer or reference, use a struct to wrap it.
		template<typename U>
		class ComponentMap : public ComponentMapBase {

			using T = std::decay_t<U>; //remove pointer or reference

		public:

			ComponentMap() = default; //constructor

			/// @brief Insert a new component value.
			/// @param handle The handle of the entity.
			/// @param v The component value.
			[[nodiscard]] size_t Insert(auto&& v) {
				m_data.push_back( std::forward<T>(v) );
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

			/// @brief Erase a component from the vector.
			/// @param index The index of the component value in the map.
			/// @return The index of the last element in the vector.
			[[nodiscard]] virtual auto Erase(size_t index)  -> size_t override {
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
			virtual void Move(ComponentMapBase* other, size_t from) override {
				m_data.push_back( static_cast<ComponentMap<std::decay_t<T>>*>(other)->Get(from) );
			};

			/// @brief Get the size of the map.
			/// @return The size of the map.
			[[nodiscard]] virtual auto Size() -> size_t override {
				return m_data.size();
			}

			/// @brief Clone a new map from this map.
			/// @return A unique pointer to the new map.
			[[nodiscard]] virtual auto Clone() -> std::unique_ptr<ComponentMapBase> override {
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


		/// @brief An archetype of entities with the same components. 
		/// All entities that have the same components are stored in the same archetype.
		class Archetype;

		/// @brief A pair of an archetype and an index. This is stored in the slot map.
		struct ArchetypeAndIndex {
			Archetype* m_archetypePtr;	//pointer to the archetype
			size_t m_archIndex;			//index of the entity in the archetype
		};

		/// @brief An archetype of entities with the same components. 
		/// All entities that have the same components are stored in the same archetype.
		class Archetype {

			template <RegistryType> friend class Registry;

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
			[[nodiscard]] bool Has(const size_t ti) {
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

			void Validate() {
				for( auto& map : m_maps ) {
					assert( map.second->Size() == m_maps[Type<Handle>()]->Size() );
				}
			}
		
			size_t GetChangeCounter() {
				return m_changeCounter;
			}

			auto GetMutex() -> mutex_t& {
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

			auto AddValue( auto&& v ) -> size_t {
				using T = std::decay_t<decltype(v)>;
				return m_maps[Type<T>()]->Insert(std::forward<T>(v));	//insert the component value
			};
	
			void Erase2(size_t index, auto& slotmap) {
				size_t last{index};
				for( auto& it : m_maps ) {
					last = it.second->Erase(index); //should always be the same handle
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

		template<typename U>
			requires (!std::is_reference_v<U>)
		class Ref {

			using T = std::decay_t<U>;

		public:
			Ref(Registry<RTYPE>& system, Handle handle, Archetype *arch, T& valueRef) 
				: m_system{system}, m_handle{handle}, m_archetype{arch}, m_valueRef{valueRef}, m_changeCounter{arch->GetChangeCounter()} {}

			auto operator()() {
				return CheckChangeCounter();
			}

			void operator=(T&& value) {
				CheckChangeCounter();
				m_valueRef = std::forward<T>(value);
			}

			auto Get() {
				return CheckChangeCounter();
			}

		private:
			auto CheckChangeCounter() -> T& {
				auto cc = m_archetype->GetChangeCounter();
				if(cc != m_changeCounter ) {
					auto [arch, ref] = m_system.Get2<T>(m_handle);
					m_archetype = arch;
					m_valueRef = ref;
					m_changeCounter = cc;
				}
				return  m_valueRef;
			}

			Registry<RTYPE>& m_system;
			Handle m_handle;
			Archetype* m_archetype;
			T& m_valueRef;
			size_t m_changeCounter;
		};


		//----------------------------------------------------------------------------------------------

		struct ArchetypeAndSize {
			Archetype* m_archetype;
			size_t m_size;
		};

		/// @brief Used for iterating over entity components.
		/// @tparam ...Ts Choose the types of the components you want the entities to have.
		template<typename... Ts>
			requires VecsIterator<Ts...>
		class Iterator {

		public:
			/// @brief Iterator constructor saving a list of archetypes and the current index.
			/// @param arch List of archetypes. 
			/// @param archidx First archetype index.
			Iterator( Registry<RTYPE>& system, std::vector<ArchetypeAndSize>& arch, size_t archidx) 
				: m_system{system}, m_archetypes{arch}, m_archidx{archidx} {
				Next();
			}

			Iterator(const Iterator& other) : m_system{other.m_system}, m_archetypes{other.m_archetypes}, 
				m_archidx{other.m_archidx}, m_entidx{other.m_entidx}, m_isLocked{false} {
			}

			~Iterator() {
				UnlockShared();
			}

			/// @brief Prefix increment operator.
			auto operator++() {
				++m_entidx;
				Next();				
				return *this;
			}

			/// @brief Access the content the iterator points to.
			auto operator*() {
				auto arch = m_archetypes[m_archidx].m_archetype;
				assert( m_entidx < arch->Size() );
				Handle handle = arch->template Map<Handle>()->Get(m_entidx);
				auto tup = std::make_tuple( Get<Ts>(m_system, handle, arch)... );
				if constexpr (sizeof...(Ts) == 1) { return std::get<0>(tup); }
				else return tup;
			}

			auto operator!=(const Iterator& other) -> bool {
				return (m_archidx != other.m_archidx) || (m_entidx != other.m_entidx);
			}

		private:

			template<typename U>
			requires (!std::is_same_v<U, Handle&> && std::is_reference_v<U>)
			[[nodiscard]] auto Get(auto &system, Handle handle, Archetype *arch) {
				using T = std::decay_t<U>;
				return Ref<T>{system, handle, arch, arch->template Map<T>()->Get(m_entidx)};
			}

			template<typename U>
			requires (!std::is_same_v<U, Handle&> && !std::is_reference_v<U>)
			[[nodiscard]] auto Get(auto &system, Handle handle, Archetype *arch) {
				using T = std::decay_t<U>;
				return arch->template Map<T>()->Get(m_entidx);
			}

			void Next() {
				while( m_archidx < m_archetypes.size() && m_entidx >= std::min(m_archetypes[m_archidx].m_size, m_archetypes[m_archidx].m_archetype->Size()) ) {
					if( m_isLocked ) UnlockShared();
					++m_archidx;
					m_entidx = 0;
				} 
				if( m_archidx < m_archetypes.size() ) LockShared();
			}

			void LockShared() {
				if constexpr (RTYPE == REGISTRYTYPE_SEQUENTIAL) return;
				if( m_archidx >= m_archetypes.size() ) return;
				if( m_isLocked ) return;
				auto arch = m_archetypes[m_archidx].m_archetype;
				m_system.SetSharedMutexPtr(&arch->GetMutex()); //thread remembers that it locked the mutex
				arch->GetMutex().lock_shared();
				m_isLocked = true;
			}

			void UnlockShared() {
				if constexpr (RTYPE == REGISTRYTYPE_SEQUENTIAL) return;
				if( m_archidx >= m_archetypes.size() ) return;
				if( !m_isLocked ) return;
				auto arch = m_archetypes[m_archidx].m_archetype;
				arch->GetMutex().unlock_shared();
				m_system.SetSharedMutexPtr(nullptr);
				m_isLocked = false;
			}

			Registry<RTYPE>& m_system;
			std::vector<ArchetypeAndSize>& m_archetypes;
			size_t m_archidx{0};
			size_t m_entidx{0};
			bool m_isLocked{false};
		}; //end of Iterator


		//----------------------------------------------------------------------------------------------

		/// @brief A view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		template<typename... Ts>
			requires VecsView<Ts...>
		class View {

		public:
			View(Registry<RTYPE>& system) : m_system{system} {}

			auto begin() {
				m_archetypes.clear();
				{
					LockGuardShared<RTYPE> lock(&m_system.m_mutex);
					for( auto& it : m_system.m_archetypes ) {
						auto arch = it.second.get();
						auto func = [&]<typename T>() {
							if( arch->Types().contains(Type<T>())) return true;
							return false;
						};
						if( (func.template operator()<Ts>() && ...) && arch->Size()>0 ) m_archetypes.push_back({arch, arch->Size()});
					}
				}
				return Iterator<Ts...>{m_system, m_archetypes, 0};
			}

			auto end() {
				return Iterator<Ts...>{m_system, m_archetypes, m_archetypes.size()};
			}

		private:
			Registry<RTYPE>& m_system;
			std::vector<ArchetypeAndSize> m_archetypes;
		}; //end of View


		//----------------------------------------------------------------------------------------------

		Registry() = default; ///< Constructor.
		~Registry() = default;

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
			LockGuard<RTYPE> lock(&m_mutex, m_sharedMutex); //lock the mutex

			std::vector<size_t> types = {Type<Handle>(), Type<Ts>()...};
			auto [handle, slot] = m_entities.Insert( {nullptr, 0} ); //get a slot for the entity

			size_t archIndex;
			size_t hs = Hash(types);
			if( !m_archetypes.contains( hs ) ) { //not found
				m_archetypes[hs] = std::make_unique<Archetype>( handle, archIndex, std::forward<Ts>(component)... );
				slot.m_value = { m_archetypes[hs].get(), archIndex };
			} else {
				auto arch = m_archetypes[hs].get();
				LockGuard<RTYPE> lock(&arch->GetMutex(), m_sharedMutex); //lock the mutex
				archIndex = arch->Insert( handle, std::forward<Ts>(component)... );
				slot.m_value = { arch, archIndex };
			}
			return handle;
		}

		/// @brief Test if an entity exists.
		/// @param handle The handle of the entity.
		/// @return true if the entity exists, else false.
		bool Exists(Handle handle) {
			LockGuardShared<RTYPE> lock(&m_mutex, m_sharedMutex); //lock the mutex
			auto& slot = m_entities[handle];
			return slot.m_version == handle.m_version;
		}

		/// @brief Test if an entity has a component.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return true if the entity has the component, else false.
		template<typename T>
		bool Has(Handle handle) {
			assert(Exists(handle));
			Archetype* arch;
			{
				LockGuardShared<RTYPE> lock(&m_mutex, m_sharedMutex); //lock the system
				arch = m_entities[handle].m_value.m_archetypePtr;
			}
			LockGuardShared<RTYPE> lock(&arch->GetMutex(), m_sharedMutex); //lock the archetype
			return arch->Has(Type<T>());
		}

		/// @brief Get the types of the components of an entity.
		/// @param handle The handle of the entity.
		/// @return A vector of type indices of the components.
		const auto& Types(Handle handle) {
			assert(Exists(handle));
			Archetype* arch;
			{
				LockGuardShared<RTYPE> lock(&m_mutex, m_sharedMutex); //lock the system
				arch = m_entities[handle].m_value.m_archetypePtr;
			}
			LockGuardShared<RTYPE> lock(&arch->GetMutex(), m_sharedMutex); //lock the mutex
			return arch->Types();
		}

		/// @brief Get component value of an entity. If the component does not exist, it will be created.
		/// This might result in moving the entity to another archetype.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.	
		/// @return The component value.
		template<typename U>
			requires (!std::is_same_v<U, Handle&> && std::is_reference_v<U>)
		[[nodiscard]] auto Get(Handle handle) {
			using T = std::decay_t<U>;
			auto [arch, ref] = Get2<T>(handle);
			return Ref<T>{*this, handle, arch, ref};
		}

		template<typename U>
			requires (!std::is_same_v<U, Handle&> && !std::is_reference_v<U>)
		[[nodiscard]] auto Get(Handle handle) {
			using T = std::decay_t<U>;
			return Get2<T>(handle).second;
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value))
		[[nodiscard]] auto Get(Handle handle)  {
			return std::make_tuple(Get<Ts>(handle)...); //defer to Get<Ts>()
		}

		/// @brief Put a new component value to an entity. If the entity does not have the component, it will be created.
		/// This might result in moving the entity to another archetype.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @param v The new value.
		template<typename U>
			requires (!is_tuple<U>::value)
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
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Put(Handle handle, std::tuple<Ts...>& v) {
			(Put2(handle, std::forward<Ts>(std::get<Ts>(v))), ...);
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Put(Handle handle, Ts&&... vs) {
			(Put2(handle, vs), ...);
		}
		
		/// @brief Erase components from an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Erase(Handle handle) {
			assert( (Has<Ts>(handle) && ...) );
			ArchetypeAndIndex *value;
			std::set<size_t> types;
			Archetype *oldArch, *arch;
			size_t hs;
			bool contained;

			{
				LockGuardShared<RTYPE> lock(&m_mutex, m_sharedMutex); //lock the mutex
				value = &m_entities[handle].m_value;
				oldArch = value->m_archetypePtr;
				{
					LockGuardShared<RTYPE> lock(&oldArch->m_mutex, m_sharedMutex); //lock the archetype
					types = oldArch->Types();
				}
				(types.erase(Type<Ts>()), ... );
				hs = Hash(types);
				contained = m_archetypes.contains(hs);
				if( contained ) { arch = m_archetypes[hs].get(); }
			}

			auto IsContained = [&]() {
				value->m_archetypePtr = arch;
				value->m_archIndex = arch->Move(types, value->m_archIndex, *oldArch, m_entities);
			};
			
			if( contained ) { 
				LockGuard<RTYPE> lock(&arch->m_mutex, m_sharedMutex, &oldArch->m_mutex, m_sharedMutex);
				return IsContained(); 
			}

			{
				LockGuard<RTYPE> lock1(&m_mutex); //lock the mutex
				if( !m_archetypes.contains(hs) ) {
					auto archPtr = std::make_unique<Archetype>();
					arch = archPtr.get();
		 			LockGuard<RTYPE> lock2(&arch->m_mutex, m_sharedMutex, &oldArch->m_mutex, m_sharedMutex);
					arch->Clone(*oldArch, types);
					m_archetypes[hs] = std::move(archPtr);
					return IsContained();
				}
				arch = m_archetypes[hs].get();
			}

 			LockGuard<RTYPE> lock(&arch->m_mutex, m_sharedMutex, &oldArch->m_mutex, m_sharedMutex);
			return IsContained();
		}

		/// @brief Erase an entity from the registry.
		/// @param handle The handle of the entity.
		void Erase(Handle handle) {
			assert(Exists(handle));
			LockGuard<RTYPE> lock(&m_mutex); //lock the mutex
			auto& value = m_entities[handle].m_value;
			value.m_archetypePtr->Erase(value.m_archIndex, m_entities); //erase the entity from the archetype (do both locked)
			m_entities.Erase(handle); //erase the entity from the entity list
		}

		/// @brief Clear the registry by removing all entities.
		void Clear() {
			LockGuard<RTYPE> lock(&m_mutex); //lock the mutex
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

		void Print() {
			std::cout << "Entities: " << m_entities.Size() << std::endl;
			for( auto& it : m_archetypes ) {
				it.second->print();
			}
			std::cout << std::endl << std::endl;
		}

		auto GetSharedMutexPtr() -> mutex_t* {
			return m_sharedMutex;
		}

		auto SetSharedMutexPtr(mutex_t* ptr) {
			m_sharedMutex = ptr;
		}

		void Validate() {
			LockGuardShared<RTYPE> lock(&m_mutex);
			for( auto& it : m_archetypes ) {
				auto arch = it.second.get();
				LockGuardShared<RTYPE> lock(&arch->m_mutex);
				arch->Validate();
			}
		}

	private:

		template<typename U>
		void Put2(Handle handle, U&& v) {
			using T = std::decay_t<U>;
			Get2<T>(handle).second = std::forward<T>(v);
		}

		/// @brief Try to get a component value of an entity.
		/// If the component does not exist, it will be created.
		/// This might result in moving the entity to another archetype.
		/// @tparam U The type of the component.
		/// @param handle The handle of the entity.
		/// @return A pair of the archetype pointer and the component value.
		template<typename U>
		auto Get2(Handle handle) -> std::pair<Archetype*, std::decay_t<U>&> {
			using T = std::decay_t<U>;

			size_t hs;
			bool contained;
			ArchetypeAndIndex *value;
			Archetype *arch, *oldArch;
			{
				std::vector<size_t> types;
				LockGuardShared<RTYPE> lock(&m_mutex); //lock the system
				auto& entry = m_entities[handle];
				value = &entry.m_value;
				oldArch = value->m_archetypePtr;
				{
					LockGuardShared<RTYPE> lock(&oldArch->m_mutex, m_sharedMutex); //lock the archetype
					if(oldArch->Has(Type<T>())) {
						return std::pair<Archetype*, T&>{oldArch, oldArch->template Get<T>(value->m_archIndex)};
					}
					types.reserve(oldArch->Types().size() + 1);
					std::copy( oldArch->Types().begin(), oldArch->Types().end(), std::back_inserter(types) );
				}
				types.push_back(Type<T>());
				hs = Hash(types);
				contained = m_archetypes.contains(hs);
				if( contained ) { arch = m_archetypes[hs].get(); }
			}
			
			auto IsContained = [&]() {
				value->m_archetypePtr = arch;
				value->m_archIndex = arch->Move(oldArch->Types(), value->m_archIndex, *oldArch, m_entities);
				arch->AddValue(T{});
				return std::pair<Archetype*, T&>{arch, arch->template Get<T>(value->m_archIndex)};
			};

			if(contained) { 
				LockGuard<RTYPE> lock(&arch->m_mutex, m_sharedMutex, &oldArch->m_mutex, m_sharedMutex);
				return IsContained(); 
			}

			{
				LockGuard<RTYPE> lock1(&m_mutex); //lock the system
				if( !m_archetypes.contains(hs) ) { //still not found?
					auto archPtr = std::make_unique<Archetype>();
					arch = archPtr.get();
					LockGuard<RTYPE> lock2(&arch->m_mutex, m_sharedMutex, &oldArch->m_mutex, m_sharedMutex);
					arch->Clone(*oldArch, oldArch->Types());
					arch->template AddComponent<T>();
					m_archetypes[hs] = std::move(archPtr);
					return IsContained();
				}
				arch = m_archetypes[hs].get();
			}

			LockGuard<RTYPE> lock(&arch->m_mutex, m_sharedMutex, &oldArch->m_mutex, m_sharedMutex);
			return IsContained();
		}

		mutex_t m_mutex; //mutex for thread safety
		SlotMap<ArchetypeAndIndex> m_entities;
		std::unordered_map<size_t, std::unique_ptr<Archetype>> m_archetypes; //Mapping vector of type index to archetype
		inline static thread_local mutex_t* m_sharedMutex{nullptr}; //thread local variable for locking
	};


	template<typename T>
	inline std::ostream& operator<<(std::ostream& os, Registry<REGISTRYTYPE_SEQUENTIAL>::Ref<T>& ref) {
    	return os <<  ref(); 
	}

	template<typename T>
	inline std::ostream& operator<<(std::ostream& os, Registry<REGISTRYTYPE_PARALLEL>::Ref<T>& ref) {
    	return os <<  ref(); 
	}

} //end of namespace vecs



