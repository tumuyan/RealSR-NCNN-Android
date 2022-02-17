// realcugan implemented with ncnn library

#ifndef REALCUGAN_H
#define REALCUGAN_H

#include <string>

// ncnn
#include "net.h"
#include "gpu.h"
#include "layer.h"

class FeatureCache;
class RealCUGAN
{
public:
    RealCUGAN(int gpuid, bool tta_mode = false, int num_threads = 1);
    ~RealCUGAN();

#if _WIN32
    int load(const std::wstring& parampath, const std::wstring& modelpath);
#else
    int load(const std::string& parampath, const std::string& modelpath);
#endif

    int process(const ncnn::Mat& inimage, ncnn::Mat& outimage) const;

    int process_cpu(const ncnn::Mat& inimage, ncnn::Mat& outimage) const;

    int process_se(const ncnn::Mat& inimage, ncnn::Mat& outimage) const;

    int process_cpu_se(const ncnn::Mat& inimage, ncnn::Mat& outimage) const;

    int process_se_rough(const ncnn::Mat& inimage, ncnn::Mat& outimage) const;

    int process_cpu_se_rough(const ncnn::Mat& inimage, ncnn::Mat& outimage) const;

    int process_se_very_rough(const ncnn::Mat& inimage, ncnn::Mat& outimage) const;

    int process_cpu_se_very_rough(const ncnn::Mat& inimage, ncnn::Mat& outimage) const;

protected:
    int process_se_stage0(const ncnn::Mat& inimage, const std::vector<std::string>& names, const std::vector<std::string>& outnames, const ncnn::Option& opt, FeatureCache& cache) const;
    int process_se_stage2(const ncnn::Mat& inimage, const std::vector<std::string>& names, ncnn::Mat& outimage, const ncnn::Option& opt, FeatureCache& cache) const;
    int process_se_sync_gap(const ncnn::Mat& inimage, const std::vector<std::string>& names, const ncnn::Option& opt, FeatureCache& cache) const;

    int process_se_very_rough_stage0(const ncnn::Mat& inimage, const std::vector<std::string>& names, const std::vector<std::string>& outnames, const ncnn::Option& opt, FeatureCache& cache) const;
    int process_se_very_rough_sync_gap(const ncnn::Mat& inimage, const std::vector<std::string>& names, const ncnn::Option& opt, FeatureCache& cache) const;

    int process_cpu_se_stage0(const ncnn::Mat& inimage, const std::vector<std::string>& names, const std::vector<std::string>& outnames, FeatureCache& cache) const;
    int process_cpu_se_stage2(const ncnn::Mat& inimage, const std::vector<std::string>& names, ncnn::Mat& outimage, FeatureCache& cache) const;
    int process_cpu_se_sync_gap(const ncnn::Mat& inimage, const std::vector<std::string>& names, FeatureCache& cache) const;

    int process_cpu_se_very_rough_stage0(const ncnn::Mat& inimage, const std::vector<std::string>& names, const std::vector<std::string>& outnames, FeatureCache& cache) const;
    int process_cpu_se_very_rough_sync_gap(const ncnn::Mat& inimage, const std::vector<std::string>& names, FeatureCache& cache) const;

public:
    // realcugan parameters
    int noise;
    int scale;
    int tilesize;
    int prepadding;
    int syncgap;

private:
    ncnn::VulkanDevice* vkdev;
    ncnn::Net net;
    ncnn::Pipeline* realcugan_preproc;
    ncnn::Pipeline* realcugan_postproc;
    ncnn::Pipeline* realcugan_4x_postproc;
    ncnn::Layer* bicubic_2x;
    ncnn::Layer* bicubic_3x;
    ncnn::Layer* bicubic_4x;
    bool tta_mode;
};

#endif // REALCUGAN_H
