#include <thread>

#include <avisynth.h>

#include "Anime4KCPP.hpp"
#include "Benchmark.hpp"

#ifdef _MSC_VER
#define AC_ASP_STDCALL __stdcall
#define AC_ASP_CDECL __cdecl
#define AC_ASP_EXPORT __declspec(dllexport)
#else
#define AC_ASP_STDCALL __attribute__((__stdcall__))
#define AC_ASP_CDECL __attribute__((__cdecl__))
#define AC_ASP_EXPORT
#endif

enum AC_Parameters
{
    AC_passes = 1,
    AC_pushColorCount = 2,
    AC_strengthColor = 3,
    AC_strengthGradient = 4,
    AC_zoomFactor = 5,
    AC_ACNet = 6,
    AC_GPUMode = 7,
    AC_GPGPUModel = 8,
    AC_HDN = 9,
    AC_HDNLevel = 10,
    AC_platformID = 11,
    AC_deviceID = 12,
    AC_OpenCLQueueNum = 13,
    AC_OpenCLParallelIO = 14,
};

enum class GPGPU
{
    OpenCL, CUDA
};

class Anime4KCPPFilter : public GenericVideoFilter
{
public:
    Anime4KCPPFilter(
        PClip _child,
        const Anime4KCPP::Parameters& parameters,
        bool CNN,
        bool GPUMode,
        GPGPU GPGPUModel,
        int pID,
        int dID,
        int OpenCLQueueNum,
        bool OpenCLParallelIO,
        IScriptEnvironment* env
    );

    PVideoFrame AC_ASP_STDCALL GetFrame(int n, IScriptEnvironment* env);
    template <typename T>
    PVideoFrame FilterYUV(int n, IScriptEnvironment* env);
    template <typename T>
    PVideoFrame FilterGrayscale(int n, IScriptEnvironment* env);

    PVideoFrame FilterRGB(int n, IScriptEnvironment* env);
private:
    Anime4KCPP::Parameters parameters;
    Anime4KCPP::ACInitializer initializer;
    bool GPUMode;
    bool CNN;
    GPGPU GPGPUModel;
};

Anime4KCPPFilter::Anime4KCPPFilter(
    PClip _child,
    const Anime4KCPP::Parameters& parameters,
    bool CNN,
    bool GPUMode,
    GPGPU GPGPUModel,
    int pID,
    int dID,
    int OpenCLQueueNum,
    bool OpenCLParallelIO,
    IScriptEnvironment* env
) :
    GenericVideoFilter(_child),
    parameters(parameters),
    GPUMode(GPUMode),
    CNN(CNN),
    GPGPUModel(GPGPUModel)
{
    if ((!vi.IsRGB24() && (!vi.IsYUV() || !vi.IsPlanar()) && !vi.IsY()) ||
        (vi.BitsPerComponent() != 8 && vi.BitsPerComponent() != 16 && vi.BitsPerComponent() != 32))
    {
        env->ThrowError("Anime4KCPP: supported data type: RGB24 and Grayscale or YUV with 8 or 16bit integer or 32bit float)");
    }

    if (!vi.IsRGB24() && !vi.Is444() && !vi.IsY() && !CNN)
    {
        env->ThrowError("Anime4KCPP: RGB24 or YUV444 or Grayscale is needed for Anime4K09");
    }

    vi.height = static_cast<int>(std::round(vi.height * parameters.zoomFactor));
    vi.width = static_cast<int>(std::round(vi.width * parameters.zoomFactor));

    if (GPUMode)
    {
        switch (GPGPUModel)
        {
        case GPGPU::OpenCL:
#ifdef ENABLE_OPENCL
            if (CNN)
                initializer.pushManager<Anime4KCPP::OpenCL::Manager<Anime4KCPP::OpenCL::ACNet>>(
                    pID, dID,
                    Anime4KCPP::CNNType::Default,
                    OpenCLQueueNum,
                    OpenCLParallelIO);
            else
                initializer.pushManager<Anime4KCPP::OpenCL::Manager<Anime4KCPP::OpenCL::Anime4K09>>(
                    pID, dID,
                    OpenCLQueueNum,
                    OpenCLParallelIO);
#endif // ENABLE_OPENCL
            break;
        case GPGPU::CUDA:
#ifdef ENABLE_CUDA
            initializer.pushManager<Anime4KCPP::Cuda::Manager>(dID);
#endif // ENABLE_CUDA
            break;
        }

        if (initializer.init() != initializer.size())
        {
            std::ostringstream oss("Unable to initialize:\n", std::ios_base::ate);
            for (auto& error : initializer.failure())
                oss << "  " << error;
            oss << '\n';
            env->ThrowError(oss.str().c_str());
        }
    }
}


