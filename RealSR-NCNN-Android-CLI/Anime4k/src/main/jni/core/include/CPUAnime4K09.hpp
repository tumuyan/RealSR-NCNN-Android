#ifndef ANIME4KCPP_CORE_CPU_ANIME4K09_HPP
#define ANIME4KCPP_CORE_CPU_ANIME4K09_HPP

#include "AC.hpp"

namespace Anime4KCPP::CPU
{
    class AC_EXPORT Anime4K09;
}

class Anime4KCPP::CPU::Anime4K09 :public AC
{
public:
    using AC::AC;

    std::string getInfo() const override;
    std::string getFiltersInfo() const override;
private:
    void processYUVImage() override;
    void processRGBImage() override;
    void processGrayscale() override;

    Processor::Type getProcessorType() const noexcept override;
    std::string getProcessorInfo() const override;
};

#endif // !ANIME4KCPP_CORE_CPU_ANIME4K09_HPP
