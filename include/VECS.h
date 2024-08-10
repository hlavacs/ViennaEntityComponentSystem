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

	class VecsSystem {
	private:
	
		struct VecsComponentMapBase {
			~VecsComponentMapBase() = default;
			virtual void* get(VecsHandle handle) = 0;
			virtual void erase(VecsHandle handle) = 0;
			virtual bool empty() = 0;
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

			virtual bool empty() {
				return m_components.empty();
			};
		};

	public:
		VecsSystem() = default;
		virtual ~VecsSystem() = default;

		template<typename... Ts>
		requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value))
		[[nodiscard]]
		VecsHandle create( Ts&&... component ) {
			VecsHandle handle{ ++m_next_id };
			(m_entities[handle].insert(std::type_index(typeid(Ts))), ...);

			auto func = [&](VecsHandle handle, auto&& v) {
				get<std::decay_t<decltype(v)>>(handle) = v;
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
			if( it == m_entities.end() ) {
				return false;
			}
			return it->second.find(std::type_index(typeid(T))) != it->second.end();
		}

		const auto& types(VecsHandle handle) {
			assert(exists(handle));
			return m_entities[handle];
		}

		template<typename T>
		T& get(VecsHandle handle) {
			assert(exists(handle));
			auto ti = std::type_index(typeid(T));
			if( m_component_maps.find(ti) ==  m_component_maps.end() ) {
				m_component_maps[ti] = std::make_unique<VecsComponentMap<T>>();
			}
			return *((T*)m_component_maps[ti]->get(handle));
		}

		template<typename... Ts>
		void erase(VecsHandle handle) {
			assert(exists(handle));
			m_entities.erase(handle);

			auto func = [&](VecsHandle handle, auto ti) {
				m_component_maps[ti]->erase(handle);
				if(m_component_maps[ti]->empty()) {
					m_component_maps.erase(ti);
				}
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

	private:
		std::size_t m_next_id{0};
		std::unordered_map<VecsHandle, std::set<std::type_index>> m_entities;
		std::unordered_map<std::type_index, std::unique_ptr<VecsComponentMapBase>> m_component_maps;
	};

}


