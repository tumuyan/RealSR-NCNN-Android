#ifndef ANIME4KCPP_CORE_CUDA_ACNET_HPP
#define ANIME4KCPP_CORE_CUDA_ACNET_HPP

#ifdef ENABLE_CUDA

#include "AC.hpp"
#include "CNN.hpp"

namespace Anime4KCPP::Cuda
{
    class AC_EXPORT ACNet;
}

class Anime4KCPP::Cuda::ACNet :public AC
{
public:
    explicit ACNet(const Parameters& parameters);
    void setParameters(const Parameters& parameters) override;

    std::string getInfo() const override;
    std::string getFiltersInfo() const override;
private:
    void processYUVImage() override;
    void processRGBImage() override;
    void processGrayscale() override;

    Processor::Type getProcessorType() const noexcept override;
    std::string getProcessorInfo() const override;
private:
    int ACNetTypeIndex;
};

#endif // ENABLE_CUDA

#endif // !ANIME4KCPP_CORE_CUDA_ACNET_HPP
