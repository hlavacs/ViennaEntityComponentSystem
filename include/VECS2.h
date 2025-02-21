#pragma once

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <variant>
#include <array>
#include <deque>
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
#include <thread>
#include <VTLL.h>
#include <VSTY.h>


#include "VECSVector.h"
#include "VECSHashMap.h"
#include "VECSArchetype.h"
#include "VECSArchetype2.h"
#include "VECSSlotMap.h"
#include "VECSMutex.h"
#include "VECSHandle.h"

using namespace std::chrono_literals;



namespace vecs2 {


	//----------------------------------------------------------------------------------------------
	//Registry concepts and types

	using Handle = vecs::Handle;
	using mutex_t = vecs::mutex_t;

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

	const int REGISTRYTYPE_SEQUENTIAL = 0;
	const int REGISTRYTYPE_PARALLEL = 1;

	template<int T>
	concept RegistryType = (T == REGISTRYTYPE_SEQUENTIAL || T == REGISTRYTYPE_PARALLEL);

	/// @brief A registry for entities and components.
	template <int RTYPE>
		requires RegistryType<RTYPE>
	class Registry {

		/// @brief Entry for the seach cache
		struct TypeSetAndHash {
			std::set<size_t> m_types;	//set of types that have been searched for
			size_t m_hash;				//hash of the set
		};

		template<vecs::VecsPOD T>
		struct SlotMapAndMutex {
			vecs::SlotMap<T> m_slotMap;
			mutex_t m_mutex;
			SlotMapAndMutex( uint32_t storageIndex, uint32_t bits ) : m_slotMap{storageIndex, bits}, m_mutex{} {};
			SlotMapAndMutex( const SlotMapAndMutex& other ) : m_slotMap{other.m_slotMap}, m_mutex{} {};
		};

		using NUMBER_SLOTMAPS = std::conditional_t< RTYPE == REGISTRYTYPE_SEQUENTIAL, 
			std::integral_constant<int, 1>, std::integral_constant<int, 16>>;
	

	public:	
		
		//----------------------------------------------------------------------------------------------

		/// @brief A structure holding a pointer to an archetype and the current size of the archetype.
		struct ArchetypeAndSize {
			Archetype<RTYPE>* 	m_archetype;	//pointer to the archetype
			size_t 		m_size;			//size of the archetype
			ArchetypeAndSize(Archetype<RTYPE>* arch, size_t size) : m_archetype{arch}, m_size{size} {}
		};


		//----------------------------------------------------------------------------------------------

