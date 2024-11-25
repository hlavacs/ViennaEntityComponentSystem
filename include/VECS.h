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

	template <typename T>
	inline size_t Hash( const T& hashes ) {
		std::size_t seed = 0;
		if constexpr (std::is_same_v<T, const std::vector<size_t>&>) 
			std::sort(hashes.begin(), hashes.end());
		for( auto& v : hashes ) {
			seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
		}
		return seed;
	} 

	using mutex_t = std::shared_mutex;


	//----------------------------------------------------------------------------------------------

	using LockGuardType = int;
	const int LOCKGUARDTYPE_SEQUENTIAL = 0;
	const int LOCKGUARDTYPE_PARALLEL = 1;

	template<int LTYPE = LOCKGUARDTYPE_SEQUENTIAL>
	struct LockGuard {
		LockGuard(mutex_t* mutex, mutex_t* sharedMutex) 
			: m_mutex{mutex}, m_sharedMutex{sharedMutex}, m_other{nullptr}, m_sharedOther{nullptr} { 

			if constexpr (LTYPE == LOCKGUARDTYPE_SEQUENTIAL) return;
			if( mutex == m_sharedMutex ) { m_mutex->unlock_shared(); }
			m_mutex->lock(); 
		}

		LockGuard(mutex_t* mutex, mutex_t* sharedMutex, mutex_t* other, mutex_t* sharedOther) 
			: m_mutex{mutex}, m_sharedMutex{sharedMutex}, m_other{other}, m_sharedOther{sharedOther} { 

			if constexpr (LTYPE == LOCKGUARDTYPE_SEQUENTIAL) return;
			if( m_mutex == m_sharedMutex ) { m_mutex->unlock_shared(); }
			if( m_other == m_sharedOther ) { m_other->unlock_shared(); }
			std::min(m_mutex, m_other)->lock();
			std::max(m_mutex, m_other)->lock();
		}

		~LockGuard() { 
			if constexpr (LTYPE == LOCKGUARDTYPE_SEQUENTIAL) return;
			if(m_other) { 
				m_other->unlock();
				if( m_other == m_sharedOther ) { m_other->lock_shared(); }
			}
			m_mutex->unlock();
			if( m_mutex == m_sharedMutex ) { m_mutex->lock_shared(); }
		}

		mutex_t* m_mutex;
		mutex_t* m_sharedMutex;
		mutex_t* m_other{nullptr};
		mutex_t* m_sharedOther;
	};

	template<int LTYPE = LOCKGUARDTYPE_SEQUENTIAL>
	struct LockGuardShared {
		LockGuardShared(mutex_t* mutex, mutex_t* sharedMutex=nullptr) : m_mutex{mutex}, m_sharedMutex{sharedMutex} { 
			if constexpr (LTYPE == LOCKGUARDTYPE_SEQUENTIAL) return;
			if( mutex != m_sharedMutex ) m_mutex->lock_shared();
		}

		~LockGuardShared() { 
			if constexpr (LTYPE == LOCKGUARDTYPE_SEQUENTIAL) return;
			if( m_mutex != m_sharedMutex ) m_mutex->unlock_shared();
		}

		mutex_t* m_mutex;
		mutex_t* m_sharedMutex;
	};



	//----------------------------------------------------------------------------------------------
	//Slot Maps

	using SlotMapType = int;
	const int SLOTMAPTYPE_SEQUENTIAL = 0;
	const int SLOTMAPTYPE_PARALLEL = 1;

	template<typename T, size_t SIZE = 1024, SlotMapType STYPE = SLOTMAPTYPE_SEQUENTIAL>
	class SlotMap {

		using next_t = std::conditional_t<STYPE==SLOTMAPTYPE_PARALLEL, std::atomic<int64_t>, int64_t>;
		using vers_t = std::conditional_t<STYPE==SLOTMAPTYPE_PARALLEL, std::atomic<size_t>, size_t>;

		struct Slot {
			next_t m_nextFree{-1}; //index of the next free slot
			vers_t m_version{0};	//version of the slot
			T 	   m_value{};		//value of the slot
		};

	public:
		SlotMap() {
			m_firstFree = 0;
			for( int64_t i = 1; i <= SIZE-1; ++i ) {
				m_slots.emplace_back( i, 0uL, T{} );
			}
			m_slots.emplace_back( -1, 0, T{} );
		}
		
		~SlotMap() = default;

		auto Insert(T&& value) -> std::pair<size_t,Slot&> {
			size_t index;
			if( m_firstFree != -1 ) {
				index = m_firstFree;
				auto& slot = m_slots[m_firstFree];
				m_firstFree = slot.m_nextFree;
				slot.m_value = std::forward<T>(value);
			} else {
				m_slots.emplace_back( 0, 0, std::forward<T>(value) );
				index = m_slots.size() - 1;
			}
			++m_size;
			return {index, m_slots[index]};
		}

		void Erase(size_t index) {
			if constexpr (STYPE == SLOTMAPTYPE_PARALLEL) { m_mutex.lock_shared(); }
			assert(index < m_slots.size());
			auto& slot = m_slots[index];
			++slot.m_version;	//increment the version to invalidate the slot
			slot.m_nextFree = m_firstFree;
			m_firstFree = index;
			--m_size;
			if constexpr (STYPE == SLOTMAPTYPE_PARALLEL) { m_mutex.unlock_shared(); }
		}

		auto operator[](size_t index) -> Slot& {
			assert(index < m_slots.size());
			auto& value = m_slots[index];
			return value;
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
		mutex_t m_mutex;
		size_t m_size{0};
		std::deque<Slot> m_slots;
		next_t m_firstFree{-1};
	};


	//----------------------------------------------------------------------------------------------

	using RegistryType = int;
	const int REGISTRYTYPE_SEQUENTIAL = 0;
	const int REGISTRYTYPE_PARALLEL = 1;

	struct Handle {
		uint32_t m_index;
		uint32_t m_version;
	};

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
				m_data.emplace_back( std::forward<T>(v) );
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
			std::vector<T> m_data; //vector of component values
		}; //end of ComponentMap

		//----------------------------------------------------------------------------------------------


		/// @brief An archetype of entities with the same components. 
		/// All entities that have the same components are stored in the same archetype.
		class Archetype;

		/// @brief A pair of an archetype and an index. This is stored in the slot map.
		struct ArchetypeAndIndex {
			Archetype* m_archetype_ptr;	//pointer to the archetype
			size_t m_archIndex;			//index of the entity in the archetype
		};

		/// @brief An archetype of entities with the same components. 
		/// All entities that have the same components are stored in the same archetype.
		class Archetype {

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
				(AddValue( archIndex, std::forward<Ts>(values) ), ...); //insert all components, get index of the handle
				AddComponent<Handle>(); //insert the handle
				AddValue( archIndex, handle ); //insert the handle
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
				size_t index;
				(AddValue( index, std::forward<Ts>(values) ), ...); //insert all components, get index of the handle
				AddValue( index, handle ); //insert the handle
				++m_changeCounter;
				return index;
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
			void Erase(size_t index, SlotMap<ArchetypeAndIndex>& slotmap) {
				size_t last{index};
				for( auto& it : m_maps ) {
					last = it.second->Erase(index); //should always be the same handle
				}
				if( index < last ) {
					auto& lastHandle = static_cast<ComponentMap<Handle>*>(m_maps[Type<Handle>()].get())->Get(index);
					slotmap[lastHandle.m_index].m_value.m_archIndex = index;
				}
				++m_changeCounter;
			}

			/// @brief Move components from another archetype to this one.
			size_t Move( auto& types, size_t other_index, Archetype& other, SlotMap<ArchetypeAndIndex>& slotmap) {
				for( auto& ti : types ) { //go through all maps
					m_types.insert(ti); //add the type to the list
					m_maps[ti] = other.Map(ti)->Clone(); //make a component map like this one
					m_maps[ti]->Move(other.Map(ti), other_index); //insert the new value
				}
				other.Erase(other_index, slotmap); //erase from old component map
				++m_changeCounter;
				return m_maps[Type<Handle>()]->Size() - 1; //return the index of the new entity
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

			/// @brief Get the types of the components.
			const std::set<size_t>& Types() {
				return m_types;
			}

			/// @brief Get the maps of the components.
			const std::unordered_map<size_t, std::unique_ptr<ComponentMapBase>>& Maps() {
				return m_maps;
			}

			void print() {
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

			/// @brief Add a new value to the archetype.
			/// @param v The new value.
			/// @return The index of the entity in the archetype.
			void AddValue( size_t& index, auto&& v ) {
				using T = std::decay_t<decltype(v)>;
				index = m_maps[Type<T>()]->Insert(std::forward<T>(v));	//insert the component value
			};

			size_t GetChangeCounter() {
				return m_changeCounter;
			}

			auto GetMutex() -> mutex_t& {
				return m_mutex;
			}

			auto GetSharedMutexPtr() -> mutex_t* {
				return m_sharedMutex;
			}

			auto SetSharedMutexPtr(mutex_t* ptr) {
				m_sharedMutex = ptr;
			}

		private:
			
			mutex_t m_mutex; //mutex for thread safety
			size_t m_changeCounter{0}; //changes invalidate references
			std::set<size_t> m_types; //types of components
			std::unordered_map<size_t, std::unique_ptr<ComponentMapBase>> m_maps; //map from type index to component data
			inline static thread_local mutex_t* m_sharedMutex{nullptr}; //thread local variable for locking
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

			auto operator()() -> T {
				return CheckChangeCounter();
			}

			void operator=(T&& value) {
				CheckChangeCounter() = std::forward<T>(value);
			}

		private:
			T& CheckChangeCounter() {
				if(m_archetype->GetChangeCounter() != m_changeCounter ) {
					auto [arch, ref] = m_system.TryComponent<T>(m_handle);
					m_archetype = arch;
					m_valueRef = ref;
					m_changeCounter = arch->GetChangeCounter();
				}
				return m_valueRef;
			}

			Registry<RTYPE>& m_system;
			Handle m_handle;
			Archetype* m_archetype;
			T& m_valueRef;
			size_t m_changeCounter;
		};


		//----------------------------------------------------------------------------------------------

		/// @brief Used for iterating over entity components.
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
				assert( m_entidx < m_archetypes[m_archidx]->Size() );
				Handle handle = m_archetypes[m_archidx]->template Map<Handle>()->Get(m_entidx);
				return m_system.Get<Ts...>(handle);
			}

			auto operator!=(const Iterator& other) -> bool {
				return (m_archidx != other.m_archidx) || (m_entidx != other.m_entidx);
			}

		private:

			void Next() {
				while( m_archidx < m_archetypes.size() && m_entidx >= m_archetypes[m_archidx]->Size() ) {
					if( m_isLocked ) UnlockShared();
					++m_archidx;
					m_entidx = 0;
				} 
				if( m_archidx < m_archetypes.size() ) LockShared();
			}

			void LockShared() {
				if constexpr (RTYPE == REGISTRYTYPE_SEQUENTIAL) return;
				if( m_archidx >= m_archetypes.size() ) return;
				auto arch = m_archetypes[m_archidx];
				arch->GetMutex().lock_shared();
				arch->SetSharedMutexPtr(&arch->GetMutex()); //thread remembers that it locked the mutex
				m_isLocked = true;
			}

			void UnlockShared() {
				if constexpr (RTYPE == REGISTRYTYPE_SEQUENTIAL) return;
				if( m_archidx >= m_archetypes.size() ) return;
				if( !m_isLocked ) return;
				auto arch = m_archetypes[m_archidx];
				arch->SetSharedMutexPtr(nullptr);
				arch->GetMutex().unlock_shared();
				m_isLocked = false;
			}

			Registry<RTYPE>& m_system;
			std::vector<Archetype*>& m_archetypes;
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
				for( auto& it : m_system.m_archetypes ) {
					auto arch = it.second.get();
					auto func = [&]<typename T>() {
						if( arch->Types().contains(Type<T>())) return true;
						return false;
					};
					if( (func.template operator()<Ts>() && ...) ) m_archetypes.push_back(it.second.get());
				}
				return Iterator<Ts...>{m_system, m_archetypes, 0};
			}

			auto end() {
				return Iterator<Ts...>{m_system, m_archetypes, m_archetypes.size()};
			}

		private:
			Registry<RTYPE>& m_system;
			std::vector<Archetype*> m_archetypes;
		}; //end of View


		//----------------------------------------------------------------------------------------------

		Registry() = default;

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
			std::vector<size_t> types = {Type<Handle>(), Type<Ts>()...};
			auto [index, slot] = m_entities.Insert( {nullptr, 0} ); //get a slot for the entity
			Handle handle{(uint32_t)index, (uint32_t)slot.m_version}; //create a handle

			size_t archIndex;
			size_t hs = Hash(types);
			if( !m_archetypes.contains( hs ) ) { //not found
				m_archetypes[hs] = std::make_unique<Archetype>( handle, archIndex, std::forward<Ts>(component)... );
			} else {
				archIndex = m_archetypes[hs]->Insert( handle, std::forward<Ts>(component)... );
			}
			slot.m_value = { m_archetypes[hs].get(), archIndex };
			return handle;
		}

		/// @brief Test if an entity exists.
		/// @param handle The handle of the entity.
		/// @return true if the entity exists, else false.
		bool Exists(Handle handle) {
			auto& slot = m_entities[handle.m_index];
			return slot.m_version == handle.m_version;
		}

		/// @brief Test if an entity has a component.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return true if the entity has the component, else false.
		template<typename T>
		bool Has(Handle handle) {
			assert(Exists(handle));
			auto& value = m_entities[handle.m_index].m_value;
			return value.m_archetype_ptr->Has(Type<T>());
		}

		/// @brief Get the types of the components of an entity.
		/// @param handle The handle of the entity.
		/// @return A vector of type indices of the components.
		const auto& Types(Handle handle) {
			assert(Exists(handle));
			auto& value = m_entities[handle.m_index].m_value;
			return value.m_archetype_ptr->Types();
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
			auto [arch, ref] = TryComponent<T>(handle);
			return Ref<T>{*this, handle, arch, ref};
		}

		template<typename U>
			requires (!std::is_same_v<U, Handle&> && !std::is_reference_v<U>)
		[[nodiscard]] auto Get(Handle handle) {
			using T = std::decay_t<U>;
			return Get2<T>(handle);
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
			using T = std::decay_t<U>;
			assert(Exists(handle));
			Get2<T&>(handle) = std::forward<T>(v);
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param v The new values in a tuple
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Put(Handle handle, std::tuple<Ts...>& v) {
			(Put(handle, std::forward<Ts>(std::get<Ts>(v))), ...);
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Put(Handle handle, Ts&&... vs) {
			(Put(handle, vs), ...);
		}
		
		/// @brief Erase components from an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Erase(Handle handle) {
			assert( (Has<Ts>(handle) && ...) );

			auto& value = m_entities[handle.m_index].m_value;
			auto types = value.m_archetype_ptr->Types();
			(types.erase(Type<Ts>()), ... );
			std::vector<size_t> tv(types.begin(), types.end());

			size_t hs = Hash(tv);
			if( !m_archetypes.contains(hs) ) { //not found
				m_archetypes[hs] = std::make_unique<Archetype>();
			}
			auto oldArchetype = value.m_archetype_ptr;
			value.m_archetype_ptr = m_archetypes[hs].get();
			value.m_archIndex = value.m_archetype_ptr->Move(types, value.m_archIndex, *oldArchetype, m_entities);
		}

		/// @brief Erase an entity from the registry.
		/// @param handle The handle of the entity.
		void Erase(Handle handle) {
			assert(Exists(handle));
			auto& value = m_entities[handle.m_index].m_value;
			value.m_archetype_ptr->Erase(value.m_archIndex, m_entities); //erase the entity from the archetype (do both locked)
			m_entities.Erase(handle.m_index); //erase the entity from the entity list
		}

		/// @brief Clear the registry by removing all entities.
		void Clear() {
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

		void print() {
			std::cout << "Entities: " << m_entities.Size() << std::endl;
			for( auto& it : m_archetypes ) {
				it.second->print();
			}
			std::cout << std::endl << std::endl;
		}	


	private:

		template<typename U>
			requires (!std::is_same_v<U, Handle&>)
		[[nodiscard]] auto Get2(Handle handle) -> U {
			using T = std::decay_t<U>;
			auto [arch, ref] = TryComponent<T>(handle);
			return ref;
		}

		template<typename U>
		auto TryComponent(Handle handle) -> std::pair<Archetype*, std::decay_t<U>&> {
			using T = std::decay_t<U>;

			auto& value = m_entities[handle.m_index].m_value;
			if(value.m_archetype_ptr->Has(Type<T>())) {
				return std::pair<Archetype*, T&>{value.m_archetype_ptr, value.m_archetype_ptr->template Get<T>(value.m_archIndex)};
			}

			std::vector<size_t> types( value.m_archetype_ptr->Types().begin(), value.m_archetype_ptr->Types().end() );
			types.push_back(Type<T>());

			size_t hs = Hash(types);
			if(!m_archetypes.contains(hs)) { //not found
				m_archetypes[hs] = std::make_unique<Archetype>();
				m_archetypes[hs]->template AddComponent<T>();
			}
			auto oldArchetype = value.m_archetype_ptr;
			value.m_archetype_ptr = m_archetypes[hs].get();
			types.pop_back();
			value.m_archIndex = value.m_archetype_ptr->Move(types, value.m_archIndex, *oldArchetype, m_entities);
			m_archetypes[hs]->template AddValue(value.m_archIndex, T{});
			return std::pair<Archetype*, T&>{value.m_archetype_ptr, value.m_archetype_ptr->template Get<T>(value.m_archIndex)};
		}

		mutex_t m_mutex; //mutex for thread safety
		SlotMap<ArchetypeAndIndex> m_entities;
		std::unordered_map<size_t, std::unique_ptr<Archetype>> m_archetypes; //Mapping vector of type index to archetype
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



