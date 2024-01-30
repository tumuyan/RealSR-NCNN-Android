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

#include "FilterProcessor.hpp"
#include "OpenCLAnime4K09.hpp"
#include "OpenCLAnime4K09Kernel.hpp"

#define ALIGN_UP(x, size) (((x) + (size) - 1) & (~((size) - 1)))

namespace Anime4KCPP::OpenCL::detail
{
    //init OpenCL arguments
    static bool isInitializedFlag = false;
    static cl::Program program;
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

    static void runKernelN(const cv::Mat& orgImg, cv::Mat& dstImg, int channelType, const Parameters& param)
    {
        cl_float nWidth, nHeight;
        if (param.zoomFactor == 2.0)
        {
            nWidth = 1.0f / dstImg.cols;
            nHeight = 1.0f / dstImg.rows;
        }
        else
        {
            nWidth = static_cast<cl_float>(orgImg.cols) / dstImg.cols;
            nHeight = static_cast<cl_float>(orgImg.rows) / dstImg.rows;
        }

        static constexpr std::array<cl::size_type, 3> start = { 0,0,0 };
        const std::array<cl::size_type, 3> orgRegion = { static_cast<cl::size_type>(orgImg.cols),static_cast<cl::size_type>(orgImg.rows),1 };
        const std::array<cl::size_type, 3> dstRegion = { static_cast<cl::size_type>(dstImg.cols),static_cast<cl::size_type>(dstImg.rows),1 };
        const std::array<cl::size_type, 2> size =
        {
            ALIGN_UP(dstImg.cols, workGroupSizeBase),
            ALIGN_UP(dstImg.rows, workGroupSizeBase)
        };

        const cl_float pushColorStrength = static_cast<cl_float>(param.strengthColor);
        const cl_float pushGradientStrength = static_cast<cl_float>(param.strengthGradient);
        const cl_float normalizedWidth = nWidth;
        const cl_float normalizedHeight = nHeight;

        try
        {
            //init frame
            cl::CommandQueue& commandQueue = commandQueueList[commandQueueCount++ % commandQueueNum];

            //kernel for each thread
            cl::Kernel kernelGetGray(program, (param.zoomFactor == 2.0) ? "getGray" : "getGrayLanczos4");
            cl::Kernel kernelPushColor(program, "pushColor");
            cl::Kernel kernelGetGradient(program, "getGradient");
            cl::Kernel kernelPushGradient(program, "pushGradient");

            cl::ImageFormat imageFormat(CL_RGBA, channelType);
            cl::ImageFormat tmpFormat(CL_RGBA, channelType == CL_FLOAT ? CL_HALF_FLOAT : channelType);

            //imageBuffer
            cl::Image2D imageBuffer0(context, CL_MEM_READ_ONLY, imageFormat, orgImg.cols, orgImg.rows);
            cl::Image2D imageBuffer1(context, CL_MEM_READ_WRITE, imageFormat, dstImg.cols, dstImg.rows);
            cl::Image2D imageBuffer2(context, CL_MEM_READ_WRITE, tmpFormat, dstImg.cols, dstImg.rows);
            cl::Image2D imageBuffer3(context, CL_MEM_READ_WRITE, tmpFormat, dstImg.cols, dstImg.rows);

            //set arguments
            //getGray
            kernelGetGray.setArg(0, sizeof(cl_mem), &imageBuffer0);
            kernelGetGray.setArg(1, sizeof(cl_mem), &imageBuffer1);
            kernelGetGray.setArg(2, sizeof(cl_float), &normalizedWidth);
            kernelGetGray.setArg(3, sizeof(cl_float), &normalizedHeight);

            //pushColor
            kernelPushColor.setArg(0, sizeof(cl_mem), &imageBuffer1);
            kernelPushColor.setArg(1, sizeof(cl_mem), &imageBuffer2);
            kernelPushColor.setArg(2, sizeof(cl_float), &pushColorStrength);

            //getGradient
            kernelGetGradient.setArg(0, sizeof(cl_mem), &imageBuffer2);
            kernelGetGradient.setArg(1, sizeof(cl_mem), &imageBuffer3);

            //pushGradient
            kernelPushGradient.setArg(0, sizeof(cl_mem), &imageBuffer3);
            kernelPushGradient.setArg(1, sizeof(cl_mem), &imageBuffer1);
            kernelPushGradient.setArg(2, sizeof(cl_float), &pushGradientStrength);

            int i;
            //enqueue
            commandQueue.enqueueWriteImage(imageBuffer0, CL_FALSE, start, orgRegion, orgImg.step, 0, orgImg.data);
            commandQueue.enqueueNDRangeKernel(kernelGetGray, cl::NullRange, cl::NDRange(size[0], size[1]));
            for (i = 0; i < param.passes && i < param.pushColorCount; i++)//pcc for push color count
            {
                commandQueue.enqueueNDRangeKernel(kernelPushColor, cl::NullRange, cl::NDRange(size[0], size[1]));
                commandQueue.enqueueNDRangeKernel(kernelGetGradient, cl::NullRange, cl::NDRange(size[0], size[1]));
                commandQueue.enqueueNDRangeKernel(kernelPushGradient, cl::NullRange, cl::NDRange(size[0], size[1]));
            }
            if (i < param.passes)
            {
                //reset getGradient
                kernelGetGradient.setArg(0, sizeof(cl_mem), &imageBuffer1);
                kernelGetGradient.setArg(1, sizeof(cl_mem), &imageBuffer2);
                //reset pushGradient
                kernelPushGradient.setArg(0, sizeof(cl_mem), &imageBuffer2);
                kernelPushGradient.setArg(1, sizeof(cl_mem), &imageBuffer1);
                kernelPushGradient.setArg(2, sizeof(cl_float), &pushGradientStrength);

                while (i++ < param.passes)
                {
                    commandQueue.enqueueNDRangeKernel(kernelGetGradient, cl::NullRange, cl::NDRange(size[0], size[1]));
                    commandQueue.enqueueNDRangeKernel(kernelPushGradient, cl::NullRange, cl::NDRange(size[0], size[1]));
                }
            }
            //blocking read
            commandQueue.enqueueReadImage(imageBuffer1, CL_TRUE, start, dstRegion, dstImg.step, 0, dstImg.data);
        }
        catch (const cl::Error& e)
        {
            throw ACException<ExceptionType::GPU, true>("Failed to run OpenCL Anime4K09 kernel", e.what(), e.err());
        }
    }