PVideoFrame AC_ASP_STDCALL Anime4KCPPFilter::GetFrame(int n, IScriptEnvironment* env)
{
    if (vi.IsY())
    {
        switch (vi.BitsPerComponent())
        {
        case 8:
            return FilterGrayscale<std::uint8_t>(n, env);
        case 16:
            return FilterGrayscale<std::uint16_t>(n, env);
        case 32:
            return FilterGrayscale<float>(n, env);
        }
    }
    if (vi.IsYUV())
    {
        switch (vi.BitsPerComponent())
        {
        case 8:
            return FilterYUV<std::uint8_t>(n, env);
        case 16:
            return FilterYUV<std::uint16_t>(n, env);
        case 32:
            return FilterYUV<float>(n, env);
        }
    }
    return FilterRGB(n, env);
}

template<typename T>
PVideoFrame Anime4KCPPFilter::FilterYUV(int n, IScriptEnvironment* env)
{
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame dst = env->NewVideoFrameP(vi, &src);

    int srcPitchY = src->GetPitch(PLANAR_Y);
    int dstPitchY = dst->GetPitch(PLANAR_Y);
    int srcPitchU = src->GetPitch(PLANAR_U);
    int dstPitchU = dst->GetPitch(PLANAR_U);
    int srcPitchV = src->GetPitch(PLANAR_V);
    int dstPitchV = dst->GetPitch(PLANAR_V);

    int srcHY = src->GetHeight(PLANAR_Y);
    int srcLY = src->GetRowSize(PLANAR_Y) / sizeof(T);
    int srcHU = src->GetHeight(PLANAR_U);
    int srcLU = src->GetRowSize(PLANAR_U) / sizeof(T);
    int srcHV = src->GetHeight(PLANAR_V);
    int srcLV = src->GetRowSize(PLANAR_V) / sizeof(T);

    T* srcpY = const_cast<T*>(reinterpret_cast<const T*>(src->GetReadPtr(PLANAR_Y)));
    T* srcpU = const_cast<T*>(reinterpret_cast<const T*>(src->GetReadPtr(PLANAR_U)));
    T* srcpV = const_cast<T*>(reinterpret_cast<const T*>(src->GetReadPtr(PLANAR_V)));

    std::uint8_t* dstpY = dst->GetWritePtr(PLANAR_Y);
    std::uint8_t* dstpU = dst->GetWritePtr(PLANAR_U);
    std::uint8_t* dstpV = dst->GetWritePtr(PLANAR_V);

    std::unique_ptr<Anime4KCPP::AC> ac;

    if (GPUMode)
    {
        switch (GPGPUModel)
        {
        case GPGPU::OpenCL:
#ifdef ENABLE_OPENCL
            if (CNN)
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::OpenCL_ACNet);
            else
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::OpenCL_Anime4K09);
#endif
            break;
        case GPGPU::CUDA:
#ifdef ENABLE_CUDA
            if (CNN)
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::Cuda_ACNet);
            else
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::Cuda_Anime4K09);
#endif
            break;
        }
    }
    else
    {
        if (CNN)
            ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::CPU_ACNet);
        else
            ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::CPU_Anime4K09);
    }

    try
    {
        ac->loadImage(
            srcHY, srcLY, srcPitchY, srcpY,
            srcHU, srcLU, srcPitchU, srcpU,
            srcHV, srcLV, srcPitchV, srcpV);
        ac->process();
        ac->saveImage(dstpY, dstPitchY, dstpU, dstPitchU, dstpV, dstPitchV);
    }
    catch (const std::exception& e)
    {
        env->ThrowError(e.what());
    }

    return dst;
}

