#ifndef ANIME4KCPP_CORE_VIDEO_IO_HPP
#define ANIME4KCPP_CORE_VIDEO_IO_HPP

#ifdef ENABLE_VIDEO

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <memory>
#include <queue>
#include <unordered_map>
#include <utility>
#include <cstddef>

#include <opencv2/opencv.hpp>

#include "VideoCodec.hpp"

#if (CV_VERSION_MAJOR < 3)
#error OpenCV3 is needed at least!
#endif
#if (CV_VERSION_MAJOR == 3) && (CV_VERSION_MINOR <= 2)
#define OLD_OPENCV_API
#elif (CV_VERSION_MAJOR > 4) || \
(CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR > 5) || \
(CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR == 5 && CV_VERSION_REVISION >= 2)
#define NEW_OPENCV_API
#endif

namespace Anime4KCPP::Video
{
    class VideoIO;

    using Frame = std::pair<cv::Mat, std::size_t>;
}

class Anime4KCPP::Video::VideoIO
{
public:
    VideoIO() = default;
    VideoIO(const VideoIO&) = delete;
    VideoIO& operator=(const VideoIO&) = delete;
    virtual ~VideoIO();
    //initialize frame process callback function `p` and thread count `t`, it's ready to call process after this
    VideoIO& init(std::function<void()>&& p, std::size_t t) noexcept;
    //initialize VideoCapture
    bool openReader(const std::string& srcFile, bool hw);
    //initialize VideoWriter
    bool openWriter(const std::string& dstFile, Codec codec, const cv::Size& size, double forceFps, bool hw);
    //get the specifying video property from VideoCapture
    double get(int p);
    double getProgress() noexcept;
    void read(Frame& frame);
    void write(const Frame& frame);
    void release();
    bool isPaused() noexcept;
    void stopProcess() noexcept;
    void pauseProcess();
    void continueProcess();

    virtual void process() = 0;
protected:
    void setProgress(double p) noexcept;
protected:
    std::size_t threads = 0;
    std::size_t limit = 0;
    std::function<void()> processor;
    cv::VideoCapture reader;
    cv::VideoWriter writer;

    std::queue<Frame> rawFrames;
    std::unordered_map<std::size_t, cv::Mat> frameMap;

    std::mutex mtxRead;
    std::condition_variable cndRead;
    std::mutex mtxWrite;
    std::condition_variable cndWrite;

    std::atomic<double> progress;
    std::unique_ptr<std::promise<void>> pausePromise;
    bool pause = false;
    bool stop = false;
};

#endif // ENABLE_VIDEO

#endif // !ANIME4KCPP_CORE_VIDEO_IO_HPP
