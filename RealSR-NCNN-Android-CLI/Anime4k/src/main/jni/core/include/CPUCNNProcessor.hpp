#ifndef ANIME4KCPP_CORE_CPU_CNN_PROCESSOR_HPP
#define ANIME4KCPP_CORE_CPU_CNN_PROCESSOR_HPP

#ifndef ENABLE_OPENCV_DNN

#include "AC.hpp"

namespace Anime4KCPP::CPU
{
    class CNNProcessor;
}

class Anime4KCPP::CPU::CNNProcessor
{
protected:
    void conv1To8(const cv::Mat& img, const float* kernels, const float* biases, cv::Mat& tmpMat);
    void conv8To8(const float* kernels, const float* biases, cv::Mat& tmpMat);
    void convTranspose8To1(cv::Mat& img, const float* kernels, cv::Mat& tmpMat);
};

#endif // !ENABLE_OPENCV_DNN

#endif // !ANIME4KCPP_CORE_CPU_CNN_PROCESSOR_HPP
