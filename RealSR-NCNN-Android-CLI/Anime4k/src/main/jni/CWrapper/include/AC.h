#ifndef ANIME4KCPP_CWRAPPER_AC_H
#define ANIME4KCPP_CWRAPPER_AC_H

#include <stddef.h>
#include <stdint.h>

#include <ac_c_export.h>

#ifdef _WIN32
#define AC_C_API __stdcall
#else
#define AC_C_API
#endif

#define acLoadImageRGB acLoadImageRGBPlanarB
#define acLoadImageYUV acLoadImageYUVPlanarB
#define acLoadImageRGBBytes acLoadImageRGBPackedB
#define acSaveImageRGB acSaveImageRGBPlanarB
#define acSaveImageRGBBytes acSaveImageRGBPacked
#define acSaveImagBufferSizeYUV acSaveImageBufferSizeRGB

#define acSaveImageYUV acSaveImageRGB
#define acSaveImageYUV444Bytes acSaveImageRGBBytes

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    typedef enum ac_processType
    {
        AC_CPU_Anime4K09,
        AC_CPU_ACNet,
        AC_OpenCL_Anime4K09,
        AC_OpenCL_ACNet,
        AC_Cuda_Anime4K09,
        AC_Cuda_ACNet,
        //deprecated, use AC_CPU_Anime4K09
        AC_CPU = AC_CPU_Anime4K09,
        //deprecated, use AC_CPU_ACNet
        AC_CPUCNN = AC_CPU_ACNet,
        //deprecated, use AC_OpenCL_Anime4K09
        AC_GPU = AC_OpenCL_Anime4K09,
        //deprecated, use AC_OpenCL_ACNet
        AC_GPUCNN = AC_OpenCL_ACNet
    } ac_processType;

    typedef enum ac_CNNType
    {
        AC_Default,
        AC_ACNetHDNL0,
        AC_ACNetHDNL1,
        AC_ACNetHDNL2,
        AC_ACNetHDNL3
    } ac_CNNType;

    typedef enum ac_GPGPU
    {
        AC_CUDA,
        AC_OpenCL
    } ac_GPGPU;

    typedef enum ac_manager
    {
        AC_Manager_OpenCL_Anime4K09 = 1 << 0,
        AC_Manager_OpenCL_ACNet = 1 << 1,
        AC_Manager_Cuda = 1 << 2
    } ac_manager;

    typedef enum ac_bool
    {
        AC_FALSE = 0,
        AC_TRUE = 1
    } ac_bool;

    typedef enum ac_error
    {
        AC_OK = 0,
        AC_ERROR_NULL_INSTANCE,
        AC_ERROR_NULL_PARAMETERS,
        AC_ERROR_NULL_DATA,
        AC_ERROR_INIT_PROCESSOR,
        AC_ERROR_PORCESSOR_TYPE,
        AC_ERROR_LOAD_IMAGE,
        AC_ERROR_LOAD_VIDEO,
        AC_ERROR_INIT_VIDEO_WRITER,
        AC_ERROR_GPU_PROCESS,
        AC_ERROR_SAVE_TO_NULL_POINTER,
        AC_ERROR_NOT_YUV444,
        AC_ERROR_YUV444_AND_RGB32_AT_SAME_TIME,
        AC_ERROR_OPENCL_NOT_SUPPORTED,
        AC_ERROR_CUDA_NOT_SUPPORTED,
        AC_ERROR_PREVIEW_GUI_DISABLE,
        AC_ERROR_IMAGE_IO_DISABLE,
        AC_ERROR_INSUFFICIENT_BUFFER_SIZE,
        AC_ERROR_FAILED_TO_ENCODE,

        //deprecated, use AC_ERROR_INIT_PROCESSOR
        AC_ERROR_INIT_GPU = AC_ERROR_INIT_PROCESSOR,
    } ac_error;

    typedef enum ac_codec
    {
        OTHER = -1,
        MP4V = 0,
        DXVA = 1,
        AVC1 = 2,
        VP09 = 3,
        HEVC = 4,
        AV01 = 5
    } ac_codec;

    typedef struct ac_parameters
    {
        int passes;
        int pushColorCount;
        double strengthColor;
        double strengthGradient;
        double zoomFactor;
        ac_bool fastMode;
        ac_bool preprocessing;
        ac_bool postprocessing;
        uint8_t preFilters;
        uint8_t postFilters;
        ac_bool HDN;
        int HDNLevel;
        ac_bool alpha;
    } ac_parameters;

    typedef struct ac_version
    {
        char coreVersion[32];
        char wrapperVersion[32];
    } ac_version;

    typedef struct ac_OpenCLAnime4K09Data
    {
        int pID;
        int dID;
        int OpenCLQueueNum;
        ac_bool OpenCLParallelIO;
    } ac_OpenCLAnime4K09Data;

    typedef struct ac_OpenCLACNetData
    {
        int pID;
        int dID;
        int OpenCLQueueNum;
        ac_bool OpenCLParallelIO;
        ac_CNNType CNNType;
    } ac_OpenCLACNetData;

    typedef struct ac_CUDAData
    {
        int dID;
    } ac_CUDAData;

    typedef struct ac_managerData
    {
        ac_OpenCLAnime4K09Data* OpenCLAnime4K09Data;
        ac_OpenCLACNetData* OpenCLACNetData;
        ac_CUDAData* CUDAData;
    } ac_managerData;

    typedef void* ac_instance;
    typedef void* ac_videoProcessor;
    typedef unsigned int ac_manager_t;

    AC_C_DEPRECATED AC_C_EXPORT ac_version AC_C_API acGetVersion(void);
    AC_C_DEPRECATED AC_C_EXPORT ac_instance AC_C_API acGetInstance(
        ac_bool initGPU, ac_bool initGPUCNN,
        unsigned int platformID, unsigned int deviceID,
        ac_parameters* parameters, ac_processType type,
        ac_error* error
    );
    AC_C_DEPRECATED AC_C_EXPORT ac_error AC_C_API acInitGPU(void);
    AC_C_DEPRECATED AC_C_EXPORT void AC_C_API acReleaseGPU(void);
    AC_C_DEPRECATED AC_C_EXPORT ac_error AC_C_API acInitGPUCNN(void);
    AC_C_DEPRECATED AC_C_EXPORT void AC_C_API acReleaseGPUCNN(void);
    AC_C_DEPRECATED AC_C_EXPORT void AC_C_API acFreeInstance(ac_instance instance, ac_bool releaseGPU, ac_bool releaseGPUCNN);
    AC_C_DEPRECATED AC_C_EXPORT void AC_C_API acBenchmark(int pID, int dID, double* CPUScore, double* GPUScore);
    //acCheckGPUSupport may need to run two times for getting length of info string first
    AC_C_DEPRECATED AC_C_EXPORT ac_bool AC_C_API acCheckGPUSupport(unsigned int pID, unsigned int dID, char* info, size_t* length);
    AC_C_DEPRECATED AC_C_EXPORT ac_bool AC_C_API acIsInitializedGPU(void);
    AC_C_DEPRECATED AC_C_EXPORT ac_bool AC_C_API acIsInitializedGPUCNN(void);
    AC_C_DEPRECATED AC_C_EXPORT ac_error AC_C_API acInitGPU2(unsigned int managers, ac_managerData* managerData);
    AC_C_DEPRECATED AC_C_EXPORT void AC_C_API acReleaseGPU2(void);

    AC_C_EXPORT void AC_C_API acGetVersion2(ac_version* v);
    AC_C_EXPORT ac_error AC_C_API acInitProcessor(ac_manager_t managers, ac_managerData* managerData);
    AC_C_EXPORT void AC_C_API acReleaseAllProcessors(void);

    AC_C_EXPORT ac_instance AC_C_API acGetInstance2(
        ac_manager_t managers, ac_managerData* managerData,
        ac_parameters* parameters, ac_processType type,
        ac_error* error
    );
    AC_C_EXPORT ac_instance AC_C_API acGetInstance3(
        ac_parameters* parameters, ac_processType type,
        ac_error* error
    );

    AC_C_EXPORT void AC_C_API acFreeInstance2(ac_instance instance);
    AC_C_EXPORT ac_error AC_C_API acInitParameters(ac_parameters* parameters);
    AC_C_EXPORT ac_error AC_C_API acLoadImage(ac_instance instance, const char* srcFile);
    AC_C_EXPORT ac_error AC_C_API acLoadImageFromBuffer(ac_instance instance, const uint8_t* buf, size_t size);
    AC_C_EXPORT ac_error AC_C_API acProcess(ac_instance instance);
    AC_C_EXPORT ac_error AC_C_API acShowImage(ac_instance instance, ac_bool R2B);
    AC_C_EXPORT ac_error AC_C_API acSaveImage(ac_instance instance, const char* dstFile);
    AC_C_EXPORT ac_error AC_C_API acSaveImageToBuffer(ac_instance instance, const char* suffix, uint8_t* buf, size_t size);
    AC_C_EXPORT ac_error AC_C_API acSetParameters(ac_instance instance, ac_parameters* parameters);
    AC_C_EXPORT ac_error AC_C_API acLoadImageRGBPlanarB(ac_instance instance, int rows, int cols, size_t stride, uint8_t* r, uint8_t* g, uint8_t* b, ac_bool inputAsYUV444);
    AC_C_EXPORT ac_error AC_C_API acLoadImageYUVPlanarB(ac_instance instance,
        int rowsY, int colsY, size_t strideY, uint8_t* y,
        int rowsU, int colsU, size_t strideU, uint8_t* u,
        int rowsV, int colsV, size_t strideV, uint8_t* v);
    AC_C_EXPORT ac_error AC_C_API acLoadImageRGBPackedB(ac_instance instance, int rows, int cols, size_t stride, uint8_t* data, ac_bool inputAsYUV444, ac_bool inputAsRGB32);
    AC_C_EXPORT ac_error AC_C_API acLoadImageGrayscaleB(ac_instance instance, int rows, int cols, size_t stride, uint8_t* data);
    AC_C_EXPORT ac_error AC_C_API acLoadImageRGBPlanarW(ac_instance instance, int rows, int cols, size_t stride, uint16_t* r, uint16_t* g, uint16_t* b, ac_bool inputAsYUV444);
    AC_C_EXPORT ac_error AC_C_API acLoadImageYUVPlanarW(ac_instance instance,
        int rowsY, int colsY, size_t strideY, uint16_t* y,
        int rowsU, int colsU, size_t strideU, uint16_t* u,
        int rowsV, int colsV, size_t strideV, uint16_t* v);
    AC_C_EXPORT ac_error AC_C_API acLoadImageRGBPackedW(ac_instance instance, int rows, int cols, size_t stride, uint16_t* data, ac_bool inputAsYUV444, ac_bool inputAsRGB32);
    AC_C_EXPORT ac_error AC_C_API acLoadImageGrayscaleW(ac_instance instance, int rows, int cols, size_t stride, uint16_t* data);
    AC_C_EXPORT ac_error AC_C_API acLoadImageRGBPlanarF(ac_instance instance, int rows, int cols, size_t stride, float* r, float* g, float* b, ac_bool inputAsYUV444);
    AC_C_EXPORT ac_error AC_C_API acLoadImageYUVPlanarF(ac_instance instance,
        int rowsY, int colsY, size_t strideY, float* y,
        int rowsU, int colsU, size_t strideU, float* u,
        int rowsV, int colsV, size_t strideV, float* v);
    AC_C_EXPORT ac_error AC_C_API acLoadImageRGBPackedF(ac_instance instance, int rows, int cols, size_t stride, float* data, ac_bool inputAsYUV444, ac_bool inputAsRGB32);
    AC_C_EXPORT ac_error AC_C_API acLoadImageGrayscaleF(ac_instance instance, int rows, int cols, size_t stride, float* data);
    AC_C_EXPORT ac_error AC_C_API acSaveImageRGBPlanar(ac_instance instance, uint8_t* r, size_t strideR, uint8_t* g, size_t strideG, uint8_t* b, size_t strideB);
    AC_C_EXPORT ac_error AC_C_API acSaveImageRGBPacked(ac_instance instance, uint8_t* data, size_t stride);
    //acGetInfo may need to run two times for getting length of info string first
    AC_C_EXPORT ac_error AC_C_API acGetInfo(ac_instance instance, char* info, size_t* length);
    //acGetFiltersInfo may need to run two times for getting length of info string first
    AC_C_EXPORT ac_error AC_C_API acGetFiltersInfo(ac_instance instance, char* info, size_t* length);
    AC_C_EXPORT ac_bool AC_C_API acCheckGPUSupport2(ac_GPGPU GPGPUModel, unsigned int pID, unsigned int dID, char* info, size_t* length);
    //acListGPUs may need to run two times for getting length of info string and length (platforms) of devices first
    AC_C_EXPORT void AC_C_API acListGPUs(char* info, size_t* length, size_t* platforms, size_t* devices);
    AC_C_EXPORT void AC_C_API acGetLastCoreErrorString(char* err, size_t* length);
    AC_C_EXPORT double AC_C_API acBenchmark2(ac_processType processType, int pID, int dID);
    AC_C_EXPORT ac_processType AC_C_API acGetProcessType(ac_instance instance, ac_error* error);
    //acGetProcessorInfo may need to run two times for getting length of info string first
    AC_C_EXPORT ac_error AC_C_API acGetProcessorInfo(ac_instance instance, char* info, size_t* length);
    AC_C_EXPORT ac_error AC_C_API acSaveImageBufferSize(ac_instance instance, size_t* dataSize, size_t dstStride);
    AC_C_EXPORT ac_error AC_C_API acSaveImageBufferSizeRGB(
        ac_instance instance,
        size_t* rSize, size_t dstStrideR,
        size_t* gSize, size_t dstStrideG,
        size_t* bSize, size_t dstStrideB);
    AC_C_EXPORT ac_error AC_C_API saveImageShape(ac_instance instance, int* cols, int* rows, int* channels);

