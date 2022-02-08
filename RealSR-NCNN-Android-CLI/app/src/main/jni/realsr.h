// realsr implemented with ncnn library

#ifndef REALSR_H
#define REALSR_H

#include <string>

// ncnn
#include "net.h"
#include "gpu.h"
#include "layer.h"

class RealSR
{
public:
    RealSR(int gpuid, bool tta_mode = false);
    ~RealSR();

#if _WIN32
    int load(const std::wstring& parampath, const std::wstring& modelpath);
#else
    int load(const std::string& parampath, const std::string& modelpath);
#endif

    int process(const ncnn::Mat& inimage, ncnn::Mat& outimage) const;
//    int load(  AAssetManager *mgr , int style_type);

public:
    // realsr parameters
    int scale;
    int tilesize;
    int prepadding;


private:
    ncnn::Net net;
    ncnn::Pipeline* realsr_preproc;
    ncnn::Pipeline* realsr_postproc;
    ncnn::Layer* bicubic_4x;
    bool tta_mode;
    int style=-1;
    bool use_gpu= true;
};

#endif // REALSR_H
