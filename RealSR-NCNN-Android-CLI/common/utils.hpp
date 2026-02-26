#ifndef REALSR_NCNN_ANDROID_CLI_UTILS_HPP
#define REALSR_NCNN_ANDROID_CLI_UTILS_HPP

#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstdio>
#include <fstream>
#include <cmath>

#include <opencv2/opencv.hpp>

#if _WIN32
typedef std::wstring path_t;
#define PATHSTR(s) L##s
#else
typedef std::string path_t;
#define PATHSTR(s) s
#endif

#if _WIN32

// 在 Windows 下，imread 不能直接读取包含中文的宽字符路径，需用 _wfopen 读取文件为字节流，再用 imdecode 解码
static cv::Mat imread_unicode(const std::wstring& wpath, int flags = cv::IMREAD_UNCHANGED) {
    // 打开文件为二进制流
    FILE* fp = _wfopen(wpath.c_str(), L"rb");
    if (!fp) return cv::Mat();
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<unsigned char> buf(filesize);
    fread(buf.data(), 1, filesize, fp);
    fclose(fp);
    // 解码为Mat
    return cv::imdecode(buf, flags);
}


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
    std::vector<unsigned char> buf;
    if (!cv::imencode(ext, image, buf, params)) return false;
    // 用 _wfopen 写入
    FILE* fp = _wfopen(wpath.c_str(), L"wb");
    if (!fp) return false;
    fwrite(buf.data(), 1, buf.size(), fp);
    fclose(fp);
    return true;
}

#endif

static void imread(const path_t &imagepath, cv::Mat &inBGR, cv::Mat& inAlpha) {
        // 读取图像
        cv::Mat image;
        inAlpha = cv::Mat();
        #if _WIN32
            image = imread_unicode(imagepath, cv::IMREAD_UNCHANGED);
        #else
           image = cv::imread(imagepath, cv::IMREAD_UNCHANGED);
        #endif
        if (image.empty()) {
#if _WIN32
            fwprintf(stderr, L"decode image %ls failed\n", imagepath.c_str());
#else // _WIN32

            fprintf(stderr, "decode image %s failed\n", imagepath.c_str());
#endif // _WIN32
            inBGR = cv::Mat();
            return;
        }
        
        cv::Mat inimage;
        int c = image.channels();

        if (c == 1) {
            // 如果图像有1个通道，转换为3个通道
            cv::cvtColor(image, inBGR, cv::COLOR_GRAY2BGR);
            
            // return ;
        } else if (image.channels() == 4) {
            // 如果图像有4个通道，分离通道
            std::vector<cv::Mat> channels;
            cv::split(image, channels);
            cv::Mat alphaChannel = channels[3];

            // 方法2：检测是否全不透明（alpha == 255）
            cv::Mat opaque_mask = alphaChannel == 255;
            if (cv::countNonZero(opaque_mask) == alphaChannel.total()) {
                #if _WIN32  
                           fwprintf(stderr, L"ignore alpha channel, %ls\n", imagepath.c_str());  
                #else  
                           fprintf(stderr, "ignore alpha channel, %s\n", imagepath.c_str());  
                #endif
            } else {
                inAlpha = alphaChannel;
            }
            cv::merge(channels.data(), 3, inBGR);
            // return;
        } else if (c == 3) {
            inBGR = image;
        } else {
#if _WIN32  
            fwprintf(stderr, L"[err] channel=%d, %ls\n", image.channels(), imagepath.c_str());
#else  
            fprintf(stderr, "[err] channel=%d, %s\n", image.channels(), imagepath.c_str());
#endif

            inBGR = cv::Mat();
        }
}