template<typename T>
PVideoFrame Anime4KCPPFilter::FilterGrayscale(int n, IScriptEnvironment* env)
{
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame dst = env->NewVideoFrameP(vi, &src);

    int srcPitchY = src->GetPitch(PLANAR_Y);
    int dstPitchY = dst->GetPitch(PLANAR_Y);

    int srcHY = src->GetHeight(PLANAR_Y);
    int srcLY = src->GetRowSize(PLANAR_Y) / sizeof(T);

    T* srcpY = const_cast<T*>(reinterpret_cast<const T*>(src->GetReadPtr(PLANAR_Y)));

    std::uint8_t* dstpY = dst->GetWritePtr(PLANAR_Y);

    std::unique_ptr<Anime4KCPP::AC> ac;

    if (GPUMode)
    {
        switch (GPGPUModel)
        {
        case GPGPU::OpenCL:
#ifdef ENABLE_OPENCL
            if (CNN)
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::OpenCL_ACNet);
            else
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::OpenCL_Anime4K09);
#endif
            break;
        case GPGPU::CUDA:
#ifdef ENABLE_CUDA
            if (CNN)
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::Cuda_ACNet);
            else
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::Cuda_Anime4K09);
#endif
            break;
        }
    }
    else
    {
        if (CNN)
            ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::CPU_ACNet);
        else
            ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::CPU_Anime4K09);
    }

    try
    {
        ac->loadImage(srcHY, srcLY, srcPitchY, srcpY, false, false, true);
        ac->process();
        ac->saveImage(dstpY, dstPitchY);
    }
    catch (const std::exception& e)
    {
        env->ThrowError(e.what());
    }

    return dst;
}

PVideoFrame Anime4KCPPFilter::FilterRGB(int n, IScriptEnvironment* env)
{
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame dst = env->NewVideoFrameP(vi, &src);

    int srcPitch = src->GetPitch();
    int dstPitch = dst->GetPitch();

    int srcH = src->GetHeight();
    int srcL = src->GetRowSize();

    std::uint8_t* srcp = const_cast<std::uint8_t*>(src->GetReadPtr());
    std::uint8_t* dstp = dst->GetWritePtr();

    std::unique_ptr<Anime4KCPP::AC> ac;

    if (GPUMode)
    {
        switch (GPGPUModel)
        {
        case GPGPU::OpenCL:
#ifdef ENABLE_OPENCL
            if (CNN)
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::OpenCL_ACNet);
            else
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::OpenCL_Anime4K09);
#endif
            break;
        case GPGPU::CUDA:
#ifdef ENABLE_CUDA
            if (CNN)
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::Cuda_ACNet);
            else
                ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::Cuda_Anime4K09);
#endif
            break;
        }
    }
    else
    {
        if (CNN)
            ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::CPU_ACNet);
        else
            ac = Anime4KCPP::ACCreator::createUP(parameters, Anime4KCPP::Processor::Type::CPU_Anime4K09);
    }

    try
    {
        ac->loadImage(srcH, srcL / 3, srcPitch, srcp);
        ac->process();
        ac->saveImage(dstp, dstPitch);
    }
    catch (const std::exception& e)
    {
        env->ThrowError(e.what());
    }

    return dst;
}

