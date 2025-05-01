//
// Created by Yazii on 2025/3/30.
//
#ifndef REALSR_NCNN_ANDROID_CLI_UTILS_HPP
#define REALSR_NCNN_ANDROID_CLI_UTILS_HPP

#include <string>
#include "MNN/Tensor.hpp"
#include "MNN/Interpreter.hpp"
#include "MNN/ImageProcess.hpp"

using namespace MNN;

/**
 * 获取MNN后端类型的可读名称
 * @param backend_type MNNForwardType枚举值
 * @return 后端名称字符串（如"CPU"），未知类型返回数字字符串
 */
static std::string get_backend_name(MNNForwardType backend_type) {
    switch (backend_type) {
        case MNN_FORWARD_CPU:
            return "CPU";
        case MNN_FORWARD_CPU_EXTENSION:
            return "CPU Extension";
        case MNN_FORWARD_OPENCL:
            return "OpenCL";
        case MNN_FORWARD_OPENGL:
            return "OpenGL";
        case MNN_FORWARD_VULKAN:
            return "Vulkan";
        case MNN_FORWARD_METAL:
            return "Metal";
        case MNN_FORWARD_CUDA:
            return "CUDA";
        case MNN_FORWARD_NN:
            return "NN API";
        default:
            return std::to_string(static_cast<int>(backend_type));
    }
}


static std::string float2str(float f) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if (f >= 10000000000000000.0f) {
        oss << (f / 10000000000000000.0f) << " E";
    } else if (f >= 1000000000000000.0f) {
        oss << (f / 1000000000000000.0f) << " P";
    } else if (f >= 1000000000000.0f) {
        oss << (f / 1000000000000.0f) << " T";
    } else if (f >= 1000000000.0f) {
        oss << (f / 1000000000.0f) << " G";
    } else if (f >= 1000000.0f) {
        oss << (f / 1000000.0f) << " M";
    } else if (f >= 1000.0f) {
        oss << (f / 1000.0f) << " K";
    } else {
        oss << f;
    }

    return oss.str();
}


static std::string float2str(float v, int unit) {
    float f = v * pow(10, unit);
    return float2str(f);
}


typedef enum {
    UnSet = 0,
    RGB     = 1,
    BGR      = 2,
    RGBA      = 3,
    BGRA     = 4,
    YCbCr    = 5,
    YUV      = 6,
    GRAY = 10,
    Gray2YCbCr = 11,
    Gray2YUV = 12,
} ColorType;


inline const char* colorTypeToStr(ColorType type) {
    switch (type) {
        case RGB: return "RGB";
        case BGR: return "BGR";
        case RGBA: return "RGBA";
        case BGRA: return "BGRA";
        case YCbCr: return "YCbCr";
        case YUV: return "YUV";
        case GRAY: return "GRAY";
        case Gray2YCbCr: return "Gray2YCbCr";
        case Gray2YUV: return "Gray2YUV";
        default: return "UNKNOWN";
    }
}


/**
 * @brief 将一个 CV_8U 格式的掩码区域以半透明红色绘制到 CV_8UC3 图像上
 *
 * @param color_image 输入/输出图像 (CV_8UC3)。掩码区域将绘制到此图像上。
 *                    该图像会被修改。
 * @param mask_8u 掩码图像 (CV_8U)。非零像素表示绘制区域。
 *                必须与 color_image 尺寸相同。
 * @param alpha 半透明度因子 (0.0 到 1.0)。0.0 表示完全透明，1.0 表示完全不透明。
 * @param red_color 用于绘制的红色。默认为纯红色 cv::Scalar(0, 0, 255)。
 */
static void drawSemiTransparentMask(cv::Mat& color_image, const cv::Mat& mask_8u, double alpha, const cv::Scalar& red_color = cv::Scalar(0, 0, 255))
{
    // 检查图像尺寸是否匹配
    if (color_image.size() != mask_8u.size()) {
        std::cerr << "Error: Image and mask sizes do not match!" << std::endl;
        return;
    }

    // 检查图像类型
    if (color_image.type() != CV_8UC3) {
        std::cerr << "Error: Input image must be CV_8UC3!" << std::endl;
        return;
    }
    if (mask_8u.type() != CV_8U) {
        std::cerr << "Error: Mask must be CV_8U!" << std::endl;
        return;
    }

    // 确保 alpha 在有效范围内
    alpha = std::max(0.0, std::min(1.0, alpha));
    double beta = 1.0 - alpha;

    // 创建一个临时红色覆盖层图像，与彩色图像大小和类型相同
    cv::Mat red_overlay(color_image.size(), color_image.type(), red_color);

    // 使用 addWeighted 混合红色覆盖层和原始图像（在所有像素上进行）
    // blended_full = alpha * red_overlay + (1 - alpha) * color_image
    cv::Mat blended_full;
    cv::addWeighted(red_overlay, alpha, color_image, beta, 0.0, blended_full);

    // 使用 mask_8u 将混合后的结果拷贝回 color_image
    // 只有 mask_8u 中非零的像素位置才会被拷贝
    blended_full.copyTo(color_image, mask_8u);

    // 注意：这里 color_image 已经被修改
}


#endif //REALSR_NCNN_ANDROID_CLI_UTILS_HPP