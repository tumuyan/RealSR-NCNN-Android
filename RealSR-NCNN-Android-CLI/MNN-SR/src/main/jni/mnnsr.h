// MNNSR_H implemented with MNN library

#ifndef MNNSR_H
#define MNNSR_H

#include <string>

#include <opencv2/opencv.hpp>
// ncnn
#include "net.h"
#include "gpu.h"
#include "layer.h"
#include <chrono>
#include "MNN/Tensor.hpp"
#include "MNN/Interpreter.hpp"
#include "MNN/ImageProcess.hpp"
using namespace std::chrono;
class MNNSR
{
public:
    // TTA not use
    MNNSR(int gpuid, bool tta_mode = false, int num_threads = 1);
    ~MNNSR();

#if _WIN32
    int load(const std::wstring& parampath, const std::wstring& modelpath);
#else
    int load(const std::string& modelpath);
#endif
    void preProcessTile(const cv::Mat& tile);
    int process(const cv::Mat& inimage, cv::Mat& outimage);

    bool finish();

public:
    int scale;
    int tilesize;
    int prepadding;
    std::string net_input_name = "data";
    std::string net_output_name = "output";


    float* input_buffer;
    float* output_buffer;
    MNNForwardType backend_type;
private:
    ncnn::VulkanDevice* vkdev;
    ncnn::Net net;
    ncnn::Pipeline* realsr_preproc;
    ncnn::Pipeline* realsr_postproc;
    ncnn::Layer* bicubic_2x;
    ncnn::Layer* bicubic_3x;
    ncnn::Layer* bicubic_4x;
    bool tta_mode;

    MNN::Interpreter* interpreter;
    MNN::Session* session;
    MNN::Tensor* interpreter_input;
    MNN::Tensor* interpreter_output;
    MNN::Tensor* input_tensor;
    MNN::Tensor* output_tensor;
    std::shared_ptr<MNN::CV::ImageProcess> pretreat_ = nullptr;
    const float meanVals_[3] = {127.5f, 127.5f, 127.5f };
    const float normVals_[3] = {0.007843137f, 0.007843137f, 0.007843137f};
};

#endif // MNNSR_H
