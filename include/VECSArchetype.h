#pragma once

namespace vecs {

	//----------------------------------------------------------------------------------------------
	//Archetype

	/// @brief An archetype of entities with the same components. 
	/// All entities that have the same components are stored in the same archetype. 
	/// The components are stored in the component maps. Note that the archetype class is not templated,
	/// but some methods including a constructor are templated. Thus the class knows only type indices
	/// of its components, not the types themselves.
	class Archetype {

	public:

		/// @brief A pair of an archetype and an index. This is stored in the slot map.
		struct ArchetypeAndIndex {
			Archetype* m_arch;	//pointer to the archetype
			size_t m_index;			//index of the entity in the archetype
		};

		/// @brief Constructor, creates the archetype.
		Archetype() {
			AddComponent<Handle>(); //insert the handle			
		}

		/// @brief Insert a new entity with components to the archetype.
		/// @tparam ...Ts Value types of the components.
		/// @param handle The handle of the entity.
		/// @param ...values The values of the components.
		/// @return The index of the entity in the archetype.
		template<typename... Ts>
		size_t Insert(Handle handle, Ts&& ...values) {
			assert(m_maps.size() == sizeof...(Ts) + 1);
			assert((m_maps.contains(Type<Ts>()) && ...));
			(AddValue(std::forward<Ts>(values)), ...); //insert all components, get index of the handle
			return AddValue(handle); //insert the handle
		}

		/// @brief Get referece to the types of the components.
		/// @return A reference to the container of the types.
		[[nodiscard]] auto& Types() {
			return m_types;
		}

		/// @brief Test if the archetype has a component.
		/// @param ti Hash of the type index of the component.
		/// @return true if the archetype has the component, else false.
		bool Has(const size_t ti) {
			return m_types.contains(ti);
		}

		/// @brief Get component value of an entity. 
		/// @tparam T The type of the component.
		/// @param archIndex The index of the entity in the archetype.
		/// @return The component value.
		template<typename U>
		[[nodiscard]] auto Get(size_t archIndex) -> U& {
			using T = std::decay_t<U>;
			assert(m_maps.contains(Type<T>()));
			assert(m_maps[Type<T>()]->size() > archIndex);
			return (*Map<U>())[archIndex]; //Map<U>() decays the type
		}

		/// @brief Get component values of an entity.
		/// @tparam ...Ts Types of the components to get.
		/// @param handle Handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires (sizeof...(Ts) > 1)
		[[nodiscard]] auto Get(size_t archIndex) -> std::tuple<Ts&...> {
			assert((m_maps.contains(Type<Ts>()) && ...));
			//assert( (m_maps[Type<Ts>()]->size() > archIndex && ...) );
			return std::tuple<std::decay_t<Ts>&...>{ (*Map<std::decay_t<Ts>>())[archIndex]... };
		}

		/// @brief Put component values to an entity.
		/// @tparam ...Ts Types of the components to put.
		/// @param archIndex The index of the entity in the archetype.
		/// @param ...vs The component values.
		template<typename... Ts>
		void Put(size_t archIndex, Ts&& ...vs) {
			assert((m_maps.contains(Type<Ts>()) && ...));
			auto fun = [&]<typename T>(T && v) { (*Map<std::decay_t<T>>())[archIndex] = std::forward<T>(v); };
			(fun.template operator()(std::forward<decltype(vs)>(vs)), ...);
		}

		/// @brief Erase an entity
		/// @param index The index of the entity in the archetype.
		/// @param slotmaps The slot maps vector of the registry.
		auto Erase(size_t index) -> Handle {
			return Erase2(index);
		}

		/// @brief Move components from another archetype to this one. In the other archetype,
		/// the last entity is moved to the erased one. This might result in a reindexing of the moved entity in the slot map.
		/// @param other The other archetype.
		/// @param other_index The index of the entity in the other archetype.
		/// @return A pair of the index of the new entity in this archetype and the handle of the moved entity.
		auto Move(Archetype& other, size_t other_index) -> std::pair<size_t, Handle> {
			for (auto& ti : m_types) { //go through all maps
				if (m_maps.contains(ti)) {
					if (other.m_maps.contains(ti)) {
						m_maps[ti]->copy(other.Map(ti), other_index); //insert the new value
					}
					else {
						m_maps[ti]->push_back(); //insert an empty value
					}
				}
			}
			++m_changeCounter;
			return { m_maps[Type<Handle>()]->size() - 1, other.Erase2(other_index) };
		}

