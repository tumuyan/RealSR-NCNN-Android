#ifdef ENABLE_NCNN

#include <net.h>

#include "ACNetType.hpp"
#include "NCNNACNet.hpp"
#include "NCNNACNetID.hpp"
#include "NCNNACNetModel.hpp"

namespace Anime4KCPP::NCNN::detail
{
    static bool isInitializedFlag = false;
    static ncnn::VulkanDevice* vkdev = nullptr;
    static ncnn::Net net[Anime4KCPP::ACNetType::TotalTypeCount];

    static void processCPU(const cv::Mat& orgImg, cv::Mat& dstImg, const int scaleTimes, int index, ncnn::Mat& holder)
    {
        ncnn::Mat in;
        if (orgImg.step == orgImg.cols * sizeof(float))
            in = ncnn::Mat{ orgImg.cols, orgImg.rows, 1, orgImg.data };
        else
        {
            in.create(orgImg.cols, orgImg.rows, 1);
            float* dst = reinterpret_cast<float*>(in.data);
            std::uint8_t* src = orgImg.data;
            for (int i = 0; i < orgImg.rows; i++)
            {
                std::memcpy(dst, src, orgImg.cols * sizeof(float));
                dst += orgImg.cols;
                src += orgImg.step;
            }
        }

        for (int i = 0; i < scaleTimes; i++)
        {
            ncnn::Extractor ex = net[index].create_extractor();
            ex.input(ACNetParamID::BLOB_data, in);
            ex.extract(ACNetParamID::BLOB_output, holder);
            in = holder;
        }

        dstImg = cv::Mat{ holder.h, holder.w,CV_32FC1, holder.data };
    }

    static void processVK(const cv::Mat& orgImg, cv::Mat& dstImg, const int scaleTimes, int index, ncnn::Mat& holder)
    {
        ncnn::VkAllocator* blob_vkallocator = vkdev->acquire_blob_allocator();
        ncnn::VkAllocator* staging_vkallocator = vkdev->acquire_staging_allocator();

        ncnn::Option opt = net[index].opt;
        opt.blob_vkallocator = blob_vkallocator;
        opt.workspace_vkallocator = blob_vkallocator;
        opt.staging_vkallocator = staging_vkallocator;

        ncnn::Mat in;
        if (orgImg.step == orgImg.cols * sizeof(float))
            in = ncnn::Mat{ orgImg.cols, orgImg.rows, 1, orgImg.data };
        else
        {
            in.create(orgImg.cols, orgImg.rows, 1);
            float* dst = reinterpret_cast<float*>(in.data);
            std::uint8_t* src = orgImg.data;
            for (int i = 0; i < orgImg.rows; i++)
            {
                std::memcpy(dst, src, orgImg.cols * sizeof(float));
                dst += orgImg.cols;
                src += orgImg.step;
            }
        }

        ncnn::VkCompute cmd(vkdev);

        ncnn::VkMat vkIn;
        ncnn::VkMat vkOut;

        cmd.record_upload(in, vkIn, opt);
        for (int i = 0; i < scaleTimes; i++)
        {
            ncnn::Extractor ex = net[index].create_extractor();

            ex.set_blob_vkallocator(blob_vkallocator);
            ex.set_workspace_vkallocator(blob_vkallocator);
            ex.set_staging_vkallocator(staging_vkallocator);

            ex.input(ACNetParamID::BLOB_data, vkIn);
            ex.extract(ACNetParamID::BLOB_output, vkOut, cmd);
            vkIn = vkOut;
        }
        cmd.record_download(vkOut, holder, opt);

        cmd.submit_and_wait();

        dstImg = cv::Mat{ holder.h, holder.w,CV_32FC1, holder.data };

        vkdev->reclaim_blob_allocator(blob_vkallocator);
        vkdev->reclaim_staging_allocator(staging_vkallocator);
    }

    static void runKernel(const cv::Mat& orgImg, cv::Mat& dstImg, int scaleTimes, int index, ncnn::Mat& dataHolder)
    {
        float normScale;

        switch (orgImg.depth())
        {
        case CV_8U:
            normScale = 255.0f;
            break;
        case CV_16U:
            normScale = 65535.0f;
            break;
        case CV_32F:
            normScale = 0.0f;
            break;
        default:
            throw ACException<ExceptionType::RunTimeError>("Unsupported image data type");
        }

        cv::Mat blob;

        if (normScale != 0.0f)
        {
            orgImg.convertTo(blob, CV_32FC1, 1.0 / normScale);
        }

        if (vkdev == nullptr)
        {
            processCPU(blob, dstImg, scaleTimes, index, dataHolder);
        }
        else
        {
            processVK(blob, dstImg, scaleTimes, index, dataHolder);
        }

        if (normScale != 0.0f)
        {
            dstImg.convertTo(dstImg, orgImg.type(), normScale);
        }
    }
}

