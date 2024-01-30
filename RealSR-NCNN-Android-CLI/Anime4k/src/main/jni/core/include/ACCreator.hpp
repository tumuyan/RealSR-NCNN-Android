#ifndef ANIME4KCPP_CORE_AC_CREATOR_HPP
#define ANIME4KCPP_CORE_AC_CREATOR_HPP

#include "ACCPU.hpp"

#ifdef ENABLE_OPENCL
#include "ACOpenCL.hpp"
#endif

#ifdef ENABLE_CUDA
#include "ACCuda.hpp"
#endif

#ifdef ENABLE_NCNN
#include "ACNCNN.hpp"
#endif

namespace Anime4KCPP
{
    class AC_EXPORT ACCreator;
}

//Anime4KCPP Processor Factory
class Anime4KCPP::ACCreator
{
public:
    static std::unique_ptr<AC> createUP(const Parameters& parameters, Processor::Type type);
    static AC* create(const Parameters& parameters, Processor::Type type);
    static void release(AC* ac) noexcept;
};

#endif // !ANIME4KCPP_CORE_AC_CREATOR_HPP