inline std::string format_time_ms(double ms)
{
    constexpr double ms_per_sec = 1000.0;
    constexpr double ms_per_min = 60 * ms_per_sec;
    constexpr double ms_per_hour = 60 * ms_per_min;
    constexpr double ms_per_day = 24 * ms_per_hour;

    std::ostringstream oss;
    if (ms < 2 * ms_per_min)
    {
        oss << std::fixed << std::setprecision(2) << (ms / ms_per_sec);
    }
    else if (ms < 2 * ms_per_hour)
    {
        int min = static_cast<int>(ms / ms_per_min);
        double sec = (ms - min * ms_per_min) / ms_per_sec;
        oss << min << "m";
        if (sec >= 0.01)
            oss << std::fixed << std::setprecision(2) << sec;
    }
    else if (ms < 2 * ms_per_day)
    {
        int hour = static_cast<int>(ms / ms_per_hour);
        int min = static_cast<int>((ms - hour * ms_per_hour) / ms_per_min);
        oss << hour << "h";
        if (min > 0)
            oss << min << "m";
    }
    else
    {
        int days_count = static_cast<int>(ms / ms_per_day);
        int hour = static_cast<int>((ms - days_count * ms_per_day) / ms_per_hour);
        oss << days_count << "d";
        if (hour > 0)
            oss << hour << "h";
    }
    return oss.str();
}

inline std::string format_time_s(double sec)
{
    constexpr double sec_per_min = 60.0;
    constexpr double sec_per_hour = 3600.0;
    constexpr double sec_per_day = 86400.0;

    std::ostringstream oss;
    if (sec < 2 * sec_per_min)
    {
        oss << std::fixed << std::setprecision(2) << sec;
    }
    else if (sec < 2 * sec_per_hour)
    {
        int min = static_cast<int>(sec / sec_per_min);
        double s = sec - min * sec_per_min;
        oss << min << "m";
        if (s >= 0.01)
            oss << std::fixed << std::setprecision(2) << s;
    }
    else if (sec < 2 * sec_per_day)
    {
        int hour = static_cast<int>(sec / sec_per_hour);
        int min = static_cast<int>((sec - hour * sec_per_hour) / sec_per_min);
        oss << hour << "h";
        if (min > 0)
            oss << min << "m";
    }
    else
    {
        int days_count = static_cast<int>(sec / sec_per_day);
        int hour = static_cast<int>((sec - days_count * sec_per_day) / sec_per_hour);
        oss << days_count << "d";
        if (hour > 0)
            oss << hour << "h";
    }
    return oss.str();
}

static std::string float2str(float v, int unit = 0)
{
    float f = v * pow(10, unit);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if (f >= 10000000000000000.0f)
    {
        oss << (f / 10000000000000000.0f) << " E";
    }
    else if (f >= 1000000000000000.0f)
    {
        oss << (f / 1000000000000000.0f) << " P";
    }
    else if (f >= 1000000000000.0f)
    {
        oss << (f / 1000000000000.0f) << " T";
    }
    else if (f >= 1000000000.0f)
    {
        oss << (f / 1000000000.0f) << " G";
    }
    else if (f >= 1000000.0f)
    {
        oss << (f / 1000000.0f) << " M";
    }
    else if (f >= 1000.0f)
    {
        oss << (f / 1000.0f) << " K";
    }
    else
    {
        oss << f;
    }

    return oss.str();
}

static cv::Mat resize_alpha_bicubic(const cv::Mat& alpha, int scale) {
    if (alpha.empty()) return cv::Mat();
    
    int new_width = alpha.cols * scale;
    int new_height = alpha.rows * scale;
    
    cv::Mat scaled_alpha;
    cv::resize(alpha, scaled_alpha, cv::Size(new_width, new_height), 0, 0, cv::INTER_CUBIC);
    
    return scaled_alpha;
}

static void merge_rgb_alpha(const cv::Mat& rgb, const cv::Mat& alpha, cv::Mat& out) {
    if (alpha.empty()) {
        out = rgb.clone();
        return;
    }
    
    cv::Mat alpha_scaled;
    if (alpha.cols != rgb.cols || alpha.rows != rgb.rows) {
        cv::resize(alpha, alpha_scaled, cv::Size(rgb.cols, rgb.rows), 0, 0, cv::INTER_CUBIC);
    } else {
        alpha_scaled = alpha;
    }
    
    std::vector<cv::Mat> channels;
    cv::split(rgb, channels);
    channels.push_back(alpha_scaled);
    cv::merge(channels, out);
}

#endif // REALSR_NCNN_ANDROID_CLI_UTILS_HPP