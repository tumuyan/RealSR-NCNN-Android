#ifndef ANIME4KCPP_CORE_AC_CPU_HPP
#define ANIME4KCPP_CORE_AC_CPU_HPP

#include "CPUAnime4K09.hpp"
#include "CPUACNet.hpp"
#include "ACManager.hpp"

namespace Anime4KCPP
{
    namespace CPU
    {
        class Manager;
    }

    namespace Processor
    {
        template<>
        struct GetManager<CPU::ACNet> {
            using Manager = CPU::Manager;
        };
        template<>
        struct GetManager<CPU::Anime4K09> {
            using Manager = CPU::Manager;
        };
    }
}

class Anime4KCPP::CPU::Manager : public Anime4KCPP::Processor::Manager
{
public:
    Manager() = default;
    void init() override;
    void release() noexcept override;
    bool isInitialized() noexcept override;
    bool isSupport() noexcept override;
    const char* name() noexcept override { return "CPU Processor Manager"; };
};

inline void Anime4KCPP::CPU::Manager::init() {}

inline void Anime4KCPP::CPU::Manager::release() noexcept {}

inline bool Anime4KCPP::CPU::Manager::isInitialized() noexcept
{
    return true;
}

inline bool Anime4KCPP::CPU::Manager::isSupport() noexcept
{
    return true;
}

#endif // !ANIME4KCPP_CORE_AC_CPU_HPP
