#ifdef ENABLE_VIDEO

#include "VideoIO.hpp"

Anime4KCPP::Video::VideoIO::~VideoIO()
{
    stopProcess();
    release();
}

Anime4KCPP::Video::VideoIO& Anime4KCPP::Video::VideoIO::init(std::function<void()>&& p, std::size_t t) noexcept
{
    processor = std::move(p);
    limit = threads = t;
    stop = false;
    return *this;
}

bool Anime4KCPP::Video::VideoIO::openReader(const std::string& srcFile, bool hw)
{
#ifdef NEW_OPENCV_API
    reader.open(srcFile, cv::CAP_FFMPEG,
        {
            cv::CAP_PROP_HW_ACCELERATION, hw ? cv::VIDEO_ACCELERATION_ANY : cv::VIDEO_ACCELERATION_NONE,
        });
#else
    reader.open(srcFile);
#endif // defined(WIN32) && defined(NEW_OPENCV_API)

    return reader.isOpened();
}

bool Anime4KCPP::Video::VideoIO::openWriter(const std::string& dstFile, const Codec codec, const cv::Size& size, const double forceFps, bool hw)
{
    double fps;

    if (forceFps < 1.0)
        fps = reader.get(cv::CAP_PROP_FPS);
    else
        fps = forceFps;

#if defined(NEW_OPENCV_API)
    auto videoAcceleration = hw ? cv::VIDEO_ACCELERATION_ANY : cv::VIDEO_ACCELERATION_NONE;
#endif

    switch (codec)
    {
    case Codec::MP4V:
#ifndef OLD_OPENCV_API
#ifdef NEW_OPENCV_API
        writer.open(dstFile, cv::CAP_FFMPEG, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, size,
            {
                cv::VIDEOWRITER_PROP_HW_ACCELERATION, videoAcceleration,
            });
#else
        writer.open(dstFile, cv::CAP_FFMPEG, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, size);
#endif  // NEW_OPENCV_API
        if (!writer.isOpened())
#endif // !OLD_OPENCV_API
        {
            writer.open(dstFile, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, size);
            if (!writer.isOpened())
                return false;
        }
        break;

#if defined(_WIN32) && !defined(OLD_OPENCV_API) //DXVA encoding for windows
    case Codec::DXVA:
#ifdef NEW_OPENCV_API
        writer.open(dstFile, cv::CAP_MSMF, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), std::ceil(fps), size,
            {
                cv::VIDEOWRITER_PROP_HW_ACCELERATION, videoAcceleration,
            });
#else
        writer.open(dstFile, cv::CAP_MSMF, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), std::ceil(fps), size);
#endif
        if (!writer.isOpened())
        {
            writer.open(dstFile, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, size);
            if (!writer.isOpened())
                return false;
        }
        break;
#endif

    case Codec::AVC1:
#ifndef OLD_OPENCV_API
#ifdef NEW_OPENCV_API
        writer.open(dstFile, cv::CAP_FFMPEG, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), fps, size,
            {
                cv::VIDEOWRITER_PROP_HW_ACCELERATION, videoAcceleration,
            });
#else
        writer.open(dstFile, cv::CAP_FFMPEG, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), fps, size);
#endif  // NEW_OPENCV_API
        if (!writer.isOpened())
#endif // !OLD_OPENCV_API
        {
            writer.open(dstFile, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), fps, size);
            if (!writer.isOpened())
                return false;
        }
        break;

    case Codec::VP09:
#ifndef OLD_OPENCV_API
#ifdef NEW_OPENCV_API
        writer.open(dstFile, cv::CAP_FFMPEG, cv::VideoWriter::fourcc('v', 'p', '0', '9'), fps, size,
            {
                cv::VIDEOWRITER_PROP_HW_ACCELERATION, videoAcceleration,
            });
#else
        writer.open(dstFile, cv::CAP_FFMPEG, cv::VideoWriter::fourcc('v', 'p', '0', '9'), fps, size);
#endif  // NEW_OPENCV_API
        if (!writer.isOpened())
