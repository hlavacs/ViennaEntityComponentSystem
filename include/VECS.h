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
}


namespace vecs
{
	using Handle = std::size_t;

	template <typename> struct is_tuple : std::false_type {};
	template <typename ...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};

	class Registry {
	private:
	
		struct ComponentMapBase {
			~ComponentMapBase() = default;
			virtual std::any get(Handle handle) = 0;
			virtual void erase(Handle handle) = 0;
			virtual std::any data() = 0;
			virtual void move(std::any from, std::any to) = 0;
			virtual std::unique_ptr<ComponentMapBase> create() = 0;
		};

		template<typename T>
		struct ComponentMap : ComponentMapBase {

			std::unordered_map<Handle, std::size_t> m_index;
			std::vector<std::pair<Handle, T>> m_data;

			virtual std::any get(Handle handle) {
				if( m_index.find(handle) == m_index.end() ) {
					m_index[handle] = m_data.size();
					m_data.push_back(std::make_pair(handle, T{}));
				}
				return &m_data[m_index[handle]];
			};

			virtual void erase(Handle handle) {
				std::size_t index = m_index[handle];
				std::size_t last = m_data.size() - 1;
				if( index < last ) {
					m_data[index] = std::move(m_data[last]);
					m_index[m_data[last].first] = index;
				}
				m_data.pop_back();
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
		};

		struct Archetype {
				std::set<std::type_index> m_types;
				std::map<std::type_index, std::unique_ptr<ComponentMapBase>> m_component_maps;
		};

	public:

		bool valid(Handle handle) {
			return handle != 0;
		}

		template<typename... Ts>
		requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value))
		[[nodiscard]]
		auto create( Ts&&... component ) -> Handle{
			Handle handle{ ++m_next_id };
			(m_entities[handle].insert(type<Ts>()), ...);

			auto func = [&](Handle handle, auto&& v) {
				*ptr<std::decay_t<decltype(v)>>(handle) = v;
			};

			(func(handle, component), ...);
			return handle;
		}

		bool exists(Handle handle) {
			assert(valid(handle));
			return m_entities.find(handle) != m_entities.end();
		}

		template<typename T>
		bool has(Handle handle) {
			assert(valid(handle));
			auto it = m_entities.find(handle);
			return it != m_entities.end() && it->second.find(type<T>()) != it->second.end();
		}

		const auto& types(Handle handle) {
			assert(exists(handle));
			return m_entities[handle];
		}

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
				entt.erase(ti);
				m_component_maps[ti]->erase(handle);
			};

			(func(handle, std::type_index(typeid(Ts))), ...);
		}

		void erase(Handle handle) {
			assert(exists(handle));
			for( auto& it : m_entities.find(handle)->second ) {
				m_component_maps[it]->erase(handle);
			}
			m_entities.erase(handle);
		}

		template<typename T>
		[[nodiscard]]
		auto data() -> const std::vector<std::pair<Handle, T>>& {
			if(m_component_maps.find(type<T>()) == m_component_maps.end()) {
				m_component_maps[type<T>()] = std::make_unique<ComponentMap<T>>();
			}
			return *std::any_cast< std::vector<std::pair<Handle, T>>* > (m_component_maps[type<T>()]->data());
		}

	private:

		template<typename T>
		auto type() -> std::type_index {
			return std::type_index(typeid(std::decay_t<T>));
		}

		template<typename T>
		auto ptr(Handle handle) -> T* {
			if( m_component_maps.find(type<T>()) ==  m_component_maps.end() ) {
				m_component_maps[type<T>()] = std::make_unique<ComponentMap<T>>();
			}
			return &(std::any_cast<std::pair<Handle,T>*>(m_component_maps[type<T>()]->get(handle)))->second;
		}

		std::size_t m_next_id{0};
		std::unordered_map<Handle, std::set<std::type_index>> m_entities; //should have ref to archetype instead of set
		std::unordered_map<std::type_index, std::unique_ptr<ComponentMapBase>> m_component_maps; //get rid of this

		std::unordered_map<std::size_t, Archetype> m_archetypes; //TODO: archetype 
		std::unordered_multimap<std::type_index, std::size_t> m_archetype_index;
	};

}


