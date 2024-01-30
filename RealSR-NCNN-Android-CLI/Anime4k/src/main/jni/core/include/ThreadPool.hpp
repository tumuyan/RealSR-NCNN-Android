#ifndef ANIME4KCPP_CORE_THREADPOOL_HPP
#define ANIME4KCPP_CORE_THREADPOOL_HPP

#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <queue>
#include <vector>
#include <cstddef>

namespace Anime4KCPP::Utils
{
    class ThreadPool;
}

class Anime4KCPP::Utils::ThreadPool
{
public:
    explicit ThreadPool(std::size_t maxThreadCount);
    ~ThreadPool();

    template<typename F>
    void exec(F&& f);

    template<typename F, typename... Args>
    auto exec(F&& f, Args&&... args);

private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::condition_variable cnd;
    std::mutex mtx;
    bool stop;
};

inline Anime4KCPP::Utils::ThreadPool::ThreadPool(std::size_t maxThreadCount)
    :stop(false)
{
    threads.reserve(maxThreadCount);

    for (int i = 0; i < maxThreadCount; i++)
        threads.emplace_back([this]()
            {
                for (;;)
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cnd.wait(lock, [this]
                        {
                            return stop || !tasks.empty();
                        });

                    if (stop && tasks.empty())
                        return;

                    auto task = std::move(tasks.front());
                    tasks.pop();

                    lock.unlock();

                    task();
                }
            });
}

inline Anime4KCPP::Utils::ThreadPool::~ThreadPool()
{
    {
        const std::lock_guard<std::mutex> lock(mtx);
        stop = true;
    }
    cnd.notify_all();
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
}

template<typename F>
inline void Anime4KCPP::Utils::ThreadPool::exec(F&& f)
{
    {
        const std::lock_guard<std::mutex> lock(mtx);
        tasks.emplace(std::forward<F>(f));
    }
    cnd.notify_one();
}

template<typename F, typename ...Args>
inline auto Anime4KCPP::Utils::ThreadPool::exec(F&& f, Args && ...args)
{
    auto task = std::make_shared<std::packaged_task<decltype(std::declval<F>()(std::declval<Args>()...))()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    auto ret = task->get_future();

    {
        const std::lock_guard<std::mutex> lock(mtx);
        tasks.emplace([task]() { (*task)(); });
    }
    cnd.notify_one();

    return ret;
}

#endif // !ANIME4KCPP_CORE_THREADPOOL_HPP
