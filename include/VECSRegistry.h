#pragma once

namespace vecs {

	//----------------------------------------------------------------------------------------------
	//Registry concepts and types

	template<typename... Ts>
	concept VecsDataType = ((vtll::unique<vtll::tl<Ts...>>::value) && (!vtll::has_type< vtll::tl<Ts...>, Handle>::value));

	template<typename... Ts>
	concept VecsView = ((vtll::unique<vtll::tl<Ts...>>::value) && (sizeof...(Ts) > 0) && (!std::is_same_v<Handle&, Ts> && ...));

	template<typename... Ts>
	concept VecsIterator = (vtll::unique<vtll::tl<Ts...>>::value);

	template<typename... Ts> requires VecsIterator<Ts...> class Iterator;
	template<typename... Ts> requires VecsView<Ts...> class View;

	//----------------------------------------------------------------------------------------------
	//Registry 


	/// @brief A registry for entities and components.
	class Registry{

		/// @brief Entry for the seach cache
		struct TypeSetAndHash {
			std::set<size_t> m_types;	//set of types that have been searched for
			size_t m_hash;				//hash of the set
		};

		template<vecs::VecsPOD T>
		struct SlotMapAndMutex {
			SlotMap<T> m_slotMap;
			Mutex_t m_mutex;
			SlotMapAndMutex( uint32_t storageIndex, uint32_t bits ) : m_slotMap{storageIndex, bits}, m_mutex{} {};
			SlotMapAndMutex( const SlotMapAndMutex& other ) : m_slotMap{other.m_slotMap}, m_mutex{} {};
		};

		#ifdef REGISTRYTYPE_SEQUENTIAL
			using NUMBER_SLOTMAPS = std::integral_constant<int, 1>;
		#else
			using NUMBER_SLOTMAPS = std::integral_constant<int, 16>;
		#endif

		using Slot_t = typename SlotMap<typename Archetype::ArchetypeAndIndex>::Slot;
		using SlotMaps_t = std::vector<SlotMapAndMutex<typename Archetype::ArchetypeAndIndex>>;
		using HashMap_t = std::map<size_t, std::unique_ptr<Archetype>>;

	public:	

		//----------------------------------------------------------------------------------------------

		template<typename U>
			requires (!std::is_reference_v<U>)
		class Ref {

			using T = std::decay_t<U>;

		public:
			Ref() = default;
			Ref(Handle handle, Slot_t& slot) : m_handle{handle}, m_slot{&slot}, m_archetype{slot.m_value.m_arch} {}
			Ref(const Ref& other) : m_handle{other.m_handle}, m_slot{other.m_slot}, m_archetype{other.m_archetype} {}

			bool IsValid() { return m_slot != nullptr; }
			bool Exists() { return m_slot->m_version == m_handle.GetVersion(); }
			auto operator()() -> T& {return GetReference(); }
			auto operator=(T&& value) -> void { GetReference() = std::forward<T>(value); }
			     operator T&() { return GetReference(); }
			auto Value() -> T& { return GetReference(); }
			auto Get() -> T& { return GetReference(); }

		private:
			auto GetReference() -> T& {
				auto arch = m_slot->m_value.m_arch;
				auto index = m_slot->m_value.m_index;
				if( !m_slot || m_slot->m_version != m_handle.GetVersion() || ( arch != m_archetype && !arch->Has(Type<T>()) )  ) {
					if( !arch->Has(Type<T>()) ) {
						std::cout << "Reference to type " << typeid(std::declval<T>()).name() << " invalidated because of adding or erasing a component or erasing an entity!" << std::endl;
						assert(false);
						exit(-1);
					}
					m_archetype = arch;
				}
				return (*arch->template Map<T>())[index];
			}

			Handle m_handle{};
			Slot_t* m_slot{nullptr};
			Archetype *m_archetype{nullptr};
		};

		//----------------------------------------------------------------------------------------------

		template<typename U, auto P, typename D>
			requires (!std::is_reference_v<vsty::strong_type_t<U, P, D>>)
		class Ref<vsty::strong_type_t<U, P, D>> {

			using T = vsty::strong_type_t<U, P, D>;

