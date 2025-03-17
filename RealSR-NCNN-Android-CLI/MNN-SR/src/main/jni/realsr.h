// realsr implemented with ncnn library

#ifndef REALSR_H
#define REALSR_H

#include <string>
// ncnn
#include "net.h"
#include "gpu.h"
#include "layer.h"
#include <chrono>

using namespace std::chrono;
class RealSR
{
public:
    RealSR(int gpuid, bool tta_mode = false, int num_threads = 1);
    ~RealSR();

#if _WIN32
    int load(const std::wstring& parampath, const std::wstring& modelpath);
#else
    int load(const std::string& parampath, const std::string& modelpath);
#endif

    int process(const ncnn::Mat& inimage, ncnn::Mat& outimage) const;

    int process_cpu(const ncnn::Mat& inimage, ncnn::Mat& outimage) const;

public:
    // realsr parameters
    int scale;
    int tilesize;
    int prepadding;
    std::string net_input_name = "data";
    std::string net_output_name = "output";
private:
    ncnn::VulkanDevice* vkdev;
    ncnn::Net net;
    ncnn::Pipeline* realsr_preproc;
    ncnn::Pipeline* realsr_postproc;
    ncnn::Layer* bicubic_2x;
    ncnn::Layer* bicubic_3x;
    ncnn::Layer* bicubic_4x;
    bool tta_mode;
};

#endif // REALSR_H
