#ifndef ANIME4KCPP_CORE_CPU_ACNET_PROCESSOR_HPP
#define ANIME4KCPP_CORE_CPU_ACNET_PROCESSOR_HPP

#include "AC.hpp"
#include "CPUCNNProcessor.hpp"
#include "CNN.hpp"

namespace Anime4KCPP::CPU
{
    class ACNetProcessor;
#ifdef ENABLE_OPENCV_DNN
    class ACNetHDN;
#else
    class ACNetHDNL0;
    class ACNetHDNL1;
    class ACNetHDNL2;
    class ACNetHDNL3;
#endif
    std::unique_ptr<ACNetProcessor> createACNetProcessor(int typeIndex);
}

class Anime4KCPP::CPU::ACNetProcessor 
#ifndef ENABLE_OPENCV_DNN
    : public CNNProcessor
#endif
{
public:
    ACNetProcessor() = default;
    virtual ~ACNetProcessor() = default;
    virtual void process(const cv::Mat& src, cv::Mat& dst, int scaleTimes) = 0;
};

#ifdef ENABLE_OPENCV_DNN
class Anime4KCPP::CPU::ACNetHDN : public ACNetProcessor
{
public:
    ACNetHDN(std::string modelPath);
    ~ACNetHDN() override = default;
    void process(const cv::Mat& src, cv::Mat& dst, int scaleTimes) override;
private:
    cv::dnn::Net net;
};

#else

class Anime4KCPP::CPU::ACNetHDNL0 : public ACNetProcessor
{
public:
    ACNetHDNL0() = default;
    ~ACNetHDNL0() override = default;
    void process(const cv::Mat& src, cv::Mat& dst, int scaleTimes) override;
private:
    alignas(32) static const float kernelsL1[9 * 8];
    alignas(32) static const float kernels[8][9 * 8 * 8];
    alignas(32) static const float kernelsL10[4 * 8];
    alignas(32) static const float biasL1[8];
    alignas(32) static const float biases[8][8];
};

class Anime4KCPP::CPU::ACNetHDNL1 : public ACNetProcessor
{
public:
    ACNetHDNL1() = default;
    ~ACNetHDNL1() override = default;
    void process(const cv::Mat& src, cv::Mat& dst, int scaleTimes) override;
private:
    alignas(32) static const float kernelsL1[9 * 8];
    alignas(32) static const float kernels[8][9 * 8 * 8];
    alignas(32) static const float kernelsL10[4 * 8];
    alignas(32) static const float biasL1[8];
    alignas(32) static const float biases[8][8];
};

class Anime4KCPP::CPU::ACNetHDNL2 : public ACNetProcessor
{
public:
    ACNetHDNL2() = default;
    ~ACNetHDNL2() override = default;
    void process(const cv::Mat& src, cv::Mat& dst, int scaleTimes) override;
private:
    alignas(32) static const float kernelsL1[9 * 8];
    alignas(32) static const float kernels[8][9 * 8 * 8];
    alignas(32) static const float kernelsL10[4 * 8];
    alignas(32) static const float biasL1[8];
    alignas(32) static const float biases[8][8];
};

class Anime4KCPP::CPU::ACNetHDNL3 : public ACNetProcessor
{
public:
    ACNetHDNL3() = default;
    ~ACNetHDNL3() override = default;
    void process(const cv::Mat& src, cv::Mat& dst, int scaleTimes) override;
private:
    alignas(32) static const float kernelsL1[9 * 8];
    alignas(32) static const float kernels[8][9 * 8 * 8];
    alignas(32) static const float kernelsL10[4 * 8];
    alignas(32) static const float biasL1[8];
    alignas(32) static const float biases[8][8];
};
#endif // ENABLE_OPENCV_DNN

#endif // !ANIME4KCPP_CORE_CPU_ACNET_PROCESSOR_HPP