#ifdef ENABLE_VIDEO

    AC_C_EXPORT ac_videoProcessor AC_C_API acGetVideoProcessor(
        ac_parameters* parameters, ac_processType type, ac_error* error
    );
    AC_C_EXPORT ac_videoProcessor AC_C_API acGetVideoProcessorWithThreads(
        ac_parameters* parameters, ac_processType type, unsigned int threads ,ac_error* error
    );
    AC_C_EXPORT ac_videoProcessor AC_C_API acGetVideoProcessorFromInstance(ac_instance instance);
    AC_C_EXPORT ac_videoProcessor AC_C_API acGetVideoProcessorFromInstanceWithThreads(ac_instance instance, unsigned int threads);
    AC_C_EXPORT void AC_C_API acFreeVideoProcessor(ac_videoProcessor instance);
    AC_C_EXPORT ac_error AC_C_API acLoadVideo(ac_videoProcessor instance, const char* srcFile);
    AC_C_EXPORT ac_error AC_C_API acProcessVideo(ac_videoProcessor instance);
    AC_C_EXPORT ac_error AC_C_API acProcessWithPrintProgress(ac_videoProcessor instance);
    /*
    Processing with callback funciton:
    callBack(double progress);
    progress form 0 to 1
    */
    AC_C_EXPORT ac_error AC_C_API acProcessWithProgress(ac_videoProcessor instance, void (*callBack)(double));
    /*
    Processing with callback funciton:
    callBack(double progress, double elapsedTime);
    progress form 0 to 1
    */
    AC_C_EXPORT ac_error AC_C_API acProcessWithProgressTime(ac_videoProcessor instance, void (*callBack)(double, double));
    AC_C_EXPORT ac_error AC_C_API acStopVideoProcess(ac_videoProcessor instance);
    AC_C_EXPORT ac_error AC_C_API acPauseVideoProcess(ac_videoProcessor instance);
    AC_C_EXPORT ac_error AC_C_API acContinueVideoProcess(ac_videoProcessor instance);
    AC_C_EXPORT ac_error AC_C_API acSetSaveVideoInfo(ac_videoProcessor instance, const char* dstFile, ac_codec codec, double fps);
    AC_C_EXPORT ac_error AC_C_API acSaveVideo(ac_videoProcessor instance);

#endif

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !ANIME4KCPP_CWRAPPER_AC_H
