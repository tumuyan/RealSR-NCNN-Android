#ifndef ANIME4KCPP_CORE_VIDEO_PROCESSOR_HPP
#define ANIME4KCPP_CORE_VIDEO_PROCESSOR_HPP

#ifdef ENABLE_VIDEO

#include "AC.hpp"
#include "VideoIO.hpp"

namespace Anime4KCPP
{
    class AC_EXPORT VideoProcessor;
}

class Anime4KCPP::VideoProcessor
{
public:
    VideoProcessor(const Parameters& parameters, Processor::Type type, unsigned int threads);
    explicit VideoProcessor(const AC& config, unsigned int threads = 0);
    void setVideoSaveInfo(const std::string& dstFile, Codec codec = Codec::MP4V, double fps = 0.0, bool hw = false);
    void loadVideo(const std::string& srcFile, bool hw = false);
    void saveVideo();

    void process();
    void processWithProgress(const std::function<void(double)>&& callBack);
    void stopVideoProcess() noexcept;
    void pauseVideoProcess();
    void continueVideoProcess();

    std::string getInfo() const;
private:
    double fps;
    double totalFrameCount;
    int height, width;
    unsigned int threads;

    Parameters param;
    Processor::Type type;
    std::unique_ptr<Video::VideoIO> videoIO;
};

#endif // ENABLE_VIDEO

#endif // !ANIME4KCPP_CORE_VIDEO_PROCESSOR_HPP
