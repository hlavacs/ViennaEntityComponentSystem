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

namespace vecs
{
	using VecsHandle = std::size_t;

	template <typename> struct is_tuple : std::false_type {};
	template <typename ...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};

	class VecsSystem {
	private:
	
		struct VecsComponentMapBase {
			~VecsComponentMapBase() = default;
			virtual void* get(VecsHandle handle) = 0;
			virtual void erase(VecsHandle handle) = 0;
			virtual void* components() = 0;
		};

		template<typename T>
		struct VecsComponentMap : VecsComponentMapBase {
			std::unordered_map<VecsHandle, T> m_components;

			virtual void* get(VecsHandle handle) {
				return &m_components[handle];
			};

			virtual void erase(VecsHandle handle) {
				m_components.erase(handle);
			};

			virtual void* components() {
				return &m_components;
			};

		};

	public:
		VecsSystem() = default;
		virtual ~VecsSystem() = default;

		bool null(VecsHandle handle) {
			return handle == 0;
		}

		template<typename... Ts>
		requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value))
		[[nodiscard]]
		VecsHandle create( Ts&&... component ) {
			VecsHandle handle{ ++m_next_id };
			(m_entities[handle].insert(type<Ts>()), ...);

			auto func = [&](VecsHandle handle, auto&& v) {
				*ptr<std::decay_t<decltype(v)>>(handle) = v;
			};

			(func(handle, component), ...);
			return handle;
		}

		bool exists(VecsHandle handle) {
			assert(handle);
			return m_entities.find(handle) != m_entities.end();
		}

		template<typename T>
		bool has(VecsHandle handle) {
			assert(handle);
			auto it = m_entities.find(handle);
			return it != m_entities.end() && it->second.find(type<T>()) != it->second.end();
		}

		const auto& types(VecsHandle handle) {
			assert(exists(handle));
			return m_entities[handle];
		}

		template<typename T>
		[[nodiscard]]
		auto get(VecsHandle handle) -> T {
			assert(exists(handle));
			return *ptr<T>(handle);
		}

		template<typename... Ts>
		requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value))
		[[nodiscard]]
		auto get(VecsHandle handle) -> std::tuple<Ts...> {
			return std::tuple<Ts...>(get<Ts>(handle)...);
		}

		template<typename T>
		requires (!is_tuple<T>::value)
		void put(VecsHandle handle, T&& v) {
			assert(exists(handle));
			*ptr<std::decay_t<decltype(v)>>(handle) = v;
		}

		template<typename... Ts>
		requires (vtll::unique<vtll::tl<Ts...>>::value)
		void put(VecsHandle handle, std::tuple<Ts...>& v) {
			(put(handle, std::get<Ts>(v)), ...);
		}

		template<typename... Ts>
		requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value))
		void put(VecsHandle handle, Ts&&... vs) {
			(put(handle, vs), ...);
		}

		template<typename... Ts>
		void erase(VecsHandle handle) {
			assert( (has<Ts>(handle) && ...) );
			auto& entt = m_entities.find(handle)->second;

			auto func = [&](VecsHandle handle, auto&& ti) {
				entt.erase(ti);
				m_component_maps[ti]->erase(handle);
			};

			(func(handle, std::type_index(typeid(Ts))), ...);
		}

		template<>
		void erase<void>(VecsHandle handle) {
			assert(exists(handle));
			for( auto& it : m_entities.find(handle)->second ) {
				m_component_maps[it]->erase(handle);
			}
			m_entities.erase(handle);
		}


		template<typename T>
		[[nodiscard]]
		const std::unordered_map<VecsHandle, T>& components() {
			if(m_component_maps.find(type<T>()) == m_component_maps.end()) {
				m_component_maps[type<T>()] = std::make_unique<VecsComponentMap<T>>();
			}
			return *((const std::unordered_map<VecsHandle, T>*) m_component_maps[type<T>()]->components());
		}

	private:

		template<typename T>
		std::type_index type() {
			return std::type_index(typeid(std::decay_t<T>));
		}

		template<typename T>
		auto ptr(VecsHandle handle) -> T* {
			if( m_component_maps.find(type<T>()) ==  m_component_maps.end() ) {
				m_component_maps[type<T>()] = std::make_unique<VecsComponentMap<T>>();
			}
			return static_cast<T*>(m_component_maps[type<T>()]->get(handle));
		}

		std::size_t m_next_id{0};
		std::unordered_map<VecsHandle, std::set<std::type_index>> m_entities;
		std::unordered_map<std::type_index, std::unique_ptr<VecsComponentMapBase>> m_component_maps;
	};

}


