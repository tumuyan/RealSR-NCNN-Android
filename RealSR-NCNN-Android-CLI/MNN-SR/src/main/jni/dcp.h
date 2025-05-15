//
// Created by Yazii on 2025/5/1.
// DCP model implementation converted from https://github.com/nanoskript/deepcreampy-onnx-docker
// it couldn't works yet
//

#ifndef REALSR_NCNN_ANDROID_CLI_DCP_H
#define REALSR_NCNN_ANDROID_CLI_DCP_H


#include <string>

#include <opencv2/opencv.hpp>
#include <chrono>
#include "MNN/Tensor.hpp"
#include "MNN/Interpreter.hpp"
#include "MNN/ImageProcess.hpp"
#include "utils.hpp"

#include <vector>
#include <tuple>
#include <utility>
#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>

using namespace std::chrono;

class DCP {

public:
    DCP();

    ~DCP();

#if _WIN32
    int load(const std::wstring& modelpath, bool cachemodel, bool nchw = true);
#else

    int load(const std::string &modelpath, bool cachemodel, bool nchw = true);

#endif

    int process(const cv::Mat &inimage, cv::Mat &outimage, const cv::Mat &mask = cv::Mat());

    int decensor(const cv::Mat &inimage, cv::Mat &outimage);

    cv::Mat TensorToCvMat(void);


    // DCP
    cv::Mat predict(const cv::Mat &crop_img_array, const cv::Mat &mask_array, bool is_mosaic);

    cv::Mat find_mask(const cv::Mat &colored, const cv::Scalar &mask_color);

    std::vector<std::vector<cv::Point>>
    find_regions(const cv::Mat &image, const cv::Scalar &mask_color);

    cv::Rect
    expand_bounding(const cv::Mat &ori, const std::vector<cv::Point> &region, double expand_factor);

    std::pair<cv::Mat, cv::Rect>
    predict_region(const cv::Mat &ori, const std::vector<cv::Point> &region, const cv::Mat &mask,
                   bool is_mosaic);

    cv::Mat decensor(const cv::Mat &ori, const cv::Mat &colored, bool is_mosaic);

public:
    int tilesize=256;
    int prepadding;

    float *input_buffer2;
    float *input_buffer;
    float *output_buffer;
    MNNForwardType backend_type;

private:

    bool inputs_num = 1;
    int model_channel = 3;
    int model_channel2 = 3;
    MNN::Interpreter *interpreter;
    MNN::Session *session;
    MNN::Tensor *interpreter_input2;
    MNN::Tensor *interpreter_input;
    MNN::Tensor *interpreter_output;
    MNN::Tensor *input_tensor2;
    MNN::Tensor *input_tensor;
    MNN::Tensor *output_tensor;
    std::shared_ptr<MNN::CV::ImageProcess> pretreat_ = nullptr;
    std::shared_ptr<MNN::CV::ImageProcess> pretreat_1ch_ = nullptr;
    const float meanVals_[3] = {0, 0, 0};
    const float normVals_[3] = {1.0 / 255, 1.0 / 255, 1.0 / 255};
    bool cachemodel;
};

#endif //REALSR_NCNN_ANDROID_CLI_DCP_H
