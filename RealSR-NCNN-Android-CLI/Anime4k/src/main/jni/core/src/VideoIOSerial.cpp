#ifdef ENABLE_VIDEO

#include "VideoIOSerial.hpp"

void Anime4KCPP::Video::VideoIOSerial::process()
{
    double totalFrame = 0;
    if (totalFrameCountOverride > 0)
        totalFrame = static_cast<double>(totalFrameCountOverride);
    else
        totalFrame = reader.get(cv::CAP_PROP_FRAME_COUNT);
    bool hasTotalFrames = (totalFrame > 0);

    stop = false;

    for (std::size_t frameCount = 0;; frameCount++)
    {
        {
            const std::lock_guard<std::mutex> lock(mtxRead);
            if (stop)
                break;
        }

        cv::Mat frame;
        if (!reader.read(frame))
            break;

        rawFrames.emplace(frame, frameCount);
        processor();
        auto it = frameMap.find(frameCount);
        writer.write(it->second);
        frameMap.erase(it);
        
        if (hasTotalFrames && totalFrame > 0)
            setProgress(static_cast<double>(frameCount) / totalFrame);
        else
            setProgress(0.0);
    }
}

#endif
