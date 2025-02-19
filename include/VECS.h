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
#include "VECSSlotMap.h"
#include "VECSMutex.h"
#include "VECSHandle.h"

using namespace std::chrono_literals;


namespace vecs {


	//----------------------------------------------------------------------------------------------
	//Registry concepts and types

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

		template<VecsPOD T>
		struct SlotMapAndMutex {
			SlotMap<T> m_slotMap;
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

		/// @brief Used for iterating over entity components. Iterators are created by a view. 
		/// When iterating over an archetype, the archetype is locked in shared mode to prevent changes.
		/// The iterator unlocks the archetype when it changes to another archetype or when it is destroyed.
		/// Because of the shared lock, the iterator can be used in parallel for different archetypes, but 
		/// there are opertions that must be delayed until the shared lock is freed.
		/// @tparam ...Ts Choose the types of the components you want the entities to have.
		template<typename... Ts>
			requires VecsIterator<Ts...>
		class Iterator {

		public:
			/// @brief Iterator constructor saving a list of archetypes and the current index.
			/// @param arch List of archetypes. 
			/// @param archidx First archetype index.
			Iterator( Registry<RTYPE>& system, std::vector<ArchetypeAndSize>& arch, size_t archidx) 
				: m_system(system), m_archetypes{arch}, m_archidx{archidx} {
					
				system.increaseIterators(); //Tell the system that an iterator is created
				if( m_archetypes.size() > 0 ) { 
					m_size = std::min(m_archetypes.front().m_size, m_archetypes.front().m_archetype->Size());
				}
				Next(); //go to the first entity
			}

			/// @brief Copy constructor.
			Iterator(const Iterator& other) : m_system{other.m_system}, m_archetypes{other.m_archetypes}, 
				m_archidx{other.m_archidx}, m_size{other.m_size}, m_entidx{other.m_entidx}, m_isLocked{false} {
				m_system.increaseIterators();
			}

			/// @brief Destructor, unlocks the archetype.
			~Iterator() {
				UnlockShared();
				if( m_system.decreaseIterators() == 0 ) m_system.RunDelayedTransactions();
			}

			/// @brief Prefix increment operator.
			auto operator++() {
				++m_entidx;
				Next();				
				return *this;
			}

			/// @brief Access the content the iterator points to.
			auto operator*() {
				assert( m_archetype && m_entidx < m_size );
				Handle handle = (*m_mapHandle)[m_entidx];
				auto tup = std::make_tuple( Get<Ts>(m_system, handle, m_archetype)... );
				if constexpr (sizeof...(Ts) == 1) { return std::get<0>(tup); }
				else return tup;
			}

			/// @brief Compare two iterators.
			auto operator!=(const Iterator& other) -> bool {
				return (m_archidx != other.m_archidx) || (m_entidx != other.m_entidx);
			}

		private:

			/// @brief Get a component value from the archetype.
			template<typename U>
				requires (!std::is_same_v<U, Handle&>) // && !std::is_reference_v<U>)
			[[nodiscard]] auto Get(auto &system, Handle handle, Archetype<RTYPE>* arch) {
				using T = std::decay_t<U>;
				return (*arch->template Map<T>())[m_entidx];
			}

			/// @brief Go to the next entity. If this means changing the archetype, unlock the current archetype and lock the new one.
			void Next() {
				while( m_archidx < m_archetypes.size() && m_entidx >= std::min(m_archetypes[m_archidx].m_size, m_archetypes[m_archidx].m_archetype->Size()) ) {
					if( m_isLocked ) UnlockShared();
					++m_archidx;
					m_entidx = 0;
				} 
				if( m_archidx < m_archetypes.size() ) {
					m_archetype = m_archetypes[m_archidx].m_archetype;
					m_size = std::min(m_archetypes[m_archidx].m_size, m_archetypes[m_archidx].m_archetype->Size());
					m_mapHandle = m_archetype->template Map<Handle>();
					LockShared();
				} else {
					m_archetype = nullptr;
				}
			}

			/// @brief Lock the archetype in shared mode.
			void LockShared() {
				if constexpr (RTYPE == REGISTRYTYPE_PARALLEL) {
					if( !m_archetype || m_isLocked ) return;
					m_archetype->GetMutex().lock_shared();
					m_system.m_currentArchetype = m_archetype;
					m_isLocked = true;
				}
			}

			/// @brief Unlock the archetype.
			void UnlockShared() {
				if constexpr (RTYPE == REGISTRYTYPE_PARALLEL) {
					if( !m_archetype || !m_isLocked ) return;
					m_archetype->GetMutex().unlock_shared();
					m_system.m_currentArchetype = nullptr;
					m_isLocked = false;
				}
			}

