// MNNSR_H implemented with MNN library

#ifndef MNNSR_H
#define MNNSR_H

#include <string>

#include <opencv2/opencv.hpp>

#include <chrono>
#include "MNN/Tensor.hpp"
#include "MNN/Interpreter.hpp"
#include "MNN/ImageProcess.hpp"
#include "utils.hpp"
#include "dcp.h"

using namespace std::chrono;

class MNNSR {
public:
    MNNSR(int color_type, int decensor_mode);

    ~MNNSR();

#if _WIN32
    int load(const std::wstring& modelpath, bool cachemodel, const bool nchw = true);
#else

    int load(const std::string &modelpath, bool cachemodel, const bool nchw = true);

#endif

    int process(const cv::Mat &inimage, cv::Mat &outimage, const cv::Mat &mask = cv::Mat());

    int decensor(const cv::Mat &inimage, cv::Mat &outimage, const bool det_box = false);

    cv::Mat TensorToCvMat(void);

public:
    int scale;
    ColorType color;
    int model_channel = 3;
    uint tilesize;
    uint prepadding;

    float *input_buffer;
    float *output_buffer;
    MNNForwardType backend_type;
    DCP *dcp = nullptr;


private:
    MNN::Interpreter *interpreter;
    MNN::Session *session;
    MNN::Tensor *interpreter_input;
    MNN::Tensor *interpreter_output;
    MNN::Tensor *input_tensor;
    MNN::Tensor *output_tensor;
    std::shared_ptr<MNN::CV::ImageProcess> pretreat_ = nullptr;
    const float meanVals_[3] = {0, 0, 0};
    const float normVals_[3] = {1.0 / 255, 1.0 / 255, 1.0 / 255};
    bool cachemodel;
    int decensor_mode=-1;

    bool scale_checked = false; // Flag to check scale only once
    float interp_scale = 1.0f;  // Interpolation factor to match target scale
};

#endif // MNNSR_H
