#ifndef ANIME4KCPP_CORE_OPENCL_ACNET_HPP
#define ANIME4KCPP_CORE_OPENCL_ACNET_HPP

#ifdef ENABLE_OPENCL

#include "AC.hpp"
#include "CNN.hpp"

namespace Anime4KCPP::OpenCL
{
    class AC_EXPORT ACNet;
}

class Anime4KCPP::OpenCL::ACNet :public AC
{
public:
    explicit ACNet(const Parameters& parameters);
    void setParameters(const Parameters& parameters) override;

    std::string getInfo() const override;
    std::string getFiltersInfo() const override;

    static void init(
        const int platformID = 0,
        const int deviceID = 0,
        const CNNType type = CNNType::Default,
        const int OpenCLQueueNum = 4,
        const bool OpenCLParallelIO = false);
    static void release() noexcept;
    static bool isInitialized() noexcept;
private:
    void processYUVImage() override;
    void processRGBImage() override;
    void processGrayscale() override;

    Processor::Type getProcessorType() const noexcept override;
    std::string getProcessorInfo() const override;

    static void initOpenCL(const CNNType type);
private:
    int ACNetTypeIndex;
};

#endif // ENABLE_OPENCL

#endif // !ANIME4KCPP_CORE_OPENCL_ACNET_HPP
