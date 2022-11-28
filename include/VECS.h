#pragma once

#include <typeinfo>
#include <typeindex>
#include <unordered_map>

namespace vecs {

	struct VECSSystem {

		struct VECSComponentContainerBase {
			std::type_index						m_type_index;
			std::unordered_map<uint64_t, void*> m_data;

			virtual ~VECSComponentContainerBase() = 0;
		};

		template<typename T>
		struct VECSComponentContainer : VECSComponentContainerBase {
			VECSComponentContainer<T>() : VECSComponentContainerBase() {
				m_type_index = std::type_index(type_id(T));
			}

			~VECSComponentContainer<T>() {
				while(m_data.size() > 0) {
					auto it = m_data.begin();
					delete (T*)*it;
					m_data.erase(it);
				}
			}
		};

		std::unordered_map<std::type_index, std::shared_ptr<VECSComponentContainerBase>> m_components;


	};

}