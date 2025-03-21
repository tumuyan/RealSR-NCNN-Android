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
    int load(const std::wstring& modelpath);
#else
    int load(const std::string& modelpath);
#endif
    //void preProcessTile(const cv::Mat& tile);
    int process(const cv::Mat& inimage, cv::Mat& outimage);
    void transform(const cv::Mat &mat);
    void copyOutputTile(cv::Mat outputTile,cv::Mat  outimage, int out_x0, int out_y0);

public:
    int scale;
    int tilesize;
    int prepadding;

    float* input_buffer;
    float* output_buffer;
    MNNForwardType backend_type;

private:
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
