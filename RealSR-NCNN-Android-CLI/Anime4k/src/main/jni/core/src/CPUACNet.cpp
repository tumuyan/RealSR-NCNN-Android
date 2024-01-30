#include "CPUACNet.hpp"
#include "ACNetType.hpp"

Anime4KCPP::CPU::ACNet::ACNet(const Parameters& parameters) :
    AC(parameters), processor(createACNetProcessor(GET_ACNET_TYPE_INDEX(param.HDN, param.HDNLevel))) {}

void Anime4KCPP::CPU::ACNet::setParameters(const Parameters& parameters)
{
    AC::setParameters(parameters);
    processor = createACNetProcessor(GET_ACNET_TYPE_INDEX(param.HDN, param.HDNLevel));
}

std::string Anime4KCPP::CPU::ACNet::getInfo() const
{
    std::ostringstream oss;
    oss << AC::getInfo()
        << "----------------------------------------------" << '\n'
        << "Zoom Factor: " << param.zoomFactor << '\n'
        << "HDN Mode: " << std::boolalpha << param.HDN << '\n'
        << "HDN Level: " << (param.HDN ? param.HDNLevel : 0) << '\n'
        << "----------------------------------------------" << '\n';
    return oss.str();
}

std::string Anime4KCPP::CPU::ACNet::getFiltersInfo() const
{
    std::ostringstream oss;
    oss << AC::getFiltersInfo()
        << "----------------------------------------------" << '\n'
        << "Filter not supported" << '\n'
        << "----------------------------------------------" << '\n';
    return oss.str();
}

void Anime4KCPP::CPU::ACNet::processYUVImage()
{
    if (!param.fastMode)
    {
        int scaleTimes = Utils::fastCeilLog2(param.zoomFactor);
        if (!scaleTimes)
            scaleTimes++;

        processor->process(orgImg, dstImg, scaleTimes);

        if (param.isNonIntegerScale())
        {
            cv::resize(dstImg, dstImg, cv::Size(width, height), 0.0, 0.0, cv::INTER_AREA);
        }

        cv::resize(orgU, dstU, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
        cv::resize(orgV, dstV, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
    }
    else
    {
        cv::Mat tmpImg = orgImg;

        if (param.zoomFactor > 2.0)
            cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor / 2.0, param.zoomFactor / 2.0, cv::INTER_CUBIC);
        else if (param.zoomFactor < 2.0)
            cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor / 2.0, param.zoomFactor / 2.0, cv::INTER_AREA);

        processor->process(tmpImg, dstImg, 1);

        cv::resize(orgU, dstU, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
        cv::resize(orgV, dstV, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
    }
}

void Anime4KCPP::CPU::ACNet::processRGBImage()
{
    if (!param.fastMode)
    {
        int scaleTimes = Utils::fastCeilLog2(param.zoomFactor);
        if (!scaleTimes)
            scaleTimes++;

        cv::Mat tmpImg;
        cv::cvtColor(orgImg, tmpImg, cv::COLOR_BGR2YUV);

        processor->process(tmpImg, dstImg, scaleTimes);

        if (param.isNonIntegerScale())
        {
            cv::resize(dstImg, dstImg, cv::Size(width, height), 0.0, 0.0, cv::INTER_AREA);
        }

        cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
        cv::mixChannels(dstImg, tmpImg, std::vector<int>{0, 0});

        cv::cvtColor(tmpImg, dstImg, cv::COLOR_YUV2BGR);
    }
    else
    {
        cv::Mat tmpImg;
        cv::cvtColor(orgImg, tmpImg, cv::COLOR_BGR2YUV);

        if (param.zoomFactor > 2.0)
            cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor / 2.0, param.zoomFactor / 2.0, cv::INTER_CUBIC);
        else if (param.zoomFactor < 2.0)
            cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor / 2.0, param.zoomFactor / 2.0, cv::INTER_AREA);

        processor->process(tmpImg, dstImg, 1);

        cv::resize(tmpImg, tmpImg, cv::Size(0, 0), 2.0, 2.0, cv::INTER_CUBIC);
        cv::mixChannels(dstImg, tmpImg, std::vector<int>{0, 0});

        cv::cvtColor(tmpImg, dstImg, cv::COLOR_YUV2BGR);
    }
}

void Anime4KCPP::CPU::ACNet::processGrayscale()
{
    if (!param.fastMode)
    {
        int scaleTimes = Utils::fastCeilLog2(param.zoomFactor);
        if (!scaleTimes)
            scaleTimes++;

        processor->process(orgImg, dstImg, scaleTimes);

        if (param.isNonIntegerScale())
        {
            cv::resize(dstImg, dstImg, cv::Size(width, height), 0.0, 0.0, cv::INTER_AREA);
        }
    }
    else
    {
        cv::Mat tmpImg = orgImg;

        if (param.zoomFactor > 2.0)
            cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor / 2.0, param.zoomFactor / 2.0, cv::INTER_CUBIC);
        else if (param.zoomFactor < 2.0)
            cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor / 2.0, param.zoomFactor / 2.0, cv::INTER_AREA);

        processor->process(tmpImg, dstImg, 1);
    }
}

Anime4KCPP::Processor::Type Anime4KCPP::CPU::ACNet::getProcessorType() const noexcept
{
    return Processor::Type::CPU_ACNet;
}

std::string Anime4KCPP::CPU::ACNet::getProcessorInfo() const
{
    std::ostringstream oss;
    oss << "Processor type: " << getProcessorType();
    return oss.str();
}
