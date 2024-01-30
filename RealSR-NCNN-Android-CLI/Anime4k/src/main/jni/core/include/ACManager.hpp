#ifndef ANIME4KCPP_CORE_AC_MANAGER_HPP
#define ANIME4KCPP_CORE_AC_MANAGER_HPP

namespace Anime4KCPP::Processor
{
    class Manager;

    template<typename T>
    struct GetManager {};
}

class Anime4KCPP::Processor::Manager
{
public:
    virtual ~Manager() = default;

    virtual void init() = 0;
    virtual void release() noexcept = 0;
    virtual bool isInitialized() noexcept = 0;
    virtual bool isSupport() noexcept = 0;
    virtual const char* name() noexcept = 0;
};

#endif // !ANIME4KCPP_CORE_AC_MANAGER_HPP
