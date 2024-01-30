#ifdef ENABLE_OPENCL

#define CL_HPP_ENABLE_EXCEPTIONS
#ifdef LEGACY_OPENCL_API
#define CL_HPP_TARGET_OPENCL_VERSION 120
#else
#define CL_HPP_TARGET_OPENCL_VERSION 200
#endif // LEGACY_OPENCL_API
#define CL_HPP_MINIMUM_OPENCL_VERSION 110
#include <CL/opencl.hpp>

#include "ACOpenCL.hpp"

Anime4KCPP::OpenCL::GPUList Anime4KCPP::OpenCL::listGPUs() noexcept
{
    std::vector<cl::Platform> platforms;
    std::vector<cl::Device> devices;
    std::vector<int> devicesVector;
    std::ostringstream msg;

    try
    {
        cl::Platform::get(&platforms);

        const int platformsNumber = static_cast<int>(platforms.size());

        if (platformsNumber == 0)
        {
            return GPUList(0, { 0 }, "No supported OpenCL GPU");
        }

        for (int i = 0; i < platformsNumber; i++)
        {
            std::string platformName;
            platforms[i].getInfo<std::string>(CL_PLATFORM_NAME, &platformName);
            msg << "Platform " << i << ": " << platformName << '\n';
            platforms[i].getDevices(CL_DEVICE_TYPE_ALL, &devices);

            const int devicesNumber = static_cast<int>(devices.size());
            if (devicesNumber == 0)
            {
                msg << " No supported GPU in this platform" << '\n';
            }

            devicesVector.emplace_back(devicesNumber);
            for (int j = 0; j < devicesNumber; j++)
            {
                std::string deviceName;
                devices[j].getInfo<std::string>(CL_DEVICE_NAME, &deviceName);
                msg << " Device " << j << ": " << deviceName << '\n';
            }
            devices.clear();
        }
        return GPUList{ platformsNumber, devicesVector, msg.str() };
    }
    catch (const std::exception& e)
    {
        return GPUList{ 0, { 0 }, std::string{ "Failed to list opencl gpu infomation: " } + e.what() };
    }
}

Anime4KCPP::OpenCL::GPUInfo Anime4KCPP::OpenCL::checkGPUSupport(const int pID, const int dID) noexcept
{
    std::vector<cl::Platform> platforms;
    std::vector<cl::Device> devices;
    cl::Platform platform;
    cl::Device device;

    std::string platformName;
    std::string deviceName;

    try
    {
        cl::Platform::get(&platforms);
        if (pID >= 0 && pID < platforms.size())
            platform = platforms[pID];
        else
            platform = platforms[0];
        platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        if (dID >= 0 && dID < devices.size())
            device = devices[dID];
        else
            device = devices[0];

        platform.getInfo<std::string>(CL_PLATFORM_NAME, &platformName);
        device.getInfo<std::string>(CL_DEVICE_NAME, &deviceName);

        return GPUInfo{ true,
            std::string("Platform: ") + platformName + "\n" + " Device: " + deviceName };
    }
    catch (const std::exception& e)
    {
        return GPUInfo{ false, std::string{ "Failed to check OpenCL GPU support: " } + e.what() };
    }
}

Anime4KCPP::OpenCL::GPUList::GPUList(
    const int platforms,
    std::vector<int> devices,
    std::string message
) :platforms(platforms), devices(std::move(devices)), message(std::move(message)) {}

int Anime4KCPP::OpenCL::GPUList::operator[](int pID) const
{
    return devices.at(pID);
}

std::string& Anime4KCPP::OpenCL::GPUList::operator()() noexcept
{
    return message;
}

Anime4KCPP::OpenCL::GPUInfo::GPUInfo(const bool supported, std::string message) :
    supported(supported), message(std::move(message)) {}

std::string& Anime4KCPP::OpenCL::GPUInfo::operator()() noexcept
{
    return message;
}

Anime4KCPP::OpenCL::GPUInfo::operator bool() const noexcept
{
    return supported;
}

#endif
