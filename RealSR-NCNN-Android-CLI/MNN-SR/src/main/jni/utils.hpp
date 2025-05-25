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
        case MNN_FORWARD_AUTO:
            return "Auto";
        default:
            return std::to_string(static_cast<int>(backend_type));
    }
}

#include <string>
#include <sstream>
#include <iomanip>

inline std::string format_time_ms(double ms) {
    constexpr double ms_per_sec = 1000.0;
    constexpr double ms_per_min = 60 * ms_per_sec;
    constexpr double ms_per_hour = 60 * ms_per_min;
    constexpr double ms_per_day = 24 * ms_per_hour;

    std::ostringstream oss;
    if (ms < 2 * ms_per_min) {
        // 小于2分钟，只显示秒（保留2位小数，无单位）
        oss << std::fixed << std::setprecision(2) << (ms / ms_per_sec);
    }
    else if (ms < 2 * ms_per_hour) {
        // 小于2小时，显示分和秒
        int min = static_cast<int>(ms / ms_per_min);
        double sec = (ms - min * ms_per_min) / ms_per_sec;
        oss << min << "m";
        if (sec >= 0.01)
            oss << std::fixed << std::setprecision(2) << sec;
    }
    else if (ms < 2 * ms_per_day) {
        // 小于2天，显示小时和分
        int hour = static_cast<int>(ms / ms_per_hour);
        int min = static_cast<int>((ms - hour * ms_per_hour) / ms_per_min);
        oss << hour << "h";
        if (min > 0)
            oss << min << "m";
    }
    else {
        // 2天及以上，显示天和小时
        int day = static_cast<int>(ms / ms_per_day);
        int hour = static_cast<int>((ms - day * ms_per_day) / ms_per_hour);
        oss << day << "d";
        if (hour > 0)
            oss << hour << "h";
    }
    return oss.str();
}


#include <string>
#include <sstream>
#include <iomanip>

inline std::string format_time_s(double sec) {
    constexpr double sec_per_min = 60.0;
    constexpr double sec_per_hour = 3600.0;
    constexpr double sec_per_day = 86400.0;

    std::ostringstream oss;
    if (sec < 2 * sec_per_min) {
        // 小于2分钟，只显示秒（保留2位小数，无单位）
        oss << std::fixed << std::setprecision(2) << sec;
    }
    else if (sec < 2 * sec_per_hour) {
        // 小于2小时，显示分和秒
        int min = static_cast<int>(sec / sec_per_min);
        double s = sec - min * sec_per_min;
        oss << min << "m";
        if (s >= 0.01)
            oss << std::fixed << std::setprecision(2) << s;
    }
    else if (sec < 2 * sec_per_day) {
        // 小于2天，显示小时和分
        int hour = static_cast<int>(sec / sec_per_hour);
        int min = static_cast<int>((sec - hour * sec_per_hour) / sec_per_min);
        oss << hour << "h";
        if (min > 0)
            oss << min << "m";
    }
    else {
        // 2天及以上，显示天和小时
        int day = static_cast<int>(sec / sec_per_day);
        int hour = static_cast<int>((sec - day * sec_per_day) / sec_per_hour);
        oss << day << "d";
        if (hour > 0)
            oss << hour << "h";
    }
    return oss.str();
}


static std::string float2str(float v, int unit =0) {
    float f = v * pow(10, unit);
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
//	alpha = (alpha > 1.0) ? 1.0 : (alpha < 0.0) ? 0.0 : alpha;
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


#if _WIN32
#include <fstream>
// 在 Windows 下，imread 不能直接读取包含中文的宽字符路径，需用 _wfopen 读取文件为字节流，再用 imdecode 解码
static cv::Mat imread_unicode(const std::wstring& wpath, int flags = cv::IMREAD_UNCHANGED) {
    // 打开文件为二进制流
    FILE* fp = _wfopen(wpath.c_str(), L"rb");
    if (!fp) return cv::Mat();
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uchar> buf(filesize);
    fread(buf.data(), 1, filesize, fp);
    fclose(fp);
    // 解码为Mat
    return cv::imdecode(buf, flags);
}

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <cstdio>

// 保存图片到宽字符路径
static bool imwrite_unicode(const std::wstring& wpath, const cv::Mat& image, const std::vector<int>& params = {}) {
    // 推断文件扩展名
    std::string ext = ".png";
    size_t dot = wpath.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        std::wstring wext = wpath.substr(dot);
        char extbuf[16] = { 0 };
        wcstombs(extbuf, wext.c_str(), sizeof(extbuf) - 1);
        ext = extbuf;
    }
    // 编码图片为字节流
    std::vector<uchar> buf;
    if (!cv::imencode(ext, image, buf, params)) return false;
    // 用 _wfopen 写入
    FILE* fp = _wfopen(wpath.c_str(), L"wb");
    if (!fp) return false;
    fwrite(buf.data(), 1, buf.size(), fp);
    fclose(fp);
    return true;
}
#endif
#endif //REALSR_NCNN_ANDROID_CLI_UTILS_HPP