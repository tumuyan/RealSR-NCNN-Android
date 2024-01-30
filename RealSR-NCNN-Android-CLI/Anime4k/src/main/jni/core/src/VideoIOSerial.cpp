#ifdef ENABLE_VIDEO

#include "VideoIOSerial.hpp"

void Anime4KCPP::Video::VideoIOSerial::process()
{
    double totalFrame = reader.get(cv::CAP_PROP_FRAME_COUNT);

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
        setProgress(static_cast<double>(frameCount) / totalFrame);
    }
}

#endif
