#ifndef ANIME4KCPP_CORE_CUDA_INTERFACE_HPP
#define ANIME4KCPP_CORE_CUDA_INTERFACE_HPP

#ifdef ENABLE_CUDA

#include <string>

namespace Anime4KCPP::Cuda
{
    enum class ACCudaDataType
    {
        AC_8U, AC_16U, AC_32F
    };

    struct ACCudaParamAnime4K09
    {
        int orgW, orgH;
        int W, H;
        std::size_t stride;
        int passes, pushColorCount;
        float strengthColor, strengthGradient;
    };

    struct ACCudaParamACNet
    {
        int orgW, orgH;
        std::size_t stride;
    };

    void cuRunKernelAnime4K09(const void* inputData, void* outputData, ACCudaDataType type, ACCudaParamAnime4K09* param);

    void cuRunKernelACNetHDN0(const void* inputData, void* outputData, ACCudaDataType type, ACCudaParamACNet* param);
    void cuRunKernelACNetHDN1(const void* inputData, void* outputData, ACCudaDataType type, ACCudaParamACNet* param);
    void cuRunKernelACNetHDN2(const void* inputData, void* outputData, ACCudaDataType type, ACCudaParamACNet* param);
    void cuRunKernelACNetHDN3(const void* inputData, void* outputData, ACCudaDataType type, ACCudaParamACNet* param);

    void cuSetDeviceID(const int id) noexcept;
    int cuGetDeviceID() noexcept;
    void cuReleaseCuda() noexcept;

    int cuGetDeviceCount() noexcept;
    std::string cuGetDeviceInfo(const int id);
    std::string cuGetCudaInfo();
    bool cuCheckDeviceSupport(const int id) noexcept;
}

#endif // ENABLE_CUDA

#endif // !ANIME4KCPP_CORE_CUDA_INTERFACE_HPP
