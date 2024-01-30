#include "CoreInfo.hpp"

const char* Anime4KCPP::CoreInfo::version()
{
    return 
        ANIME4KCPP_CORE_VERSION
        "-"
        ANIME4KCPP_CORE_VERSION_STATUS
        " "
        ANIME4KCPP_CORE_BUILD_DATE
        " "
        __TIME__;
}

const char* Anime4KCPP::CoreInfo::CPUOptimizationMode()
{
    return
#if defined(ENABLE_OPENCV_DNN)
        "OpenCV DNN"
#elif defined(USE_RYZEN)
        "SIMD (AVX2)"
#elif defined(USE_EIGEN3)
        "Eigen3"
#else
        "Normal"
#endif // ENABLE_OPENCV_DNN
#ifdef ENABLE_FAST_MATH
        ", Fast Math"
#endif // ENABLE_FAST_MATH
        ;
}

#define AC_ENUM_ITEM
#include "ACRegister.hpp"
#undef AC_ENUM_ITEM
#define PROCESSOR_STRING(...) PROCESSOR_STRING_IMPL(__VA_ARGS__)
#define PROCESSOR_STRING_IMPL(...) #__VA_ARGS__
const char* Anime4KCPP::CoreInfo::supportedProcessors()
{
    return
        PROCESSOR_STRING(PROCESSOR_ENUM);
}
