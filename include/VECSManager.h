#pragma once
#define REGISTRYTYPE_PARALLEL

#include <VECS.h>
#include <VECSRegistry.h>
#include <VECSThreadPool.h>
#include <thread>
#include <future>
#include <queue>
#include <syncstream>

namespace vecs {

    //----------------------------------------------------------------------------------------------
	//Manager

    /// @brief A manager for handling registries and parallel access
    class Manager {
    public:

    ////////// CONSTRUCTORS //////////
        Manager() {
            m_threadpool = std::make_shared<vecs::ThreadPool>();
            m_system = std::make_shared<vecs::Registry>();
        }

        Manager(std::shared_ptr<IThreadPool> threadpool, std::shared_ptr<vecs::Registry> registry) {
            m_threadpool = std::move(threadpool);
            m_system = std::move(registry);
        }

        ~Manager() = default;  // Destructor



    ////////// CONVENIENCE THREADPOOL //////////
        void waitIdle() {
            m_threadpool->waitForIdle();
        }



    ////////// ACCESSING VIEWS/ENTITIES //////////
		/// @brief Get a view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		/// @return A view of the entity components
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto GetView(std::vector<size_t>&& yes={}, std::vector<size_t>&& no={}) -> Registry::View<Ts...> {
            std::shared_lock lock(m_system->GetMutex());
            auto view = m_system->GetView<Ts...>(std::forward<std::vector<size_t>>(yes), std::forward<std::vector<size_t>>(no));
            return view;
		}


        /// @brief Apply a function on specific components of entities.
        /// @tparam ...Ts The types of the components.
        template<typename... Ts, typename Func>
          requires(vtll::unique<vtll::tl<Ts...>>::value)
        void ForEachView(Func&& func, std::vector<size_t>&& yes = {}, std::vector<size_t>&& no = {}) {
          // Get the view
          Registry::View<Ts...> view = GetView<Ts...>(std::move(yes), std::move(no));

          // m_archetypes gets initialized whenever an iterator is created for View
            view.begin();

          // Loop over archetypes and split workload among threads
          for (auto& archs : view.GetArchetypes()) {
            m_threadpool->enqueue([&, arch = archs.m_arch, size = archs.m_size] {
              for (size_t i = 0; i < size; ++i) {
                if constexpr (sizeof...(Ts) == 1) {         // only one component
                  func((*arch->template Map<Ts>())[i]...);
                } else {                                    // multiple components
                  std::apply(func, std::tuple<Ts...>{(*arch->template Map<Ts>())[i]...});   // unpack the tuple of components for func
                }
              }
            });
          }
          // wait for all threads to finish work
          m_threadpool->waitForIdle();
        }
        

        /// @brief Get a single component value of an entity.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return The component value or reference to it.
        template<typename T>
        auto Get(Handle handle) -> Registry::to_ref_t<T> {
            std::shared_lock lock(m_system->GetSlotMapMutex(handle.GetStorageIndex()));
            auto comp = m_system->Get<T>(handle);
            return comp;
        }


        /// @brief Get multiple component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
        template<typename... Ts>
            requires (sizeof...(Ts)>1 && vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle&>::value)
        auto Get(Handle handle) -> std::tuple<Registry::to_ref_t<Ts>...> {
            std::shared_lock lock(m_system->GetSlotMapMutex(handle.GetStorageIndex()));
            auto comps = m_system->Get<Ts...>(handle);
            return comps;
        }



    ////////// ADDING/CHANGING ENTITIES/COMPONENTS/TAGS //////////

        /// @brief Create an entity with components.
		/// @tparam ...Ts The types of the components.
		/// @param ...component The new values.
		/// @return Future Handle of new entity.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		[[nodiscard]] auto Insert( Ts&&... component ) -> std::future<Handle> {

            auto res_prom = std::make_shared<std::promise<Handle>>();
            std::future<vecs::Handle> res_fut = res_prom->get_future();

            m_threadpool->enqueue( [res_prom, this, ...component = std::forward<Ts>(component)]() mutable {
                    std::scoped_lock lock(m_system->GetMutex());
                    res_prom->set_value(m_system->Insert(std::forward<Ts>(component)...));
            });

            return res_fut;
        }


        /// @brief Create multiple entities with default-initialized components.
        /// @tparam ...Ts The types of the components.
        /// @param out The vector for newly inserted handles.
        /// @return Vector of newly inserted handles.
        template <typename... Ts>
          requires((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value) &&
                   !vtll::has_type<vtll::tl<Ts...>, Handle>::value)
        [[nodiscard]] auto InsertBulk(std::vector<Handle>& out) -> std::vector<Handle> {
          std::vector<std::future<Handle>> futures;
          futures.reserve(out.size());

          for (size_t i = 0; i < out.size(); ++i) {
            futures.push_back(Insert(Ts{}...));
          }

          for (size_t i = 0; i < out.size(); ++i) {
            out[i] = futures[i].get();
          }
   
          return out;
        }


        /// @brief Put new component values as a tuple to an entity.
        /// @attention Do not use within a loop over a View.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param v The new values in a tuple
		template<typename... Ts>
        requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
        void Put(Handle handle, std::tuple<Ts...>& v) {

            if (m_system->HasAll(handle, std::forward<Ts>(std::get<Ts>(v))...)){
                m_threadpool->enqueue( [&] {
                    std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
                    m_system->Put(handle, v);
                });
            } else {
                auto arch = m_system->GetArchetypeIfExists<Ts...>();
                if (arch) {
                    std::scoped_lock lock(arch->GetMutex(), m_system->GetArchetypeMutex(handle));
                    m_system->Put(handle, v);
                } else {
                    std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
                    m_system->Put(handle, v);
                }
            }
        }