    static void runKernelP(const cv::Mat& orgImg, cv::Mat& dstImg, int channelType, const Parameters& param)
    {
        cl_float nWidth, nHeight;
        if (param.zoomFactor == 2.0)
        {
            nWidth = 1.0f / dstImg.cols;
            nHeight = 1.0f / dstImg.rows;
        }
        else
        {
            nWidth = static_cast<cl_float>(orgImg.cols) / dstImg.cols;
            nHeight = static_cast<cl_float>(orgImg.rows) / dstImg.rows;
        }

        static constexpr std::array<cl::size_type, 3> start = { 0,0,0 };
        const std::array<cl::size_type, 3> orgRegion = { static_cast<cl::size_type>(orgImg.cols),static_cast<cl::size_type>(orgImg.rows),1 };
        const std::array<cl::size_type, 3> dstRegion = { static_cast<cl::size_type>(dstImg.cols),static_cast<cl::size_type>(dstImg.rows),1 };
        const std::array<cl::size_type, 2> size =
        {
            ALIGN_UP(dstImg.cols, workGroupSizeBase),
            ALIGN_UP(dstImg.rows, workGroupSizeBase)
        };

        const cl_float pushColorStrength = static_cast<cl_float>(param.strengthColor);
        const cl_float pushGradientStrength = static_cast<cl_float>(param.strengthGradient);
        const cl_float normalizedWidth = nWidth;
        const cl_float normalizedHeight = nHeight;

        try
        {
            //init frame
            cl::CommandQueue& commandQueue = commandQueueList[commandQueueCount++ % commandQueueNum];

            //kernel for each thread
            cl::Kernel kernelGetGray(program, (param.zoomFactor == 2.0) ? "getGray" : "getGrayLanczos4");
            cl::Kernel kernelPushColor(program, "pushColor");
            cl::Kernel kernelGetGradient(program, "getGradient");
            cl::Kernel kernelPushGradient(program, "pushGradient");

            cl::ImageFormat imageFormat(CL_RGBA, channelType);
            cl::ImageFormat tmpFormat(CL_RGBA, channelType == CL_FLOAT ? CL_HALF_FLOAT : channelType);

            //imageBuffer
            cl::Image2D imageBuffer0(context, CL_MEM_READ_ONLY, imageFormat, orgImg.cols, orgImg.rows);
            cl::Image2D imageBuffer1(context, CL_MEM_READ_WRITE, imageFormat, dstImg.cols, dstImg.rows);
            cl::Image2D imageBuffer2(context, CL_MEM_READ_WRITE, tmpFormat, dstImg.cols, dstImg.rows);
            cl::Image2D imageBuffer3(context, CL_MEM_READ_WRITE, tmpFormat, dstImg.cols, dstImg.rows);

            //set arguments
            //getGray
            kernelGetGray.setArg(0, sizeof(cl_mem), &imageBuffer0);
            kernelGetGray.setArg(1, sizeof(cl_mem), &imageBuffer1);
            kernelGetGray.setArg(2, sizeof(cl_float), &normalizedWidth);
            kernelGetGray.setArg(3, sizeof(cl_float), &normalizedHeight);

            //pushColor
            kernelPushColor.setArg(0, sizeof(cl_mem), &imageBuffer1);
            kernelPushColor.setArg(1, sizeof(cl_mem), &imageBuffer2);
            kernelPushColor.setArg(2, sizeof(cl_float), &pushColorStrength);

            //getGradient
            kernelGetGradient.setArg(0, sizeof(cl_mem), &imageBuffer2);
            kernelGetGradient.setArg(1, sizeof(cl_mem), &imageBuffer3);

            //pushGradient
            kernelPushGradient.setArg(0, sizeof(cl_mem), &imageBuffer3);
            kernelPushGradient.setArg(1, sizeof(cl_mem), &imageBuffer1);
            kernelPushGradient.setArg(2, sizeof(cl_float), &pushGradientStrength);

            int i;
            std::vector<cl::Event> waitForWriteFinishedEvent(1);
            std::vector<cl::Event> waitForReadReadyEvent(1);
            cl::Event& writeFinishedEvent = waitForWriteFinishedEvent.front();
            cl::Event& readReadyEvent = waitForReadReadyEvent.front();

            //enqueue
            commandQueueIO.enqueueWriteImage(imageBuffer0, CL_FALSE, start, orgRegion, orgImg.step, 0, orgImg.data, nullptr, &writeFinishedEvent);
            commandQueue.enqueueNDRangeKernel(kernelGetGray, cl::NullRange, cl::NDRange(size[0], size[1]), cl::NullRange, &waitForWriteFinishedEvent);
            for (i = 0; i < param.passes && i < param.pushColorCount; i++)//pcc for push color count
            {
                commandQueue.enqueueNDRangeKernel(kernelPushColor, cl::NullRange, cl::NDRange(size[0], size[1]));
                commandQueue.enqueueNDRangeKernel(kernelGetGradient, cl::NullRange, cl::NDRange(size[0], size[1]));
                commandQueue.enqueueNDRangeKernel(kernelPushGradient, cl::NullRange, cl::NDRange(size[0], size[1]));
            }
            if (i < param.passes)
            {
                //reset getGradient
                kernelGetGradient.setArg(0, sizeof(cl_mem), &imageBuffer1);
                kernelGetGradient.setArg(1, sizeof(cl_mem), &imageBuffer2);
                //reset pushGradient
                kernelPushGradient.setArg(0, sizeof(cl_mem), &imageBuffer2);
                kernelPushGradient.setArg(1, sizeof(cl_mem), &imageBuffer1);
                kernelPushGradient.setArg(2, sizeof(cl_float), &pushGradientStrength);

                while (i++ < param.passes)
                {
                    commandQueue.enqueueNDRangeKernel(kernelGetGradient, cl::NullRange, cl::NDRange(size[0], size[1]));
                    commandQueue.enqueueNDRangeKernel(kernelPushGradient, cl::NullRange, cl::NDRange(size[0], size[1]));
                }
            }
            //blocking read
            commandQueue.enqueueMarkerWithWaitList(nullptr, &readReadyEvent);
            commandQueueIO.enqueueReadImage(imageBuffer1, CL_TRUE, start, dstRegion, dstImg.step, 0, dstImg.data, &waitForReadReadyEvent);
        }
        catch (const cl::Error& e)
        {
            throw ACException<ExceptionType::GPU, true>("Failed to run OpenCL Anime4K09 kernel", e.what(), e.err());
        }
    }