#endif // !OLD_OPENCV_API
        {
            writer.open(dstFile, cv::VideoWriter::fourcc('v', 'p', '0', '9'), fps, size);
            if (!writer.isOpened())
                return false;
        }
        break;

    case Codec::HEVC:
#ifndef OLD_OPENCV_API
#ifdef NEW_OPENCV_API
        writer.open(dstFile, cv::CAP_FFMPEG, cv::VideoWriter::fourcc('h', 'e', 'v', '1'), fps, size,
            {
                cv::VIDEOWRITER_PROP_HW_ACCELERATION, videoAcceleration,
            });
#else
        writer.open(dstFile, cv::CAP_FFMPEG, cv::VideoWriter::fourcc('h', 'e', 'v', '1'), fps, size);
#endif  // NEW_OPENCV_API
        if (!writer.isOpened())
#endif // !OLD_OPENCV_API
        {
            writer.open(dstFile, cv::VideoWriter::fourcc('h', 'e', 'v', '1'), fps, size);
            if (!writer.isOpened())
                return false;
        }
        break;

    case Codec::AV01:
#ifndef OLD_OPENCV_API
#ifdef NEW_OPENCV_API
        writer.open(dstFile, cv::CAP_FFMPEG, cv::VideoWriter::fourcc('a', 'v', '0', '1'), fps, size,
            {
                cv::VIDEOWRITER_PROP_HW_ACCELERATION, videoAcceleration,
            });
#else
        writer.open(dstFile, cv::CAP_FFMPEG, cv::VideoWriter::fourcc('a', 'v', '0', '1'), fps, size);
#endif  // NEW_OPENCV_API
        if (!writer.isOpened())
#endif // !OLD_OPENCV_API
        {
            writer.open(dstFile, cv::VideoWriter::fourcc('a', 'v', '0', '1'), fps, size);
            if (!writer.isOpened())
                return false;
        }
        break;

    case Codec::OTHER:
        writer.open(dstFile, -1, fps, size);
        if (!writer.isOpened())
            return false;
        break;
    default:
        writer.open(dstFile, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, size);
        if (!writer.isOpened())
            return false;
    }
    return true;
}

double Anime4KCPP::Video::VideoIO::get(int p)
{
    return reader.get(p);
}

double Anime4KCPP::Video::VideoIO::getProgress() noexcept
{
    return progress;
}

void Anime4KCPP::Video::VideoIO::read(Frame& frame)
{
    {
        const std::lock_guard<std::mutex> lock(mtxRead);
        frame = std::move(rawFrames.front());
        rawFrames.pop();
    }
    cndRead.notify_one();
}

void Anime4KCPP::Video::VideoIO::write(const Frame& frame)
{
    {
        const std::lock_guard<std::mutex> lock(mtxWrite);
        frameMap.emplace(frame.second, frame.first);
    }
    cndWrite.notify_one();
}

void Anime4KCPP::Video::VideoIO::release()
{
    reader.release();
    writer.release();

    if (!rawFrames.empty())
        std::queue<Frame>().swap(rawFrames);
    if (!frameMap.empty())
        frameMap.clear();
}

bool Anime4KCPP::Video::VideoIO::isPaused() noexcept
{
    return pause;
}

void Anime4KCPP::Video::VideoIO::stopProcess() noexcept
{
    {
        std::scoped_lock lock(mtxRead, mtxWrite);
        stop = true;
    }
    cndRead.notify_one();
    cndWrite.notify_one();
}

void Anime4KCPP::Video::VideoIO::pauseProcess()
{
    if (!pause)
    {
        pausePromise = std::make_unique<std::promise<void>>();
        std::thread t([this, f = pausePromise->get_future()]()
        {
            const std::lock_guard<std::mutex> lock(mtxRead);
            f.wait();
        });
        t.detach();
        pause = true;
    }
}

void Anime4KCPP::Video::VideoIO::continueProcess()
{
    if (pause)
    {
        pausePromise->set_value();
        pause = false;
    }
}

void Anime4KCPP::Video::VideoIO::setProgress(double p) noexcept
{
    progress = p;
}

#endif