		public:
			Ref() = default;		
			Ref(Handle handle, Slot_t& slot) : m_handle{handle}, m_slot{&slot}, m_archetype{slot.m_value.m_arch} {}
			Ref(const Ref& other) : m_handle{other.m_handle}, m_slot{other.m_slot}, m_archetype{other.m_archetype} {}

			bool IsValid() { return m_slot != nullptr; }
			bool Exists() { return m_slot->m_version == m_handle.GetVersion(); }
			auto operator()() -> U& {return GetReference()(); }
			auto operator=(T&& value) -> void { GetReference()() = std::forward<T>(value); }
			     operator T&() { return GetReference(); }
				 operator U&() { return GetReference()(); }
			auto Value() -> U& { return GetReference()(); }
			auto Get() -> T& { return GetReference(); }

		private:
			auto GetReference() -> T& {
				auto arch = m_slot->m_value.m_arch;
				auto index = m_slot->m_value.m_index;
				if( !m_slot || m_slot->m_version != m_handle.GetVersion() || ( arch != m_archetype && !arch->Has(Type<T>()) ) ) {
					std::cout << "Reference to type " << typeid(std::declval<T>()).name() << " invalidated because of adding or erasing a component or erasing an entity!" << std::endl;
					assert(false);
					exit(-1);
				}
				if( arch != m_archetype ) {
					m_archetype = arch;
				}
				return (*arch->template Map<T>())[index];
			}

			Handle m_handle{};
			Slot_t* m_slot{nullptr};
			Archetype *m_archetype{nullptr};
		};

		template<typename T>
		using to_ref_t = std::conditional<std::is_reference_v<T>, Ref<std::decay_t<T>>, T>::type;


		//----------------------------------------------------------------------------------------------

		/// @brief A structure holding a pointer to an archetype and the current size of the archetype.
		struct ArchetypeAndSize {
			Archetype* 	m_arch;	//pointer to the archetype
			size_t 				m_size;	//size of the archetype
			ArchetypeAndSize(Archetype* arch, size_t size) : m_arch{arch}, m_size{size} {}
		};


		//----------------------------------------------------------------------------------------------


		/// @brief Used for iterating over entity components. Iterators are created by a view. 
		template<typename... Ts>
		class Iterator {

		public:
			/// @brief Iterator constructor saving a list of archetypes and the current index.
			/// @param arch List of archetypes. 
			/// @param archidx First archetype index.
			Iterator( Registry& system, std::vector<ArchetypeAndSize>& arch, size_t archidx) 
				: m_registry(system), m_archetypes{arch}, m_archidx{archidx}, m_entidx{0} {
				m_archidx>0 ? m_end = true : m_end = false;
			}

			/// @brief Copy constructor.
			Iterator(const Iterator& other) 
				: m_registry{other.m_registry}, m_archetypes{other.m_archetypes}, m_archidx{other.m_archidx}, m_entidx{other.m_entidx} {

			}

			/// @brief Destructor, unlocks the archetype.
			~Iterator() {
				if(m_end && m_archidx < m_archetypes.size()) { m_registry.FillGaps(m_archetypes[m_archidx].m_arch);	}
				Archetype::m_iteratingArchetype = nullptr;
			}

			/// @brief Prefix increment operator.
			auto operator++() {
				if( m_archidx >= m_archetypes.size() ) { return *this; }
				++m_entidx;
				Archetype::m_iteratingIndex = m_entidx;
				while( m_entidx >= m_archetypes[m_archidx].m_arch->Number() || m_entidx >= m_archetypes[m_archidx].m_size ) {
					m_entidx = 0;
					m_registry.FillGaps(m_archetypes[m_archidx].m_arch);
					++m_archidx;
					if( m_archidx >= m_archetypes.size() ) { break; }
				}
				return *this;
			}

			/// @brief Access the content the iterator points to.
			auto operator*() {
				if(m_archidx < m_archetypes.size()) {
					Archetype::m_iteratingArchetype = m_archetypes[m_archidx].m_arch;
					Archetype::m_iteratingIndex = m_entidx;
				}

				auto tup = std::make_tuple( Get<Ts>()... );
				if constexpr (sizeof...(Ts) == 1) { return std::get<0>(tup); }
				else return tup;
			}

