#ifndef ANIME4KCPP_CORE_VIDEO_IO_THREADS_HPP
#define ANIME4KCPP_CORE_VIDEO_IO_THREADS_HPP

#ifdef ENABLE_VIDEO

#include "VideoIO.hpp"

namespace Anime4KCPP::Video
{
    class VideoIOThreads;
}

class Anime4KCPP::Video::VideoIOThreads :public VideoIO
{
public:
    void process() override;
private:
    std::atomic<std::size_t> finished;
};

#endif // ENABLE_VIDEO

#endif // !ANIME4KCPP_CORE_VIDEO_IO_THREADS_HPP
