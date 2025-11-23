#ifndef MNNSR_H
#define MNNSR_H

#include <string>

#include <opencv2/opencv.hpp>
// ncnn
#include "net.h"
#include "gpu.h"
#include "layer.h"
#include <chrono>
#include <MNN/expr/Module.hpp>
#include <MNN/expr/Executor.hpp>
#include <MNN/expr/ExprCreator.hpp>
#include <MNN/Interpreter.hpp>
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

    cv::Mat TensorToCvMat(MNN::Express::VARP output);

public:
    int scale;
    ColorType color;
    int model_channel = 3;
    uint tilesize;
    uint prepadding;

    MNNForwardType backend_type;
    DCP *dcp = nullptr;


private:
    std::shared_ptr<MNN::Express::Module> net;
    std::shared_ptr<MNN::Express::Executor::RuntimeManager> rtmgr;
    std::shared_ptr<MNN::CV::ImageProcess> pretreat_ = nullptr;
    const float meanVals_[3] = {0, 0, 0};
    const float normVals_[3] = {1.0 / 255, 1.0 / 255, 1.0 / 255};
    bool cachemodel;
    int decensor_mode=-1;

    bool scale_checked = false; // Flag to check scale only once
    float interp_scale = 1.0f;  // Interpolation factor to match target scale
};

#endif // MNNSR_H