			/// @brief Compare two iterators.
			auto operator!=(const Iterator& other) -> bool {
				return (m_archidx != other.m_archidx) || (m_entidx != other.m_entidx);
			}

		private:

			template<typename T>
				requires (!std::is_reference_v<T>)
			auto Get() -> T {
				return (*m_archetypes[m_archidx].m_arch->template Map<T>())[m_entidx];
			}

			template<typename T>
				requires std::is_reference_v<T>
			auto Get() -> to_ref_t<T> {
				auto arch = m_archetypes[m_archidx].m_arch;
				Handle handle = (*arch->template Map<Handle>())[m_entidx];
				return to_ref_t<T>( handle, m_registry.GetSlot(handle));
			}

			Registry& m_registry; ///< Reference to the registry system.
			Vector<Handle>*	m_mapHandle{nullptr}; ///< Pointer to the comp map holding the handle of the current archetype.
			std::vector<ArchetypeAndSize>& m_archetypes; ///< List of archetypes.
			size_t 	m_end{false};	///< True if this is the end iterator.
			size_t 	m_archidx{0};	///< Index of the current archetype.
			size_t 	m_entidx{0};	///< Index of the current entity.
		}; //end of Iterator


		//----------------------------------------------------------------------------------------------

		/// @brief A view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		template<typename... Ts>
		class View {

		public:
			View(Registry& system, HashMap_t& map, auto&& tagsYes, auto&& tagsNo ) : 
				m_system{system}, m_map(map), m_tagsYes{tagsYes}, m_tagsNo{tagsNo} {
			} ///< Constructor.

			/// @brief Get an iterator to the first entity. 
			/// The archetype is locked in shared mode to prevent changes. 
			/// @return Iterator to the first entity.
			auto begin() {
				m_archetypes.clear();
				for( auto& map : m_map ) { //go through all archetypes
					auto arch = map.second.get();
					if( arch->Size() == 0 ) { continue; } //skip empty archetypes
					bool hasTypes = (arch->Has(Type<Ts>()) && ...); //should have all types
					bool hasAllTagsYes = true; //should have all tags
					bool hasNoTagsNo = true; //should not have any of these tags
					for( auto& tag : m_tagsYes ) { if( !arch->Has(tag) ) { hasAllTagsYes = false; break; } }
					for( auto& tag : m_tagsNo ) { if( arch->Has(tag) ) { hasNoTagsNo = false; break; } }
					if( hasTypes && hasAllTagsYes && hasNoTagsNo ) { //all conditions met
						m_archetypes.push_back({arch, arch->Size()});
					}
				}
				return Iterator<Ts...>{m_system, m_archetypes, 0};
			}

			/// @brief Get an iterator to the end of the view.
			auto end() {
				return Iterator<Ts...>{m_system, m_archetypes, m_archetypes.size()};
			}

		private:

			Registry& 				m_system;	///< Reference to the registry system.
			std::vector<size_t> 			m_tagsYes;	///< List of tags that must be present.
			std::vector<size_t> 			m_tagsNo;	///< List of tags that must not be present.
			HashMap_t& 						m_map;		///< List of archetypes.
			std::vector<ArchetypeAndSize>  	m_archetypes;	///< List of archetypes.
		}; //end of View


		//----------------------------------------------------------------------------------------------

		template<typename... Ts> friend class Iterator;

		Registry() { 
			m_slotMaps.reserve(NUMBER_SLOTMAPS::value); //resize the slot storage
			for( uint32_t i = 0; i < NUMBER_SLOTMAPS::value; ++i ) {
				m_slotMaps.emplace_back( SlotMapAndMutex<typename Archetype::ArchetypeAndIndex>{ i, 6  } ); 
			}
			//If it is in Debug Mode - connect to Console
#ifdef _DEBUG
			GetConsoleComm(this);
#endif
		};

		~Registry() = default;	///< Destructor.

		/// @brief Get the number of entities in the system.
		/// @return The number of entities.
		size_t Size() {
			return m_size;
		}

