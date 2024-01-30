#ifndef ANIME4KCPP_CORE_CNN_HPP
#define ANIME4KCPP_CORE_CNN_HPP

namespace Anime4KCPP
{
    class CNNType;
}

class Anime4KCPP::CNNType
{
public:
    enum Value { Default, ACNetHDNL0, ACNetHDNL1, ACNetHDNL2, ACNetHDNL3 };

    CNNType() = default;
    constexpr CNNType(Value v) : value(v) { }

    explicit operator bool() = delete;
    constexpr operator Value() const { return value; }

    constexpr const char* toString() const
    {
        switch (value)
        {
        case Anime4KCPP::CNNType::Default:
            return "Default";
        case Anime4KCPP::CNNType::ACNetHDNL0:
            return "ACNetHDNL0";
        case Anime4KCPP::CNNType::ACNetHDNL1:
            return "ACNetHDNL1";
        case Anime4KCPP::CNNType::ACNetHDNL2:
            return "ACNetHDNL2";
        case Anime4KCPP::CNNType::ACNetHDNL3:
            return "ACNetHDNL3";
        default:
            return "ACNetHDNL0";
        }
    }

private:
    Value value;
};

#endif // !ANIME4KCPP_CORE_CNN_HPP
