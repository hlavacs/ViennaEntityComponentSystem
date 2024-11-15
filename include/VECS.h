#pragma once

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <variant>
#include <array>
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

namespace std {
	template<>
	struct hash<std::vector<size_t>*> {
		std::size_t operator()(std::vector<size_t>* hashes) const {
			std::size_t seed = 0;
			std::sort(hashes->begin(), hashes->end());
			for( auto& v : *hashes ) {
				seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
			}
			return seed;
		}
	};

	template<>
	struct hash<std::vector<size_t>&> {
		std::size_t operator()(std::vector<size_t>& types) const {
			return std::hash<std::vector<size_t>*>{}(&types);
		}
	};

	template<>
	struct hash<std::set<size_t>*> {
		std::size_t operator()(std::set<std::size_t>* hashes) const {
			std::size_t seed = 0;
			for( auto& v : *hashes ) {
				seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
			}
			return seed;
		}
	};

	template<>
	struct hash<std::set<size_t>&> {
		std::size_t operator()(std::set<size_t>& types) const {
			return std::hash<std::set<size_t>*>{}(&types);
		}
	};

}


namespace vecs {



	template <typename> struct is_tuple : std::false_type {};
	template <typename ...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};


	//----------------------------------------------------------------------------------------------
	//Slot Maps

	enum class SlotMapType : int {
		SEQUENTIAL = 0,
		PARALLEL
	};	

	template<typename T, uint32_t SIZE = 1014, SlotMapType = SlotMapType::SEQUENTIAL>
	class SlotMap {

		struct Slot {
			int64_t m_nextFree{-1}; //index of the next free slot
			size_t 	m_version{0};	//version of the slot
			T 		m_value{};		//value of the slot
		};

	public:
		SlotMap() {
			if( SIZE == 0 ) return;
			m_slots.reserve(SIZE);
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
			assert(index < m_slots.size());
			auto& slot = m_slots[index];
			++slot.m_version;	//increment the version to invalidate the slot
			slot.m_nextFree = m_firstFree;
			m_firstFree = index;
			--m_size;
		}

		auto operator[](size_t index) -> Slot& {
			assert(index < m_slots.size());
			return m_slots[index];
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
		size_t m_size{0};
		std::vector<Slot> m_slots;
		int64_t m_firstFree{-1};
	};



	//----------------------------------------------------------------------------------------------

	enum class RegistryType : int {
		SEQUENTIAL = 0,
		PARALLEL
	};	

	struct Handle {
		uint32_t m_index;
		uint32_t m_version;
	};


	template<typename T>
	auto Type() -> std::size_t {
		return std::type_index(typeid(std::decay_t<T>)).hash_code();
	}

	struct VecsWrite {}; //dummy type for write access when using the iterator and view classes

	template<typename... Ts>
		requires (vtll::unique<vtll::tl<Ts...>>::value)
	class Iterator;

	template<typename... Ts>
		//requires ((vtll::unique<vtll::tl<Ts...>>::value) && (sizeof...(Ts) > 0) && (!std::is_same_v<vtll::tl<VecsWrite>, vtll::tl<Ts...>>))
	class View;

	
	/// @brief A registry for entities and components.
	template <RegistryType RTYPE = RegistryType::SEQUENTIAL>
	class Registry {

		template<typename... Ts> requires (vtll::unique<vtll::tl<Ts...>>::value) friend class Iterator;

		template<typename... Ts> 
			//requires ((vtll::unique<vtll::tl<Ts...>>::value) && (sizeof...(Ts) > 0) && (!std::is_same_v<vtll::tl<VecsWrite>, vtll::tl<Ts...>>))
		friend class View;

		using mutex_t = std::conditional_t<RTYPE == RegistryType::SEQUENTIAL, int32_t, std::shared_mutex>;

	private:
	
		//----------------------------------------------------------------------------------------------

		/// @brief Base class for component maps.
		class ComponentMapBase {

			friend class Archetype; //for eaccessing the mutex
			template<typename... Ts> requires (vtll::unique<vtll::tl<Ts...>>::value)
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

			void Lock() { if constexpr (RTYPE == RegistryType::PARALLEL) m_mutex.lock(); }
			void Unlock() { if constexpr (RTYPE == RegistryType::PARALLEL) m_mutex.unlock(); }
			void LockShared() { if constexpr (RTYPE == RegistryType::PARALLEL) m_mutex.lock_shared(); }
			void UnlockShared() { if constexpr (RTYPE == RegistryType::PARALLEL) m_mutex.unlock_shared(); }

		private:
			mutex_t m_mutex; //mutex for thread safety
		}; //end of ComponentMapBase

		//----------------------------------------------------------------------------------------------

		/// @brief A map of components of the same type.
		/// @tparam T The value type for this comoonent map.
		template<typename U>
		class ComponentMap : public ComponentMapBase {

			using T = std::decay_t<U>;

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
			/// @return Value of the component value.
			auto Get(std::size_t index) -> T& {
				assert(index < m_data.size());
				return m_data[index];
			};

			/// @brief Get the data vector of the components.
			/// @return Reference to the data vector.
			[[nodiscard]] auto& Data() {
				return m_data;
			};

			/// @brief Erase a component from the vector.
			/// @param index The index of the component value in the map.
			/// @return The handle of the entity that was moved over the erased one so its index can be changed, or nullopt
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

			/// @brief Create a new map.
			/// @return A unique pointer to the new map.
			[[nodiscard]] virtual auto Clone() -> std::unique_ptr<ComponentMapBase> override {
				return std::make_unique<ComponentMap<T>>();
			};

			void Clear() override {
				m_data.clear();
			}

		private:
			std::vector<T> m_data; //vector of component values
		}; //end of ComponentMap

		//----------------------------------------------------------------------------------------------

		class Archetype;

		struct ArchetypeAndIndex {
			Archetype* m_archetype_ptr;	//pointer to the archetype
			size_t m_archIndex;			//index of the entity in the archetype
		};

		/// @brief An archetype of entities with the same components.
		class Archetype {

			template<typename... Ts>
				//requires ((vtll::unique<vtll::tl<Ts...>>::value) && (sizeof...(Ts) > 0) && (!std::is_same_v<vtll::tl<VecsWrite>, vtll::tl<Ts...>>))
			friend class View;
			
			template<typename... Ts> requires (vtll::unique<vtll::tl<Ts...>>::value) friend class Iterator;

		public:

			Archetype() = default;

			/// @brief Constructor, called if a new entity should be created with components, and the archetype does not exist yet.
			/// @tparam ...Ts 
			/// @param handle The handle of the entity.
			/// @param ...values The values of the components.
			template<typename... Ts>
				requires (vtll::unique<vtll::tl<Ts...>>::value)
			Archetype(Handle handle, size_t& archIndex, Ts&& ...values ) {
				(AddComponent<Ts>(), ...); //insert component types
				(AddValue( archIndex, std::forward<Ts>(values) ), ...); //insert all components, get index of the handle
				AddComponent<Handle>(); //insert the handle
				AddValue( archIndex, handle ); //insert the handle
			}

			template<typename... Ts>
				requires (vtll::unique<vtll::tl<Ts...>>::value)
			size_t Insert(Handle handle, Ts&& ...values ) {
				assert( m_types.size() == sizeof...(Ts) + 1 );
				assert( (m_maps.contains(Type<Ts>()) && ...) );
				size_t index;
				(AddValue( index, std::forward<Ts>(values) ), ...); //insert all components, get index of the handle
				AddValue( index, handle ); //insert the handle
				return index;
			}

			/// @brief Get the types of the components.
			/// @return A vector of type indices.
			[[nodiscard]] const auto&  Types() const {
				return m_types;
			}

			/// @brief Test if the archetype has a component.
			/// @param ti The type index of the component.
			/// @return true if the archetype has the component, else false.
			[[nodiscard]] bool Has(const size_t ti) {
				return m_types.contains(ti);
			}

			/// @brief Get component value of an entity. 
			/// @tparam T The type of the component.
			/// @param index The index of the entity in the archetype.
			/// @return The component value.
			template<typename T>
			[[nodiscard]] auto Get(size_t archIndex) -> T& {			
				return static_cast<ComponentMap<std::decay_t<T>>*>(m_maps[Type<T>()].get())->Get(archIndex);
			}

			/// @brief Get component values of an entity.
			/// @tparam ...Ts 
			/// @param handle Handle oif the entity.
			/// @return A tuple of the component values.
			template<typename... Ts>
				requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value))
			[[nodiscard]] auto Get(size_t archIndex) -> std::tuple<Ts&...> {
				return std::tuple<Ts&...>{ static_cast<ComponentMap<std::decay_t<Ts>>*>(m_maps[Type<Ts>()].get())->Get(archIndex)... };
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
			}

			/// @brief Move components from another archetype to this one.
			size_t Move( auto& types, size_t other_index, Archetype& other, SlotMap<ArchetypeAndIndex>& slotmap) {
				for( auto& ti : types ) { //go through all maps
					if( m_types.end() == m_types.find(ti) ) {
						m_types.insert(ti); //add the type to the list
						m_maps[ti] = other.Map(ti)->Clone(); //make a component map like this one
					}
					m_maps[ti]->Move(other.Map(ti), other_index); //insert the new value
				}
				other.Erase(other_index, slotmap); //erase from old component map
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

			void Clear() {
				for( auto& map : m_maps ) {
					map.second->Clear();
				}
			}

			const std::set<size_t>& Types() {
				return m_types;
			}

			const std::unordered_map<size_t, std::unique_ptr<ComponentMapBase>>& Maps() {
				return m_maps;
			}

			void Lock() { m_mutex.lock(); }
			void Unlock() { m_mutex.unlock(); }
			void LockShared() { m_mutex.lock_shared(); }
			void UnlockShared() { m_mutex.unlock_shared(); }


			/// @brief Add a new component to the archetype.
			/// @tparam T The type of the component.
			template<typename T>
			void AddComponent() {
				size_t ti = Type<T>();
				m_types.insert(ti);	//add the type to the list
				m_maps[ti] = std::make_unique<ComponentMap<T>>(); //create the component map
			};

		private:

			/// @brief Add a new value to the archetype.
			/// @param v The new value.
			/// @return The index of the entity in the archetype.
			void AddValue( size_t& index, auto&& v ) {
				using T = std::decay_t<decltype(v)>;
				index = m_maps[Type<T>()]->Insert(std::forward<T>(v));	//insert the component value
			};

			mutex_t m_mutex; //mutex for thread safety

			std::set<size_t> m_types; //types of components
			std::unordered_map<size_t, std::unique_ptr<ComponentMapBase>> m_maps; //map from type index to component data
		}; //end of Archetype


	public:

		/*
		//----------------------------------------------------------------------------------------------

		/// @brief Used for iterating over entity components.
		/// @tparam ...Ts Choose the types of the components you want the entities to have.
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		class Iterator {

			using typelist = vtll::tl<Ts...>;
			static const size_t size = sizeof...(Ts);
			static const size_t max = std::numeric_limits<size_t>::max();
			static const size_t writeidx = vtll::index_of<typelist, VecsWrite>::value;

			static consteval size_t writeidxRead( size_t writeidx ) {
				if( writeidx == max ) return 0;
				if( writeidx == 0 ) return 0;
				return writeidx - 1;
			}

			static consteval size_t writeidxWrite( size_t writeidx, size_t size ) {
				if( writeidx == max ) return 0;
				if( writeidx == size - 1) return 0;
				return writeidx + 1;
			}			

			using read_t = 	std::conditional_t<writeidx == max, typelist, 
								std::conditional_t< writeidx == 0, vtll::tl<>, vtll::sublist< typelist, 0, writeidxRead( writeidx ) > >
							>;

			using write_t = std::conditional_t<writeidx == max || writeidx == size - 1, vtll::tl<>, 
								vtll::sublist<typelist, writeidxWrite( writeidx, size ), size - 1>
							>;

		public:
			/// @brief Iterator constructor saving a list of archetypes and the current index.
			/// @param arch List of archetypes. 
			/// @param archidx First archetype index.
			Iterator(std::vector<Archetype*>& arch, size_t archidx) : m_archetypes{arch}, m_archidx{archidx} {
				if( m_archidx < m_archetypes.size() ) lock();
			}

			~Iterator() {
				if( m_archidx < m_archetypes.size() ) unlock();
			}

			/// @brief Prefix increment operator.
			auto operator++() {
				if( ++m_entidx >= m_archetypes[m_archidx]->maps().begin()->second->size() ) {
					unlock();
					do {
						++m_archidx;
						m_entidx = 0;
					} while( m_archidx < m_archetypes.size() && m_archetypes[m_archidx]->maps().begin()->second->size() == 0 );
				}
				if( m_archidx < m_archetypes.size() ) lock();
				return *this;
			}

			/// @brief Access the content the iterator points to.
			auto operator*() {
				auto tup = std::make_tuple(static_cast<ComponentMap<Ts>*>(m_archetypes[m_archidx]->map(type<Ts>()))->get(m_entidx)...);
				if constexpr (sizeof...(Ts) == 1) {
					return std::get<0>(tup);
				} else {
					return tup;
				}
			}

			auto operator!=(const Iterator& other) {
				return m_archidx != other.m_archidx || m_entidx != other.m_entidx;
			}

		private:
			void lock() {
				if constexpr (RTYPE == RegistryType::PARALLEL) 
					(m_archetypes[m_archidx]->map(type<Ts>())->lock(), ...);
			}

			void unlock() {
				if constexpr (RTYPE == RegistryType::PARALLEL) 
					(m_archetypes[m_archidx]->map(type<Ts>())->unlock(), ...);
			}

			size_t m_archidx{0};
			size_t m_entidx{0};
			std::vector<Archetype*>& m_archetypes;
		}; //end of Iterator

		//----------------------------------------------------------------------------------------------

		/// @brief A view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		template<typename... Ts>
			//requires ((vtll::unique<vtll::tl<Ts...>>::value) && (sizeof...(Ts) > 0) && (!std::is_same_v<vtll::tl<VecsWrite>, vtll::tl<Ts...>>) )
		class View {

		public:
			View(Registry<RTYPE>& system) : m_system{system} {}

			auto begin() {
				m_archetypes.clear();
				for( auto& it : m_system.m_archetypes ) {
					auto arch = it.second.get();
					auto func = [&]<typename T>() {
						if( std::find(arch->types().begin(), arch->types().end(), type<T>()) == arch->types().end() ) return false;
						return true;
					};
					if( (func.template operator()<Ts>() && ...) ) m_archetypes.push_back(it.second.get());
				}
				return Iterator<Ts...>{m_archetypes, 0};
			}

			auto end() {
				return Iterator<Ts...>{m_archetypes, m_archetypes.size()};
			}

		private:
			Registry<RTYPE>& m_system;
			std::vector<Archetype*> m_archetypes;
		}; //end of View
		*/

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
			if( !m_archetypes.contains(&types) ) { 
				m_archetypes[&types] = std::make_unique<Archetype>( handle, archIndex, std::forward<Ts>(component)... );
			} else {
				archIndex = m_archetypes[&types]->Insert( handle, std::forward<Ts>(component)... );
			}
			slot.m_value = { m_archetypes[&types].get(), archIndex };
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
		template<typename T>
		[[nodiscard]] auto Get(Handle handle) -> T {
			auto& value = m_entities[handle.m_index].m_value;
			if(value.m_archetype_ptr->Has(Type<T>())) {
				return value.m_archetype_ptr->template Get<T>(value.m_archIndex);
			}
			std::vector<size_t> types( value.m_archetype_ptr->Types().begin(), value.m_archetype_ptr->Types().end() );
			types.push_back(Type<T>());

			if(!m_archetypes.contains(&types)) { //not found
				m_archetypes[&types] = std::make_unique<Archetype>();
				m_archetypes[&types]->AddComponent<T>();
			}
			auto oldArchetype = value.m_archetype_ptr;
			value.m_archetype_ptr = m_archetypes[&types].get();
			types.pop_back();
			value.m_archIndex = value.m_archetype_ptr->Move(types, value.m_archIndex, *oldArchetype, m_entities);
			return value.m_archetype_ptr->template Get<T>(value.m_archIndex);
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value))
		[[nodiscard]] auto Get(Handle handle) -> std::tuple<Ts...> {
			auto& value = m_entities[handle.m_index].m_value;
			return std::tuple<Ts...>(value.m_archetype_ptr->template Get<Ts>(value.m_archIndex)...); //defer locking to archetype!
		}

		/// @brief Put a new component value to an entity. If the entity does not have the component, it will be created.
		/// This might result in moving the entity to another archetype.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @param v The new value.
		template<typename T>
			requires (!is_tuple<T>::value)
		void Put(Handle handle, T&& v) {
			assert(Exists(handle));
			auto& value = m_entities[handle.m_index].m_value;
			value.m_archetype_ptr->template Get<T>(value.m_archIndex) = std::forward<T>(v);
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param v The new values in a tuple
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Put(Handle handle, std::tuple<Ts...>& v) {
			(Put(handle, std::get<Ts>(v)), ...);
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

			if( !m_archetypes.contains(&tv) ) { //not found
				m_archetypes[&tv] = std::make_unique<Archetype>();
			}
			auto oldArchetype = value.m_archetype_ptr;
			value.m_archetype_ptr = m_archetypes[&tv].get();
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

		/*
		/// @brief Get a view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		/// @return A view of the entity components
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto view() -> View<Ts...>{
			return {*this};
		}

		/// @brief Validate the registry by checking if all component maps have the same size.
		void validate() {
			size_t sz{0};
			for( auto& arch : m_archetypes ) {
				sz += arch.second->size();
				assert(arch.second->validate());
			}
			assert(sz == size());
		}
		*/

	private:
		mutex_t m_mutex; //mutex for thread safety

		SlotMap<ArchetypeAndIndex> m_entities;
		std::unordered_map<std::vector<size_t>*, std::unique_ptr<Archetype>> m_archetypes; //Mapping vector of type index to archetype
	};

	
} //end of namespace vecs


