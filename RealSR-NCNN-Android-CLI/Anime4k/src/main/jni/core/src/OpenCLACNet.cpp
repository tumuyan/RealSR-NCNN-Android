#ifdef ENABLE_OPENCL

#include <fstream>
#include <atomic>

#define CL_HPP_ENABLE_EXCEPTIONS
#ifdef LEGACY_OPENCL_API
#define CL_HPP_TARGET_OPENCL_VERSION 120
#else
#define CL_HPP_TARGET_OPENCL_VERSION 200
#endif // LEGACY_OPENCL_API
#define CL_HPP_MINIMUM_OPENCL_VERSION 110
#include <CL/opencl.hpp>

#include "ACNetType.hpp"
#include "OpenCLACNet.hpp"
#include "OpenCLACNetKernel.hpp"

#define ALIGN_UP(x, size) (((x) + (size) - 1) & (~((size) - 1)))

namespace Anime4KCPP::OpenCL::detail
{
    static constexpr int L2 = 0, L3 = 1, L4 = 2, L5 = 3, L6 = 4, L7 = 5, L8 = 6, L9 = 7;

    //init OpenCL arguments
    static bool isInitializedFlag = false;
    static cl::Program program[Anime4KCPP::ACNetType::TotalTypeCount];
    static cl::Device device;
    static cl::Context context;
    static cl::CommandQueue commandQueueIO;
    static std::size_t commandQueueNum = 4;
    static std::atomic<std::size_t> commandQueueCount = 0;
    static std::vector<cl::CommandQueue> commandQueueList;
    static bool parallelIO = false;
    static int pID = 0;
    static int dID = 0;
    static std::size_t workGroupSizeBase = 32;

    [[maybe_unused]]
    static std::string readKernel(const std::string& fileName)
    {
        std::ifstream kernelFile("kernels/" + fileName);
        if (!kernelFile.is_open())
            throw ACException<ExceptionType::IO>("Failed to open kernel file.");

        std::ostringstream source;
        source << kernelFile.rdbuf();

        return source.str();
    }

