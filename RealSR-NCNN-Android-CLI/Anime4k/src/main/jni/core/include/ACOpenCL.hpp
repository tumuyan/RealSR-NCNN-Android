#ifndef ANIME4KCPP_CORE_AC_OPENCL_HPP
#define ANIME4KCPP_CORE_AC_OPENCL_HPP

#ifdef ENABLE_OPENCL

#include "OpenCLAnime4K09.hpp"
#include "OpenCLACNet.hpp"
#include "ACManager.hpp"

namespace Anime4KCPP
{
    namespace OpenCL
    {
        template<typename T>
        class Manager;

        struct AC_EXPORT GPUList;
        struct AC_EXPORT GPUInfo;

        //return platforms, devices of each platform, all devices information
        AC_EXPORT GPUList listGPUs() noexcept;
        //return result and information
        AC_EXPORT GPUInfo checkGPUSupport(int pID, int dID) noexcept;
    }

    namespace Processor
    {
        template<>
        struct GetManager<OpenCL::ACNet> {
            using Manager = OpenCL::Manager<OpenCL::ACNet>;
        };
        template<>
        struct GetManager<OpenCL::Anime4K09> {
            using Manager = OpenCL::Manager<OpenCL::Anime4K09>;
        };
    }
}

struct Anime4KCPP::OpenCL::GPUList
{
    int platforms;
    std::vector<int> devices;
    std::string message;

    GPUList(int platforms, std::vector<int> devices, std::string message);
    int operator[](int pID) const;
    std::string& operator()() noexcept;
};

struct Anime4KCPP::OpenCL::GPUInfo
{
    bool supported;
    std::string message;

    GPUInfo(bool supported, std::string message);
    std::string& operator()() noexcept;
    operator bool() const noexcept;
};

template<typename T>
class Anime4KCPP::OpenCL::Manager : public Anime4KCPP::Processor::Manager
{
public:
    template<typename P = T, std::enable_if_t<std::is_same<P, Anime4KCPP::OpenCL::Anime4K09>::value>* = nullptr>
    Manager(int pID = 0, int dID = 0, int OpenCLQueueNum = 1, bool OpenCLParallelIO = false) noexcept;

    template<typename P = T, std::enable_if_t<std::is_same<P, Anime4KCPP::OpenCL::ACNet>::value>* = nullptr>
    Manager(int pID = 0, int dID = 0, CNNType type = CNNType::Default, int OpenCLQueueNum = 1, bool OpenCLParallelIO = false) noexcept;

    void init() override;
    void release() noexcept override;
    bool isInitialized() noexcept override;
    bool isSupport() noexcept override;

    const char* name() noexcept override {
        if constexpr (std::is_same<T, Anime4KCPP::OpenCL::ACNet>::value)
            return "OpenCL ACNet Processor Manager"; 
        else
            return "OpenCL Anime4K09 Processor Manager";
    };

private:
    template<typename P = T>
    std::enable_if_t<std::is_same<P, Anime4KCPP::OpenCL::Anime4K09>::value> initImpl();
    template<typename P = T>
    std::enable_if_t<std::is_same<P, Anime4KCPP::OpenCL::ACNet>::value> initImpl();

private:
    int pID, dID;
    int OpenCLQueueNum;
    bool OpenCLParallelIO;
    CNNType type;
};

template<typename T>
template<typename P, std::enable_if_t<std::is_same<P, Anime4KCPP::OpenCL::Anime4K09>::value>*>
inline Anime4KCPP::OpenCL::Manager<T>::Manager(const int pID, const int dID, const int OpenCLQueueNum, const bool OpenCLParallelIO) noexcept
    : pID(pID), dID(dID), OpenCLQueueNum(OpenCLQueueNum > 0 ? OpenCLQueueNum : 1),
    OpenCLParallelIO(OpenCLParallelIO), type(Anime4KCPP::CNNType::Default) {}

template<typename T>
template<typename P, std::enable_if_t<std::is_same<P, Anime4KCPP::OpenCL::ACNet>::value>*>
inline Anime4KCPP::OpenCL::Manager<T>::Manager(const int pID, const int dID, const CNNType type, const int OpenCLQueueNum, const bool OpenCLParallelIO) noexcept
    : pID(pID), dID(dID), OpenCLQueueNum(OpenCLQueueNum > 0 ? OpenCLQueueNum : 1),
    OpenCLParallelIO(OpenCLParallelIO), type(type) {}

template<typename T>
inline void Anime4KCPP::OpenCL::Manager<T>::init()
{
    initImpl();
}

template<typename T>
inline void Anime4KCPP::OpenCL::Manager<T>::release() noexcept
{
    if (T::isInitialized())
        T::release();
}

template<typename T>
inline bool Anime4KCPP::OpenCL::Manager<T>::isInitialized() noexcept
{
    return T::isInitialized();
}

template<typename T>
inline bool Anime4KCPP::OpenCL::Manager<T>::isSupport() noexcept
{
    return Anime4KCPP::OpenCL::checkGPUSupport(pID, dID);
}

template<typename T>
template<typename P>
inline std::enable_if_t<std::is_same<P, Anime4KCPP::OpenCL::Anime4K09>::value> Anime4KCPP::OpenCL::Manager<T>::initImpl()
{
    if (!T::isInitialized())
        T::init(pID, dID, OpenCLQueueNum, OpenCLParallelIO);
}

template<typename T>
template<typename P>
inline std::enable_if_t<std::is_same<P, Anime4KCPP::OpenCL::ACNet>::value> Anime4KCPP::OpenCL::Manager<T>::initImpl()
{
    if (!T::isInitialized())
        T::init(pID, dID, type, OpenCLQueueNum, OpenCLParallelIO);
}

#endif // ENABLE_OPENCL

#endif // !ANIME4KCPP_CORE_AC_OPENCL_HPP
