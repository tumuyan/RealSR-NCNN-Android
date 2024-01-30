#ifndef ANIME4KCPP_CORE_OPENCL_ANIME4K09_HPP
#define ANIME4KCPP_CORE_OPENCL_ANIME4K09_HPP

#ifdef ENABLE_OPENCL

#include "AC.hpp"

namespace Anime4KCPP::OpenCL
{
    class AC_EXPORT Anime4K09;
}

class Anime4KCPP::OpenCL::Anime4K09 :public AC
{
public:
    using AC::AC;

    std::string getInfo() const override;
    std::string getFiltersInfo() const override;

    static void init(int platformID = 0, int deviceID = 0, int OpenCLQueueNum = 4, bool OpenCLParallelIO = false);
    static void release() noexcept;
    static bool isInitialized() noexcept;
private:
    void processYUVImage() override;
    void processRGBImage() override;
    void processGrayscale() override;

    Processor::Type getProcessorType() const noexcept override;
    std::string getProcessorInfo() const override;

    static void initOpenCL();
};

#endif // ENABLE_OPENCL

#endif // !ANIME4KCPP_CORE_OPENCL_ANIME4K09_HPP