    static void runKernelN(const cv::Mat& orgImg, cv::Mat& dstImg, int channelType, int index)
    {
        static constexpr std::array<cl::size_type, 3> start = { 0,0,0 };
        const std::array<cl::size_type, 3> orgRegion = { static_cast<cl::size_type>(orgImg.cols),static_cast<cl::size_type>(orgImg.rows),1 };
        const std::array<cl::size_type, 3> dstRegion = { static_cast<cl::size_type>(dstImg.cols),static_cast<cl::size_type>(dstImg.rows),1 };
        const std::array<cl::size_type, 2> orgSize =
        {
            ALIGN_UP(orgImg.cols, workGroupSizeBase),
            ALIGN_UP(orgImg.rows, workGroupSizeBase)
        };
        const std::array<cl::size_type, 2> dstSize =
        {
            ALIGN_UP(dstImg.cols, workGroupSizeBase),
            ALIGN_UP(dstImg.rows, workGroupSizeBase)
        };

        try
        {
            cl::CommandQueue& commandQueue = commandQueueList[commandQueueCount++ % commandQueueNum];

            //kernel for each thread
            cl::Kernel kernelConv1To8L1(program[index], "conv1To8");
            cl::Kernel kernelConv8To8L2(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L3(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L4(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L5(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L6(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L7(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L8(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L9(program[index], "conv8To8");
            cl::Kernel kernelConvTranspose8To1L10(program[index], "convTranspose8To1");

            cl::ImageFormat imageFormat(CL_R, channelType);
            cl::ImageFormat tmpFormat(CL_RGBA, CL_HALF_FLOAT);

            cl::Image2D imageBufferOrg(context, CL_MEM_READ_ONLY, imageFormat, orgImg.cols, orgImg.rows);
            cl::Image2DArray imageBufferTmp1(context, CL_MEM_READ_WRITE, tmpFormat, 2, orgImg.cols, orgImg.rows, 0, 0);
            cl::Image2DArray imageBufferTmp2(context, CL_MEM_READ_WRITE, tmpFormat, 2, orgImg.cols, orgImg.rows, 0, 0);
            cl::Image2D imageBufferDst(context, CL_MEM_WRITE_ONLY, imageFormat, dstImg.cols, dstImg.rows);

            kernelConv1To8L1.setArg(0, sizeof(cl_mem), &imageBufferOrg);
            kernelConv1To8L1.setArg(1, sizeof(cl_mem), &imageBufferTmp1);

            kernelConv8To8L2.setArg(0, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L2.setArg(1, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L2.setArg(2, sizeof(cl_int), &L2);

            kernelConv8To8L3.setArg(0, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L3.setArg(1, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L3.setArg(2, sizeof(cl_int), &L3);

            kernelConv8To8L4.setArg(0, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L4.setArg(1, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L4.setArg(2, sizeof(cl_int), &L4);

            kernelConv8To8L5.setArg(0, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L5.setArg(1, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L5.setArg(2, sizeof(cl_int), &L5);

            kernelConv8To8L6.setArg(0, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L6.setArg(1, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L6.setArg(2, sizeof(cl_int), &L6);

            kernelConv8To8L7.setArg(0, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L7.setArg(1, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L7.setArg(2, sizeof(cl_int), &L7);

            kernelConv8To8L8.setArg(0, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L8.setArg(1, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L8.setArg(2, sizeof(cl_int), &L8);

            kernelConv8To8L9.setArg(0, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L9.setArg(1, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L9.setArg(2, sizeof(cl_int), &L9);

            kernelConvTranspose8To1L10.setArg(0, sizeof(cl_mem), &imageBufferTmp1);
            kernelConvTranspose8To1L10.setArg(1, sizeof(cl_mem), &imageBufferDst);

            commandQueue.enqueueWriteImage(imageBufferOrg, CL_FALSE, start, orgRegion, orgImg.step, 0, orgImg.data);
            commandQueue.enqueueNDRangeKernel(kernelConv1To8L1, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L2, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L3, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L4, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L5, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L6, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L7, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L8, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L9, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConvTranspose8To1L10, cl::NullRange, cl::NDRange(dstSize[0], dstSize[1]));
            commandQueue.enqueueReadImage(imageBufferDst, CL_TRUE, start, dstRegion, dstImg.step, 0, dstImg.data);
        }
        catch (const cl::Error& e)
        {
            throw ACException<ExceptionType::GPU, true>("Failed to run OpenCL ACNet kernel", e.what(), e.err());
        }
    }

    static void runKernelP(const cv::Mat& orgImg, cv::Mat& dstImg, int channelType, int index)
    {
        static constexpr std::array<cl::size_type, 3> start = { 0,0,0 };
        const std::array<cl::size_type, 3> orgRegion = { static_cast<cl::size_type>(orgImg.cols),static_cast<cl::size_type>(orgImg.rows),1 };
        const std::array<cl::size_type, 3> dstRegion = { static_cast<cl::size_type>(dstImg.cols),static_cast<cl::size_type>(dstImg.rows),1 };
        const std::array<cl::size_type, 2> orgSize =
        {
            ALIGN_UP(orgImg.cols, workGroupSizeBase),
            ALIGN_UP(orgImg.rows, workGroupSizeBase)
        };
        const std::array<cl::size_type, 2> dstSize =
        {
            ALIGN_UP(dstImg.cols, workGroupSizeBase),
            ALIGN_UP(dstImg.rows, workGroupSizeBase)
        };

        try
        {
            cl::CommandQueue& commandQueue = commandQueueList[commandQueueCount++ % commandQueueNum];

            //kernel for each thread
            cl::Kernel kernelConv1To8L1(program[index], "conv1To8");
            cl::Kernel kernelConv8To8L2(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L3(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L4(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L5(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L6(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L7(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L8(program[index], "conv8To8");
            cl::Kernel kernelConv8To8L9(program[index], "conv8To8");
            cl::Kernel kernelConvTranspose8To1L10(program[index], "convTranspose8To1");

            cl::ImageFormat imageFormat(CL_R, channelType);
            cl::ImageFormat tmpFormat(CL_RGBA, CL_HALF_FLOAT);

            cl::Image2D imageBufferOrg(context, CL_MEM_READ_ONLY, imageFormat, orgImg.cols, orgImg.rows);
            cl::Image2DArray imageBufferTmp1(context, CL_MEM_READ_WRITE, tmpFormat, 2, orgImg.cols, orgImg.rows, 0, 0);
            cl::Image2DArray imageBufferTmp2(context, CL_MEM_READ_WRITE, tmpFormat, 2, orgImg.cols, orgImg.rows, 0, 0);
            cl::Image2D imageBufferDst(context, CL_MEM_WRITE_ONLY, imageFormat, dstImg.cols, dstImg.rows);

            kernelConv1To8L1.setArg(0, sizeof(cl_mem), &imageBufferOrg);
            kernelConv1To8L1.setArg(1, sizeof(cl_mem), &imageBufferTmp1);

            kernelConv8To8L2.setArg(0, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L2.setArg(1, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L2.setArg(2, sizeof(cl_int), &L2);

            kernelConv8To8L3.setArg(0, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L3.setArg(1, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L3.setArg(2, sizeof(cl_int), &L3);

            kernelConv8To8L4.setArg(0, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L4.setArg(1, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L4.setArg(2, sizeof(cl_int), &L4);

            kernelConv8To8L5.setArg(0, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L5.setArg(1, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L5.setArg(2, sizeof(cl_int), &L5);

            kernelConv8To8L6.setArg(0, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L6.setArg(1, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L6.setArg(2, sizeof(cl_int), &L6);

            kernelConv8To8L7.setArg(0, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L7.setArg(1, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L7.setArg(2, sizeof(cl_int), &L7);

            kernelConv8To8L8.setArg(0, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L8.setArg(1, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L8.setArg(2, sizeof(cl_int), &L8);

            kernelConv8To8L9.setArg(0, sizeof(cl_mem), &imageBufferTmp2);
            kernelConv8To8L9.setArg(1, sizeof(cl_mem), &imageBufferTmp1);
            kernelConv8To8L9.setArg(2, sizeof(cl_int), &L9);

            kernelConvTranspose8To1L10.setArg(0, sizeof(cl_mem), &imageBufferTmp1);
            kernelConvTranspose8To1L10.setArg(1, sizeof(cl_mem), &imageBufferDst);


            std::vector<cl::Event> waitForWriteFinishedEvent(1);
            std::vector<cl::Event> waitForReadReadyEvent(1);
            cl::Event& writeFinishedEvent = waitForWriteFinishedEvent.front();
            cl::Event& readReadyEvent = waitForReadReadyEvent.front();

            commandQueueIO.enqueueWriteImage(imageBufferOrg, CL_FALSE, start, orgRegion, orgImg.step, 0, orgImg.data, nullptr, &writeFinishedEvent);
            commandQueue.enqueueNDRangeKernel(kernelConv1To8L1, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]), cl::NullRange, &waitForWriteFinishedEvent);
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L2, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L3, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L4, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L5, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L6, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L7, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L8, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConv8To8L9, cl::NullRange, cl::NDRange(orgSize[0], orgSize[1]));
            commandQueue.enqueueNDRangeKernel(kernelConvTranspose8To1L10, cl::NullRange, cl::NDRange(dstSize[0], dstSize[1]), cl::NullRange, nullptr, &readReadyEvent);
            commandQueueIO.enqueueReadImage(imageBufferDst, CL_TRUE, start, dstRegion, dstImg.step, 0, dstImg.data, &waitForReadReadyEvent);
        }
        catch (const cl::Error& e)
        {
            throw ACException<ExceptionType::GPU, true>("Failed to run OpenCL ACNet kernel", e.what(), e.err());
        }
    }

    static void runKernel(const cv::Mat& orgImg, cv::Mat& dstImg, int scaleTimes, int index)
    {
        int channelType;
        switch (orgImg.depth())
        {
        case CV_8U:
            channelType = CL_UNORM_INT8;
            break;
        case CV_16U:
            channelType = CL_UNORM_INT16;
            break;
        case CV_32F:
            channelType = CL_FLOAT;
            break;
        default:
            throw ACException<ExceptionType::RunTimeError>("Unsupported image data type");
        }

        cv::Mat tmpImg = orgImg;
        for (int i = 0; i < scaleTimes; i++)
        {
            dstImg.create(tmpImg.rows * 2, tmpImg.cols * 2, tmpImg.type());
            if (parallelIO)
                runKernelP(tmpImg, dstImg, channelType, index);
            else
                runKernelN(tmpImg, dstImg, channelType, index);
            tmpImg = dstImg;
        }
    }
}

Anime4KCPP::OpenCL::ACNet::ACNet(const Parameters& parameters) :
    AC(parameters), ACNetTypeIndex(GET_ACNET_TYPE_INDEX(param.HDN, param.HDNLevel)) {}

void Anime4KCPP::OpenCL::ACNet::setParameters(const Parameters& parameters)
{
    AC::setParameters(parameters);
    ACNetTypeIndex = GET_ACNET_TYPE_INDEX(param.HDN, param.HDNLevel);
}

void Anime4KCPP::OpenCL::ACNet::init(const int platformID, const int deviceID, const CNNType type, const int OpenCLQueueNum, const bool OpenCLParallelIO)
{
    if (!detail::isInitializedFlag)
    {
        detail::pID = platformID;
        detail::dID = deviceID;
        detail::commandQueueNum = OpenCLQueueNum > 1 ? OpenCLQueueNum : 1;
        detail::parallelIO = OpenCLParallelIO;
        initOpenCL(type);
        detail::isInitializedFlag = true;
    }
}

void Anime4KCPP::OpenCL::ACNet::release() noexcept
{
    if (detail::isInitializedFlag)
    {
        detail::context = nullptr;
        std::fill(detail::commandQueueList.begin(), detail::commandQueueList.end(), nullptr);
        detail::commandQueueIO = nullptr;
        for (int i = HDNL0; i < TotalTypeCount; i++)
            detail::program[i] = nullptr;
        detail::device = nullptr;
        detail::isInitializedFlag = false;
    }
}

bool Anime4KCPP::OpenCL::ACNet::isInitialized() noexcept
{
    return detail::isInitializedFlag;
}

void Anime4KCPP::OpenCL::ACNet::processYUVImage()
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

void Anime4KCPP::OpenCL::ACNet::processRGBImage()
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

void Anime4KCPP::OpenCL::ACNet::processGrayscale()
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

std::string Anime4KCPP::OpenCL::ACNet::getInfo() const
{
    std::ostringstream oss;
    oss << AC::getInfo()
        << "----------------------------------------------" << '\n'
        << "OpenCL Platform ID:" << detail::pID << '\n'
        << "OpenCL Device ID:" << detail::dID << '\n'
        << "Zoom Factor: " << param.zoomFactor << '\n'
        << "HDN Mode: " << std::boolalpha << param.HDN << '\n'
        << "HDN Level: " << (param.HDN ? param.HDNLevel : 0) << '\n'
        << "Number of OpenCL Command Queues:" << detail::commandQueueNum << '\n'
        << "OpenCL Parallel IO Command Queues:" << std::boolalpha << detail::parallelIO << '\n'
        << "----------------------------------------------" << '\n';
    return oss.str();
}

std::string Anime4KCPP::OpenCL::ACNet::getFiltersInfo() const
{
    std::ostringstream oss;
    oss << AC::getFiltersInfo()
        << "----------------------------------------------" << '\n'
        << "Filter not supported" << '\n'
        << "----------------------------------------------" << '\n';
    return oss.str();
}

void Anime4KCPP::OpenCL::ACNet::initOpenCL(const CNNType type)
{
    std::vector<cl::Platform> platforms;
    std::vector<cl::Device> devices;

    try
    {
        cl::Platform::get(&platforms);
        ((detail::pID >= 0 && detail::pID < platforms.size()) ? platforms[detail::pID] : platforms[0])
            .getDevices(CL_DEVICE_TYPE_ALL, &devices);

        detail::device = (detail::dID >= 0 && detail::dID < devices.size()) ? devices[detail::dID] : devices[0];

        detail::context = cl::Context(detail::device);

        detail::commandQueueList.resize(detail::commandQueueNum);
        for (std::size_t i = 0; i < detail::commandQueueNum; i++)
        {
            detail::commandQueueList[i] = cl::CommandQueue(detail::context, detail::device);
        }
        if (detail::parallelIO)
        {
            detail::commandQueueIO = cl::CommandQueue(detail::context, detail::device);
        }

#ifndef BUILT_IN_KERNEL
        //read kernel files
        std::string ACNetKernelSourceString[TotalTypeCount];
        std::string kernelFiles[TotalTypeCount] =
        { "ACNetKernel.cl", "ACNetHDNL1Kernel.cl" ,"ACNetHDNL2Kernel.cl" ,"ACNetHDNL3Kernel.cl" };
#endif // BUILT_IN_KERNEL

#ifdef ENABLE_FAST_MATH
        const char* buildFlags = "-cl-fast-relaxed-math";
#else
        const char* buildFlags = nullptr;
#endif // ENABLE_FAST_MATH

        std::vector<ACNetType> types;

        switch (type)
        {
        case CNNType::ACNetHDNL0:
            types.emplace_back(ACNetType::HDNL0);
            break;
        case CNNType::ACNetHDNL1:
            types.emplace_back(ACNetType::HDNL1);
            break;
        case CNNType::ACNetHDNL2:
            types.emplace_back(ACNetType::HDNL2);
            break;
        case CNNType::ACNetHDNL3:
            types.emplace_back(ACNetType::HDNL3);
            break;
        case CNNType::Default:
        default:
            for (int i = HDNL0; i < TotalTypeCount; i++)
                types.emplace_back(static_cast<ACNetType>(i));
            break;
        }

        std::for_each(types.begin(), types.end(), 
            [&](ACNetType type) 
            {
#ifndef BUILT_IN_KERNEL
                //read kernel files
                ACNetKernelSourceString[type] = detail::readKernel(kernelFiles[type]);
#endif // BUILT_IN_KERNEL
                detail::program[type] = cl::Program(detail::context, ACNetKernelSourceString[type]);
                try
                {
                    detail::program[type].build(detail::device, buildFlags);
                }
                catch (const cl::BuildError& e)
                {
                    throw ACException<ExceptionType::GPU, true>("Kernel build error", e.getBuildLog().front().second, e.err());
                }
            });

        cl::Kernel tmpKernel{ detail::program[types.front()], "conv8To8" };
        tmpKernel.getWorkGroupInfo(detail::device, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, &detail::workGroupSizeBase);
    }
    catch (const cl::Error& e)
    {
        throw ACException<ExceptionType::GPU, true>("Failed to initialize OpenCL", e.what(), e.err());
    }
}

Anime4KCPP::Processor::Type Anime4KCPP::OpenCL::ACNet::getProcessorType() const noexcept
{
    return Processor::Type::OpenCL_ACNet;
}

std::string Anime4KCPP::OpenCL::ACNet::getProcessorInfo() const
{
    std::ostringstream oss;
    oss << "Processor type: " << getProcessorType() << '\n'
        << "Current OpenCL devices:" << '\n'
        << " " + detail::device.getInfo<CL_DEVICE_NAME>();
    return oss.str();
}

#endif