AVSValue AC_ASP_CDECL createAnime4KCPP(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    PClip clip = args[0].AsClip();

    if (clip == nullptr)
        env->ThrowError("Anime4KCPP: No input clip found, has clip of 'src' paramenter been specified ?");

    Anime4KCPP::Parameters parameters(
        args[AC_passes].AsInt(),
        args[AC_pushColorCount].AsInt(),
        args[AC_strengthColor].AsFloat(),
        args[AC_strengthGradient].AsFloat(),
        args[AC_zoomFactor].AsFloat(),
        false, false, false, 4, 40,
        args[AC_HDN].AsBool(),
        args[AC_HDNLevel].AsInt()
    );

    bool CNN = args[AC_ACNet].AsBool();
    bool GPUMode = args[AC_GPUMode].AsBool();
    int pID = args[AC_platformID].AsInt();
    int dID = args[AC_deviceID].AsInt();
    int OpenCLQueueNum = args[AC_OpenCLQueueNum].AsInt();
    bool OpenCLParallelIO = args[AC_OpenCLParallelIO].AsBool();
    const char* GPGPUModelTmp = args[AC_GPGPUModel].AsString();

    if (!args[AC_passes].Defined())
        parameters.passes = 2;
    if (!args[AC_pushColorCount].Defined())
        parameters.pushColorCount = 2;
    if (!args[AC_strengthColor].Defined())
        parameters.strengthColor = 0.3;
    if (!args[AC_strengthGradient].Defined())
        parameters.strengthGradient = 1.0;
    if (!args[AC_zoomFactor].Defined())
        parameters.zoomFactor = 2.0;
    if (!args[AC_HDN].Defined())
        parameters.HDN = false;
    if (!args[AC_HDNLevel].Defined())
        parameters.HDNLevel = 1;
    if (!args[AC_ACNet].Defined())
        CNN = false;
    if (!args[AC_GPUMode].Defined())
        GPUMode = false;
    if (!args[AC_platformID].Defined())
        pID = 0;
    if (!args[AC_deviceID].Defined())
        dID = 0;
    if (!args[AC_OpenCLQueueNum].Defined())
    {
        int currentThreads = static_cast<int>(std::thread::hardware_concurrency());
        OpenCLQueueNum = (currentThreads < 1) ? 1 : currentThreads;
    }
    if (!args[AC_OpenCLParallelIO].Defined())
        OpenCLParallelIO = false;
    if (!args[AC_GPGPUModel].Defined())
#ifdef ENABLE_OPENCL
        GPGPUModelTmp = "opencl";
#elif defined(ENABLE_CUDA)
        GPGPUModelTmp = "cuda";
#else
        GPGPUModelTmp = "cpu";
#endif 

    std::string GPGPUModelString = GPGPUModelTmp;
    std::transform(GPGPUModelString.begin(), GPGPUModelString.end(), GPGPUModelString.begin(), ::tolower);

    GPGPU GPGPUModel = GPGPU::OpenCL;
    if (GPGPUModelString == "opencl")
    {
#ifndef ENABLE_OPENCL
        env->ThrowError("Anime4KCPP: OpenCL is unsupported");
#endif // ENABLE_OPENCL
        GPGPUModel = GPGPU::OpenCL;
    }
    else if (GPGPUModelString == "cuda")
    {
#ifndef ENABLE_CUDA
        env->ThrowError("Anime4KCPP: CUDA is unsupported");
#endif // ENABLE_CUDA
        GPGPUModel = GPGPU::CUDA;
    }
    else if (GPGPUModelString == "cpu")
    {
        if (GPUMode)
        {
            env->ThrowError("Anime4KCPP: GPU mode is unsupported");
        }
    }
    else
        env->ThrowError("Anime4KCPP: GPGPUModel must be \"cuda\" or \"opencl\"");

    if (parameters.strengthColor < 0.0 || parameters.strengthColor > 1.0)
        env->ThrowError("Anime4KCPP: strengthColor must range from 0 to 1!");

    if (parameters.strengthGradient < 0.0 || parameters.strengthGradient > 1.0)
        env->ThrowError("Anime4KCPP: strengthGradient must range from 0 to 1!");

    if (parameters.zoomFactor < 1.0)
        env->ThrowError("Anime4KCPP: zoomFactor must >= 1.0!");

    if (parameters.HDNLevel < 1 || parameters.HDNLevel > 3)
        env->ThrowError("Anime4KCPP: HDNLevel must range from 1 to 3!");

    if (OpenCLQueueNum < 1)
        env->ThrowError("Anime4KCPP: OpenCLQueueNum must >= 1!");

    if (GPUMode)
    {
        std::string info;
        switch (GPGPUModel)
        {
        case GPGPU::OpenCL:
#ifdef ENABLE_OPENCL
        {
            Anime4KCPP::OpenCL::GPUInfo ret =
                Anime4KCPP::OpenCL::checkGPUSupport(pID, dID);
            if (!ret)
            {
                std::ostringstream err;
                err <<
                    "Anime4KCPP: The current device is unavailable\n"
                    "Your input is: \n"
                    "    platform ID: " << pID << "\n"
                    "    device ID: " << dID << "\n"
                    "Error: \n"
                    "    " + ret() << '\n';
                env->ThrowError(err.str().c_str());
            }
        }
#endif
        break;
        case GPGPU::CUDA:
#ifdef ENABLE_CUDA
        {
            Anime4KCPP::Cuda::GPUInfo ret =
                Anime4KCPP::Cuda::checkGPUSupport(dID);
            if (!ret)
            {
                std::ostringstream err;
                err <<
                    "Anime4KCPP: The current device is unavailable\n"
                    "Your input is: \n"
                    "    device ID: " << dID << "\n"
                    "Error: \n"
                    "    " + ret() << '\n';
                env->ThrowError(err.str().c_str());
            }
        }
#endif
        break;
        }
    }

    return new Anime4KCPPFilter(
        clip,
        parameters,
        CNN,
        GPUMode,
        GPGPUModel,
        pID,
        dID,
        OpenCLQueueNum,
        OpenCLParallelIO,
        env
    );
}