			Registry<RTYPE>& 		m_system; ///< Reference to the registry system.
			Archetype<RTYPE>*		 		m_archetype{nullptr}; ///< Pointer to the current archetype.
			Vector<Handle>*	m_mapHandle{nullptr}; ///< Pointer to the comp map holding the handle of the current archetype.
			std::vector<ArchetypeAndSize>& m_archetypes; ///< List of archetypes.
			size_t 	m_archidx{0};	///< Index of the current archetype.
			size_t 	m_size{0};		///< Size of the current archetype.
			size_t 	m_entidx{0};		///< Index of the current entity.
			bool 	m_isLocked{false};	///< Flag if the archetype is locked.
		}; //end of Iterator


		//----------------------------------------------------------------------------------------------


		/// @brief A view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		template<typename... Ts>
			requires VecsView<Ts...>
		class View {

		public:
			template<typename U>
			View(Registry<RTYPE>& system, U&& tagsYes, U&& tagsNo ) : 
				m_system{system}, m_tagsYes{std::forward<U>(tagsYes)}, m_tagsNo{std::forward<U>(tagsNo)} {
			} ///< Constructor.

			/// @brief Get an iterator to the first entity. 
			/// The archetype is locked in shared mode to prevent changes. 
			/// @return Iterator to the first entity.
			auto begin() {
				m_archetypes.clear();
				auto types = std::vector<size_t>({Type<std::decay_t<Ts>>()...});
				auto hs = Hash(types);
				{
					LockGuardShared<RTYPE> lock(&m_system.GetMutex()); //lock the cache
					if(FindAndCopy(hs) ) return Iterator<Ts...>{m_system, m_archetypes, 0};
				}

				LockGuard<RTYPE> lock(&m_system.GetMutex()); //lock the cache
				if(FindAndCopy(hs) ) return Iterator<Ts...>{m_system, m_archetypes, 0};

				auto& archetypes = m_system.m_searchCacheMap[hs]; //create empty set
				assert(archetypes.size() == 0);
				for( auto& it : m_system.m_archetypes ) {
					auto arch = it.second.get();
					auto func = [&]<typename T>() {
						if( arch->Types().contains(Type<std::decay_t<T>>())) return true;
						return false;
					};
					if( (func.template operator()<Ts>() && ...) ) archetypes.insert(arch);
				}
				m_system.m_searchCacheSet.emplace_back(TypeSetAndHash{{}, hs});
				( m_system.m_searchCacheSet.back().m_types.insert(Type<std::decay_t<Ts>>()), ... );
				FindAndCopy(hs);
				return Iterator<Ts...>{m_system, m_archetypes, 0};
			}

			/// @brief Get an iterator to the end of the view.
			auto end() {
				return Iterator<Ts...>{m_system, m_archetypes, m_archetypes.size()};
			}

		private:

			/// @brief Find archetypes with the same components in the search cache and copy them to the list.
			/// @param hs Hash of the types of the components.
			/// @return true if the archetypes were found in the cache, else false.
			inline auto FindAndCopy2(size_t hs) -> bool {
				if( m_system.m_searchCacheMap.contains(hs) ) {
					for( auto& arch : m_system.m_searchCacheMap[hs] ) {
						m_archetypes.emplace_back( arch, arch->Size() );
					}
					return true;
				}
				return false;
			}

			inline auto FindAndCopy(size_t hs) -> bool {
				for( auto& arch : m_system.m_archetypes ) {
					bool found = (arch.second->Types().contains(Type<std::decay_t<Ts>>()) && ...) && 
		 						 (std::ranges::all_of( m_tagsYes, [&](size_t ti){ return arch.second->Types().contains(ti); } )) &&
								 (std::ranges::none_of( m_tagsNo, [&](size_t ti){ return arch.second->Types().contains(ti); } ) );
					if(found) m_archetypes.emplace_back( arch.second.get(), arch.second->Size() );
				}
				return true;
			}

			Registry<RTYPE>& 				m_system;	///< Reference to the registry system.
			std::vector<size_t> 			m_tagsYes;	///< List of tags that must be present.
			std::vector<size_t> 			m_tagsNo;	///< List of tags that must not be present.
			std::vector<ArchetypeAndSize>  	m_archetypes;	///< List of archetypes.
		}; //end of View

		//----------------------------------------------------------------------------------------------