    static void runKernel(const cv::Mat& orgImg, cv::Mat& dstImg, const Parameters& param)
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

        if (parallelIO)
            runKernelP(orgImg, dstImg, channelType, param);
        else
            runKernelN(orgImg, dstImg, channelType, param);
    }
}

void Anime4KCPP::OpenCL::Anime4K09::init(const int platformID, const int deviceID, const int OpenCLQueueNum, const bool OpenCLParallelIO)
{
    if (!detail::isInitializedFlag)
    {
        detail::pID = platformID;
        detail::dID = deviceID;
        detail::commandQueueNum = OpenCLQueueNum > 1 ? OpenCLQueueNum : 1;
        detail::parallelIO = OpenCLParallelIO;
        initOpenCL();
        detail::isInitializedFlag = true;
    }
}

void Anime4KCPP::OpenCL::Anime4K09::release() noexcept
{
    if (detail::isInitializedFlag)
    {
        detail::context = nullptr;
        std::fill(detail::commandQueueList.begin(), detail::commandQueueList.end(), nullptr);
        detail::commandQueueIO = nullptr;
        detail::program = nullptr;
        detail::device = nullptr;
        detail::isInitializedFlag = false;
    }
}

bool Anime4KCPP::OpenCL::Anime4K09::isInitialized() noexcept
{
    return detail::isInitializedFlag;
}