		/// @brief Create an entity with components.
		/// @tparam ...Ts The types of the components.
		/// @param ...component The new values.
		/// @return Handle of new entity.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		[[nodiscard]] auto Insert( Ts&&... component ) -> Handle {
			size_t slotMapIndex = GetNewSlotmapIndex();
			auto [handle, slot] = m_slotMaps[slotMapIndex].m_slotMap.Insert( {nullptr, 0} ); //get a slot for the entity
			slot.m_value.m_arch = GetArchetype<Ts...>(nullptr, {}, {});
			slot.m_value.m_index = slot.m_value.m_arch->Insert( handle, std::forward<Ts>(component)... ); //insert the entity into the archetype
			++m_size;
			return handle;
		}

		/// @brief Test if an entity exists.
		/// @param handle The handle of the entity.
		/// @return true if the entity exists, else false.
		bool Exists(Handle handle) {
			auto& slot = GetSlot(handle);
			return slot.m_version == handle.GetVersion();
		}

		/// @brief Test if an entity has a component.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return true if the entity has the component, else false.
		template<typename T>
		bool Has(Handle handle) {
			assert(Exists(handle));
			auto arch = GetArchetypeAndIndex(handle).m_arch;
			return arch->Has(Type<T>());
		}

		/// @brief Test if an entity has a tag.
		/// @param handle The handle of the entity.
		/// @param tag The tag to test.
		/// @return true if the entity has the tag, else false.
		bool Has(Handle handle, size_t ti) {
			assert(Exists(handle));
			auto arch = GetArchetypeAndIndex(handle).m_arch;
			return arch->Has(ti);
		}

		/// @brief Get the types of the components of an entity.
		/// @param handle The handle of the entity.
		/// @return A vector of type indices of the components.
		auto Types(Handle handle) {
			assert(Exists(handle));
			auto arch =  GetArchetypeAndIndex(handle).m_arch;
			return arch->Types();
		}

