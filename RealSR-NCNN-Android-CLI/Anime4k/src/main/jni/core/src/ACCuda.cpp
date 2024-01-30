#ifdef ENABLE_CUDA

#include "CudaInterface.hpp"
#include "ACCuda.hpp"

Anime4KCPP::Cuda::GPUList Anime4KCPP::Cuda::listGPUs() noexcept
{
    try
    {
        return GPUList(cuGetDeviceCount(), cuGetCudaInfo());
    }
    catch (const std::exception& e)
    {
        return GPUList(0, e.what());
    }
}

Anime4KCPP::Cuda::GPUInfo Anime4KCPP::Cuda::checkGPUSupport(const int dID) noexcept
{
    try
    {
        return GPUInfo(cuCheckDeviceSupport(dID), cuGetDeviceInfo(dID));
    }
    catch (const std::exception& e)
    {
        return GPUInfo(false, e.what());
    }
}

Anime4KCPP::Cuda::GPUList::GPUList(const int devices, std::string message)
    : devices(devices), message(std::move(message)) {}

std::string& Anime4KCPP::Cuda::GPUList::operator()() noexcept
{
    return message;
}

Anime4KCPP::Cuda::GPUInfo::GPUInfo(const bool supported, std::string message) :
    supported(supported), message(std::move(message)) {};

std::string& Anime4KCPP::Cuda::GPUInfo::operator()() noexcept
{
    return message;
}

Anime4KCPP::Cuda::GPUInfo::operator bool() const noexcept
{
    return supported;
}

Anime4KCPP::Cuda::Manager::Manager(const int dID) noexcept
    : dID(dID) {}

void Anime4KCPP::Cuda::Manager::init()
{
    if (cuGetDeviceID() != dID)
        cuSetDeviceID(dID);
}

void Anime4KCPP::Cuda::Manager::release() noexcept
{
    if (cuGetDeviceID() == dID)
        cuReleaseCuda();
}

bool Anime4KCPP::Cuda::Manager::isInitialized() noexcept
{
    return cuGetDeviceID() == dID;
}

bool Anime4KCPP::Cuda::Manager::isSupport() noexcept
{
    return cuCheckDeviceSupport(dID);
}

#endif