std::string Anime4KCPP::OpenCL::Anime4K09::getInfo() const
{
    std::ostringstream oss;
    oss << AC::getInfo()
        << "----------------------------------------------" << '\n'
        << "OpenCL Platform ID:" << detail::pID << '\n'
        << "OpenCL Device ID:" << detail::dID << '\n'
        << "Passes: " << param.passes << '\n'
        << "pushColorCount: " << param.pushColorCount << '\n'
        << "Zoom Factor: " << param.zoomFactor << '\n'
        << "Fast Mode: " << std::boolalpha << param.fastMode << '\n'
        << "Strength Color: " << param.strengthColor << '\n'
        << "Strength Gradient: " << param.strengthGradient << '\n'
        << "Number of OpenCL Command Queues:" << detail::commandQueueNum << '\n'
        << "OpenCL Parallel IO Command Queues:" << std::boolalpha << detail::parallelIO << '\n'
        << "----------------------------------------------" << '\n';
    return oss.str();
}

std::string Anime4KCPP::OpenCL::Anime4K09::getFiltersInfo() const
{
    std::ostringstream oss;

    oss << AC::getFiltersInfo()
        << "----------------------------------------------" << '\n'
        << "Preprocessing filters list:" << '\n'
        << "----------------------------------------------" << '\n';
    if (!param.preprocessing)
        oss << "Preprocessing disabled" << '\n';
    else
    {
        std::vector<std::string>preFiltersString = FilterProcessor::filterToString(param.preFilters);
        if (preFiltersString.empty())
            oss << "Preprocessing disabled" << '\n';
        else
            for (auto& filters : preFiltersString)
                oss << filters << '\n';
    }

    oss << "----------------------------------------------" << '\n'
        << "Postprocessing filters list:" << '\n'
        << "----------------------------------------------" << '\n';
    if (!param.postprocessing)
        oss << "Postprocessing disabled" << '\n';
    else
    {
        std::vector<std::string>postFiltersString = FilterProcessor::filterToString(param.postFilters);
        if (postFiltersString.empty())
            oss << "Postprocessing disabled" << '\n';
        else
            for (auto& filters : postFiltersString)
                oss << filters << '\n';
    }

    return oss.str();
}