		/// @brief Get a component value of an entity.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return The component value or reference to it.
		template<typename T>
		auto Get(Handle handle) -> to_ref_t<T> {
			return std::get<0>(Get2<T>(handle));
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires (sizeof...(Ts)>1 && vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle&>::value)
		[[nodiscard]] auto Get(Handle handle) -> std::tuple<to_ref_t<Ts>...> {
			return Get2<Ts...>(handle);
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param v The new values in a tuple
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
		void Put(Handle handle, std::tuple<Ts...>& v) {
			Put2(handle, std::forward<Ts>(std::get<Ts>(v))...);
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
			requires ((vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
		void Put(Handle handle, Ts&&... vs) {
			Put2(handle, std::forward<Ts>(vs)...);
		}

		/// @brief Add tags to an entity.
		/// @tparam ...Ts The types of the tags.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to add.
		template<typename... Ts>
			requires (std::is_integral_v<std::decay_t<Ts>> && ...)
		void AddTags(Handle handle, Ts... tags) {
			AddTags(handle, std::vector<size_t>{tags...});
		}
		
		/// @brief Add tags to an entity.
		/// @param handle The handle of the entity.
		/// @param tags The tags to add.
		/// @param ...tags The tags to add.
		void AddTags(Handle handle, const std::vector<size_t>&& tags) {
			auto& archAndIndex = GetArchetypeAndIndex(handle);
			auto oldArch = archAndIndex.m_arch;
			auto newArch = GetArchetype(oldArch, std::forward<decltype(tags)>(tags), {});
			Move(newArch, oldArch, archAndIndex);
		}

		/// @brief Erase tags from an entity.
		/// @tparam ...Ts The types of the tags.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to erase.
		template<typename... Ts>
			requires (std::is_integral_v<std::decay_t<Ts>> && ...)
		void EraseTags(Handle handle, Ts... tags) {
			EraseTags(handle, std::vector<size_t>{tags...});
		}

		/// @brief Erase tags from an entity.
		/// @tparam ...Ts The types of the tags.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to erase.
		void EraseTags(Handle handle, const std::vector<size_t>&& tags) {
			auto& archAndIndex = GetArchetypeAndIndex(handle);
			auto oldArch = archAndIndex.m_arch;
			auto newArch = GetArchetype(oldArch, {}, std::forward<decltype(tags)>(tags));
			Move(newArch, oldArch, archAndIndex);
		}
		
		/// @brief Erase components from an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.		
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Erase(Handle handle) {
			auto& archAndIndex = GetArchetypeAndIndex(handle);
			auto arch = archAndIndex.m_arch;
			assert( (arch->Has(Type<Ts>()) && ...) );
			auto newArch = GetArchetype(arch, {}, std::vector<size_t>{Type<Ts>()...});	
			Move(newArch, arch, archAndIndex);		
		}

		/// @brief Erase an entity from the registry.
		/// @param handle The handle of the entity.
		void Erase(Handle handle) {
			auto& slot = GetSlot(handle);
			auto& archAndIndex = slot.m_value;
			ReindexMovedEntity(archAndIndex.m_arch->Erase(archAndIndex.m_index), archAndIndex.m_index);
			slot.m_version++; //invalidate the slot
			--m_size;		
		}

		/// @brief Clear the registry by removing all entities.
		void Clear() {
			for( auto& arch : m_archetypes ) { arch.second->Clear(); }
			for( auto& slotmap : m_slotMaps ) { slotmap.m_slotMap.Clear(); }
			m_size = 0;
		}

		/// @brief Get a view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		/// @return A view of the entity components
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto GetView(std::vector<size_t>&& yes={}, std::vector<size_t>&& no={}) -> View<Ts...> {
			return {*this, m_archetypes, std::forward<std::vector<size_t>>(yes), std::forward<std::vector<size_t>>(no),};
		}

		/// @brief Print the registry.
		/// Print the number of entities and the archetypes.
		void Print() {
			std::cout << "-----------------------------------------------------------------------------------------------" << std::endl;
			std::cout << "Entities: " << Size() << std::endl;
			for( auto& it : m_archetypes ) {
				std::cout << "Archetype Hash: " << it.first << std::endl;
				it.second->Print();
			}
			std::cout << std::endl << std::endl;
		}

		/// @brief Validate the registry.
		/// Make sure all archetypes have the same size in all component maps.
		void Validate() {
			for( auto& it : m_archetypes ) { 
				auto arch = it.second.get();
				arch->Validate();
			}
		}

		/// @brief Get the mutex of the archetype.
		/// @return Reference to the mutex.
		[[nodiscard]] inline auto GetSlotMapMutex(size_t index) -> Mutex_t& {
			return m_slotMaps[index].m_mutex;
		}

		/// @brief Get the mutex of the archetype.
		/// @return Reference to the mutex.
		[[nodiscard]] inline auto GetMutex() -> Mutex_t& {
			return m_mutex; 
		}

		/// @brief Swap two entities.
		/// @param h1 The handle of the first entity.
		/// @param h2 The handle of the second entity.
		/// @return true if the entities were swapped, else false.
		bool Swap( Handle h1, Handle h2 ) {
			return true;
		}

		/// @brief Fill gaps from previous erasures.
		// This is necessary when an entity is erased during iteration. The last entity is moved to the erased one
		// after Iteration is finished. This is triggered by the iterator.
		void FillGaps(Archetype* arch) {
			std::sort(arch->m_gaps.begin(), arch->m_gaps.end(), std::greater<size_t>());
			Archetype::m_iteratingArchetype = nullptr;
			for( auto& gap : arch->m_gaps ) {
				if(gap < arch->Number()) ReindexMovedEntity(arch->Erase(gap), gap);
			}
			arch->m_gaps.clear();
			Archetype::m_iteratingArchetype = arch;
		}

	private:

		/// @brief Test if a type is in a container.
		/// @param container The container to search.
		/// @param hs The type hash to search for.
		/// @return true if the type is in the container, else false.
		bool ContainsType(auto&& container, size_t hs) {
			return std::ranges::find(container, hs) != container.end();
		}

		/// @brief Add a type to a container, not yet in the container.
		/// @param container The container to add to.
		/// @param hs The type hash to add.
		void AddType(auto&& container, size_t hs) {
			if(!ContainsType( container, hs)) container.push_back(hs);
		}

		/// @brief Get the index of the entity in the archetype
		/// @param handle The handle of the entity.
		/// @return The index of the entity in the archetype.
		auto GetSlot( Handle handle ) -> Slot_t& {
			return m_slotMaps[handle.GetStorageIndex()].m_slotMap[handle];
		}

		/// @brief Get the index of the entity in the archetype
		/// @param handle The handle of the entity.
		/// @return The index of the entity and the archetype.
		auto GetArchetypeAndIndex( Handle handle ) -> Archetype::ArchetypeAndIndex& {
			return GetSlot(handle).m_value;
		}

		/// @brief Get a new index of the slotmap for the current thread.
		/// @return New index of the slotmap.
		size_t GetNewSlotmapIndex() {
			m_slotMapIndex = (m_slotMapIndex + 1) & (NUMBER_SLOTMAPS::value - 1);
			return m_slotMapIndex;
		}
		
		/// @brief Create a list of type hashes
		/// @param arch The archetype types to use
		/// @param tags Use also these tag hashes
		/// @return A vector of type hashes
		template<typename... Ts>
		auto CreateTypeList(Archetype* arch, const std::vector<size_t>&& tags, const std::vector<size_t>&& ignore) -> std::vector<size_t> {
			std::vector<size_t> all{ tags.begin(), tags.end() };
			(AddType(all, Type<Ts>()), ...);
			if(arch) { for( auto type : arch->Types() ) { if(!ContainsType(ignore, type)) { AddType(all, type); } } }
			return all;
		}

		/// @brief Get an archetype with components.
		/// @tparam ...Ts The component types.
		/// @param arch Use the types of this archetype.
		/// @param tags Should have the tags of the entity.
		/// @return A pointer to the archetype.
		template<typename... Ts>
		auto GetArchetype(Archetype* arch, const std::vector<size_t>&& tags, const std::vector<size_t>&& ignore) -> Archetype* {
			size_t hs = Hash(CreateTypeList<Ts...>(arch, std::forward<decltype(tags)>(tags), std::forward<decltype(ignore)>(ignore)));
			if( m_archetypes.contains( hs ) ) { return m_archetypes[hs].get(); }

			auto newArchUnique = std::make_unique<Archetype>();
			auto newArch = newArchUnique.get();
			if(arch) newArch->Clone(*arch, ignore); //clone old types/components and old tags
			auto fun = [&]<typename T>(){ if( !ContainsType(newArch->Types(), Type<T>()) ) { newArch->template AddComponent<T>(); } };
			(fun.template operator()<Ts>(), ...);
			for( auto tag : tags ) { 
				if(!ContainsType(newArch->Types(), tag) && !ContainsType(ignore, tag)) { newArch->AddType(tag); } 
			} //add new tags
			m_archetypes[hs] = std::move(newArchUnique); //store the archetype
			return newArch;
		}

		/// @brief If a entity is moved or erased, the last entity of the archetype is moved to the empty slot.
		/// This might result in a reindexing of the moved entity in the slot map. 
		/// @param handle The handle of the moved entity.
		/// @param index The index of the entity in the archetype.
		void ReindexMovedEntity(Handle handle, size_t index) {
			if( !handle.IsValid() ) { return; }
			auto& archAndIndex = GetArchetypeAndIndex(handle);
			archAndIndex.m_index = index;
		}

		/// @brief Move an entity to a new archetype.
		/// @param newArch The new archetype.
		/// @param oldArch The old archetype.
		/// @param archAndIndex The archetype and index of the entity.
		void Move(Archetype* newArch, Archetype* oldArch, vecs::Archetype::ArchetypeAndIndex& archAndIndex) {
			auto [newIndex, movedHandle] = newArch->Move(*oldArch, archAndIndex.m_index);
			ReindexMovedEntity(movedHandle, archAndIndex.m_index);
			archAndIndex = { newArch, newIndex };
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle&>::value)
		[[nodiscard]] auto Get2(Handle handle) {
			auto& slot = GetSlot(handle);
			auto& archAndIndex = slot.m_value; //  GetArchetypeAndIndex(handle);
			auto arch = archAndIndex.m_arch;
			if( (arch->Has(Type<Ts>()) && ...) ) { return std::tuple<to_ref_t<Ts>...>{ Get3<Ts>(handle, slot)... }; } 
			auto newArch = GetArchetype<Ts...>(arch, {}, {});
			Move(newArch, arch, archAndIndex);
			return std::tuple<to_ref_t<Ts>...>{ Get3<Ts>(handle, slot)... }; 
		}

		template<typename T>
			requires (!std::is_reference_v<T>)
		auto Get3(Handle handle, Slot_t& slot ) -> T { //Archetype* arch, size_t index) -> T {
			return slot.m_value.m_arch->template Get<T>(slot.m_value.m_index);
		}

		template<typename T>
		requires std::is_reference_v<T>
		auto Get3(Handle handle, Slot_t& slot ) { //Archetype* arch, size_t index) {
			return Ref<std::decay_t<T>>(handle, slot) ; //arch->template Get<Handle>(index), arch->template Get<std::decay_t<T>>(index));
		}

		/// @brief Change the component values of an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
		void Put2(Handle handle, Ts&&... vs) {
			auto& archAndIndex = GetArchetypeAndIndex(handle);
			auto arch = archAndIndex.m_arch;
			if( (arch->Has(Type<Ts>()) && ...) ) { arch->Put(archAndIndex.m_index, std::forward<Ts>(vs)...); return; }
			auto newArch = GetArchetype<Ts...>(arch, {}, {});
			Move(newArch, arch, archAndIndex);
			newArch->Put(archAndIndex.m_index, std::forward<Ts>(vs)...);
		}

		Size_t m_size{0}; //number of entities
		SlotMaps_t m_slotMaps; //Slotmap array for entities. Each slot map has its own mutex.
		HashMap_t m_archetypes; //Mapping hash (from type hashes) to archetype 1:1. 
		Mutex_t m_mutex; //mutex for reading and writing m_archetypes.
		inline static thread_local size_t m_slotMapIndex = NUMBER_SLOTMAPS::value - 1; //for new entities


		// Console Communication
	public:

		/// @brief Get LiveView data.
		/// @return Basic LiveView information as a JSON string.
		std::string GetLiveView() {
			std::string json = "{\"cmd\":\"liveview\",\"entities\":";
			json += std::to_string(Size());
			json += "}";
			return json;
		}

		/// @brief Return the average number of components over the Registry's entities.
		/// @return The average number of components.
		float GetAvgComp() {
			float avgComp = 0.f;
			for (auto& arch : m_archetypes) {
				avgComp += arch.second->GetComponents();
			}
			return (Size()) ? (avgComp / Size()) : Size();
		}

		/// @brief Get the estimated size of all entities.
		/// @return The estimated number of bytes, not including private data allocated by the entities or archetype management overhead.
		size_t GetEstSize() {
			size_t estSize = 0;
			for (auto& arch : m_archetypes) {
				estSize += arch.second->GetEstSize();
			}
			return estSize;
		}

		/// @brief Get an entity's contents in JSON format.
		/// @param h Handle of the entity.
		/// @return The entity's JSON representation.
		std::string ToJSON(Handle h) {
			if (!h.IsValid() || !Exists(h)) { return "null"; }
			auto& archAndIndex = GetArchetypeAndIndex(h);
			return archAndIndex.m_arch->ToJSON(archAndIndex.m_index);
		}

		/// @brief Return a complete snapshot of the current registry contents.
		/// @return The registry's JSON representation.
		std::string GetSnapshot() {
			std::string json = "{\"cmd\":\"snapshot\",\"entities\":";
			GetMutex().lock();
			json += std::to_string(Size()) + ",";
			json += "\"archetypes\":[";
			size_t art{ 0 };
			for (auto& it : m_archetypes) {
				if (art) json += ",";
				json += "{\"hash\":\"" + std::to_string(it.first) + "\"" +
					"," + it.second->ToJSON() + "}";
				art++;
			}
			GetMutex().unlock();
			json += "]";
			json += "}";
			return json;
		}
	};

	template<typename T>
	using Ref = Registry::Ref<T>;

} //end of namespace vecs