		template< template<typename...> typename Y, typename... Ts, template<typename...> typename N,  typename... Us >
			requires (VecsView<Ts...>)
		class View<Y<Ts...>, N<Us...>> {

		public:
			template<typename U>
			View(Registry<RTYPE>& system, U&& tagsYes, U&& tagsNo ) : 
				m_system{system}, m_tagsYes{std::forward<U>(tagsYes)}, m_tagsNo{std::forward<U>(tagsNo)} {} ///< Constructor.

			/// @brief Get an iterator to the first entity. 
			/// The archetype is locked in shared mode to prevent changes. 
			/// @return Iterator to the first entity.
			auto begin() {
				m_archetypes.clear();
				auto types = std::vector<size_t>({ Type<Yes<std::decay_t<Ts>...>>(), Type<No<std::decay_t<Us>...>>() } );
				auto hs = Hash(types);
				{
					LockGuardShared<RTYPE> lock(&m_system.GetMutex()); //lock the cache
					if(FindAndCopy(hs) ) return Iterator<Ts...>{m_system, m_archetypes, 0};
				}

				LockGuard<RTYPE> lock(&m_system.GetMutex()); //lock the cache
				if(FindAndCopy(hs) ) return Iterator<Ts...>{m_system, m_archetypes, 0};

				auto& archetypes = m_system.m_searchCacheMap[hs]; //create empty set
				assert(archetypes.size() == 0);
				
				for( auto& it : m_system.m_archetypes ) {

					auto arch = it.second.get();
					bool found = false;
					auto func = [&]<typename T>() {
						if( arch->Types().contains(Type<std::decay_t<T>>())) return true;
						return false;
					};

					found = (func.template operator()<Ts>() && ...) && (!func.template operator()<Us>() && ...);
					if(found) archetypes.insert(arch);
				}
				m_system.m_searchCacheSet.emplace_back(TypeSetAndHash{{}, hs});
				( m_system.m_searchCacheSet.back().m_types.insert(Type<std::decay_t<Ts>>()), ... );
				FindAndCopy(hs);
				return Iterator<Ts...>{m_system, m_archetypes, 0};
			}

			/// @brief Get an iterator to the end of the view.
			auto end() {
				return Iterator<Ts...>{m_system, m_archetypes, m_archetypes.size()};
			}

		private:

			/// @brief Find archetypes with the same components in the search cache and copy them to the list.
			/// @param hs Hash of the types of the components.
			/// @return true if the archetypes were found in the cache, else false.
			inline auto FindAndCopy2(size_t hs) -> bool {
				if( m_system.m_searchCacheMap.contains(hs) ) {
					for( auto& arch : m_system.m_searchCacheMap[hs] ) {
						m_archetypes.emplace_back( arch, arch->Size() );
					}
					return true;
				}
				return false;
			}

			inline auto FindAndCopy(size_t hs) -> bool {
				for( auto& arch : m_system.m_archetypes ) {
					bool found = (arch.second->Types().contains(Type<std::decay_t<Ts>>()) && ...) && 
								 (!arch.second->Types().contains(Type<std::decay_t<Us>>()) && ...) &&
		 						 (std::ranges::all_of( m_tagsYes, [&](size_t ti){ return arch.second->Types().contains(ti); } )) &&
								 (std::ranges::none_of( m_tagsNo, [&](size_t ti){ return arch.second->Types().contains(ti); } ) );

					if(found) m_archetypes.emplace_back( arch.second.get(), arch.second->Size() );
				}
				return true;
			}

			Registry<RTYPE>& 				m_system;	///< Reference to the registry system.
			std::vector<size_t> 			m_tagsYes;	///< List of tags that must be present.
			std::vector<size_t> 			m_tagsNo;	///< List of tags that must not be present.
			std::vector<ArchetypeAndSize>  	m_archetypes;	///< List of archetypes.

		};


		//----------------------------------------------------------------------------------------------

		Registry() { ///< Constructor.
			//m_entities.reserve(NUMBER_SLOTMAPS::value); //resize the slot storage
			for( uint32_t i = 0; i < NUMBER_SLOTMAPS::value; ++i ) {
				m_entities.emplace_back( SlotMapAndMutex<typename Archetype<RTYPE>::ArchetypeAndIndex>{ i, 6  } ); //add the first slot
			}
		};

		~Registry() = default;	///< Destructor.

		/// @brief Get the number of entities in the system.
		/// @return The number of entities.
		size_t Size() {
			size_t size = 0;
			for( auto& slot : m_entities ) {
				size += slot.m_slotMap.Size();
			}
			return size;
		}

