#ifdef ENABLE_VIDEO

#include "VideoIO.hpp"

#include <algorithm>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>

#ifdef __ANDROID__
#include <unistd.h>
#endif

namespace
{
    bool hasSpecialCharacters(const std::string& path)
    {
        for (char c : path)
        {
            if (c == '#' || c == '?' || c == '*' || c == '<' || c == '>' || 
                c == '|' || c == '"' || c == '\'' || (unsigned char)c > 127)
                return true;
        }
        return false;
    }

    std::string sanitizeFileName(const std::string& filename)
    {
        std::string result;
        for (char c : filename)
        {
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                c == '-' || c == '_' || c == '.')
            {
                result += c;
            }
            else if (c == '/')
            {
                result += '/';
            }
            else
            {
                result += '_';
            }
        }
        return result;
    }

    std::string getTempFilePath(const std::string& srcFile)
    {
#ifdef __ANDROID__
        std::string dirPath;
        size_t lastSlash = srcFile.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            dirPath = srcFile.substr(0, lastSlash);
        }
        else
        {
            const char* extStorage = getenv("EXTERNAL_STORAGE");
            if (extStorage && extStorage[0] != '\0')
                dirPath = extStorage;
            else
                dirPath = "/sdcard";
        }
        return dirPath + "/.anime4k_temp_video";
#else
        return "./anime4k_temp_video";
#endif
    }

    int copyFile(const std::string& src, const std::string& dst)
    {
        FILE* fsrc = fopen(src.c_str(), "rb");
        if (!fsrc) return -1;

        FILE* fdst = fopen(dst.c_str(), "wb");
        if (!fdst) {
            fclose(fsrc);
            return -2;
        }

        char buffer[8192];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), fsrc)) > 0)
        {
            fwrite(buffer, 1, bytesRead, fdst);
        }

        fclose(fsrc);
        fclose(fdst);
        return 0;
    }
}

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
    std::string actualPath = srcFile;
    bool useTempFile = false;

    if (hasSpecialCharacters(srcFile))
    {
        size_t lastSlash = srcFile.find_last_of("/\\");
        std::string originalName = (lastSlash != std::string::npos) ? srcFile.substr(lastSlash + 1) : srcFile;
        std::string sanitizedName = sanitizeFileName(originalName);
        std::string tempFilePath = getTempFilePath(srcFile) + "_" + sanitizedName;

        int copyResult = copyFile(srcFile, tempFilePath);
        if (copyResult == 0)
        {
            actualPath = tempFilePath;
            useTempFile = true;
        }
        else
        {
            std::cerr << "[Anime4K] Error: Failed to create temporary file (error code: " << copyResult << ")" << std::endl;
        }
    }

#ifdef NEW_OPENCV_API
    reader.open(actualPath, cv::CAP_FFMPEG,
        {
            cv::CAP_PROP_HW_ACCELERATION, hw ? cv::VIDEO_ACCELERATION_ANY : cv::VIDEO_ACCELERATION_NONE,
        });
#else
    reader.open(actualPath);
#endif

    bool opened = reader.isOpened();

    if (!opened)
    {
        int backends[] = {cv::CAP_FFMPEG, cv::CAP_ANY, cv::CAP_GSTREAMER, cv::CAP_ANDROID};
        for (int backend : backends)
        {
            cv::VideoCapture testReader;
#ifdef NEW_OPENCV_API
            testReader.open(actualPath, backend,
                {cv::CAP_PROP_HW_ACCELERATION, hw ? cv::VIDEO_ACCELERATION_ANY : cv::VIDEO_ACCELERATION_NONE});
#else
            testReader.open(actualPath, backend);
#endif
            if (testReader.isOpened())
            {
                reader.release();
                reader = std::move(testReader);
                opened = true;
                break;
            }
        }
    }

    if (!opened && useTempFile)
    {
        remove(actualPath.c_str());
    }

    return opened;
}

bool Anime4KCPP::Video::VideoIO::openWriter(const std::string& dstFile, const Codec codec, const cv::Size& size, const double forceFps, bool hw)
{
    double fps;

    if (forceFps < 1.0)
        fps = reader.get(cv::CAP_PROP_FPS);
    else
        fps = forceFps;

    if (fps <= 0) fps = 30.0;

    std::string actualDstPath = dstFile;

#ifdef __ANDROID__
    bool isAbsolutePath = (!dstFile.empty() && (dstFile[0] == '/' || dstFile[1] == ':'));
    
    if (!isAbsolutePath)
    {
        const char* extStorage = getenv("EXTERNAL_STORAGE");
        if (!extStorage || extStorage[0] == '\0')
            extStorage = "/sdcard";
        
        actualDstPath = std::string(extStorage) + "/Download/" + dstFile;
    }

    size_t dotPos = actualDstPath.rfind('.');
    bool hasVideoExtension = false;
    if (dotPos != std::string::npos)
    {
        std::string ext = actualDstPath.substr(dotPos);
        for (char& c : ext)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        hasVideoExtension = (ext == ".mp4" || ext == ".mkv" || ext == ".mov" || 
                             ext == ".webm" || ext == ".flv" || ext == ".gif");
    }
    
    if (hasVideoExtension)
    {
        std::cerr << "[Anime4K] Note: Converting output to AVI format (Android OpenCV limitation)" << std::endl;
        actualDstPath = actualDstPath.substr(0, dotPos) + ".avi";
    }
#endif

#if defined(NEW_OPENCV_API)
    auto videoAcceleration = hw ? cv::VIDEO_ACCELERATION_ANY : cv::VIDEO_ACCELERATION_NONE;
#endif

    int fourccCode;
    switch (codec)
    {
    case Codec::MP4V:
        fourccCode = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        break;
#if defined(_WIN32) && !defined(OLD_OPENCV_API)
    case Codec::DXVA:
        fourccCode = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
        break;
#endif
    case Codec::AVC1:
        fourccCode = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
        break;
    case Codec::VP09:
        fourccCode = cv::VideoWriter::fourcc('v', 'p', '0', '9');
        break;
    case Codec::HEVC:
        fourccCode = cv::VideoWriter::fourcc('h', 'e', 'v', '1');
        break;
    case Codec::AV01:
        fourccCode = cv::VideoWriter::fourcc('a', 'v', '0', '1');
        break;
    case Codec::OTHER:
        fourccCode = -1;
        break;
    default:
        fourccCode = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        break;
    }

#ifdef __ANDROID__
    fourccCode = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');

    writer.open(actualDstPath, fourccCode, fps, size);
    
    if (writer.isOpened())
    {
        return true;
    }
    else
    {
        return false;
    }
#else
    writer.open(actualDstPath, fourccCode, fps, size);

    if (writer.isOpened())
    {
        return true;
    }

    int fallbackFourCCs[] = {
        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
        cv::VideoWriter::fourcc('X', 'V', 'I', 'D'),
    };

    for (int fallbackFourCC : fallbackFourCCs)
    {
        writer.release();
        writer.open(actualDstPath, fallbackFourCC, fps, size);
        if (writer.isOpened())
            return true;
    }

#endif

    return false;
}

double Anime4KCPP::Video::VideoIO::get(int p)
{
    return reader.get(p);
}

double Anime4KCPP::Video::VideoIO::getProgress() noexcept
{
    return progress;
}

void Anime4KCPP::Video::VideoIO::setTotalFrameCount(std::size_t count) noexcept
{
    totalFrameCountOverride = count;
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
