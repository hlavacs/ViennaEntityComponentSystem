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
	using VecsHandle = std::size_t;

	template <typename> struct is_tuple : std::false_type {};
	template <typename ...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};

	class VecsSystem {
	private:
	
		class VecsComponentMapBase {

		public:
			std::function<void()> create;
			std::function<void()> destroy;

			std::function<void*(VecsHandle)> get;
			std::function<void(void*,void*)> copy;
			std::function<void(void*,void*)> move;
			std::function<void(VecsHandle)> erase;
			void* data(){ return m_data; };

			VecsComponentMapBase( VecsComponentMapBase& r) {
				create = r.create;
				destroy = r.destroy;
				get = r.get;
				copy = r.copy;
				move = r.move;
				erase = r.erase;
				create();
			};

			~VecsComponentMapBase() { destroy(); };

		protected:
			VecsComponentMapBase() = default;
			void* m_data;
			std::unordered_map<VecsHandle, std::size_t> m_index;
		};

		template<typename T>
		class VecsComponentMap : public VecsComponentMapBase {

		public:
			using VecsComponentMapVector = std::vector<std::pair<VecsHandle, T>>;

			VecsComponentMap() {
				VecsComponentMapBase::create = [&](){
					m_data = new std::vector<std::pair<VecsHandle, T>>();
				};
				VecsComponentMapBase::create();

				VecsComponentMapBase::destroy = [&](){
					delete (std::vector<std::pair<VecsHandle, T>>*) data();
				};
				
				VecsComponentMapBase::get = [&](VecsHandle handle) {
					auto dataptr = (VecsComponentMapVector*)data();

					if( m_index.find(handle) == m_index.end() ) {
						m_index[handle] = dataptr->size();
						dataptr->push_back(std::make_pair(handle, T{}));
					}
					return &(*dataptr)[m_index[handle]];
				};

				VecsComponentMapBase::copy = [](void* from, void* to) {
					*((T*)to) = *((T*)from);
				};

				VecsComponentMapBase::move = [](void* from, void* to) {
					*((T*)to) = std::move(*((T*)from));
				};

				VecsComponentMapBase::erase = [&](VecsHandle handle) {
					auto dataptr = (VecsComponentMapVector*)data();

					std::size_t index = m_index[handle];
					std::size_t last = dataptr->size() - 1;
					if( index < last ) {
						(*dataptr)[index] = std::move( (*dataptr)[last] );
						m_index[(*dataptr)[last].first] = index;
					}
					dataptr->pop_back();
				};
			}
		};

		struct VecsArchetype {
				std::set<std::type_index> m_types;
				std::map<std::type_index, std::unique_ptr<VecsComponentMapBase>> m_component_maps;
		};

	public:
		VecsSystem() = default;
		virtual ~VecsSystem() = default;

		bool valid(VecsHandle handle) {
			return handle != 0;
		}

		template<typename... Ts>
		requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value))
		[[nodiscard]]
		auto create( Ts&&... component ) -> VecsHandle{
			VecsHandle handle{ ++m_next_id };
			(m_entities[handle].insert(type<Ts>()), ...);

			auto func = [&](VecsHandle handle, auto&& v) {
				*ptr<std::decay_t<decltype(v)>>(handle) = v;
			};

			(func(handle, component), ...);
			return handle;
		}

		bool exists(VecsHandle handle) {
			assert(valid(handle));
			return m_entities.find(handle) != m_entities.end();
		}

		template<typename T>
		bool has(VecsHandle handle) {
			assert(valid(handle));
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

		void erase(VecsHandle handle) {
			assert(exists(handle));
			for( auto& it : m_entities.find(handle)->second ) {
				m_component_maps[it]->erase(handle);
			}
			m_entities.erase(handle);
		}

		template<typename T>
		[[nodiscard]]
		auto data() -> const std::vector<std::pair<VecsHandle, T>>& {
			if(m_component_maps.find(type<T>()) == m_component_maps.end()) {
				m_component_maps[type<T>()] = std::make_unique<VecsComponentMap<T>>();
			}
			return *((const std::vector<std::pair<VecsHandle, T>>*) m_component_maps[type<T>()]->data());
		}

	private:

		template<typename T>
		auto type() -> std::type_index {
			return std::type_index(typeid(std::decay_t<T>));
		}

		template<typename T>
		auto ptr(VecsHandle handle) -> T* {
			if( m_component_maps.find(type<T>()) ==  m_component_maps.end() ) {
				m_component_maps[type<T>()] = std::make_unique<VecsComponentMap<T>>();
			}
			return &(static_cast<std::pair<VecsHandle,T>*>(m_component_maps[type<T>()]->get(handle)))->second;
		}

		std::size_t m_next_id{0};
		std::unordered_map<VecsHandle, std::set<std::type_index>> m_entities; //should have ref to archetype instead of set
		std::unordered_map<std::type_index, std::unique_ptr<VecsComponentMapBase>> m_component_maps; //get rid of this

		std::unordered_map<std::size_t, VecsArchetype> m_archetypes; //TODO: archetype 
		std::unordered_multimap<std::type_index, std::size_t> m_archetype_index;
	};

}


