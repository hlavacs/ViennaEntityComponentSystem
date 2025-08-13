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

        Manager(std::shared_ptr<IThreadPool> threadpool, std::shared_ptr<vecs::Registry> registry) {
            m_threadpool = std::move(threadpool);
            m_system = std::move(registry);
        }

        ~Manager() = default;  // Destructor

        void waitIdle() {
            m_threadpool->waitForIdle();
        }

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



        //TODO: check GetNewSlotmapIndex() if different idx when in parallel

        /// @brief Create an entity with components.
		/// @tparam ...Ts The types of the components.
		/// @param ...component The new values.
		/// @return Handle of new entity.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		[[nodiscard]] auto Insert( Ts&&... component ) -> Handle {

            std::promise<vecs::Handle> res_prom;
            std::future<vecs::Handle> res_fut = res_prom.get_future();
            m_threadpool->enqueue( [&] {
                    std::scoped_lock lock(m_system->GetMutex());
                    res_prom.set_value(m_system->Insert(std::forward<Ts>(component)...));
            });
            
            vecs::Handle res = res_fut.get();

            return res;
        }


        /// @brief Put new component values as a tuple to an entity.
		/// @tparam Ts The types of the components.
		/// @param handle The handle of the entity.
		/// @param v The new values in a tuple
		template<typename... Ts>
        requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
        void Put(Handle handle, std::tuple<Ts...>& v) {
            m_threadpool->enqueue( [&] {
                std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
                //TODO: add lock for potential new archetype
                // if archetype with types of handle + v exists, lock it,
                // else lock slotmap
                m_system->Put(handle, v);
            });
        }

        /// @brief Put new component values to an entity.
        /// @tparam Ts The types of the components.
        /// @param handle The handle of the entity.
        /// @param vs The new values.
        template<typename... Ts>
            requires ((vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
        void Put(Handle handle, Ts&&... vs) {
            m_threadpool->enqueue( [&] {
                std::scoped_lock lock(m_system->GetArchetypeMutex(handle));
                //TODO: same as above
                m_system->Put(handle, std::forward<Ts>(vs)...);
            });
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


    private:
        std::shared_ptr<vecs::Registry> m_system{nullptr};
        std::shared_ptr<IThreadPool> m_threadpool;

        /// @brief Convenience print function for synchronous printing
        /// @param id The current thread id.
        /// @param std The string to print.
        void PrintSync(std::thread::id id, std::string str) {
            std::osyncstream(std::cout) << id << str << "\n";
        }
    };

}