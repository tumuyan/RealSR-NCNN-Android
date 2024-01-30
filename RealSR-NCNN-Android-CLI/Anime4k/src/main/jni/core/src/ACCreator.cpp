#include "ACCreator.hpp"

#define AC_CASE_UP_ITEM
#include "ACRegister.hpp"
#undef AC_CASE_UP_ITEM
std::unique_ptr<Anime4KCPP::AC> Anime4KCPP::ACCreator::createUP(
    const Parameters& parameters, const Processor::Type type
)
{
    switch (type)
    {
        PROCESSOR_CASE_UP
    }
    return nullptr;
}

#define AC_CASE_ITEM
#include "ACRegister.hpp"
#undef AC_CASE_ITEM
Anime4KCPP::AC* Anime4KCPP::ACCreator::create(
    const Parameters& parameters, const Processor::Type type
)
{
    switch (type)
    {
        PROCESSOR_CASE
    }
    return nullptr;
}

void Anime4KCPP::ACCreator::release(AC* ac) noexcept
{
    if (ac != nullptr)
    {
        delete ac;
        ac = nullptr;
    }
}