		Registry() { 
			m_slotMaps.reserve(NUMBER_SLOTMAPS::value); //resize the slot storage
			for( uint32_t i = 0; i < NUMBER_SLOTMAPS::value; ++i ) {
				m_slotMaps.emplace_back( SlotMapAndMutex<typename Archetype<RTYPE>::ArchetypeAndIndex>{ i, 6  } ); 
			}
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
			return Insert2<Ts...>(std::forward<Ts>(component)...);
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
		auto Get(Handle handle) -> T {
			return Get2<T>(handle);
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires (sizeof...(Ts)>1 && vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle&>::value)
		[[nodiscard]] auto Get(Handle handle) -> std::tuple<Ts...> {
			return Get2<Ts...>(handle);
		}

		/// @brief Put a new component value to an entity. If the entity does not have the component, it will be created.
		/// This might result in moving the entity to another archetype.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @param v The new value.
		template<typename U>
			requires (!is_tuple<U>::value && !std::is_same_v<std::decay_t<U>, Handle>)
		void Put(Handle handle, U&& v) {
			using T = std::decay_t<U>;
			Put2(handle, std::forward<T>(v));
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
			requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
		void Put(Handle handle, Ts&&... vs) {
			Put2(handle, std::forward<Ts>(vs)...);
		}

		/// @brief Add tags to an entity.
		void AddTags(Handle handle, const std::vector<size_t>&& tags) {
			AddTags2(handle, std::forward<decltype(tags)>(tags));
		}

		/// @brief Add tags to an entity.
		void EraseTags(Handle handle, const std::vector<size_t>&& tags) {
			EraseTags2(handle, std::forward<decltype(tags)>(tags));
		}
		
		/// @brief Erase components from an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.		
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Erase(Handle handle) {
			Erase2<Ts...>(handle);
		}

		/// @brief Erase an entity from the registry.
		/// @param handle The handle of the entity.
		void Erase(Handle handle) {
			Erase2(handle);
		}

		/// @brief Clear the registry by removing all entities.
		void Clear() {
			for( auto& arch : m_archetypes ) { arch.second->Clear(); }
			for( auto& slotmap : m_slotMaps ) { slotmap.m_slotMap.Clear(); }
			m_size = 0;
		}

		/*
		/// @brief Get a view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		/// @return A view of the entity components
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto GetView() -> View<Ts...> {
			return {*this, std::vector<size_t>{}, std::vector<size_t>{}};
		}

		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto GetView(std::vector<size_t>&& yes) -> View<Ts...> {
			return {*this, std::forward<std::vector<size_t>>(yes), std::vector<size_t>{}};
		}

		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto GetView(std::vector<size_t>&& yes, std::vector<size_t>&& no) -> View<Ts...> {
			return {*this, std::forward<std::vector<size_t>>(yes), std::forward<std::vector<size_t>>(no),};
		}
		*/

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
		[[nodiscard]] inline auto GetSlotMapMutex(size_t index) -> mutex_t& {
			return m_slotMaps[index].m_mutex;
		}

		/// @brief Get the mutex of the archetype.
		/// @return Reference to the mutex.
		[[nodiscard]] inline auto GetMutex() -> mutex_t& {
			return m_mutex; 
		}

		/// @brief Swap two entities.
		/// @param h1 The handle of the first entity.
		/// @param h2 The handle of the second entity.
		/// @return true if the entities were swapped, else false.
		bool Swap( Handle h1, Handle h2 ) {
			return true;
		}

	private:

		bool ContainsType(auto&& container, size_t hs) {
			return std::ranges::find(container, hs) != container.end();
		}

		void AddType(auto&& container, size_t hs) {
			if(!ContainsType(container, hs)) container.push_back(hs);
		}

		/// @brief Get the index of the entity in the archetype
		auto& GetSlot( Handle handle ) {
			return m_slotMaps[handle.GetStorageIndex()].m_slotMap[handle];
		}

		/// @brief Get the index of the entity in the archetype
		inline auto& GetArchetypeAndIndex( Handle handle ) {
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
		auto CreateTypeList(Archetype<RTYPE>* arch, const std::vector<size_t>&& tags) -> std::vector<size_t> {
			std::vector<size_t> all{ tags.begin(), tags.end() };
			(AddType(all, Type<Ts>()), ...);
			if(arch) { for( auto type : arch->Types() ) { AddType(all, type); } }
			return all;
		}

		/// @brief Get an archetype with components.
		/// @tparam ...Ts The component types.
		/// @param arch Use the types of this archetype.
		/// @param tags Should have the tags of the entity.
		/// @return A pointer to the archetype.
		template<typename... Ts>
		inline auto GetArchetype(Archetype<RTYPE>* arch, const std::vector<size_t>&& tags) -> Archetype<RTYPE>* {
			size_t hs = Hash(CreateTypeList<Ts...>(arch, std::forward<decltype(tags)>(tags)));
			if( m_archetypes.contains( hs ) ) { return m_archetypes[hs].get(); }

			auto newArchUnique = std::make_unique<Archetype<RTYPE>>();
			auto newArch = newArchUnique.get();
			if(arch) newArch->Clone(*arch, {}); //clone old types/components and old tags
			(newArch->template AddComponent<Ts>(), ...); //add new components
			for( auto tag : tags ) { newArch->AddType(tag); } //add new tags
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

		void Move(Archetype<RTYPE>* newArch, Archetype<RTYPE>* oldArch, vecs2::Archetype<RTYPE>::ArchetypeAndIndex& archAndIndex) {
			auto [newIndex, movedHandle] = newArch->Move(*oldArch, archAndIndex.m_index);
			ReindexMovedEntity(movedHandle, archAndIndex.m_index);
			archAndIndex = { newArch, newIndex };
		}

		/// @brief Insert a new entity with components to the registry.
		/// @tparam ...Ts Value types of the components.
		/// @param ...component The new values.
		/// @return Handle of new entity.
		template<typename... Ts>
		[[nodiscard]] auto Insert2( Ts&&... component ) -> Handle {
			size_t slotMapIndex = GetNewSlotmapIndex();
			auto [handle, slot] = m_slotMaps[slotMapIndex].m_slotMap.Insert( {nullptr, 0} ); //get a slot for the entity
			slot.m_value.m_arch = GetArchetype<Ts...>(nullptr, {});
			slot.m_value.m_index = slot.m_value.m_arch->Insert( handle, std::forward<Ts>(component)... ); //insert the entity into the archetype
			++m_size;
			return handle;
		}

		/// @brief Get component values of an entity.
		/// @tparam T The type of the components.
		/// @param handle The handle of the entity.
		/// @return Component values.
		template<typename T>
		[[nodiscard]] auto Get2(Handle handle) -> T& {
			auto archAndIndex = GetArchetypeAndIndex(handle);
			auto arch = archAndIndex.m_arch;
			if( arch->Has(Type<T>()) ) { return arch->template Get<T>(archAndIndex.m_index); }
			auto newArch = GetArchetype<T>(arch, {});
			Move(newArch, arch, archAndIndex);
			return newArch->template Get<T>(archAndIndex.m_index);
		}

		template<typename... Ts>
		requires (sizeof...(Ts)>1 && vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle&>::value)
		[[nodiscard]] auto Get2(Handle handle) -> std::tuple<Ts...> {
			auto archAndIndex = GetArchetypeAndIndex(handle);
			auto arch = archAndIndex.m_arch;
			if( (arch->Has(Type<Ts>()) && ...) ) { return std::tuple<Ts...>{ arch->template Get<Ts>(archAndIndex.m_index)... }; }
			auto newArch = GetArchetype<Ts...>(arch, {});
			Move(newArch, arch, archAndIndex);
			return std::tuple<Ts...>{ newArch->template Get<Ts>(archAndIndex.m_index)... };
		}

		/// @brief Change the component values of an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
		void Put2(Handle handle, Ts&&... vs) {
			auto archAndIndex = GetArchetypeAndIndex(handle);
			auto arch = archAndIndex.m_arch;
			if( (arch->Has(Type<Ts>()) && ...) ) { arch->Put(archAndIndex.m_index, std::forward<Ts>(vs)...); return; }
			auto newArch = GetArchetype<Ts...>(arch, {});
			Move(newArch, arch, archAndIndex);
			newArch->Put(archAndIndex.m_index, std::forward<Ts>(vs)...);
		}
		
		/// @brief Add tags to an entity.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to add.
		void AddTags2(Handle handle, const std::vector<size_t>&& tags) {
			auto archAndIndex = GetArchetypeAndIndex(handle);
			auto oldArch = archAndIndex.m_arch;
			auto newArch = GetArchetype(oldArch, tags);
			Move(newArch, oldArch, archAndIndex);
		} 

		/// @brief Erase tags from an entity.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to erase.
		void EraseTags2(Handle handle, const std::vector<size_t>&& tags) {
			
		}


		/// @brief Erase components from an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		template<typename... Ts>
		void Erase2(Handle handle) {
		}

		/// @brief Erase an entity from the registry.
		/// @param handle The handle of the entity.
		void Erase2(Handle handle) {
		}

		using SlotMaps_t = std::vector<SlotMapAndMutex<typename Archetype<RTYPE>::ArchetypeAndIndex>>;
		using HashMap_t = std::map<size_t, std::unique_ptr<Archetype<RTYPE>>>; //change to std::map
		using Size_t = std::conditional_t<RTYPE == REGISTRYTYPE_SEQUENTIAL, std::size_t, std::atomic<std::size_t>>;

		Size_t m_size{0}; //number of entities
		SlotMaps_t m_slotMaps; //Slotmaps for entities. Internally synchronized!
		HashMap_t m_archetypes; //Mapping vector of type set hash to archetype 1:1. 
		mutex_t m_mutex; //mutex for reading and writing search cache. Not needed for HashMaps.
		inline static thread_local size_t m_slotMapIndex = NUMBER_SLOTMAPS::value - 1;
	};

} //end of namespace vecs