struct Anime4KCPP::NCNN::ACNet::DataHolder
{
    ncnn::Mat data;
};

Anime4KCPP::NCNN::ACNet::ACNet(const Parameters& parameters) :
    AC(parameters),   
    ACNetTypeIndex(GET_ACNET_TYPE_INDEX(param.HDN, param.HDNLevel)),
    dataHolder(std::make_unique<DataHolder>()) {}

Anime4KCPP::NCNN::ACNet::ACNet::~ACNet() = default;

void Anime4KCPP::NCNN::ACNet::setParameters(const Parameters& parameters)
{
    AC::setParameters(parameters);
    ACNetTypeIndex = GET_ACNET_TYPE_INDEX(param.HDN, param.HDNLevel);
}

std::string Anime4KCPP::NCNN::ACNet::getInfo() const
{
    std::ostringstream oss;
    oss << AC::getInfo()
        << "----------------------------------------------" << '\n'
        << "NCNN device product ID: " << (detail::vkdev ? std::to_string(detail::vkdev->info.device_id()) : "-1") << '\n'
        << "Zoom Factor: " << param.zoomFactor << '\n'
        << "HDN Mode: " << std::boolalpha << param.HDN << '\n'
        << "HDN Level: " << (param.HDN ? param.HDNLevel : 0) << '\n'
        << "----------------------------------------------" << '\n';
    return oss.str();
}

std::string Anime4KCPP::NCNN::ACNet::getFiltersInfo() const
{
    std::ostringstream oss;
    oss << AC::getFiltersInfo()
        << "----------------------------------------------" << '\n'
        << "Filter not supported" << '\n'
        << "----------------------------------------------" << '\n';
    return oss.str();
}

void Anime4KCPP::NCNN::ACNet::init(
    std::string& modelPath, std::string& paramPath,
    int type, const int deviceID, const int threads)
{
    if (detail::isInitializedFlag)
        return;

    if (detail::vkdev == nullptr)
        detail::vkdev = deviceID < 0 ? nullptr : ncnn::get_gpu_device((deviceID >= ncnn::get_gpu_count()) ? 0 : deviceID);

    if (type >= ACNetType::TotalTypeCount || type < ACNetType::HDNL0)
        type = ACNetType::HDNL0;

    detail::net[type].set_vulkan_device(detail::vkdev);
    detail::net[type].opt.use_vulkan_compute = detail::vkdev ? true : false;

    detail::net[type].opt.use_fp16_arithmetic = false;
    detail::net[type].opt.use_fp16_packed = true;
    detail::net[type].opt.use_fp16_storage = true;

    detail::net[type].opt.use_int8_packed = true;
    detail::net[type].opt.use_int8_storage = true;
    detail::net[type].opt.use_int8_inference = false;

    detail::net[type].opt.num_threads = threads;

    if (detail::net[type].load_param(paramPath.c_str()) || detail::net[type].load_model(modelPath.c_str()))
    {
        release();
        throw ACException<ExceptionType::IO, true>(
            "Failed to load ncnn model or param",
            std::string("model path: ") + modelPath.c_str() + "\nparam path: " + paramPath.c_str(),
            __LINE__);
    }

    detail::isInitializedFlag = true;
}

void Anime4KCPP::NCNN::ACNet::init(int type, const int deviceID, const int threads)
{
    if (detail::isInitializedFlag)
        return;

    if (detail::vkdev == nullptr)
        detail::vkdev = deviceID < 0 ? nullptr : ncnn::get_gpu_device((deviceID >= ncnn::get_gpu_count()) ? 0 : deviceID);

    if (type >= ACNetType::TotalTypeCount || type < ACNetType::HDNL0)
        type = ACNetType::HDNL0;

    detail::net[type].set_vulkan_device(detail::vkdev);
    detail::net[type].opt.use_vulkan_compute = detail::vkdev ? true : false;

    detail::net[type].opt.use_fp16_arithmetic = false;
    detail::net[type].opt.use_fp16_packed = true;
    detail::net[type].opt.use_fp16_storage = true;

    detail::net[type].opt.use_int8_packed = true;
    detail::net[type].opt.use_int8_storage = true;
    detail::net[type].opt.use_int8_inference = false;

    detail::net[type].opt.num_threads = threads;

    if (!detail::net[type].load_param(ACNetParamBin) || !detail::net[type].load_model(ACNetModelBin[type]))
    {
        release();
        throw ACException<ExceptionType::IO, false>("Failed to load ncnn model or param");
    }

    detail::isInitializedFlag = true;
}

