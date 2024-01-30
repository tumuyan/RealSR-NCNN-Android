#ifndef ANIME4KCPP_CORE_VIDEO_CODEC_HPP
#define ANIME4KCPP_CORE_VIDEO_CODEC_HPP

#ifdef ENABLE_VIDEO

namespace Anime4KCPP
{
    enum class Codec
    {
        OTHER = -1, MP4V = 0, DXVA = 1, AVC1 = 2, VP09 = 3, HEVC = 4, AV01 = 5
    };
}

#endif // ENABLE_VIDEO

#endif // !ANIME4KCPP_CORE_VIDEO_CODEC_HPP
