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
#include <VTLL.h>


using namespace std::chrono_literals;

namespace std {
	template<>
	struct hash<std::set<std::type_index>> {
		std::size_t operator()(const std::set<std::type_index>& v) const {
			std::size_t seed = 0;
			for( auto& it : v ) {
				seed ^= std::hash<std::type_index>{}(it) + 0x9e3779b9 + (seed<<6) + (seed>>2);
			}
			return seed;
		}
	};

	template<>
	struct hash<std::vector<std::size_t>> {
		std::size_t operator()(std::vector<std::size_t>& hashes) const {
			std::size_t seed = 0;
			std::sort(hashes.begin(), hashes.end());
			for( auto& v : hashes ) {
				seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
			}
			return seed;
		}
	};

}


namespace vecs
{
	using Handle = std::size_t;

	template <typename> struct is_tuple : std::false_type {};
	template <typename ...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};


	template<typename T>
	auto type() -> std::type_index {
		return std::type_index(typeid(std::decay_t<T>));
	}

	class Registry {
	private:
	
		//----------------------------------------------------------------------------------------------

		class ComponentMapBase {
		public:
			~ComponentMapBase() = default;
			virtual std::size_t insert(Handle handle, std::any v) = 0;
			virtual std::any get(std::size_t index) = 0;
			virtual Handle erase(std::size_t index) = 0;
			virtual std::any data() = 0;
			virtual void move(std::any from, std::any to) = 0;
			virtual size_t size() = 0;
			virtual std::unique_ptr<ComponentMapBase> create() = 0;
		};

		template<typename T>
		class ComponentMap : ComponentMapBase {

		public:

			virtual std::size_t insert(Handle handle, std::any v) {
				m_data.push_back(std::make_pair(handle, std::any_cast<T>(v)));
				return m_data.size() - 1;
			};

			virtual std::any get(std::size_t index) {
				assert(index < m_data.size());
				return &m_data[index];
			};

			virtual Handle erase(std::size_t index) {
				Handle ret{0};
				std::size_t last = m_data.size() - 1;
				if( index < last ) {
					ret = m_data[last].first;
					m_data[index] = std::move(m_data[last]);
				}
				m_data.pop_back();
				return ret;
			};

			virtual std::any data() {
				return &m_data;
			};

			virtual void move(std::any from, std::any to) {
				if constexpr (std::is_move_assignable_v<T>) {
					*std::any_cast<T*>(to) = std::move(*std::any_cast<T*>(from));
				} else {
					*std::any_cast<T*>(to) = *std::any_cast<T*>(from);
				}
			};

			virtual std::unique_ptr<ComponentMapBase> create() {
				return std::make_unique<ComponentMap<T>>();
			};

			size_t size() {
				return m_data.size();
			}

		private:
			std::vector<std::pair<Handle, T>> m_data;
		};

		//----------------------------------------------------------------------------------------------

		class Archetype {

		public:
			template<typename... Ts>
				requires (vtll::unique<vtll::tl<Ts...>>::value)
			Archetype() {
				auto func = [&]<typename T>() {
					m_maps[type<T>()] = std::make_unique<ComponentMap<T>>();
					m_types.insert(type<T>());
				};

				(func.template operator()<Ts>(), ...);
			}

			template<typename T>
			Archetype(const Archetype& other, size_t index, T* value) {
				m_types = other.m_types;
				if (value) {
					assert(std::find(m_types.begin(), m_types.end(), type<T>()) == m_types.end());
					m_types.push_back(type<T>());
					m_maps[type<T>()] = std::make_unique<ComponentMap<T>>();
				} else {
					auto it = std::find(m_types.begin(), m_types.end(), type<T>());
					assert(it != m_types.end());
					m_types.erase(it);
				}

				for( auto& it : other.m_maps ) {
					if (value) {
						if( it.first == type<T>() ) {
							continue;
						}
					}
					m_maps[it.first] = it.second->create();
				}
			}

			const auto& types() {
				return m_types;
			}

			bool has(std::type_index ti) {
				return (std::find(m_types.begin(), m_types.end(), ti) != std::end(m_types));
			}

			template<typename... Ts>
			requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value))
			[[nodiscard]]
			auto create( Handle handle, Ts&&... component ) -> size_t {
				size_t index{0};
				bool first{true};
				auto func = [&](auto&& v) {
					size_t nindex = it.second->map<std::decay_t<decltype(v)>>.insert(handle, v);
					assert(first || index == nindex);
					index = nindex;
					first = false;
				;
				(func(component), ...);
				return index;
			}

			template<typename T>
			[[nodiscard]]
			T get(std::size_t index) {
				auto it = std::find(m_types.begin(), m_types.end(), ti);
				assert(it != std::end(m_types) && index < m_maps[*it]->size());
				return std::any_cast<T>(m_maps[*it]->get(index));
			}

			Handle erase(std::size_t index) {
				
			}

			template<typename T>
			auto data() -> std::vector<std::pair<Handle, T>>* {
				auto it = m_maps.find(type<T>());
				assert(it != m_maps.end());
				return std::any_cast<std::vector<std::pair<Handle, T>>*>(it->second->data());
			}

		private:

			template<typename T>
			auto ptr(Handle handle) -> T* {
				auto it = m_entities.find(handle);
				assert(it != m_entities.end());
				auto& [arch, index] = it->second;

				if( arch->m_maps.find(type<T>()) == arch->m_maps.end() ) {
					arch->m_maps[type<T>()] = std::make_unique<ComponentMap<T>>();

				}

				return &(std::any_cast<std::pair<Handle,T>*>(m_entities[type<T>()]->get(handle)))->second;
			}

			std::vector<std::type_index> m_types;
			std::unordered_map<std::type_index, std::unique_ptr<ComponentMapBase>> m_maps;
		};


		struct ArchetypeIndex {
			Archetype* m_archetype;
			size_t m_index;
		};

		//----------------------------------------------------------------------------------------------

	public:

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
		requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value))
		[[nodiscard]]
		auto create( Ts&&... component ) -> Handle {
			Handle handle{ ++m_next_id };

			std::vector<std::type_index> types = {type<Ts>()...};
			auto it = m_archetypes.find(&types);
			if( it == m_archetypes.end() ) {
				m_archetypes[&types] = std::make_unique<Archetype( std::forward<Ts>(component)... )>;
				it = m_archetypes.find(&types);
			}

			auto index = it->second->create(handle, std::forward<Ts>(component)...);

			m_entities[handle] = { it, index };
			return handle;
		}

		/// @brief Test if an entity exists.
		/// @param handle The handle of the entity.
		/// @return true if the entity exists, else false.
		bool exists(Handle handle) {
			assert(valid(handle));
			return m_entities.find(handle) != m_entities.end();
		}

		/// @brief Test if an entity has a component.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return true if the entity has the component, else false.
		template<typename T>
		bool has(Handle handle) {
			assert(valid(handle));
			return 	m_entities.find(handle) != m_entities.end() && 
					m_entities[handle].m_archetype->m_types.find(type<T>()) != m_entities[handle].m_archetype->m_types.end();
		}

		/// @brief Get the types of the components of an entity.
		/// @param handle The handle of the entity.
		/// @return A vector of type indices of the components.
		const auto& types(Handle handle) {
			assert(exists(handle));
			return m_entities[handle].m_archetype->m_types;
		}

		/// @brief Get component value of an entity.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.	
		/// @return The component value.
		template<typename T>
		[[nodiscard]]
		auto get(Handle handle) -> T {
			assert(exists(handle));
			return *ptr<T>(handle);
		}

		template<typename... Ts>
		requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value))
		[[nodiscard]]
		auto get(Handle handle) -> std::tuple<Ts...> {
			return std::tuple<Ts...>(get<Ts>(handle)...);
		}

		template<typename T>
		requires (!is_tuple<T>::value)
		void put(Handle handle, T&& v) {
			assert(exists(handle));
			*ptr<std::decay_t<decltype(v)>>(handle) = v;
		}

		template<typename... Ts>
		requires (vtll::unique<vtll::tl<Ts...>>::value)
		void put(Handle handle, std::tuple<Ts...>& v) {
			(put(handle, std::get<Ts>(v)), ...);
		}

		template<typename... Ts>
		requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value))
		void put(Handle handle, Ts&&... vs) {
			(put(handle, vs), ...);
		}

		template<typename... Ts>
		void erase(Handle handle) {
			assert( (has<Ts>(handle) && ...) );
			auto& entt = m_entities.find(handle)->second;

			auto func = [&](Handle handle, auto&& ti) {
				entt.second.first->map<std::decay_t<decltype(ti)>>.erase(entt.second.second);
				m_entities[ti]->erase(handle);

			};

			(func(handle, std::type_index(typeid(Ts))), ...);
		}

		void erase(Handle handle) {
			assert(exists(handle));
			for( auto& it : m_entities.find(handle)->second ) {
				m_entities[it]->erase(handle);
			}
			m_entities.erase(handle);
		}

		template<typename T>
		[[nodiscard]]
		auto data() -> const std::vector<std::pair<Handle, T>>& {
			return *std::any_cast< std::vector<std::pair<Handle, T>>* > (m_entities[type<T>()].m_archetype->data());
		}

	private:

		template<typename T>
		auto ptr(Handle handle) -> T* {
			auto it = m_entities.find(handle);
			assert(it != m_entities.end());
			auto& [arch, index] = it->second;

			if( arch->m_maps.find(type<T>()) == arch->m_maps.end() ) {
				arch->m_maps[type<T>()] = std::make_unique<ComponentMap<T>>();

			}

			return &(std::any_cast<std::pair<Handle,T>*>(m_entities[type<T>()]->get(handle)))->second;
		}

		std::size_t m_next_id{0};
		//std::unordered_map<Handle, std::set<std::type_index>> m_entities; //should have ref to archetype instead of set
		//std::unordered_map<std::type_index, std::unique_ptr<ComponentMapBase>> m_component_maps; //get rid of this

		std::unordered_map<Handle, ArchetypeIndex> m_entities; //Archetype and index in archetype
		std::unordered_map<std::vector<std::type_index>*, std::unique_ptr<Archetype>> m_archetypes; //Mapping vector of type index to archetype
		std::unordered_map<std::type_index, std::set<Archetype*>> m_has_types; //Mapping type index to archetype
	};

}


