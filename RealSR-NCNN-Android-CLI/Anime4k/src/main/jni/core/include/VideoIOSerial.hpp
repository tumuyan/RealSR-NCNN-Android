#ifndef ANIME4KCPP_CORE_VIDEO_IO_SERIAL_HPP
#define ANIME4KCPP_CORE_VIDEO_IO_SERIAL_HPP

#ifdef ENABLE_VIDEO

#include "VideoIO.hpp"

namespace Anime4KCPP::Video
{
    class VideoIOSerial;
}

class Anime4KCPP::Video::VideoIOSerial :public VideoIO
{
public:
    void process() override;
};

#endif // ENABLE_VIDEO

#endif // !ANIME4KCPP_CORE_VIDEO_IO_SERIAL_HPP