void Anime4KCPP::NCNN::ACNet::init(const int deviceID, const int threads)
{
    if (detail::isInitializedFlag)
        return;

    if (detail::vkdev == nullptr)
        detail::vkdev = deviceID < 0 ? nullptr : ncnn::get_gpu_device((deviceID >= ncnn::get_gpu_count()) ? 0 : deviceID);

    for (int type = ACNetType::HDNL0; type < ACNetType::TotalTypeCount; type++)
    {
        detail::net[type].set_vulkan_device(detail::vkdev);
        detail::net[type].opt.use_vulkan_compute = detail::vkdev ? true : false;

        detail::net[type].opt.use_fp16_arithmetic = false;
        detail::net[type].opt.use_fp16_packed = true;
        detail::net[type].opt.use_fp16_storage = true;

        detail::net[type].opt.use_int8_packed = true;
        detail::net[type].opt.use_int8_storage = true;
        detail::net[type].opt.use_int8_inference = false;

        detail::net[type].opt.num_threads = threads;

        if (!detail::net[type].load_param(ACNetParamBin) || !detail::net[type].load_model(ACNetModelBin[type]))
        {
            release();
            throw ACException<ExceptionType::IO, false>("Failed to load ncnn model or param");
        }
    }

    detail::isInitializedFlag = true;
}

void Anime4KCPP::NCNN::ACNet::release() noexcept
{
    if (!detail::isInitializedFlag)
        return;

    for (int i = ACNetType::HDNL0; i < ACNetType::TotalTypeCount; i++)
        detail::net[i].clear();

    if (detail::vkdev)
        ncnn::destroy_gpu_instance();

    detail::vkdev = nullptr;

    detail::isInitializedFlag = false;
}

bool Anime4KCPP::NCNN::ACNet::isInitialized() noexcept
{
    return detail::isInitializedFlag;
}

void Anime4KCPP::NCNN::ACNet::processYUVImage()
{
    if (!param.fastMode)
    {
        int scaleTimes = Utils::fastCeilLog2(param.zoomFactor);
        if (!scaleTimes)
            scaleTimes++;

        detail::runKernel(orgImg, dstImg, scaleTimes, ACNetTypeIndex, dataHolder->data);

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

        detail::runKernel(tmpImg, dstImg, 1, ACNetTypeIndex, dataHolder->data);

        cv::resize(orgU, dstU, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
        cv::resize(orgV, dstV, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
    }
}

void Anime4KCPP::NCNN::ACNet::processRGBImage()
{
    if (!param.fastMode)
    {
        int scaleTimes = Utils::fastCeilLog2(param.zoomFactor);
        if (!scaleTimes)
            scaleTimes++;

        cv::Mat tmpImg = orgImg;
        cv::cvtColor(tmpImg, tmpImg, cv::COLOR_BGR2YUV);

        cv::Mat yuv[3];
        cv::split(tmpImg, yuv);

        detail::runKernel(yuv[Y], dstImg, scaleTimes, ACNetTypeIndex, dataHolder->data);

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
        cv::cvtColor(tmpImg, orgImg, cv::COLOR_BGR2YUV);

        if (param.zoomFactor > 2.0)
            cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor / 2.0, param.zoomFactor / 2.0, cv::INTER_CUBIC);
        else if (param.zoomFactor < 2.0)
            cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor / 2.0, param.zoomFactor / 2.0, cv::INTER_AREA);

        cv::Mat yuv[3];
        cv::split(tmpImg, yuv);

        detail::runKernel(yuv[Y], dstImg, 1, ACNetTypeIndex, dataHolder->data);

        cv::resize(yuv[U], yuv[U], cv::Size(0, 0), 2.0, 2.0, cv::INTER_CUBIC);
        cv::resize(yuv[V], yuv[V], cv::Size(0, 0), 2.0, 2.0, cv::INTER_CUBIC);

        cv::merge(std::vector<cv::Mat>{ dstImg, yuv[U], yuv[V] }, dstImg);
        cv::cvtColor(dstImg, dstImg, cv::COLOR_YUV2BGR);
    }
}

void Anime4KCPP::NCNN::ACNet::processGrayscale()
{
    if (!param.fastMode)
    {
        int scaleTimes = Utils::fastCeilLog2(param.zoomFactor);
        if (!scaleTimes)
            scaleTimes++;

        detail::runKernel(orgImg, dstImg, scaleTimes, ACNetTypeIndex, dataHolder->data);

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

        detail::runKernel(tmpImg, dstImg, 1, ACNetTypeIndex, dataHolder->data);
    }
}

Anime4KCPP::Processor::Type Anime4KCPP::NCNN::ACNet::getProcessorType() const noexcept
{
    return Processor::Type::NCNN_ACNet;
}

std::string Anime4KCPP::NCNN::ACNet::getProcessorInfo() const
{
    std::ostringstream oss;
    oss << "Processor type: " << getProcessorType() << '\n'
        << "Current NCNN devices:" << '\n'
        << (detail::vkdev ? (std::string{ " Type: " } + detail::vkdev->info.device_name()) : " Type: CPU");
    return oss.str();
}

#endif