AVSValue AC_ASP_CDECL listGPUs(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    std::string GPGPUModel;
    const char* tmpStr = args[0].AsString();
    if (!args[0].Defined())
        GPGPUModel = "opencl";
    else
        GPGPUModel = tmpStr;
    std::transform(GPGPUModel.begin(), GPGPUModel.end(), GPGPUModel.begin(), ::tolower);

#ifdef ENABLE_OPENCL
    if (GPGPUModel == "opencl")
        env->ThrowError(Anime4KCPP::OpenCL::listGPUs()().c_str());
    else
#endif
#ifdef ENABLE_CUDA
        if (GPGPUModel == "cuda")
            env->ThrowError(Anime4KCPP::Cuda::listGPUs()().c_str());
        else
#endif // ENABLE_CUDA
            env->ThrowError("unkonwn GPGPUModel");

    return AVSValue{};
}

AVSValue AC_ASP_CDECL benchmark(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    int pID = 0;
    int dID = 0;

    if (args[0].Defined())
        pID = args[0].AsInt();
    if (args[1].Defined())
        dID = args[1].AsInt();

    double CPUScoreDVD = Anime4KCPP::benchmark<Anime4KCPP::CPU::ACNet, 720, 480>();
    double CPUScoreHD = Anime4KCPP::benchmark<Anime4KCPP::CPU::ACNet, 1280, 720>();
    double CPUScoreFHD = Anime4KCPP::benchmark<Anime4KCPP::CPU::ACNet, 1920, 1080>();

#ifdef ENABLE_OPENCL
    double OpenCLScoreDVD = Anime4KCPP::benchmark<Anime4KCPP::OpenCL::ACNet, 720, 480>(pID, dID, Anime4KCPP::CNNType::ACNetHDNL0);
    double OpenCLScoreHD = Anime4KCPP::benchmark<Anime4KCPP::OpenCL::ACNet, 1280, 720>(pID, dID, Anime4KCPP::CNNType::ACNetHDNL0);
    double OpenCLScoreFHD = Anime4KCPP::benchmark<Anime4KCPP::OpenCL::ACNet, 1920, 1080>(pID, dID, Anime4KCPP::CNNType::ACNetHDNL0);
#endif

#ifdef ENABLE_CUDA
    double CudaScoreDVD = Anime4KCPP::benchmark<Anime4KCPP::Cuda::ACNet, 720, 480>(dID);
    double CudaScoreHD = Anime4KCPP::benchmark<Anime4KCPP::Cuda::ACNet, 1280, 720>(dID);
    double CudaScoreFHD = Anime4KCPP::benchmark<Anime4KCPP::Cuda::ACNet, 1920, 1080>(dID);
#endif 

    std::ostringstream oss;

    oss << "Benchmark test under 8-bit integer input and serial processing...\n\n";

    oss
        << "CPU score:\n"
        << " DVD(480P->960P): " << CPUScoreDVD << " FPS\n"
        << " HD(720P->1440P): " << CPUScoreHD << " FPS\n"
        << " FHD(1080P->2160P): " << CPUScoreFHD << " FPS\n\n";

#ifdef ENABLE_OPENCL
    oss
        << "OpenCL score: (pID = " << pID << ", dID = " << dID << ")\n"
        << " DVD(480P->960P): " << OpenCLScoreDVD << " FPS\n"
        << " HD(720P->1440P): " << OpenCLScoreHD << " FPS\n"
        << " FHD(1080P->2160P): " << OpenCLScoreFHD << " FPS\n\n";
#endif 

#ifdef ENABLE_CUDA
    oss
        << "CUDA score: (dID = " << dID << ")\n"
        << " DVD(480P->960P): " << CudaScoreDVD << " FPS\n"
        << " HD(720P->1440P): " << CudaScoreHD << " FPS\n"
        << " FHD(1080P->2160P): " << CudaScoreFHD << " FPS\n\n";
#endif 

    env->ThrowError(oss.str().c_str());
    return AVSValue();
}

