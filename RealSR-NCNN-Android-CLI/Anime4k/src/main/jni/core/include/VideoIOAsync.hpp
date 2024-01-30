#ifndef ANIME4KCPP_CORE_VIDEO_IO_ASYNC_HPP
#define ANIME4KCPP_CORE_VIDEO_IO_ASYNC_HPP

#if defined(ENABLE_VIDEO) && !defined(DISABLE_PARALLEL)

#include "VideoIO.hpp"

namespace Anime4KCPP::Video
{
    class VideoIOAsync;
}

class Anime4KCPP::Video::VideoIOAsync :public VideoIO
{
public:
    void process() override;
private:
    std::atomic<std::size_t> finished;
};

#endif // ENABLE_VIDEO

#endif // !ANIME4KCPP_CORE_VIDEO_IO_ASYNC_HPP
