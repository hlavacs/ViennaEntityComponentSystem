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
		std::size_t operator()(std::vector<std::size_t>* hashes) const {
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

}


namespace vecs
{
	using Handle = std::size_t;

	template <typename> struct is_tuple : std::false_type {};
	template <typename ...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};


	template<typename T>
	auto type() -> std::size_t {
		return std::type_index(typeid(std::decay_t<T>)).hash_code();
	}

	template<typename... Ts>
		requires (vtll::unique<vtll::tl<Ts...>>::value)
	auto typevector() -> std::vector<size_t> {
		auto ret = std::vector<size_t>{type<Ts>()...};
		if constexpr( !vtll::has_type< vtll::tl<Ts...>, Handle>::value ) {
			ret.push_back(type<Handle>());
		}
		return ret;
	}

	struct VecsWrite {}; //dummy type for write access

	template<typename... Ts>
		requires (vtll::unique<vtll::tl<Ts...>>::value)
	class Iterator;

	template<typename... Ts>
		//requires ((vtll::unique<vtll::tl<Ts...>>::value) && (sizeof...(Ts) > 0) && (!std::is_same_v<vtll::tl<VecsWrite>, vtll::tl<Ts...>>))
	class View;

	enum RegistryType {
		SEQUENTIAL,
		PARALLEL
	};	
	
	/// @brief A registry for entities and components.
	template <RegistryType RTYPE = SEQUENTIAL>
	class Registry {

		template<typename... Ts> requires (vtll::unique<vtll::tl<Ts...>>::value) friend class Iterator;

		template<typename... Ts> 
			//requires ((vtll::unique<vtll::tl<Ts...>>::value) && (sizeof...(Ts) > 0) && (!std::is_same_v<vtll::tl<VecsWrite>, vtll::tl<Ts...>>))
		friend class View;

		using mutex_t = std::conditional_t<RTYPE == SEQUENTIAL, int32_t, std::shared_mutex>;

	private:
	
		//----------------------------------------------------------------------------------------------

		/// @brief Base class for component maps.
		class ComponentMapBase {

			friend class Archetype; //for eaccessing the mutex
			template<typename... Ts> requires (vtll::unique<vtll::tl<Ts...>>::value)
			friend class Iterator;

		public:
			ComponentMapBase() = default;
			~ComponentMapBase() = default;
			
			size_t insert(auto&& v) {
				using T = std::decay_t<decltype(v)>;
				return static_cast<ComponentMap<T>*>(this)->insert(std::forward<T>(v));
			}

			virtual void erase(std::size_t index) = 0;
			virtual void move(ComponentMapBase* other, size_t from) = 0;
			virtual auto size() -> size_t= 0;
			virtual auto create() -> std::unique_ptr<ComponentMapBase> = 0;
			virtual void clear() = 0;

			void lock() { m_mutex.lock(); }
			void unlock() { m_mutex.unlock(); }
			void lock_shared() { m_mutex.lock_shared(); }
			void unlock_shared() { m_mutex.unlock_shared(); }

		private:
			mutex_t m_mutex; //mutex for thread safety
		}; //end of ComponentMapBase

		//----------------------------------------------------------------------------------------------

		/// @brief A map of components of the same type.
		/// @tparam T The value type for this comoonent map.
		template<typename T>
			requires std::same_as<T, std::decay_t<T>>
		class ComponentMap : public ComponentMapBase {

		public:

			ComponentMap() = default; //constructor

			/// @brief Insert a new component value.
			/// @param handle The handle of the entity.
			/// @param v The component value.
			[[nodiscard]] size_t insert(auto&& v) {
				m_data.emplace_back( std::forward<T>(v) );
				return m_data.size() - 1;
			};

			/// @brief Get reference to the component of an entity.
			/// @param index The index of the component value in the map.
			/// @return Value of the component value.
			auto get(std::size_t index) -> T& {
				assert(index < m_data.size());
				return m_data[index];
			};

			/// @brief Get the data vector of the components.
			/// @return Reference to the data vector.
			[[nodiscard]] auto& data() {
				return m_data;
			};

			/// @brief Erase a component from the vector.
			/// @param index The index of the component value in the map.
			/// @return The handle of the entity that was moved over the erased one so its index can be changed, or nullopt
			virtual void erase(size_t index) override {
				size_t last = m_data.size() - 1;
				assert(index <= last);
				if( index < last ) {
					m_data[index] = std::move( m_data[last] ); //move the last entity to the erased one
				}
				m_data.pop_back(); //erase the last entity
				return;
			};

			/// @brief Move a component from another map to this map.
			/// @param other The other map.
			/// @param from The index of the component in the other map.
			virtual void move(ComponentMapBase* other, size_t from) override {
				m_data.push_back( static_cast<ComponentMap<std::decay_t<T>>*>(other)->get(from) );
			};

			/// @brief Get the size of the map.
			/// @return The size of the map.
			[[nodiscard]] virtual auto size() -> size_t override {
				return m_data.size();
			}

			/// @brief Create a new map.
			/// @return A unique pointer to the new map.
			[[nodiscard]] virtual auto create() -> std::unique_ptr<ComponentMapBase> override {
				return std::make_unique<ComponentMap<T>>();
			};

			void clear() override {
				m_data.clear();
			}

		private:
			std::vector<T> m_data; //vector of component values
		}; //end of ComponentMap

		//----------------------------------------------------------------------------------------------

		struct ActionCreate{};
		struct ActionInsert{};
		struct ActionRemove{};

		/// @brief An archetype of entities with the same components.
		class Archetype {

			template<typename... Ts>
				//requires ((vtll::unique<vtll::tl<Ts...>>::value) && (sizeof...(Ts) > 0) && (!std::is_same_v<vtll::tl<VecsWrite>, vtll::tl<Ts...>>))
			friend class View;
			
			template<typename... Ts> requires (vtll::unique<vtll::tl<Ts...>>::value) friend class Iterator;

		public:

			/// @brief Constructor, called if a new entity should be created with components, and the archetype does not exist yet.
			/// @tparam ...Ts 
			/// @param handle The handle of the entity.
			/// @param ...values The values of the components.
			template<typename... Ts>
				requires (vtll::unique<vtll::tl<Ts...>>::value)
			Archetype( ActionCreate c, Handle handle, Ts&& ...values ) {
				add( handle ); //insert the handle
				(add( std::forward<Ts>(values) ), ...); //insert all components
				std::sort(m_types.begin(), m_types.end()); //sort the types
				m_index[handle] = size() - 1; //store the index of the new entity in the archetype
				assert(validate());
			}

			/// @brief Constructor, called if components of an entity are added, but the new archetype does not exist yet.
			/// @tparam T Possible new component type.
			/// @param other The original archetype.
			/// @param index The index of the entity in the old archetype.
			/// @param value Pointer to a new component value if component is added, or nullptr if the component is removed.
			template<typename... Ts>
			Archetype( ActionInsert i, Archetype& other, Handle handle, Ts&&... value) {
				m_types = other.m_types; //make a copy of the types
				(add( value ), ...); //add the new components
				std::sort(m_types.begin(), m_types.end()); //sort the types
				move(m_types, other.m_index[handle], other); //move rest of components 
				m_index[handle] = size() - 1; //store the index of the new entity in the archetype
				assert(validate());
			}

			/// @brief Constructor, called if components of an entity are removed, but the new archetype does not exist yet.
			/// @tparam Ts Old types of the components that should be moved from the old archteype to the new one.
			/// @param other The original archetype.
			/// @param handle The handle of the entity.
			/// @param value Pointer to the old component values.
			template<typename... Ts>
			Archetype(ActionRemove r, Archetype& other, Handle handle, Ts*... value) {
				m_types = other.m_types; //make a copy of the types
				auto func = [&](auto&& v) {
					auto it = std::find(m_types.begin(), m_types.end(), type<std::decay_t<decltype(v)>>());
					if( it != std::end(m_types) ) {
						m_types.erase(it); //remove the type from the list
					}
				};
				(func( *value ), ...); //insert all components
				std::sort(m_types.begin(), m_types.end()); //sort the types
				move(m_types, other.m_index[handle], other); //copy rest of components and store the index of the new entity 
				m_index[handle] = size() - 1; //store the index of the new entity in the archetype
				assert(validate());
			}

			/// @brief Get the types of the components.
			/// @return A vector of type indices.
			[[nodiscard]] const auto&  types() const {
				return m_types;
			}

			/// @brief Test if the archetype has a component.
			/// @param ti The type index of the component.
			/// @return true if the archetype has the component, else false.
			[[nodiscard]] bool has(const size_t ti) {
				if constexpr (RTYPE == PARALLEL) std::shared_lock lock(m_mutex);
				return (std::find(m_types.begin(), m_types.end(), ti) != std::end(m_types));
			}

			/// @brief Create an entity with components.
			/// @tparam ...Ts The types of the components, must be unique.
			/// @param handle The handle of the entity.
			/// @param ...component The new values.
			/// @return The index of the entity in the archetype.
			template<typename... Ts>
				requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value))
			[[nodiscard]] size_t insert( Handle handle, Ts&&... value ) {
				if constexpr (RTYPE == PARALLEL) m_mutex.lock();

				size_t index{0};
				auto func = [&](auto&& v) {
					using T = std::decay_t<decltype(v)>;
					auto* map = static_cast<ComponentMap<std::decay_t<T>>*>(m_maps[type<T>()].get());
					index = map->insert(std::forward<T>(v));
				};
				func(handle); //insert the handle
				(func( std::forward<Ts>(value) ), ...); //insert all components
				m_index[handle] = size() - 1; //store the index of the new entity in the archetype
				assert(validate()); //all component maps should have the same size

				if constexpr (RTYPE == PARALLEL) m_mutex.unlock();
				return index;
			}

			/// @brief Get component value of an entity. 
			/// @tparam T The type of the component.
			/// @param index The index of the entity in the archetype.
			/// @return The component value.
			template<typename T>
			[[nodiscard]] auto get(Handle handle) -> T& {
				assert(std::find(m_types.begin(), m_types.end(), type<T>()) != std::end(m_types) && m_maps.find(type<T>()) != m_maps.end() ); 
				size_t index;
				{
					if constexpr (RTYPE == PARALLEL) std::shared_lock lock(m_mutex);
					index = m_index[handle];
					assert( index < m_maps[type<T>()]->size());
				}
				if constexpr (RTYPE == PARALLEL) m_maps[type<T>()].get()->lock_shared();
				auto& ret = static_cast<ComponentMap<std::decay_t<T>>*>(m_maps[type<T>()].get())->get(index);
				if constexpr (RTYPE == PARALLEL) m_maps[type<T>()].get()->lock_shared();
				return ret;
			}

			/// @brief Get component values of an entity.
			/// @tparam ...Ts 
			/// @param handle Handle oif the entity.
			/// @return A tuple of the component values.
			template<typename... Ts>
				requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value))
			[[nodiscard]] auto get(Handle handle) -> std::tuple<Ts&...> {
				size_t index;
				{
					if constexpr (RTYPE == PARALLEL) std::shared_lock lock(m_mutex);
					index = m_index[handle];
					(assert( index < m_maps[type<Ts>()]->size() ),...);
				}
				if constexpr (RTYPE == PARALLEL) (m_maps[type<Ts>()].get()->m_mutex.shared_lock(), ...);
				auto ret = std::tuple<Ts&...>{ static_cast<ComponentMap<std::decay_t<Ts>>*>(m_maps[type<Ts>()].get())->get(index)... };
				if constexpr (RTYPE == PARALLEL) (m_maps[type<Ts>()].get()->m_mutex.unlock(), ...);
				return ret;
			}

			/// @brief Erase all components of an entity.
			/// @param index The index of the entity in the archetype.
			/// @return The handle of the entity that was moved over the erased one so its index can be changed.
			void erase(Handle handle) {
				size_t index = m_index[handle];
				size_t last = size() - 1;
				for( auto& it : m_maps ) {
					it.second->erase(index); //should always be the same handle
				}
				if( index < last ) { //an entity was moved to fill the empty slot in the vector
					ComponentMap<Handle>* map = static_cast<ComponentMap<Handle>*>(m_maps[type<Handle>()].get());
					m_index[map->get(index)] = index; //update the index of the moved entity
				}
				assert(validate());
			}

			/// @brief Get the map of the components.
			/// @tparam T The type of the component.
			/// @return Pointer to the component map.
			template<typename T>
			auto map() -> ComponentMap<T>* {
				auto it = m_maps.find(type<T>());
				assert(it != m_maps.end());
				return static_cast<ComponentMap<T>*>(it->second.get());
			}

			/// @brief Get the data of the components.
			/// @param ti Type index of the component.
			/// @return Pointer to the component map base class.
			auto map(size_t ti) -> ComponentMapBase* {
				auto it = m_maps.find(ti);
				assert(it != m_maps.end());
				return it->second.get();
			}

			/// @brief Get the number of entites in this archetype.
			/// @return The number of entities.
			size_t size() {
				return m_maps.begin()->second->size();
			}

			void clear() {
				for( auto& map : m_maps ) {
					map.second->clear();
				}
				m_index.clear();
			}

			/// @brief Test if all component maps have the same size.
			/// @return  true if all component maps have the same size, else false.
			bool validate() {
				bool first{true};
				size_t sz{0};
				for( auto& it : m_maps ) {
					size_t s = it.second->size();
					if( first || s == sz) {
						sz = s;
						first = false;
					} else return false;
				}
				return true;
			}

			const std::vector<size_t>& types() {
				return m_types;
			}

			const std::unordered_map<size_t, std::unique_ptr<ComponentMapBase>>& maps() {
				return m_maps;
			}

			void lock() { m_mutex.lock(); }
			void unlock() { m_mutex.unlock(); }
			void lock_shared() { m_mutex.lock_shared(); }
			void unlock_shared() { m_mutex.unlock_shared(); }

		private:

			/// @brief Add a new component to the archetype.
			/// @param v The new value.
			/// @return The index of the entity in the archetype.
			void add( auto&& v ) {
				using T = std::decay_t<decltype(v)>;
				size_t ti = type<T>();
				m_types.push_back(ti);	//add the type to the list
				m_maps[ti] = std::make_unique<ComponentMap<T>>(); //create the component map
				m_maps[ti]->insert(std::forward<T>(v));	//insert the component value
			};

			/// @brief Test if all component maps have the same size.
			/// @return  Index of the entity in the archetype.
			void move( auto& types, size_t other_index, auto& other) {
				for( auto& it : types ) { //go through all maps
					m_maps[it] = other.map(it)->create(); //make a component map like this one
					m_maps[it]->move(other.map(it), other_index); //insert the new value
				}
				other.erase(other_index); //erase the old entity
			}

			mutex_t m_mutex; //mutex for thread safety

			std::vector<size_t> m_types; //types of components
			std::unordered_map<Handle, size_t> m_index; //map from handle ot index of entity in data array
			std::unordered_map<size_t, std::unique_ptr<ComponentMapBase>> m_maps; //map from type index to component data
		}; //end of Archetype


	public:

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
				if constexpr (RTYPE == PARALLEL) 
					(m_archetypes[m_archidx]->map(type<Ts>())->lock(), ...);
			}

			void unlock() {
				if constexpr (RTYPE == PARALLEL) 
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

		//----------------------------------------------------------------------------------------------

		Registry() = default;

		/// @brief Get the number of entities in the system.
		/// @return The number of entities.
		size_t size() {
			if constexpr (RTYPE == PARALLEL) m_mutex.lock_shared();
			auto ret =  m_entities.size();
			if constexpr (RTYPE == PARALLEL) m_mutex.unlock_shared();
			return ret;
		}

		/// @brief Test if a handle is valid, i.e., not 0.
		/// @param handle 
		/// @return true if the handle is valid, else false.
		bool valid(Handle handle) {
			return handle != 0;
		}

		/// @brief Create an entity with components.
		/// @tparam ...Ts The types of the components.
		/// @param ...component The new values.
		/// @return Handle of new entity.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		[[nodiscard]] auto create( Ts&&... component ) -> Handle {
			std::vector<std::size_t> types = {type<Ts>()...};

			if constexpr (RTYPE == PARALLEL) m_mutex.lock();
			Handle handle = ++m_next_handle;		//protect shared data
			auto it = m_archetypes.find(&types); //find the archetype, protect shared data
			if( it == m_archetypes.end() ) { //not found
				m_archetypes[&types] = std::make_unique<Archetype>( ActionCreate{}, handle, std::forward<Ts>(component)... );
				m_entities[handle] = { m_archetypes[&types].get(), 0 }; //store the new archetype
				if constexpr (RTYPE == PARALLEL) m_mutex.unlock();
				return handle;
			}
			m_entities[handle] = { it->second.get() };

			size_t index = it->second->insert(handle, std::forward<Ts>(component)...);
			if constexpr (RTYPE == PARALLEL) m_mutex.unlock();
			return handle;
		}

		/// @brief Test if an entity exists.
		/// @param handle The handle of the entity.
		/// @return true if the entity exists, else false.
		bool exists(Handle handle) {
			assert(valid(handle));
			if constexpr (RTYPE == PARALLEL) m_mutex.lock_shared();
			auto ret = m_entities.find(handle) != m_entities.end();
			if constexpr (RTYPE == PARALLEL) m_mutex.unlock_shared();
			return ret;
		}

		/// @brief Test if an entity has a component.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return true if the entity has the component, else false.
		template<typename T>
		bool has(Handle handle) {
			assert(valid(handle));
			if constexpr (RTYPE == PARALLEL) m_mutex.lock_shared();
			auto ret = m_entities.find(handle) != m_entities.end();
			auto arch = m_entities[handle].m_archetype_ptr;
			if constexpr (RTYPE == PARALLEL) m_mutex.unlock_shared();
			return ret && arch->has(type<T>());
		}

		/// @brief Get the types of the components of an entity.
		/// @param handle The handle of the entity.
		/// @return A vector of type indices of the components.
		const auto& types(Handle handle) {
			assert(exists(handle));
			if constexpr (RTYPE == PARALLEL) m_mutex.lock_shared();
			auto arch = m_entities[handle].m_archetype_ptr;
			if constexpr (RTYPE == PARALLEL) m_mutex.unlock_shared();
			return arch->types();
		}

		/// @brief Get component value of an entity. If the component does not exist, it will be created.
		/// This might result in moving the entity to another archetype.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.	
		/// @return The component value.
		template<typename T>
		[[nodiscard]] auto get(Handle handle) -> T {
			assert(has<T>(handle));
			if constexpr (RTYPE == PARALLEL) m_mutex.lock_shared();
			auto arch = m_entities[handle].m_archetype_ptr;
			if constexpr (RTYPE == PARALLEL) m_mutex.unlock_shared();
			return arch->get<T>(handle);
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value))
		[[nodiscard]] auto get(Handle handle) -> std::tuple<Ts...> {
			if constexpr (RTYPE == PARALLEL) m_mutex.lock_shared();
			auto arch = m_entities[handle].m_archetype_ptr;
			if constexpr (RTYPE == PARALLEL) m_mutex.unlock_shared();
			return std::tuple<Ts...>(arch->get<Ts>(handle)...); //defer locking to archetype!
		}

		/// @brief Put a new component value to an entity. If the entity does not have the component, it will be created.
		/// This might result in moving the entity to another archetype.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @param v The new value.
		template<typename T>
			requires (!is_tuple<T>::value)
		void put(Handle handle, T&& v) {
			assert(exists(handle));
			if constexpr (RTYPE == PARALLEL) m_mutex.lock();
			if(!has<T>(handle)) {
				auto typ = m_entities[handle].m_archetype_ptr->types();
				typ.push_back(type<T>());
				auto it = m_archetypes.find(&typ);
				if( it == m_archetypes.end() ) {
					m_archetypes[&typ] = std::make_unique<Archetype>( ActionInsert{}, *m_entities[handle].m_archetype_ptr, handle, std::forward<T>(v) );
					m_entities[handle] = { m_archetypes[&typ].get(), 0 };
					if constexpr (RTYPE == PARALLEL) m_mutex.unlock();
					return;
				}	
			}
			auto arch = m_entities[handle].m_archetype_ptr; //implement put !!!
			arch->get<T>(handle) = v; //get the component value and assign the new value
			if constexpr (RTYPE == PARALLEL) m_mutex.unlock();
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param v The new values in a tuple
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void put(Handle handle, std::tuple<Ts...>& v) {
			(put(handle, std::get<Ts>(v)), ...);
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void put(Handle handle, Ts&&... vs) {
			(put(handle, vs), ...);
		}

		/// @brief Erase components from an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void erase(Handle handle) {
			assert( (has<Ts>(handle) && ...) );
			if constexpr (RTYPE == PARALLEL) m_mutex.lock();

			auto entt = m_entities.find(handle)->second; //get the archetype of the entity
			auto typ = entt.m_archetype_ptr->types(); //get the types from the archetype

			auto func = [&]<typename T>() { //remove the types from the list
				auto it = std::find(typ.begin(), typ.end(), type<T>());
				typ.erase(it);
			};
			(func.template operator()<Ts>(), ...);

			auto it = m_archetypes.find(&typ); //find the new archetype if possible
			if( it == m_archetypes.end() ) { //if the archetype does not exist, create it
				auto map = std::make_unique<Archetype>( ActionRemove{}, *entt.m_archetype_ptr, handle, (Ts*)nullptr... );
				m_entities[handle] = { map.get() };
				m_archetypes[&typ] = std::move(map);
				if constexpr (RTYPE == PARALLEL) m_mutex.unlock();
				return;
			}
			m_entities[handle] = { it->second.get() };
			if constexpr (RTYPE == PARALLEL) m_mutex.unlock();
		}

		/// @brief Erase an entity from the registry.
		/// @param handle The handle of the entity.
		void erase(Handle handle) {
			assert(exists(handle));
			if constexpr (RTYPE == PARALLEL) m_mutex.lock();
			m_entities[handle].m_archetype_ptr->erase(handle); //erase the entity from the archetype (do both locked)
			m_entities.erase(handle); //erase the entity from the entity list
			if constexpr (RTYPE == PARALLEL) m_mutex.unlock();
		}

		/// @brief Clear the registry by removing all entities.
		void clear() {
			if constexpr (RTYPE == PARALLEL) m_mutex.lock();

			for( auto& arch : m_archetypes ) {
				arch.second->clear();
			}
			m_entities.clear();
			if constexpr (RTYPE == PARALLEL) m_mutex.unlock();
		}

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
			if constexpr (RTYPE == PARALLEL) std::shared_lock lock(m_mutex);

			size_t sz{0};
			for( auto& arch : m_archetypes ) {
				sz += arch.second->size();
				assert(arch.second->validate());
			}
			assert(sz == size());
		}

	private:
		mutex_t m_mutex; //mutex for thread safety

		struct ArchetypeAndIndex {
			Archetype* m_archetype_ptr;
			size_t m_index;
		};

		std::size_t m_next_handle{0};	//next handle to be assigned	
		std::unordered_map<Handle, ArchetypeAndIndex> m_entities; //Archetype and index in archetype
		std::unordered_map<std::vector<size_t>*, std::unique_ptr<Archetype>> m_archetypes; //Mapping vector of type index to archetype
	};

	
}