AVSValue AC_ASP_CDECL Anime4KCPPInfo(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    std::ostringstream oss;
    oss << "Anime4KCPP VapourSynth Plugin" << '\n'
        << '\n'
        << "Anime4KCPP core information:\n"
        << "  Version: " << Anime4KCPP::CoreInfo::version() << '\n'
        << "  Parallel library: " << ANIME4KCPP_CORE_PARALLEL_LIBRARY << '\n'
        << "  Compiler: " << ANIME4KCPP_CORE_COMPILER << '\n'
        << "  Processors: " << Anime4KCPP::CoreInfo::supportedProcessors() << '\n'
        << "  CPU Optimization: " << Anime4KCPP::CoreInfo::CPUOptimizationMode() << '\n'
        << '\n'
        << "GitHub: https://github.com/TianZerL/Anime4KCPP" << std::endl;

    env->ThrowError(oss.str().c_str());
    return AVSValue{};
}

const AVS_Linkage* AVS_linkage = nullptr;

extern "C" AC_ASP_EXPORT const char* AC_ASP_STDCALL AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("Anime4KCPP",
        "c"
        "[passes]i"
        "[pushColorCount]i"
        "[strengthColor]f"
        "[strengthGradient]f"
        "[zoomFactor]f"
        "[ACNet]b"
        "[GPUMode]b"
        "[GPGPUModel]s"
        "[HDN]b"
        "[HDNLevel]i"
        "[platformID]i"
        "[deviceID]i"
        "[OpenCLQueueNum]i"
        "[OpenCLParallelIO]b",
        createAnime4KCPP, nullptr);

    env->AddFunction("Anime4KCPP2",
        "[src]c"
        "[passes]i"
        "[pushColorCount]i"
        "[strengthColor]f"
        "[strengthGradient]f"
        "[zoomFactor]f"
        "[ACNet]b"
        "[GPUMode]b"
        "[GPGPUModel]s"
        "[HDN]b"
        "[HDNLevel]i"
        "[platformID]i"
        "[deviceID]i"
        "[OpenCLQueueNum]i"
        "[OpenCLParallelIO]b",
        createAnime4KCPP, nullptr);

    env->AddFunction("listGPUs", "[GPGPUModel]s", listGPUs, nullptr);

    env->AddFunction("benchmark",
        "[platformID]i"
        "[deviceID]i",
        benchmark, nullptr);

    env->AddFunction("Anime4KCPPInfo", "", Anime4KCPPInfo, nullptr);

    return "Anime4KCPP AviSynthPlus Plugins";
}
