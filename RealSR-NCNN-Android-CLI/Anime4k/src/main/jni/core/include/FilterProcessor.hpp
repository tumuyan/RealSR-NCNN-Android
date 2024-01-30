#ifndef ANIME4KCPP_CORE_FILTER_PROCESSOR_HPP
#define ANIME4KCPP_CORE_FILTER_PROCESSOR_HPP

#include "AC.hpp"

namespace Anime4KCPP
{
    class FilterProcessor;
}

class Anime4KCPP::FilterProcessor
{
public:
    FilterProcessor(cv::Mat& srcImg, std::uint8_t filters);
    void process();
    static std::vector<std::string> filterToString(std::uint8_t filters);
private:
    void CASSharpening(cv::Mat& src);
private:
    int H, W;
    std::uint8_t filters;
    cv::Mat& srcImgRef;
};

#endif // !ANIME4KCPP_CORE_FILTER_PROCESSOR_HPP
