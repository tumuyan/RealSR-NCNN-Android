#if defined(ENABLE_VIDEO) && !defined(DISABLE_PARALLEL)

#include <forward_list>
#include <thread>

#include "VideoIOAsync.hpp"

void Anime4KCPP::Video::VideoIOAsync::process()
{
    std::forward_list<std::future<void>> futures;

    if ((limit = std::thread::hardware_concurrency()) < 4)
        limit = 4;

    finished = 0;

    futures.emplace_front(std::async(std::launch::async, [&]()
        {
            double totalFrame = reader.get(cv::CAP_PROP_FRAME_COUNT);

            for (std::size_t frameCount = 0; finished == 0 || frameCount < finished; frameCount++)
            {
                std::unique_lock<std::mutex> lock(mtxWrite);
                std::unordered_map<std::size_t, cv::Mat>::iterator it;

                while (!stop && ((it = frameMap.find(frameCount)) == frameMap.end()))
                    cndWrite.wait(lock);

                if (stop)
                    return;

                writer.write(it->second);
                frameMap.erase(it);
                setProgress(static_cast<double>(frameCount) / totalFrame);
            }
        }));

    for (std::size_t frameCount = 0;; frameCount++)
    {
        cv::Mat frame;
        if (!reader.read(frame))
        {
            finished = frameCount;
            break;
        }
        {
            std::unique_lock<std::mutex> lock(mtxRead);

            while (!stop && rawFrames.size() >= limit)
                cndRead.wait(lock);

            if (stop)
                break;

            rawFrames.emplace(frame, frameCount);
        }
        futures.emplace_front(std::async(std::launch::async, processor));
    }

    std::for_each(futures.begin(), futures.end(), std::mem_fn(&std::future<void>::wait));
}

#endif
