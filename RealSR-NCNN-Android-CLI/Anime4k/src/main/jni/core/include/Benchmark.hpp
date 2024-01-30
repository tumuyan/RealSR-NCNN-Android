#ifndef ANIME4KCPP_CORE_BENCHMARK_HPP
#define ANIME4KCPP_CORE_BENCHMARK_HPP

#include "ACInitializer.hpp"
#include "ACCreator.hpp"

namespace Anime4KCPP
{
    template<typename T, int W, int H, typename ...Types>
    double benchmark(Types&&... args);

    template<typename ...Image>
    double benchmark(AC& ac, Image&&... image);
}

template<typename T, int W, int H, typename ...Types>
inline double Anime4KCPP::benchmark(Types && ...args)
{
    ACInitializer initializer;

    initializer.pushManager<typename Processor::GetManager<T>::Manager>(std::forward<Types>(args)...);

    if (!initializer.init())
        return 0.0;

    cv::Mat testImg = cv::Mat::zeros(cv::Size(W, H), CV_8UC1);
    cv::randu(testImg, cv::Scalar::all(0), cv::Scalar::all(255));

    T ac(Parameters{});

    return benchmark(ac, testImg, testImg, testImg);
}

template<typename ...Image>
inline double Anime4KCPP::benchmark(AC& ac, Image&&... image)
{
    constexpr int times = 3;
    std::chrono::milliseconds sum(0);
    std::chrono::steady_clock::time_point s;
    std::chrono::steady_clock::time_point e;

    // Warm-up
    for (int i = 0; i < 3; i++)
    {
        ac.loadImage(image...); // YUV or BGR
        ac.process();
    }

    for (int i = 0; i < times; i++)
    {
        ac.loadImage(image...);
        s = std::chrono::steady_clock::now();
        ac.process();
        e = std::chrono::steady_clock::now();
        sum += std::chrono::duration_cast<std::chrono::milliseconds>(e - s);
    }

    return static_cast<double>(times) * 1000.0 / static_cast<double>(sum.count());
}

#endif // !ANIME4KCPP_CORE_BENCHMARK_HPP
