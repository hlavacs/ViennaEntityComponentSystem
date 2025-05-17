#pragma once

#include <VECS.h>
#include <VECSRegistry.h>
#include <thread>
#include <future>
#include <queue>

namespace vecs {

    //----------------------------------------------------------------------------------------------
	//Manager

    /// @brief A manager for handling registries and parallel access
    class Manager {

    //----------------------------------------------------------------------------------------------

        // ThreadPool SOURCE: https://www.geeksforgeeks.org/thread-pool-in-cpp/
        class ThreadPool {
        public:
            ThreadPool(size_t num = 4/*std::thread::hardware_concurrency()*/) {
                for (size_t i {0}; i < num; ++i) {
                    m_threads.emplace_back([this] {
                        while (true) {
                            std::function<void()> task;
                            {
                                // lock queue to share data safely
                                std::unique_lock<std::mutex> lock(m_queueMutex);

                                // wait for task or until pool is stopped
                                m_cv.wait(lock, [this] {
                                    return !m_tasks.empty() || m_stop;
                                });

                                // exit thread when no tasks and pool is stopped
                                if (m_stop && m_tasks.empty()) { return; }

                                // get next task from queue
                                task = std::move(m_tasks.front());
                                m_tasks.pop();
                                
                            }

                            task();
                            
                            {
                                std::unique_lock<std::mutex> lock(m_queueMutex);
                                --m_taskCount;
                                if (isIdle()) {
                                    m_idle = true;
                                    m_idle.notify_all();
                                }
                            }
                           
                        }
                    });
                }
            }

            ~ThreadPool() {
                {
                    // lock queue to set stop flag
                    std::unique_lock<std::mutex> lock(m_queueMutex);
                    m_stop = true;
                }

                // notify all threads
                m_cv.notify_all();

                for (auto& thread: m_threads) {
                    thread.join();
                }
            }


            void enqueue(std::function<void()> task) {
                {
                    std::unique_lock<std::mutex> lock(m_queueMutex);
                    m_tasks.emplace(std::move(task));
                    ++m_taskCount;
                    m_idle = false;
                }
                m_cv.notify_one();
            }


            bool isIdle() {
                return m_taskCount.load() == 0;
            }

            void waitForIdle() {
                m_idle.wait(false);
            }

        private:
            std::vector<std::thread> m_threads;
            std::queue<std::function<void()>> m_tasks;
            std::mutex m_queueMutex;

            bool m_stop = false;
            std::condition_variable m_cv;
            std::atomic<bool> m_idle = true;
            std::atomic<size_t> m_taskCount = 0;
        };



       /*// wrapper class for different tasks to queue in threadpool? -> reuse logic
       class Task {
            Task(std::invocable<> auto &) {}
            ~Task() = default;

        };
        */


    public:

        Manager() {}

        ~Manager() = default;  // Destructor

        void waitIdle() {
            m_threadpool.waitForIdle();
        }

		/// @brief Get a view of entities with specific components.
		/// @tparam ...Ts The types of the components.
		/// @return A view of the entity components
		template<typename... Ts>
			requires (vtll::unique<vtll::tl<Ts...>>::value)
		[[nodiscard]] auto GetView(std::vector<size_t>&& yes={}, std::vector<size_t>&& no={}) -> Registry::View<Ts...> {
            
            std::promise<Registry::View<Ts...>> res_prom;
            std::future<Registry::View<Ts...>> res_fut = res_prom.get_future();

            m_threadpool.enqueue( [&] {
                    this->m_system.GetMutex().lock();
                    res_prom.set_value(this->m_system.GetView<Ts...>(std::forward<std::vector<size_t>>(yes), std::forward<std::vector<size_t>>(no)));
                    this->m_system.GetMutex().unlock();
            });
            
            Registry::View<Ts...> res = res_fut.get();

            return res;
		}


        // TODO: check if component exists (relevant for parallel)
        
