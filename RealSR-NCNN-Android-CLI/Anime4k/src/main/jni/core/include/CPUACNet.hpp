#ifndef ANIME4KCPP_CORE_CPU_ACNET_HPP
#define ANIME4KCPP_CORE_CPU_ACNET_HPP

#include "CPUACNetProcessor.hpp"

namespace Anime4KCPP::CPU
{
    class AC_EXPORT ACNet;
}

class Anime4KCPP::CPU::ACNet :public AC
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
    std::unique_ptr<ACNetProcessor> processor;
};

#endif // !ANIME4KCPP_CORE_CPU_ACNET_HPP
