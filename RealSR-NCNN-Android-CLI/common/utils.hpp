#ifndef REALSR_NCNN_ANDROID_CLI_UTILS_HPP
#define REALSR_NCNN_ANDROID_CLI_UTILS_HPP

#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstdio>
#include <fstream>
#include <cmath>

#if _WIN32
#include <opencv2/opencv.hpp>

#include <fstream>
// �� Windows �£�imread ����ֱ�Ӷ�ȡ�������ĵĿ��ַ�·�������� _wfopen ��ȡ�ļ�Ϊ�ֽ��������� imdecode ����
static cv::Mat imread_unicode(const std::wstring& wpath, int flags = cv::IMREAD_UNCHANGED) {
    // ���ļ�Ϊ��������
    FILE* fp = _wfopen(wpath.c_str(), L"rb");
    if (!fp) return cv::Mat();
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uchar> buf(filesize);
    fread(buf.data(), 1, filesize, fp);
    fclose(fp);
    // ����ΪMat
    return cv::imdecode(buf, flags);
}



// save image to wide char path
static bool imwrite_unicode(const std::wstring& wpath, const cv::Mat& image, const std::vector<int>& params = {})
{
    // get file extension
    std::string file_extension = ".png";
    size_t dot = wpath.find_last_of(L'.');
    if (dot != std::wstring::npos)
    {
        std::wstring wext = wpath.substr(dot);
        char extbuf[16] = { 0 };
        wcstombs(extbuf, wext.c_str(), sizeof(extbuf) - 1);
        file_extension = extbuf;
    }
    // encode image to buffer
    std::vector<uchar> buf;
    if (!cv::imencode(file_extension, image, buf, params)) return false;
    // write with _wfopen
    FILE* fp = _wfopen(wpath.c_str(), L"wb");
    if (!fp) return false;
    fwrite(buf.data(), 1, buf.size(), fp);
    fclose(fp);
    return true;
}

#endif

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

#endif // REALSR_NCNN_ANDROID_CLI_UTILS_HPP