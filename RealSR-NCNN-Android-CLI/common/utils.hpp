//
// Created by Yazii on 2025/3/30.
//
#ifndef REALSR_NCNN_ANDROID_CLI_UTILS_HPP
#define REALSR_NCNN_ANDROID_CLI_UTILS_HPP

#include <string>

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