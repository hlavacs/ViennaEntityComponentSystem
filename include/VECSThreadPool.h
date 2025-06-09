#pragma once

#include <thread>
#include <queue>
#include <condition_variable>
#include <functional>

namespace vecs {
    //----------------------------------------------------------------------------------------------
    //ThreadPool Interface
    class IThreadPool {
        public:
            IThreadPool(size_t num = std::thread::hardware_concurrency()) {}
            virtual ~IThreadPool() {}

            virtual void enqueue(std::function<void()> task) = 0;
            virtual bool isIdle() = 0;
            virtual void waitForIdle() = 0;

        protected:
            std::vector<std::thread> m_threads;
            std::queue<std::function<void()>> m_tasks;
            std::mutex m_queueMutex;

            bool m_stop = false;
            std::condition_variable m_signal;
            std::atomic<bool> m_idle = true;
            std::atomic<size_t> m_taskCount = 0;
    };


    //----------------------------------------------------------------------------------------------
    //ThreadPool Implementation
    // ThreadPool SOURCE: https://www.geeksforgeeks.org/thread-pool-in-cpp/

    
       /*// wrapper class for different tasks to queue in threadpool? -> reuse logic
       class Task {
            Task(std::invocable<> auto &) {}
            ~Task() = default;

        };
        */
    
    class ThreadPool : public IThreadPool {
        public:
            ThreadPool(size_t num = std::thread::hardware_concurrency()) {
                for (size_t i {0}; i < num; ++i) {
                    m_threads.emplace_back([this] {
                        while (true) {
                            std::function<void()> task;
                            {
                                // lock queue to share data safely
                                std::unique_lock<std::mutex> lock(m_queueMutex);

                                // wait for task or until pool is stopped
                                m_signal.wait(lock, [this] {
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
                m_signal.notify_all();

                for (auto& thread: m_threads) {
                    thread.join();
                }
            }


            void enqueue(std::function<void()> task) override {
                {
                    std::unique_lock<std::mutex> lock(m_queueMutex);
                    m_tasks.emplace(std::move(task));
                    ++m_taskCount;
                    m_idle = false;
                }
                m_signal.notify_one();
            }

            bool isIdle() override {
                return m_taskCount.load() == 0;
            }

            void waitForIdle() override {
                m_idle.wait(false);
            } 
    };
}