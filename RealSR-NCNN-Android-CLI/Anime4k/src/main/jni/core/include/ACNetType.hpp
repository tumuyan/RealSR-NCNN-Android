#ifndef ANIME4KCPP_CORE_ACNET_TYPE_HPP
#define ANIME4KCPP_CORE_ACNET_TYPE_HPP

#define GET_ACNET_TYPE_INDEX(HDN, HDNLevel) (HDN) ? (((HDNLevel) > 3 || (HDNLevel) < 1) ? 1 : HDNLevel) : 0

namespace Anime4KCPP
{
    enum ACNetType
    {
        HDNL0 = 0,
        HDNL1,
        HDNL2,
        HDNL3,
        TotalTypeCount
    };
}

#endif // !ANIME4KCPP_CORE_ACNET_TYPE_HPP
