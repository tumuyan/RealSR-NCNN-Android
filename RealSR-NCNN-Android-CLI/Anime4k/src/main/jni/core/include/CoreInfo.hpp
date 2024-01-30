#ifndef ANIME4KCPP_CORE_INFO_HPP
#define ANIME4KCPP_CORE_INFO_HPP

#include <ac_export.h>

namespace Anime4KCPP
{
	struct AC_EXPORT CoreInfo;
}

struct Anime4KCPP::CoreInfo
{
	static const char* version();
	static const char* CPUOptimizationMode();
	static const char* supportedProcessors();
};

#endif // !ANIME4KCPP_CORE_INFO_HPP