        /// @brief Put new component values to an entity.
        /// @attention Do not use within a loop over a View.
        /// @tparam Ts The types of the components.
        /// @param handle The handle of the entity.
        /// @param vs The new values.
        template<typename... Ts>
            requires ((vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
        void Put(Handle handle, Ts&&... vs) {

            if (m_system->HasAll(handle, std::forward<Ts>(vs)...)) {
                m_threadpool->enqueue( [&] {
                    std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
                    m_system->Put(handle, std::forward<Ts>(vs)...);
                });
            } else {
                auto arch = m_system->GetArchetypeIfExists<Ts...>();
                if (arch) {
                    std::scoped_lock lock(arch->GetMutex(), m_system->GetArchetypeMutex(handle));
                    m_system->Put(handle, std::forward<Ts>(vs)...);
                } else {
                    std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
                    m_system->Put(handle, std::forward<Ts>(vs)...);
                }
            }
        }


        /// @brief Add tags to an entity.
		/// @tparam ...Ts The types of the tags.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to add.
		template<typename... Ts>
        requires (std::is_integral_v<std::decay_t<Ts>> && ...)
        void AddTags(Handle handle, Ts... tags) {
            std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
            m_system->AddTags(handle, std::forward<Ts>(tags)...);
        }



    ////////// ERASING REGISTRY/ENTITIES/COMPONENTS/TAGS //////////
        /// @brief Erase tags from an entity.
        /// @tparam ...Ts The types of the tags.
        /// @param handle The handle of the entity.
        /// @param ...tags The tags to erase.
        template<typename... Ts>
            requires (std::is_integral_v<std::decay_t<Ts>> && ...)
        void EraseTags(Handle handle, Ts... tags) {
            std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
            m_system->EraseTags(handle,std::forward<Ts>(tags)...);
        }


        /// @brief Erase components from an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.		
		template<typename... Ts>
            requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
        void Erase(Handle handle) {
            std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
            m_system->Erase<Ts...>(handle);
        }


        /// @brief Erase an entity from the registry.
		/// @param handle The handle of the entity.
        void Erase(Handle handle) {
            std::scoped_lock lock(m_system->GetSlotMapMutex(handle.GetStorageIndex()));
            m_system->Erase(handle);
        }


        /// @brief Erase multiple entities from the registry.
        /// @param handles The handles of the entities.
        void EraseBulk(std::vector<Handle>& handles) {

            // get all Handles ordered into archetypes
            std::unordered_map<size_t, std::vector<vecs::Ref<vecs::Handle>>> archs{};
            for (auto h : handles) {
                auto arch = m_system->GetArchetypeHash(h);
                auto ref = m_system->GetRef(h);
                if (archs.contains(arch)) {
                    archs.at(arch).push_back(ref);
                } else {
                    archs.insert({arch, std::vector<Ref<vecs::Handle>>({ref})});
                }
            }

            // Erase the handles, split by archetypes amongst threads
            for (auto& arch : archs) {
                m_threadpool->enqueue([&] {
                    for (auto& handle : arch.second) {
                        std::scoped_lock lock(m_system->GetArchetypeMutex(handle.Get()));
                        m_system->Erase(handle.Get());
                    }
                });
            }
            // wait for all threads to finish work
            m_threadpool->waitForIdle();
        }


        /// @brief Clear the registry by removing all entities.
		void Clear() {
            std::scoped_lock lock(m_system->GetMutex());
            m_system->Clear();
		}



    ////////// CONVENIENCE REGISTRY //////////
        /// @brief Get the current size of the registry.
        size_t Size() {
            std::shared_lock lock(m_system->GetMutex());
            return m_system->Size();
        }


        /// @brief Test if an entity exists.
        /// @param handle The handle of the entity.
        /// @return true if the entity exists, else false.
        bool Exists(Handle handle) {
            std::shared_lock lock(m_system->GetSlotMapMutex(handle.GetStorageIndex()));
            return m_system->Exists(handle);
        }


        /// @brief Test if an entity in the registry has a component.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return true if the entity has the component, else false.
        template<typename T>
        bool Has(Handle handle) {
            std::shared_lock lock(m_system->GetSlotMapMutex(handle.GetStorageIndex()));
            return m_system->Has<T>(handle);
        }

        /// @brief Test if an entity has a tag.
		/// @param handle The handle of the entity.
		/// @param tag The tag to test.
		/// @return true if the entity has the tag, else false.
		bool Has(Handle handle, size_t ti) {
            std::shared_lock lock(m_system->GetSlotMapMutex(handle.GetStorageIndex()));
			return m_system->Has(handle, ti);
		}
        
        /// @brief Test if an entity in the registry has multiple components.
        /// @tparam ...Ts Types of the components to check
        /// @param handle Handle of the entity.
        /// @return True if the entity has all components, else false.
        template<typename... Ts>
            requires ((vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
        bool HasAll(Handle handle, Ts&&... vs) {
            std::shared_lock lock(m_system->GetArchetypeMutex(handle));
            return m_system->HasAll(handle, std::forward<Ts>(vs)...);
        }



    private:
        /// @brief Convenience print function for synchronous printing
        /// @param id The current thread id.
        /// @param std The string to print.
        void PrintSync(std::thread::id id, std::string str) {
            std::osyncstream(std::cout) << id << str << "\n";
        }


        std::shared_ptr<vecs::Registry> m_system{nullptr};
        std::shared_ptr<IThreadPool> m_threadpool;
    };

}