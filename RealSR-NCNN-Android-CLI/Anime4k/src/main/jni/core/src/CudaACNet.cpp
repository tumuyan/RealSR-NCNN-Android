#ifdef ENABLE_CUDA

#include "CudaInterface.hpp"
#include "ACNetType.hpp"
#include "CudaACNet.hpp"

namespace Anime4KCPP::Cuda::detail
{
    static void runKernel(const cv::Mat& orgImg, cv::Mat& dstImg, int scaleTimes, int index)
    {
        ACCudaDataType dataType;

        switch (orgImg.depth())
        {
        case CV_8U:
            dataType = ACCudaDataType::AC_8U;
            break;
        case CV_16U:
            dataType = ACCudaDataType::AC_16U;
            break;
        case CV_32F:
            dataType = ACCudaDataType::AC_32F;
            break;
        default:
            throw ACException<ExceptionType::RunTimeError>("Unsupported image data type");
        }

        void (*cuRunKernelACNet)(const void*, void*, ACCudaDataType, ACCudaParamACNet*);

        switch (index)
        {
        case 0:
            cuRunKernelACNet = cuRunKernelACNetHDN0;
            break;
        case 1:
            cuRunKernelACNet = cuRunKernelACNetHDN1;
            break;
        case 2:
            cuRunKernelACNet = cuRunKernelACNetHDN2;
            break;
        case 3:
            cuRunKernelACNet = cuRunKernelACNetHDN3;
            break;
        default:
            cuRunKernelACNet = cuRunKernelACNetHDN1;
            break;
        }

        cv::Mat tmpImg = orgImg;
        for (int i = 0; i < scaleTimes; i++)
        {
            dstImg.create(tmpImg.rows * 2, tmpImg.cols * 2, tmpImg.type());

            ACCudaParamACNet cuParam{ tmpImg.cols, tmpImg.rows, tmpImg.step };

            cuRunKernelACNet(tmpImg.data, dstImg.data, dataType, &cuParam);

            tmpImg = dstImg;
        }
    }
}

Anime4KCPP::Cuda::ACNet::ACNet(const Parameters& parameters) :
    AC(parameters), ACNetTypeIndex(GET_ACNET_TYPE_INDEX(param.HDN, param.HDNLevel)) {}

void Anime4KCPP::Cuda::ACNet::setParameters(const Parameters& parameters)
{
    AC::setParameters(parameters);
    ACNetTypeIndex = GET_ACNET_TYPE_INDEX(param.HDN, param.HDNLevel);
}

std::string Anime4KCPP::Cuda::ACNet::getInfo() const
{
    std::ostringstream oss;
    oss << AC::getInfo()
        << "----------------------------------------------" << '\n'
        << "CUDA Device ID: " << cuGetDeviceID() << '\n'
        << "Zoom Factor: " << param.zoomFactor << '\n'
        << "HDN Mode: " << std::boolalpha << param.HDN << '\n'
        << "HDN Level: " << (param.HDN ? param.HDNLevel : 0) << '\n'
        << "----------------------------------------------" << '\n';
    return oss.str();
}

std::string Anime4KCPP::Cuda::ACNet::getFiltersInfo() const
{
    std::ostringstream oss;
    oss << AC::getFiltersInfo()
        << "----------------------------------------------" << '\n'
        << "Filter not supported" << '\n'
        << "----------------------------------------------" << '\n';
    return oss.str();
}

void Anime4KCPP::Cuda::ACNet::processYUVImage()
{
    if (!param.fastMode)
    {
        int scaleTimes = Utils::fastCeilLog2(param.zoomFactor);
        if (!scaleTimes)
            scaleTimes++;

        detail::runKernel(orgImg, dstImg, scaleTimes, ACNetTypeIndex);

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

        cv::Mat outMat;
        detail::runKernel(tmpImg, outMat, 1, ACNetTypeIndex);
        dstImg = outMat;

        cv::resize(orgU, dstU, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
        cv::resize(orgV, dstV, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
    }
}

void Anime4KCPP::Cuda::ACNet::processRGBImage()
{
    if (!param.fastMode)
    {
        int scaleTimes = Utils::fastCeilLog2(param.zoomFactor);
        if (!scaleTimes)
            scaleTimes++;

        cv::Mat tmpImg;
        cv::cvtColor(orgImg, tmpImg, cv::COLOR_BGR2YUV);

        cv::Mat yuv[3];
        cv::split(tmpImg, yuv);

        detail::runKernel(yuv[Y], dstImg, scaleTimes, ACNetTypeIndex);

        if (param.isNonIntegerScale())
        {
            cv::resize(dstImg, dstImg, cv::Size(width, height), 0.0, 0.0, cv::INTER_AREA);
        }

        cv::resize(yuv[U], yuv[U], cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
        cv::resize(yuv[V], yuv[V], cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);

        cv::merge(std::vector<cv::Mat>{ dstImg, yuv[U], yuv[V] }, dstImg);
        cv::cvtColor(dstImg, dstImg, cv::COLOR_YUV2BGR);
    }
    else
    {
        cv::Mat tmpImg;
        cv::cvtColor(orgImg, tmpImg, cv::COLOR_BGR2YUV);

        if (param.zoomFactor > 2.0)
            cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor / 2.0, param.zoomFactor / 2.0, cv::INTER_CUBIC);
        else if (param.zoomFactor < 2.0)
            cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor / 2.0, param.zoomFactor / 2.0, cv::INTER_AREA);

        cv::Mat yuv[3];
        cv::split(tmpImg, yuv);

        cv::Mat outMat;
        detail::runKernel(yuv[Y], outMat, 1, ACNetTypeIndex);

        cv::resize(yuv[U], yuv[U], cv::Size(0, 0), 2.0, 2.0, cv::INTER_CUBIC);
        cv::resize(yuv[V], yuv[V], cv::Size(0, 0), 2.0, 2.0, cv::INTER_CUBIC);

        cv::merge(std::vector<cv::Mat>{ outMat, yuv[U], yuv[V] }, dstImg);
        cv::cvtColor(dstImg, dstImg, cv::COLOR_YUV2BGR);
    }
}

void Anime4KCPP::Cuda::ACNet::processGrayscale()
{
    if (!param.fastMode)
    {
        int scaleTimes = Utils::fastCeilLog2(param.zoomFactor);
        if (!scaleTimes)
            scaleTimes++;

        detail::runKernel(orgImg, dstImg, scaleTimes, ACNetTypeIndex);

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

        cv::Mat outMat;
        detail::runKernel(tmpImg, outMat, 1, ACNetTypeIndex);
        dstImg = outMat;
    }
}

Anime4KCPP::Processor::Type Anime4KCPP::Cuda::ACNet::getProcessorType() const noexcept
{
    return Processor::Type::Cuda_ACNet;
}

std::string Anime4KCPP::Cuda::ACNet::getProcessorInfo() const
{
    std::ostringstream oss;
    oss << "Processor type: " << getProcessorType() << '\n'
        << "Current CUDA devices:" << '\n'
        << cuGetDeviceInfo(cuGetDeviceID());
    return oss.str();
}

#endif
