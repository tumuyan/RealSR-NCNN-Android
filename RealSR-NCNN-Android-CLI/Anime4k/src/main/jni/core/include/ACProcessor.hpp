#ifndef ANIME4KCPP_CORE_AC_PROCESSOR_HPP
#define ANIME4KCPP_CORE_AC_PROCESSOR_HPP

#include <sstream>

namespace Anime4KCPP
{
    namespace Processor
    {
        enum class Type;

        std::ostream& operator<< (std::ostream& stream, Anime4KCPP::Processor::Type type);
    }
}

#define AC_ENUM_ITEM
#include "ACRegister.hpp"
#undef AC_ENUM_ITEM
enum class Anime4KCPP::Processor::Type
{
    PROCESSOR_ENUM
};

#define AC_STREAM_ITEM
#include "ACRegister.hpp"
#undef AC_STREAM_ITEM
inline std::ostream& Anime4KCPP::Processor::operator<<(std::ostream& stream, Anime4KCPP::Processor::Type type)
{
    switch (type)
    {
        PROCESSOR_STREAM

    default:
        stream << "Error processor type";
        break;
    }
    return stream;
}

#endif // !ANIME4KCPP_CORE_AC_PROCESSOR_HPP