        /// @brief Get a single component value of an entity.
		/// @tparam T The type of the component.
		/// @param handle The handle of the entity.
		/// @return The component value or reference to it.
        template<typename T>
        auto GetComponent(Handle handle) -> Registry::to_ref_t<T> {

            std::promise<Registry::to_ref_t<T>> res_prom;
            std::future<Registry::to_ref_t<T>> res_fut = res_prom.get_future();

            m_threadpool.enqueue( [&] {
                    this->m_system.GetMutex().lock();
                    res_prom.set_value(this->m_system.Get<T>(handle));
                    this->m_system.GetMutex().unlock();
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
        auto GetComponent(Handle handle) -> std::tuple<Registry::to_ref_t<Ts>...> {
            
            std::promise<std::tuple<Registry::to_ref_t<Ts>...>> res_prom;
            std::future<std::tuple<Registry::to_ref_t<Ts>...>> res_fut = res_prom.get_future();

            m_threadpool.enqueue( [&] {
                    this->m_system.GetMutex().lock();
                    res_prom.set_value(this->m_system.Get<Ts...>(handle));
                    this->m_system.GetMutex().unlock();
            });
            
            std::tuple<Registry::to_ref_t<Ts>...> res = res_fut.get();

            return res;
        }



        /// @brief Create an entity with components.
		/// @tparam ...Ts The types of the components.
		/// @param ...component The new values.
		/// @return Handle of new entity.
		template<typename... Ts>
			requires ((sizeof...(Ts) > 0) && (vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
		[[nodiscard]] auto CreateEntity( Ts&&... component ) -> Handle {

            std::promise<vecs::Handle> res_prom;
            std::future<vecs::Handle> res_fut = res_prom.get_future();

            m_threadpool.enqueue( [&] {
                    this->m_system.GetMutex().lock();
                    res_prom.set_value(this->m_system.Insert(std::forward<Ts>(component)...));
                    this->m_system.GetMutex().unlock();
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
        void PutComponent(Handle handle, std::tuple<Ts...>& v) {
            m_threadpool.enqueue( [&] {
                this->m_system.GetMutex().lock();
                this->m_system.Put(handle, v);
                this->m_system.GetMutex().unlock();
            });
        }

        /// @brief Put new component values to an entity.
        /// @tparam Ts The types of the components.
        /// @param handle The handle of the entity.
        /// @param vs The new values.
        template<typename... Ts>
            requires ((vtll::unique<vtll::tl<Ts...>>::value) && !vtll::has_type< vtll::tl<std::decay_t<Ts>...>, Handle>::value)
        void PutComponent(Handle handle, Ts&&... vs) {
            m_threadpool.enqueue( [&] {
                this->m_system.GetMutex().lock();
                this->m_system.Put(handle, std::forward<Ts>(vs)...);
                this->m_system.GetMutex().unlock();
            });
        }


        /// @brief Add tags to an entity.
		/// @tparam ...Ts The types of the tags.
		/// @param handle The handle of the entity.
		/// @param ...tags The tags to add.
		template<typename... Ts>
        requires (std::is_integral_v<std::decay_t<Ts>> && ...)
        void AddTags(Handle handle, Ts... tags) {
            m_threadpool.enqueue( [&] {
                this->m_system.GetMutex().lock();
                this->m_system.AddTags(handle, std::forward<Ts>(tags)...);
                this->m_system.GetMutex().unlock();
            }); 
        }


        /// @brief Erase tags from an entity.
        /// @tparam ...Ts The types of the tags.
        /// @param handle The handle of the entity.
        /// @param ...tags The tags to erase.
        template<typename... Ts>
            requires (std::is_integral_v<std::decay_t<Ts>> && ...)
        void EraseTags(Handle handle, Ts... tags) {
            m_threadpool.enqueue( [&] {
                this->m_system.GetMutex().lock();
                this->m_system.EraseTags(handle,std::forward<Ts>(tags)...);
                this->m_system.GetMutex().unlock();
            });
        }


        /// @brief Erase components from an entity.
		/// @tparam ...Ts The types of the components.
		/// @param handle The handle of the entity.		
		template<typename... Ts>
            requires (vtll::unique<vtll::tl<Ts...>>::value && !vtll::has_type< vtll::tl<Ts...>, Handle>::value)
        void EraseComponents(Handle handle) {
            m_threadpool.enqueue( [&] {
                this->m_system.GetMutex().lock();
                this->m_system.Erase<Ts...>(handle);
                this->m_system.GetMutex().unlock();
            });
        }

        void EraseEntity(Handle handle) {
            m_threadpool.enqueue( [&] {
                this->m_system.GetMutex().lock();
                this->m_system.Erase(handle);
                this->m_system.GetMutex().unlock();
            });
        }


        /// @brief Clear the registry by removing all entities.
		void ClearRegistry() {
            m_threadpool.enqueue( [&] {
                this->m_system.GetMutex().lock();
                this->m_system.Clear();
                this->m_system.GetMutex().unlock();
            });
		}


    private:
        vecs::Registry m_system;
        ThreadPool m_threadpool;
    };

}