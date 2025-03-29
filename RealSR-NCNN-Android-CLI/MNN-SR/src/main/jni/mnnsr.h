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

class MNNSR {
public:
    MNNSR();

    ~MNNSR();

#if _WIN32
    int load(const std::wstring& modelpath, bool cachemodel);
#else

    int load(const std::string &modelpath, bool cachemodel);

#endif

    int process(const cv::Mat &inimage, cv::Mat &outimage);
    cv::Mat TensorToCvMat(void);

public:
    int scale;
    int tilesize;
    int prepadding;

    float *input_buffer;
    float *output_buffer;
    MNNForwardType backend_type;

private:
    MNN::Interpreter *interpreter;
    MNN::Session *session;
    MNN::Tensor *interpreter_input;
    MNN::Tensor *interpreter_output;
    MNN::Tensor *input_tensor;
    MNN::Tensor *output_tensor;
    std::shared_ptr<MNN::CV::ImageProcess> pretreat_ = nullptr;
    const float meanVals_[3] = {0, 0, 0};
    const float normVals_[3] = { 1.0 / 255, 1.0 / 255, 1.0 / 255 };
    bool cachemodel;
};

#endif // MNNSR_H
