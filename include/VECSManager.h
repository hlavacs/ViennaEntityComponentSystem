#pragma once

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
            
            std::promise<Registry::View<Ts...>> res_prom;
            std::future<Registry::View<Ts...>> res_fut = res_prom.get_future();

            m_threadpool->enqueue( [&] {
                    std::shared_lock lock(m_system->GetMutex());
                    res_prom.set_value(m_system->GetView<Ts...>(std::forward<std::vector<size_t>>(yes), std::forward<std::vector<size_t>>(no)));
                });
            
            Registry::View<Ts...> res = res_fut.get();
            return res;
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

            std::promise<Registry::to_ref_t<T>> res_prom;
            std::future<Registry::to_ref_t<T>> res_fut = res_prom.get_future();

            m_threadpool->enqueue( [&] {
                std::shared_lock lock(m_system->GetArchetypeMutex(handle));
                res_prom.set_value(m_system->Get<T>(handle));
            });
            Registry::to_ref_t<T> res = res_fut.get();

            return res;
        }


        /// @brief Get multiple component values of an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @return A tuple of the component values.
        template<typename... Ts>
            requires (sizeof...(Ts)>1 && vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle&>::value)
        auto Get(Handle handle) -> std::tuple<Registry::to_ref_t<Ts>...> {
            
            std::promise<std::tuple<Registry::to_ref_t<Ts>...>> res_prom;
            std::future<std::tuple<Registry::to_ref_t<Ts>...>> res_fut = res_prom.get_future();

            m_threadpool->enqueue( [&] {
                    std::shared_lock lock(m_system->GetArchetypeMutex(handle));
                    res_prom.set_value(m_system->Get<Ts...>(handle));
            });
            std::tuple<Registry::to_ref_t<Ts>...> res = res_fut.get();

            return res;
        }



    ////////// ADDING/CHANGING ENTITIES/COMPONENTS/TAGS //////////
        //TODO: check GetNewSlotmapIndex() if different idx when in parallel

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
                m_threadpool->enqueue( [&] {
                    std::scoped_lock lock(m_system->GetMutex());
                    m_system->Put(handle, v);
                });
                waitIdle();
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
                m_threadpool->enqueue( [&] {
                    std::scoped_lock lock(m_system->GetMutex());
                    m_system->Put(handle, std::forward<Ts>(vs)...);
                });
                waitIdle();
            }
        }


        /// @brief Add tags to an entity.
		/// @tparam ...Ts The types of the tags.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to add.
		template<typename... Ts>
        requires (std::is_integral_v<std::decay_t<Ts>> && ...)
        void AddTags(Handle handle, Ts... tags) {
            m_threadpool->enqueue( [&] {
                std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
                m_system->AddTags(handle, std::forward<Ts>(tags)...);
            }); 
            waitIdle();
        }



    ////////// ERASING REGISTRY/ENTITIES/COMPONENTS/TAGS //////////
        /// @brief Erase tags from an entity.
        /// @tparam ...Ts The types of the tags.
        /// @param handle The handle of the entity.
        /// @param ...tags The tags to erase.
        template<typename... Ts>
            requires (std::is_integral_v<std::decay_t<Ts>> && ...)
        void EraseTags(Handle handle, Ts... tags) {
            m_threadpool->enqueue( [&] {
                std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
                m_system->EraseTags(handle,std::forward<Ts>(tags)...);
            });
            waitIdle();
        }


        /// @brief Erase components from an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.		
		template<typename... Ts>
            requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
        void Erase(Handle handle) {
            m_threadpool->enqueue( [&] {
                std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
                m_system->Erase<Ts...>(handle);
            });
            waitIdle();
        }


        /// @brief Erase an entity from the registry.
		/// @param handle The handle of the entity.
        void Erase(Handle handle) {
            m_threadpool->enqueue( [&] {
                std::scoped_lock lock(m_system->GetSlotMapMutex(handle.GetStorageIndex()));
                m_system->Erase(handle);
            });
            waitIdle();
        }


        /// @brief Clear the registry by removing all entities.
		void Clear() {
            m_threadpool->enqueue( [&] {
                std::scoped_lock lock(m_system->GetMutex());
                m_system->Clear();
            });
		}



    ////////// CONVENIENCE REGISTRY //////////
        /// @brief Get the current size of the registry.
        size_t Size() {
            return m_system->Size();
        }


        /// @brief Test if an entity in the registry has a component.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return true if the entity has the component, else false.
        template<typename T>
        bool Has(Handle handle) {
            return m_system->Has<T>(handle);
        }

        
        /// @brief Test if an entity in the registry has multiple components.
        /// @tparam ...Ts Types of the components to check
        /// @param handle Handle of the entity.
        /// @return True if the entity has all components, else false.
        template<typename... Ts>
            requires ((vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
        bool HasAll(Handle handle, Ts&&... vs) {
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