		/// @brief Clone the archetype.
		/// @param other The archetype to clone.
		/// @param ignore Ignore these types.
		void Clone(Archetype& other, auto&& ignore) {
			for (auto& ti : other.m_types) { //go through all maps
				if (std::find(ignore.begin(), ignore.end(), ti) != ignore.end()) { continue; }
				m_types.insert(ti); //add the type to the list, could be a tag
				if (other.m_maps.contains(ti)) {
					m_maps[ti] = other.Map(ti)->clone(); //make a component map like this one
				}
			}
		}

		/// @brief Get the number of entites in this archetype.
		/// @return The number of entities.
		size_t Size() {
			return m_maps[Type<Handle>()]->size() - m_gaps.size();
		}

		/// @brief Get the number of rows in this archetype.
		/// This is the number of entities plus the number of gaps.
		/// @return The number of rows.
		size_t Number() {
			return m_maps[Type<Handle>()]->size();
		}

		/// @brief Clear the archetype.
		void Clear() {
			for (auto& map : m_maps) {
				map.second->clear();
			}
			++m_changeCounter;
		}

		/// @brief Print the archetype.
		void Print() {
			std::cout << "Archetype: " << Hash(m_types) << std::endl;
			for (auto ti : m_types) {
				std::cout << "Type: " << ti << " ";
			}
			std::cout << std::endl;
			for (auto& map : m_maps) {
				std::cout << "Map: ";
				map.second->print();
				std::cout << std::endl;
			}
			std::cout << "Entities: ";
			auto map = Map<Handle>();
			for (auto handle : *map) {
				std::cout << handle << " ";
			}
			std::cout << std::endl << std::endl;
		}



		/// @brief Validate the archetype. Make sure all maps have the same size.
		void Validate() {
			for (auto& map : m_maps) {
				assert(map.second->size() == m_maps[Type<Handle>()]->size());
			}
		}

		/// @brief Get the change counter of the archetype. It is increased when a change occurs
		/// that might invalidate a Ref object, e.g. when an entity is moved to another archetype, or erased.
		auto GetChangeCounter() -> size_t {
			return m_changeCounter;
		}

		/// @brief Get the mutex of the archetype.
		/// @return Reference to the mutex.
		[[nodiscard]] auto GetMutex() -> Mutex_t& {
			return m_mutex;
		}

		void AddType(size_t ti) {
			assert(!m_types.contains(ti));
			m_types.insert(ti);	//add the type to the list
		};

		/// @brief Add a new component to the archetype.
		/// @tparam T The type of the component.
		template<typename U>
		void AddComponent() {
			using T = std::decay_t<U>; //remove pointer or reference
			size_t ti = Type<T>();
			assert(!m_types.contains(ti));
			m_types.insert(ti);	//add the type to the list
			m_maps[ti] = std::make_unique<Vector<T>>(); //create the component map
		};

		/// @brief Add a new component value to the archetype.
		/// @param v The component value.
		/// @return The index of the component value.
		template<typename U>
		auto AddValue(U&& v) -> size_t {
			using T = std::decay_t<U>;
			return m_maps[Type<T>()]->push_back(std::forward<U>(v));	//insert the component value
		};

		auto AddEmptyValue(size_t ti) -> size_t {
			return m_maps[ti]->push_back();	//insert the component value
		};

		/// @brief Get the map of the components.
		/// @tparam T The type of the component.
		/// @return Pointer to the component map.
		template<typename U>
		auto Map() -> Vector<std::decay_t<U>>* {
			using T = std::decay_t<U>;
			auto it = m_maps.find(Type<T>());
			assert(it != m_maps.end());
			return static_cast<Vector<T>*>(it->second.get());
		}

		/// @brief Get the data of the components.
		/// @param ti Type index of the component.
		/// @return Pointer to the component map base class.
		auto Map(size_t ti) -> VectorBase* {
			auto it = m_maps.find(ti);
			assert(it != m_maps.end());
			return it->second.get();
		}

	private:

		/// @brief Erase an entity. To ensure thet consistency of the entity indices, the last entity is moved to the erased one.
		/// This might result in a reindexing of the moved entity in the slot map. Thus we need a ref to the slot map
		/// @param index The index of the entity in the archetype.
		/// @return The handle of the moved last entity.
		auto Erase2(size_t index) -> Handle {
			size_t last{ index };
			++m_changeCounter;
			if (m_iteratingArchetype == this && index <= m_iteratingIndex) {  //delayed erasure
				m_gaps.push_back(index);
				(*Map<Handle>())[index] = Handle{}; //invalidate the handle
				return Handle{};
			}
			for (auto& it : m_maps) { last = it.second->erase(index); } //Erase from the component map
			return index < last ? (*Map<Handle>())[index] : Handle{}; //return the handle of the moved entity
		}