		/// @brief Create an entity with components.
		/// @tparam ...Ts The types of the components.
		/// @param ...component The new values.
		/// @return Handle of new entity.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		[[nodiscard]] auto Insert( Ts&&... component ) -> Handle {
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype

			size_t slotMapIndex = GetSlotmapIndex();
			LockGuard<RTYPE> lock(&GetMutex(slotMapIndex)); //lock the mutex
			auto [handle, slot] = m_entities[slotMapIndex].m_slotMap.Insert( {nullptr, 0} ); //get a slot for the entity

			size_t archIndex;
			std::vector<size_t> types = {Type<Handle>(), Type<Ts>()...};
			size_t hs = Hash(types);
			if( !m_archetypes.contains( hs ) ) { //not found
				auto arch = std::make_unique<Archetype<RTYPE>>( handle, archIndex, std::forward<Ts>(component)... );
				UpdateSearchCache(arch.get());
				slot.m_value = { arch.get(), archIndex };
				m_archetypes[hs] = std::move(arch);
			} else {
				auto arch = m_archetypes[hs].get();
				LockGuard<RTYPE> lock(&arch->GetMutex()); //lock the mutex
				slot.m_value = { arch, arch->Insert( handle, std::forward<Ts>(component)... ) };
			}
			return handle;
		}

		/// @brief Test if an entity exists.
		/// @param handle The handle of the entity.
		/// @return true if the entity exists, else false.
		bool Exists(Handle handle) {
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			auto& slot = m_entities[handle.GetStorageIndex()].m_slotMap[handle];
			return slot.m_version == handle.GetVersion();
		}

