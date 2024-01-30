#ifndef ANIME4KCPP_CORE_AC_EXCEPTION_HPP
#define ANIME4KCPP_CORE_AC_EXCEPTION_HPP

#include <string>
#include <string_view>
#include <stdexcept>

#define TYPE_ITEM(Name)\
struct Name\
{\
    static constexpr const char* string = #Name;\
}

namespace Anime4KCPP
{
    namespace ExceptionType
    {
        TYPE_ITEM(IO);
        TYPE_ITEM(RunTimeError);
        TYPE_ITEM(GPU);
    }

    template<typename exceptionType, bool addtlInfo = false>
    class ACException;
}

template<typename exceptionType>
class Anime4KCPP::ACException<exceptionType, true> :public std::runtime_error
{
public:
    ACException(std::string_view errMsg, int addltErrCode);
    ACException(std::string_view errMsg, std::string_view addtlInfo, int addltErrCode);
};

template<typename exceptionType>
Anime4KCPP::ACException<exceptionType, true>::ACException(std::string_view errMsg, const int addltErrCode) :
    std::runtime_error(
        std::string{ "An error occurred.\n\nError type: " }
        .append(exceptionType::string)
        .append("\n\nError message:\n")
        .append(errMsg)
        .append("\n\nAdditional error code:\n")
        .append(std::to_string(addltErrCode))
        .append("\n")
    ) {}

template<typename exceptionType>
Anime4KCPP::ACException<exceptionType, true>::ACException(std::string_view errMsg, std::string_view addtlInfo, const int addltErrCode) :
    std::runtime_error(
        std::string{ "An error occurred.\n\nError type: " }
        .append(exceptionType::string)
        .append("\n\nError message:\n")
        .append(errMsg)
        .append("\n\nAdditional error code:\n")
        .append(std::to_string(addltErrCode))
        .append("\n\nAdditional information:\n")
        .append(addtlInfo)
        .append("\n")
    ) {}

template<typename exceptionType>
class Anime4KCPP::ACException<exceptionType, false> :public std::runtime_error
{
public:
    ACException(std::string_view errMsg);
};

template<typename exceptionType>
Anime4KCPP::ACException<exceptionType, false>::ACException(std::string_view errMsg) :
    std::runtime_error(
        std::string{ "An error occurred.\n\nError type: " }
        .append(exceptionType::string)
        .append("\n\nError message:\n")
        .append(errMsg)
        .append("\n")
    ) {}
#endif // !ANIME4KCPP_CORE_AC_EXCEPTION_HPP
