#ifndef ANIME4KCPP_CORE_PARALLEL_HPP
#define ANIME4KCPP_CORE_PARALLEL_HPP

#include <thread>

#ifndef DISABLE_PARALLEL
#if defined(USE_PPL)
#include <ppl.h>
namespace Parallel = Concurrency;
#elif defined(USE_TBB)
#include <tbb/parallel_for.h>
namespace Parallel = tbb;
#elif defined(USE_OPENMP)
#include <omp.h>
#else
#include "ThreadPool.hpp"
#endif
#endif // !DISABLE_PARALLEL

namespace Anime4KCPP::Utils
{
    unsigned int supportedThreads() noexcept;

    template <typename IndexType, typename F>
    void parallelFor(IndexType first, IndexType last, F&& func);
}

inline unsigned int Anime4KCPP::Utils::supportedThreads() noexcept
{
    auto threads = std::thread::hardware_concurrency();
    return (threads < 1) ? 1 : threads;
}

template <typename IndexType, typename F>
inline void Anime4KCPP::Utils::parallelFor(const IndexType first, const IndexType last, F&& func)
{
#ifndef DISABLE_PARALLEL
#if defined(USE_PPL) || defined(USE_TBB)
    Parallel::parallel_for(first, last, std::forward<F>(func));
#elif defined(USE_OPENMP)
#pragma omp parallel for
    for (IndexType i = first; i < last; i++)
    {
        func(i);
    }
#else // Built-in parallel library
    static const std::size_t threadNum = supportedThreads();
    if (threadNum > 1)
    {
        std::vector<std::future<void>> taskList;
        taskList.reserve(static_cast<std::size_t>(last) - static_cast<std::size_t>(first));

        static Anime4KCPP::Utils::ThreadPool pool(threadNum);

        for (IndexType i = first; i < last; i++)
        {
            taskList.emplace_back(pool.exec(
                [&func](IndexType i) {
                    func(i);
                }, i));
        }

        std::for_each(taskList.begin(), taskList.end(), std::mem_fn(&std::future<void>::wait));
    }
    else
    {
        for (IndexType i = first; i < last; i++)
        {
            func(i);
        }
    }
#endif
#else // Disable parallel
    for (IndexType i = first; i < last; i++)
    {
        func(i);
    }
#endif // !DISABLE_PARALLEL
}

#endif // !ANIME4KCPP_CORE_PARALLEL_HPP