		using Map_t = std::unordered_map<size_t, std::unique_ptr<VectorBase>>;
		Mutex_t 			m_mutex; //mutex for thread safety
		Size_t 				m_changeCounter{ 0 }; //changes invalidate references
		std::set<size_t> 	m_types; //types of components
		Map_t 				m_maps; //map from type index to component data

	public:
		//Parallelization strategy (not yet implemented):
		//- When iterating over an archetype, the archetype is locked for READING.
		//- If an entity E should be erased (erase, add or erase component), then 
		//  - the archetype is released for reading and locked for WRITING.
		//  - If E is AFTER the current entity C, then the last entity L is moved over E, release write, lock read.
		//  - If E is BEFORE or EQUAL the current entity C, filling the gap is DELAYED. Instead, the index if E
		//	  is stored in a list of delayed entities. When the iteration is finished, the gaps are closed.
		//    Also the archetype stays in write lock until the end of the iteration.
		inline static thread_local Archetype* m_iteratingArchetype{ nullptr }; //for iterating over archetypes
		inline static thread_local size_t m_iteratingIndex{ std::numeric_limits<size_t>::max() }; //current iterator index
		inline static thread_local std::vector<size_t> m_gaps{}; //gaps from previous erasures that must be filled

	public:

		std::string toJSON() {
			std::string json = "\"archetype\":{\"hash\":" + std::to_string(Hash(m_types)) + ",\"types\":[";
			//std::cout << "Archetype: " << Hash(m_types) << std::endl;
			size_t count = 0;
			for (auto ti : m_types) {
				//std::cout << "Type: " << ti << " ";
				if (count++) json += ",";
				json += std::to_string(ti);
			}
			json += "],";

			json += "\"maps\":[";
			count = 0;
			//std::cout << std::endl;
			for (auto& map : m_maps) {
				if (count++) json += ",";
				json += map.second->toJSON();
			}
			json += "],";

			//std::cout << "Entities: ";

			json += "\"entities\":[";
			count = 0;
			// get vector of all entity handles
			auto map = Map<Handle>();
			size_t aindex = 0;					 // entity index in archetype
			for (auto& handle : *map) {
				auto eindex = handle.GetIndex(); // entity index in registry
				if (aindex) json += ",";
				// write out basic entity information
				json += handle.toJSON();
				// retrieve the values of the entity components
				json += "[";
				count = 0;

				for (auto& map : m_maps) {
					auto type = map.first;
					if (count++)json += ",";
					std::string sub;
					auto typemap = Map(type);

					// This is terrible code, but, to my knowledge, there's no better way to get
					// the template types and hence the data
					if (type == Type<char>())
						sub = toJSONString(std::string(1, Get<char>(aindex)));
					else if (type == Type<unsigned char>())
						sub = toJSONString(std::string(1, Get<unsigned char>(aindex)));
					else if (type == Type<int>())
						sub = std::to_string(Get<int>(aindex));
					else if (type == Type<unsigned int>())
						sub = std::to_string(Get<unsigned int>(aindex));
					else if (type == Type<long>())
						sub = std::to_string(Get<long>(aindex));
					else if (type == Type<unsigned long>())
						sub = std::to_string(Get<unsigned long>(aindex));
					else if (type == Type<long long>())
						sub = std::to_string(Get<long long>(aindex));
					else if (type == Type<unsigned long long>())
						sub = std::to_string(Get<unsigned long long>(aindex));
					else if (type == Type<float>())
						sub = std::to_string(Get<float>(aindex));
					else if (type == Type<double>())
						sub = std::to_string(Get<double>(aindex));
					else if (type == Type<std::string>())
						sub = toJSONString(Get<std::string>(aindex));
					else if (type == Type<Handle>()) {
						// not really adding anything, but for completeness' sake ...
						sub = std::to_string(Get<Handle>(aindex).GetValue());
					}
					/* doesn't seem to be possible ...
					else if (type == Type<bool>())
						sub = Get<bool>(aindex) ? "true" : "false";
					*/
					else
						sub = "\"<unknown>\"";

					json += sub;
				}
				json += "]}";

				aindex++;
			}
			//std::cout << std::endl << std::endl;
			json += "]}";
			return json;

		}

	}; //end of Archetype

} //namespace vecs2



