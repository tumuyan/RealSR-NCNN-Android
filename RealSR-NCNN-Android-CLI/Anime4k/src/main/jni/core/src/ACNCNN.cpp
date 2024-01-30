#ifdef ENABLE_NCNN

#include <net.h>

#include "ACNetType.hpp"
#include "ACNCNN.hpp"

Anime4KCPP::NCNN::GPUList Anime4KCPP::NCNN::listGPUs() noexcept
{
    int gpuCount = ncnn::get_gpu_count();
    std::string gpuInfo;
    for (int i = 0; i < gpuCount; i++)
    {
        const ncnn::GpuInfo& info = ncnn::get_gpu_info(i);
        gpuInfo += ("Device id: " + std::to_string(i) + "\n  Type: " + info.device_name() + "\n");
    }
    if (gpuCount)
        ncnn::destroy_gpu_instance();
    return GPUList{
        gpuCount,
        gpuInfo.empty() ? "No Vulkan device found, CPU is available for NCNN" : gpuInfo
    };
}

Anime4KCPP::NCNN::GPUList::GPUList(const int devices, std::string message)
    : devices(devices), message(std::move(message)) {}

std::string& Anime4KCPP::NCNN::GPUList::operator()() noexcept
{
    return message;
}

Anime4KCPP::NCNN::Manager::Manager(std::string modelPath, std::string paramPath, const int dID, const CNNType type, const int threads)
    : modelPath(std::move(modelPath)), paramPath(std::move(paramPath)), dID(dID), threads(threads), testFlag(true)
{
    switch (type)
    {
    case CNNType::ACNetHDNL0:
        currACNetType = ACNetType::HDNL0;
        break;
    case CNNType::ACNetHDNL1:
        currACNetType = ACNetType::HDNL1;
        break;
    case CNNType::ACNetHDNL2:
        currACNetType = ACNetType::HDNL2;
        break;
    case CNNType::ACNetHDNL3:
        currACNetType = ACNetType::HDNL3;
        break;
    case CNNType::Default:
    default:
        currACNetType = ACNetType::TotalTypeCount;
        break;
    }
}

Anime4KCPP::NCNN::Manager::Manager(const int dID, const CNNType type, const int threads) noexcept
    : Manager(std::string{}, std::string{}, dID, type, threads)
{
    testFlag = false;
}

void Anime4KCPP::NCNN::Manager::init()
{
    if (!Anime4KCPP::NCNN::ACNet::isInitialized())
    {
        if (!testFlag)
        {
            if (currACNetType == ACNetType::TotalTypeCount)
                Anime4KCPP::NCNN::ACNet::init(dID, threads);
            else
                Anime4KCPP::NCNN::ACNet::init(currACNetType, dID, threads);
        }
        else
            Anime4KCPP::NCNN::ACNet::init(modelPath, paramPath, currACNetType, dID, threads);
    }

}

void Anime4KCPP::NCNN::Manager::release() noexcept
{
    if (Anime4KCPP::NCNN::ACNet::isInitialized())
        Anime4KCPP::NCNN::ACNet::release();
}

bool Anime4KCPP::NCNN::Manager::isInitialized() noexcept
{
    return Anime4KCPP::NCNN::ACNet::isInitialized();
}

bool Anime4KCPP::NCNN::Manager::isSupport() noexcept
{
    return true;
}

#endif
