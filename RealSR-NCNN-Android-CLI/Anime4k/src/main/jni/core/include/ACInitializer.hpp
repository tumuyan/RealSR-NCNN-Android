#ifndef ANIME4KCPP_CORE_AC_INITIALIZER_HPP
#define ANIME4KCPP_CORE_AC_INITIALIZER_HPP

#include <vector>
#include <string>
#include <memory>
#include <cstddef>

#include "ACManager.hpp"

namespace Anime4KCPP
{
    class ACInitializer;
}

class Anime4KCPP::ACInitializer
{
public:
    ACInitializer() = default;
    ACInitializer(const ACInitializer&) = delete;
    ACInitializer(ACInitializer&&) = delete;
    ~ACInitializer();

    template<typename Manager, typename... Types>
    void pushManager(Types&&... args);

    std::size_t init();
    void release(bool clearManagerList = false);

    std::size_t size();
    const std::vector<std::string>& failure();
private:

    std::vector<std::unique_ptr<Processor::Manager>> managers;
    std::vector<std::string> failures;
};

template<typename Manager, typename... Types>
inline void Anime4KCPP::ACInitializer::pushManager(Types&&... args)
{
    managers.emplace_back(std::make_unique<Manager>(std::forward<Types>(args)...));
}

inline Anime4KCPP::ACInitializer::~ACInitializer()
{
    release(true);
}

inline std::size_t Anime4KCPP::ACInitializer::init()
{
    std::size_t count = 0;

    if(!failures.empty())
        failures.clear();

    for (auto& manager : managers)
    {
        if (!manager->isSupport())
        {
            failures.emplace_back(manager->name());
            continue;
        }
        if (!manager->isInitialized())
        {
            try
            {
                manager->init();
            }
            catch (const std::exception& e)
            {
                failures.emplace_back(manager->name());
                failures.emplace_back(e.what());
                continue;
            }
        }
        count++;
    }

    return count;
}

inline void Anime4KCPP::ACInitializer::release(bool clearManagerList)
{
    if (!failures.empty())
        failures.clear();

    for (auto& manager : managers)
    {
        if (manager->isInitialized())
            manager->release();
    }
    if (clearManagerList)
        managers.clear();
}

inline std::size_t Anime4KCPP::ACInitializer::size()
{
    return managers.size();
}

inline const std::vector<std::string>& Anime4KCPP::ACInitializer::failure()
{
    return failures;
}

#endif // !ANIME4KCPP_CORE_AC_INITIALIZER_HPP