void Anime4KCPP::OpenCL::Anime4K09::processYUVImage()
{
    cv::Mat tmpImg;
    cv::merge(std::vector<cv::Mat>{ orgImg, orgU, orgV }, tmpImg);
    cv::cvtColor(tmpImg, tmpImg, cv::COLOR_YUV2BGR);

    dstImg.create(height , width, CV_MAKE_TYPE(tmpImg.depth() ,4));
    if (param.preprocessing)//Pretprocessing(CPU)
        FilterProcessor(tmpImg, param.preFilters).process();
    cv::cvtColor(tmpImg, tmpImg, cv::COLOR_BGR2BGRA);
    detail::runKernel(tmpImg, dstImg, param);
    cv::cvtColor(dstImg, dstImg, cv::COLOR_BGRA2BGR);
    if (param.postprocessing)//Postprocessing(CPU)
        FilterProcessor(dstImg, param.postFilters).process();

    cv::cvtColor(dstImg, dstImg, cv::COLOR_BGR2YUV);
    cv::Mat yuv[3];
    cv::split(dstImg, yuv);
    dstImg = yuv[Y];
    dstU = yuv[U];
    dstV = yuv[V];
}

void Anime4KCPP::OpenCL::Anime4K09::processRGBImage()
{
    cv::Mat tmpImg = orgImg;
    dstImg.create(height, width, CV_MAKE_TYPE(tmpImg.depth(), 4));
    if (param.preprocessing)//Pretprocessing(CPU)
        FilterProcessor(tmpImg, param.preFilters).process();
    cv::cvtColor(tmpImg, tmpImg, cv::COLOR_BGR2BGRA);
    detail::runKernel(tmpImg, dstImg, param);
    cv::cvtColor(dstImg, dstImg, cv::COLOR_BGRA2BGR);
    if (param.postprocessing)//Postprocessing(CPU)
        FilterProcessor(dstImg, param.postFilters).process();
}

void Anime4KCPP::OpenCL::Anime4K09::processGrayscale()
{
    cv::Mat tmpImg;
    cv::cvtColor(orgImg, tmpImg, cv::COLOR_GRAY2BGR);

    dstImg.create(height, width, CV_MAKE_TYPE(tmpImg.depth(), 4));
    if (param.preprocessing)//Pretprocessing(CPU)
        FilterProcessor(tmpImg, param.preFilters).process();
    cv::cvtColor(tmpImg, tmpImg, cv::COLOR_BGR2BGRA);
    detail::runKernel(tmpImg, dstImg, param);
    cv::cvtColor(dstImg, dstImg, cv::COLOR_BGRA2BGR);
    if (param.postprocessing)//Postprocessing(CPU)
        FilterProcessor(dstImg, param.postFilters).process();

    cv::cvtColor(dstImg, dstImg, cv::COLOR_BGR2GRAY);
}

void Anime4KCPP::OpenCL::Anime4K09::initOpenCL()
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
        std::string Anime4KCPPKernelSource = detail::readKernel("Anime4KCPPKernel.cl");
#else
        std::string Anime4KCPPKernelSource = Anime4KCPPKernelSourceString;
#endif // BUILT_IN_KERNEL
        detail::program = cl::Program(detail::context, Anime4KCPPKernelSource);

#ifdef ENABLE_FAST_MATH
        const char* buildFlags = "-cl-fast-relaxed-math";
#else
        const char* buildFlags = nullptr;
#endif // ENABLE_FAST_MATH
        try
        {
            detail::program.build(detail::device, buildFlags);
        }
        catch (const cl::BuildError& e)
        {
            throw ACException<ExceptionType::GPU, true>("Kernel build error", e.getBuildLog().front().second, e.err());
        }

        cl::Kernel tmpKernel{ detail::program, "pushColor" };
        tmpKernel.getWorkGroupInfo(detail::device, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, &detail::workGroupSizeBase);
    }
    catch (const cl::Error& e)
    {
        throw ACException<ExceptionType::GPU, true>("Failed to initialize OpenCL", e.what(), e.err());
    }
}

Anime4KCPP::Processor::Type Anime4KCPP::OpenCL::Anime4K09::getProcessorType() const noexcept
{
    return Processor::Type::OpenCL_Anime4K09;
}

std::string Anime4KCPP::OpenCL::Anime4K09::getProcessorInfo() const
{
    std::ostringstream oss;
    oss << "Processor type: " << getProcessorType() << '\n'
        << "Current OpenCL devices:" << '\n'
        << " " + detail::device.getInfo<CL_DEVICE_NAME>();
    return oss.str();
}

#endif
