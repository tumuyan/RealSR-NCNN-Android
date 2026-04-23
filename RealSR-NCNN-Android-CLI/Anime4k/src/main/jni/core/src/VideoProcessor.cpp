#ifdef ENABLE_VIDEO

#include <exception>
#include <vector>

#include "ACCreator.hpp"
#include "VideoProcessor.hpp"
#include "VideoIOAsync.hpp"
#include "VideoIOThreads.hpp"
#include "VideoIOSerial.hpp"

Anime4KCPP::VideoProcessor::VideoProcessor(const Parameters& parameters, const Processor::Type type, const unsigned int threads)
    :fps(0.0), totalFrameCount(0.0), height(0), width(0), threads(threads), param(parameters), type(type)
{
#ifdef DISABLE_PARALLEL
    videoIO = std::make_unique<Video::VideoIOSerial>();
#else
    if (threads > 1)
        videoIO = std::make_unique<Video::VideoIOThreads>();
    else if (threads == 1)
        videoIO = std::make_unique<Video::VideoIOSerial>();
    else
        videoIO = std::make_unique<Video::VideoIOAsync>();
#endif
}

Anime4KCPP::VideoProcessor::VideoProcessor(const AC& config, const unsigned int threads)
    :VideoProcessor(config.getParameters(), config.getProcessorType(), threads) {}

void Anime4KCPP::VideoProcessor::loadVideo(const std::string& srcFile, const bool hw)
{
    if (!videoIO->openReader(srcFile, hw))
    {
        std::string errorMsg = "Failed to load video: " + srcFile + "\n";
        errorMsg += "Possible reasons:\n";
        errorMsg += "  1. File does not exist\n";
        errorMsg += "  2. Invalid file path (use absolute path like /sdcard/...)\n";
        errorMsg += "  3. No storage permission granted\n";
        errorMsg += "  4. Unsupported video format or corrupted file\n";
#ifdef __ANDROID__
        errorMsg += "\nAndroid tips:\n";
        errorMsg += "  - Use path: /sdcard/DCIM/video.mp4 or /sdcard/Download/video.mp4\n";
        errorMsg += "  - Grant storage permission in app settings\n";
#endif
        throw ACException<ExceptionType::IO>(errorMsg);
    }

    fps = videoIO->get(cv::CAP_PROP_FPS);
    totalFrameCount = videoIO->get(cv::CAP_PROP_FRAME_COUNT);

#ifdef __ANDROID__
    if (totalFrameCount <= 0)
    {
        std::cerr << "[Anime4K] Scanning video for frame count..." << std::endl;
        
        cv::VideoCapture tempReader;
        long framePos = 0;
        
        for (int backend : {cv::CAP_ANY, cv::CAP_FFMPEG})
        {
#ifdef NEW_OPENCV_API
            tempReader.open(srcFile, backend,
                {cv::CAP_PROP_HW_ACCELERATION, cv::VIDEO_ACCELERATION_NONE});
#else
            tempReader.open(srcFile, backend);
#endif
            if (!tempReader.isOpened())
                continue;

            double testCount = tempReader.get(cv::CAP_PROP_FRAME_COUNT);
            if (testCount > 0)
            {
                totalFrameCount = static_cast<std::size_t>(std::round(testCount));
                break;
            }

            cv::Mat frame;
            while (tempReader.read(frame))
            {
                framePos++;
                if (framePos % 200 == 0)
                    std::cerr << "\r[Anime4K] Scanned " << framePos << " frames..." << std::flush;
            }
            
            if (framePos > 0)
            {
                totalFrameCount = static_cast<std::size_t>(framePos);
                break;
            }
            
            tempReader.release();
        }

        if (totalFrameCount > 0)
        {
            std::cerr << "\r[Anime4K] Found " << totalFrameCount << " frames" << std::endl;
            videoIO->setTotalFrameCount(totalFrameCount);
        }
        else
        {
            std::cerr << "\r[Anime4K] Warning: Could not determine frame count" << std::endl;
        }
    }
#endif

    height = static_cast<int>(std::round(param.zoomFactor * videoIO->get(cv::CAP_PROP_FRAME_HEIGHT)));
    width = static_cast<int>(std::round(param.zoomFactor * videoIO->get(cv::CAP_PROP_FRAME_WIDTH)));
}

void Anime4KCPP::VideoProcessor::setVideoSaveInfo(const std::string& dstFile, const Codec codec, const double fps, const bool hw)
{
    if (!videoIO->openWriter(dstFile, codec, cv::Size(width, height), fps, hw))
        throw ACException<ExceptionType::IO>("Failed to initialize video writer.");
}

void Anime4KCPP::VideoProcessor::saveVideo()
{
    videoIO->release();
}

void Anime4KCPP::VideoProcessor::process()
{
    std::once_flag eptrFlag;
    std::exception_ptr eptr;

    videoIO->init(
        [&]()
        {
            Video::Frame frame;
            videoIO->read(frame);

            try
            { // Reduce memory usage
                auto ac = ACCreator::createUP(param, type);
                ac->loadImage(frame.first);
                ac->process();
                ac->saveImage(frame.first);
            }
            catch (...)
            {
                std::call_once(eptrFlag,
                    [&]()
                    {
                        videoIO->stopProcess();
                        eptr = std::current_exception();
                    });
                return;
            }

            videoIO->write(frame);
        }
    , threads).process();

    if (eptr)
        std::rethrow_exception(eptr);
}

void Anime4KCPP::VideoProcessor::processWithProgress(const std::function<void(double)>&& callBack)
{
    std::future<void> p = std::async(std::launch::async, &VideoProcessor::process, this);
    std::chrono::milliseconds timeout(1000);
    for (;;)
    {
        std::future_status status = p.wait_for(timeout);
        if (status == std::future_status::ready)
        {
            callBack(1.0);
            p.get();
            break;
        }
        double progress = videoIO->getProgress();
        callBack(progress);
    }
}

void Anime4KCPP::VideoProcessor::stopVideoProcess() noexcept
{
    if (videoIO->isPaused())
        videoIO->continueProcess();

    videoIO->stopProcess();
}

void Anime4KCPP::VideoProcessor::pauseVideoProcess()
{
    videoIO->pauseProcess();
}

void Anime4KCPP::VideoProcessor::continueVideoProcess()
{
    videoIO->continueProcess();
}

std::string Anime4KCPP::VideoProcessor::getInfo() const
{
    std::ostringstream oss;
    oss << "------------------------" << '\n'
        << "Video information" << '\n'
        << "------------------------" << '\n'
        << "FPS: " << fps << '\n'
        << "Threads: " << threads << '\n';
    
    if (totalFrameCount > 0)
        oss << "Total frames: " << totalFrameCount << '\n';
    else
        oss << "Total frames: Unknown (will count during processing)" << '\n';
    
    oss << "------------------------" << '\n';
    return oss.str();
}

#endif // ENABLE_VIDEO