		/// @brief Test if an entity has a component.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return true if the entity has the component, else false.
		template<typename T>
		bool Has(Handle handle) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the system
			auto arch = m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value.m_archetypePtr;
			return arch->Has(Type<T>());
		}

		/// @brief Test if an entity has a tag.
		/// @param handle The handle of the entity.
		/// @param tag The tag to test.
		/// @return true if the entity has the tag, else false.
		bool Has(Handle handle, size_t ti) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the system
			auto arch = m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value.m_archetypePtr;
			return arch->Has(ti);
		}

		/// @brief Get the types of the components of an entity.
		/// @param handle The handle of the entity.
		/// @return A vector of type indices of the components.
		const auto& Types(Handle handle) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the system
			auto arch = m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value.m_archetypePtr;
			return arch->Types();
		}

		/// @brief Get a component value of an entity.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return The component value or reference to it.
		template<typename T>
		[[nodiscard]] auto Get(Handle handle) -> T {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			return Get2<T>(handle);
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires (sizeof...(Ts)>1 && vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle&>::value)
		[[nodiscard]] auto Get(Handle handle) -> std::tuple<Ts...> {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
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
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
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
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			Put2(handle, std::forward<Ts>(std::get<Ts>(v))...);
		}

		/// @brief Put new component values to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 1) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
		void Put(Handle handle, Ts&&... vs) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			Put2(handle, std::forward<Ts>(vs)...);
		}

		/// @brief Add tags to an entity.
		template<typename... Ts>
			requires (sizeof...(Ts) > 0 && (std::is_convertible_v<Ts, size_t> && ...) )
		void AddTags(Handle handle, Ts... tags) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			AddTags2(handle, tags...);
		}

		/// @brief Add tags to an entity.
		template<typename... Ts>
			requires (sizeof...(Ts) > 0 && (std::is_convertible_v<Ts, size_t> && ...) )
		void EraseTags(Handle handle, Ts... tags) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			EraseTags2(handle, tags...);
		}
		
		/// @brief Erase components from an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.		
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		void Erase(Handle handle) {
			assert( (Has<Ts>(handle) && ...) );
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype

			LockGuard<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			auto value = &m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value;
			auto oldArch = value->m_archetypePtr;
			std::set<size_t> types = oldArch->Types();
			(types.erase(Type<Ts>()), ... );
			size_t hs = Hash(types);
			Archetype<RTYPE> *arch;
			if( !m_archetypes.contains(hs) ) {
				auto archPtr = std::make_unique<Archetype<RTYPE>>();
				arch = archPtr.get();
				arch->Clone(*oldArch, types);
				UpdateSearchCache(arch);
				m_archetypes[hs] = std::move(archPtr);
			} else { arch = m_archetypes[hs].get(); }
			LockGuard<RTYPE> lock2(&arch->GetMutex(), &oldArch->GetMutex());
			value->m_archetypePtr = arch;
			value->m_archIndex = arch->Move(types, value->m_archIndex, *oldArch, m_entities);
		}

		/// @brief Erase an entity from the registry.
		/// @param handle The handle of the entity.
		void Erase(Handle handle) {
			assert(Exists(handle));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuard<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			auto& value = m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value;
			LockGuard<RTYPE> lock2(&value.m_archetypePtr->GetMutex()); //lock the archetype
			value.m_archetypePtr->Erase(value.m_archIndex, m_entities); //erase the entity from the archetype (do both locked)
			m_entities[handle.GetStorageIndex()].m_slotMap.Erase(handle); //erase the entity from the entity list
		}

		/// @brief Clear the registry by removing all entities.
		void Clear() {
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype

			for( auto& arch : m_archetypes ) {
				LockGuard<RTYPE> lock(&arch.second->GetMutex()); //lock the mutex
				arch.second->Clear();
			}
			for( auto& slotmap : m_entities ) {
				LockGuard<RTYPE> lock(&slotmap.m_mutex); //lock the mutex
				slotmap.m_slotMap.Clear();
			}
			LockGuard<RTYPE> lock(&GetMutex()); //lock the mutex
			m_searchCacheMap.clear();
			m_searchCacheSet.clear();
		}

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

		/// @brief Print the registry.
		/// Print the number of entities and the archetypes.
		void Print() {
			std::cout << "-----------------------------------------------------------------------------------------------" << std::endl;
			std::cout << "Entities: " << Size() << std::endl;
			for( auto& it : m_archetypes ) {
				std::cout << "Archetype Hash: " << it.first << std::endl;
				it.second->Print();
			}
			std::cout << "Cache Map " << m_searchCacheMap.size() << " Set: " << m_searchCacheSet.size() << std::endl;
			for( auto it : m_searchCacheMap ) {
				std::cout << "Hash: " << it.first << " Archetypes: " << it.second.size() << std::endl;
			}
			std::cout << std::endl << std::endl;
		}

		/// @brief Validate the registry.
		/// Make sure all archetypes have the same size in all component maps.
		void Validate() {
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			LockGuardShared<RTYPE> lock(&GetMutex());
			for( auto& it : m_archetypes ) {
				auto arch = it.second.get();
				LockGuardShared<RTYPE> lock(&arch->GetMutex());
				arch->Validate();
			}
		}

		/// @brief Increase the number of iterators.
		/// @return number of iterators.
		size_t increaseIterators() {
			return ++m_numIterators;
		}	

		/// @brief Decrease the number of iterators.
		/// @return number of iterators.
		size_t decreaseIterators() {
			assert(m_numIterators > 0);
			return --m_numIterators;
		}

		inline bool CheckUnlockArchetype() {
			if constexpr( RTYPE == REGISTRYTYPE_PARALLEL ) {
				if( m_currentArchetype ) {
					m_currentArchetype->GetMutex().unlock_shared();
					return true;
				}
			}
			return false;
		}

		inline bool CheckLockArchetype() {
			if constexpr( RTYPE == REGISTRYTYPE_PARALLEL ) {
				if( m_currentArchetype ) {
					m_currentArchetype->GetMutex().lock_shared();
					return true;
				}
			}
			return false;
		}

		/// @brief Delay a transaction until all iterators are destroyed.
		bool DelayTransaction( auto&& func ) {
			if constexpr( RTYPE == REGISTRYTYPE_PARALLEL ) {
				if( m_numIterators > 0 ) { 
					m_delayedTransactions.emplace_back(func); 
					return true; 
				}
			}
			func();
			return false;
		}

		/// @brief Run all delayed transactions. Since each thread has its own list of delayed transactions,
		/// this function must be called by each thread, and does not need synchronization.
		void RunDelayedTransactions() {
			for( auto& func : m_delayedTransactions ) {
				func();
			}
			m_delayedTransactions.clear();
		}

		/// @brief Get the mutex of the archetype.
		/// @return Reference to the mutex.
		[[nodiscard]] inline auto GetMutex(size_t index) -> mutex_t& {
			return m_entities[index].m_mutex;
		}

		/// @brief Get the mutex of the archetype.
		/// @return Reference to the mutex.
		[[nodiscard]] inline auto GetMutex() -> mutex_t& {
			return m_mutex; 
		}

		/// @brief Get the index of the entity in the archetype
		auto GetArchetypeIndex( Handle handle ) {
			assert(Exists(handle));
			LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the system
			return m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value.m_archIndex;
		}

		/// @brief Swap two entities.
		/// @param h1 The handle of the first entity.
		/// @param h2 The handle of the second entity.
		/// @return true if the entities were swapped, else false.
		bool Swap( Handle h1, Handle h2 ) {
			assert(Exists(h1) && Exists(h2));
			UnlockGuardShared<RTYPE> unlock(m_currentArchetype); //unlock the current archetype
			size_t index1 = h1.GetStorageIndex();
			size_t index2 = h2.GetStorageIndex();
			mutex_t* m1 = &GetMutex(index1);
			mutex_t* m2 = &GetMutex(index2);
			if( index1 == index2 ) { m2 = nullptr; } //same slotmap -> only lock once
			LockGuard<RTYPE> lock(m1, m2); //lock the slotmap(s)
			auto& slot1 = m_entities[index1].m_slotMap[h1];
			auto& slot2 = m_entities[index2].m_slotMap[h2];
			if( slot1.m_archetypePtr != slot2.m_archetypePtr ) return false;
			LockGuard<RTYPE> lock2(&slot1.m_archetypePtr->GetMutex()); //lock the archetype
			slot1.m_archetypePtr->Swap(slot1, slot2);
			return true;
		}

	private:

		/// @brief Get the index of the slotmap to use and the archetype.
		/// @param handle The handle of the entity.
		/// @return The index of the slotmap and the archetype.
		auto GetSlotAndArch(auto handle) {
			typename Archetype<RTYPE>::ArchetypeAndIndex* value = &m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value;
			return std::make_pair(value, value->m_archetypePtr);
		}

		/// @brief Clone an archetype with a subset of components.
		/// @param arch The archetype to clone.
		/// @param types The types of the components to clone.
		auto CloneArchetype(Archetype<RTYPE>* arch, const std::vector<size_t>& types) {
			auto newArch = std::make_unique<Archetype<RTYPE>>();
			newArch->Clone(*arch, types);
			UpdateSearchCache(newArch.get());
			m_archetypes[Hash(newArch->Types())] = std::move(newArch);
		}

		/// @brief Add tags to an entity.
		/// @tparam ...Ts The types of the tags must be size_t.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to add.
		template<typename... Ts>
		void AddTags2(Handle handle, Ts... tags) {
			LockGuard<RTYPE> lock1(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			auto [value, arch] = GetSlotAndArch(handle);
			assert(!arch->Types().contains(tags) && ...);
			std::vector<size_t> allTypes{ tags...};
			std::ranges::copy(arch->Types(), std::back_inserter(allTypes));
			size_t hs = Hash(allTypes);
			if( !m_archetypes.contains(hs) ) { CloneArchetype(arch, allTypes); } 
			auto newArch = m_archetypes[hs].get();
			LockGuard<RTYPE> lock2(&arch->GetMutex()); //lock the old archetype
			*value = { newArch, newArch->Move(arch->Types(), value->m_archIndex, *arch, m_entities) };
		}

		/// @brief Erase tags from an entity.
		/// @tparam ...Ts The types of the tags must be size_t.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to erase.
		template<typename... Ts>
		void EraseTags2(Handle handle, Ts... tags) {
			LockGuard<RTYPE> lock1(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			auto [value, arch] = GetSlotAndArch(handle);
			std::vector<size_t> delTypes{tags...};
			std::vector<size_t> allTypes;
			auto found = [&](size_t x) -> bool { return std::ranges::find_if( delTypes, [&](size_t y){ return x==y; } ) != delTypes.end(); };
			for( auto x : arch->Types() ) { if( !found(x) ) allTypes.push_back(x); }			
			size_t hs = Hash(allTypes);
			if( !m_archetypes.contains(hs) ) { CloneArchetype(arch, allTypes); } 
			auto newArch = m_archetypes[hs].get();
			LockGuard<RTYPE> lock2(&arch->GetMutex(), &newArch->GetMutex()); //lock the old archetype
			*value = { newArch, newArch->Move(allTypes, value->m_archIndex, *arch, m_entities) };
		}

		/// @brief Change the component values of an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param ...vs The new values.
		template<typename... Ts>
		void Put2(Handle handle, Ts&&... vs) {
			auto func = [&]<typename T>(Archetype<RTYPE>* arch, size_t archIndex, T&& v) { 
				(*arch->template Map<T>())[archIndex] = std::forward<T>(v);
			};

			std::vector<size_t> newTypes;
			{
				LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
				typename Archetype<RTYPE>::ArchetypeAndIndex* value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
				//value->m_archetypePtr->Print();	
				if(newTypes.empty()) {
					( func.template operator()<Ts>(value->m_archetypePtr, value->m_archIndex, std::forward<Ts>(vs)), ... );
					return;
				};
			}

			LockGuard<RTYPE> lock1(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			typename Archetype<RTYPE>::ArchetypeAndIndex* value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
			auto arch = value->m_archetypePtr;
			if(newTypes.empty()) {
				( func.template operator()<Ts>(arch, value->m_archIndex, std::forward<Ts>(vs)), ... );
				return;
			}
			auto newArch = CreateArchetype<std::decay_t<Ts>...>(handle, value, newTypes);
			
			value->m_archetypePtr = newArch;
			LockGuard<RTYPE> lock2(&arch->GetMutex()); //lock old archetype, new one cannot be seen yet
			value->m_archIndex = newArch->Move(arch->Types(), value->m_archIndex, *arch, m_entities); //move values
			( func.template operator()<Ts>(newArch, value->m_archIndex, std::forward<Ts>(vs)), ... );
			UpdateSearchCache(newArch); //update the search cache
			return;
		}
		
		/// @brief Get component values of an entity.
		/// @tparam T The type of the components.
		/// @param handle The handle of the entity.
		/// @return Component values.
		template<typename U>
		[[nodiscard]] auto Get2(Handle handle) -> U& {
			using T = std::decay_t<U>;
			std::vector<size_t> newTypes;
			{
				LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
				typename Archetype<RTYPE>::ArchetypeAndIndex* value = GetArchetype<T>(handle, newTypes);
				auto arch = value->m_archetypePtr;
				if(newTypes.empty()) { 
					decltype(auto) v = arch->template Get<T>(value->m_archIndex); 
					return v;
				}
			}

			LockGuard<RTYPE> lock1(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			typename Archetype<RTYPE>::ArchetypeAndIndex* value = GetArchetype<T>(handle, newTypes);
			auto arch = value->m_archetypePtr;
			if(newTypes.empty()) { 
				decltype(auto) v = arch->template Get<T>(value->m_archIndex); 
				return v;
			}
			auto newArch = CreateArchetype<T>(handle, value, newTypes);
			value->m_archetypePtr = newArch;
			LockGuard<RTYPE> lock2(&arch->GetMutex()); //lock old archetype, new one cannot be seen yet
			value->m_archIndex = newArch->Move(arch->Types(), value->m_archIndex, *arch, m_entities); //move values		
			UpdateSearchCache(newArch); //update the search cache
			decltype(auto) v = newArch->template Get<T>(value->m_archIndex);
			return v;
		}

		/// @brief Get component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
		template<typename... Ts>
			requires (sizeof...(Ts)>1 && vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle&>::value)
		[[nodiscard]] auto Get2(Handle handle) -> std::tuple<Ts...> {
			using T = vtll::Nth_type<vtll::tl<Ts...>, 0>;

			std::vector<size_t> newTypes;
			{
				LockGuardShared<RTYPE> lock(&GetMutex(handle.GetStorageIndex())); //lock the mutex
				typename Archetype<RTYPE>::ArchetypeAndIndex* value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
				auto arch = value->m_archetypePtr;
				//value->m_archetypePtr->Print();	
				if(newTypes.empty()) { return std::tuple<Ts...>{ arch->template Get<Ts>(value->m_archIndex)... };  }
			}

			LockGuard<RTYPE> lock1(&GetMutex(handle.GetStorageIndex())); //lock the mutex
			typename Archetype<RTYPE>::ArchetypeAndIndex* value = GetArchetype<std::decay_t<Ts>...>(handle, newTypes);
			auto arch = value->m_archetypePtr;
			if(newTypes.empty()) { return std::tuple<Ts...>{ arch->template Get<Ts>(value->m_archIndex)... } ; } 

			auto newArch = CreateArchetype<std::decay_t<Ts>...>(handle, value, newTypes);		
			value->m_archetypePtr = newArch;
			LockGuard<RTYPE> lock2(&arch->GetMutex()); //lock old archetype, new one cannot be seen yet
			value->m_archIndex = newArch->Move(arch->Types(), value->m_archIndex, *arch, m_entities); //move values
			return std::tuple<Ts...>{ newArch->template Get<Ts>(value->m_archIndex)... };
		}

		/// @brief Get the archetype/index of an entity and the types of the components that are currently not contained.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param newTypes A vector to store the types of the components that are not contained and should be created.
		/// @return Slotmap value of the entity containing pointer to the archetype and index in the archetype. 
		template<typename... Ts>
		inline auto GetArchetype(Handle handle, std::vector<size_t>& newTypes) -> Archetype<RTYPE>::ArchetypeAndIndex* {		
			newTypes.clear();
			newTypes.reserve(sizeof...(Ts));
			typename Archetype<RTYPE>::ArchetypeAndIndex* value = &m_entities[handle.GetStorageIndex()].m_slotMap[handle].m_value;
			
			auto func = [&]<typename T>(){ if(!value->m_archetypePtr->Has(Type<T>())) newTypes.emplace_back(Type<T>()); };
			( func.template operator()<std::decay_t<Ts>>(), ...  );
			return value;
		}

		/// @brief Create a new archetype with the types of the components that are not contained.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param value Pointer to the archetype/index of the entity.
		/// @param newTypes A vector with the types of the components that are not contained and shhould be created.
		/// @return Pointer to the new archetype.
		template<typename... Ts>
		inline auto CreateArchetype(Handle handle, Archetype<RTYPE>::ArchetypeAndIndex* value, std::vector<size_t>& newTypes) -> Archetype<RTYPE>* {
			auto arch = value->m_archetypePtr;
			std::vector<size_t> allTypes; //{newTypes};
			std::ranges::copy( newTypes, std::back_inserter(allTypes) );
			std::ranges::copy( arch->Types(), std::back_inserter(allTypes) );

			size_t hs = Hash(allTypes);
			Archetype<RTYPE> *newArch=nullptr;
			if( !m_archetypes.contains(hs) ) {
				auto newArchUnique = std::make_unique<Archetype<RTYPE>>();
				newArch = newArchUnique.get();
				newArch->Clone(*arch, arch->Types());
				
				auto func = [&]<typename T>(){ 
					if(!arch->Has(Type<T>())) { //need the types, type indices are not enough
						newArch->template AddComponent<T>();
						newArch->AddValue(T{}); 
					}
				};
				( func.template operator()<std::decay_t<Ts>>(), ...  );
				UpdateSearchCache(newArch);

				m_archetypes[hs] = std::move(newArchUnique);
			} else { 
				newArch = m_archetypes[hs].get(); 
				for( auto ti : newTypes ) { newArch->AddEmptyValue(ti); }
			}

			return newArch;
		}

		/// @brief Update the search cache with a new archetype. m_searchCacheSet contains the types of 
		/// the components that have been searched for.	FInd those previous search results that include 
		/// all components that are searched for. Add them to the set of archetypes that yielded a result
		/// for this particular search.
		/// @param arch Pointer to the new archetype.
		void UpdateSearchCache(Archetype<RTYPE>* arch) {
			LockGuard<RTYPE> lock(&arch->GetMutex()); //lock the cache
			auto types = arch->Types();
			for( auto& ts : m_searchCacheSet ) {
				if( std::includes(types.begin(), types.end(), ts.m_types.begin(), ts.m_types.end()) ) {
					m_searchCacheMap[ts.m_hash].insert(arch);
				}
			}
		}

		/// @brief Get a new index of the slotmap for the current thread.
		/// @return New index of the slotmap.
		size_t GetSlotmapIndex() {
			m_slotMapIndex = (m_slotMapIndex + 1) & (NUMBER_SLOTMAPS::value - 1);
			return m_slotMapIndex;
		}

		using SlotMaps_t = std::vector<SlotMapAndMutex<typename Archetype<RTYPE>::ArchetypeAndIndex>>;
		using HashMap_t = HashMap<std::unique_ptr<Archetype<RTYPE>>>;
		using SearchCacheMap_t = std::unordered_map<size_t, std::set<Archetype<RTYPE>*>>;
		using SearchCacheSet_t = std::vector<TypeSetAndHash>;

		inline static thread_local size_t m_slotMapIndex = NUMBER_SLOTMAPS::value - 1;

		SlotMaps_t m_entities;
		HashMap_t m_archetypes; //Mapping vector of type set hash to archetype 1:1. Internally synchronized!

		mutex_t m_mutex; //mutex for reading and writing search cache. Not needed for HashMaps.
		SearchCacheMap_t m_searchCacheMap; //Mapping vector of hash to archetype, 1:N
		SearchCacheSet_t m_searchCacheSet; //These type combinations have been searched for, for updating
		inline static thread_local size_t 		m_numIterators{0}; //thread local variable for locking
		inline static thread_local Archetype<RTYPE>* 	m_currentArchetype{nullptr}; //is there an iterator now
		inline static thread_local std::vector<std::function<void()>> m_delayedTransactions; //thread local variable for locking
	};

} //end of namespace vecs



