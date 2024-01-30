#ifndef ANIME4KCPP_CORE_NCNN_ACNET_HPP
#define ANIME4KCPP_CORE_NCNN_ACNET_HPP

#ifdef ENABLE_NCNN

#include "AC.hpp"
#include "CNN.hpp"

namespace Anime4KCPP::NCNN
{
    class AC_EXPORT ACNet;
}

class Anime4KCPP::NCNN::ACNet :public AC
{
public:
    explicit ACNet(const Parameters& parameters);
    ~ACNet() override;
    void setParameters(const Parameters& parameters) override;

    std::string getInfo() const override;
    std::string getFiltersInfo() const override;

    static void init(
        std::string& modelPath, std::string& paramPath, 
        int type, int deviceID, int threads);

    static void init(int type, int deviceID, int threads);

    static void init(int deviceID, int threads);

    static void release() noexcept;
    static bool isInitialized() noexcept;
private:
    void processYUVImage() override;
    void processRGBImage() override;
    void processGrayscale() override;

    Processor::Type getProcessorType() const noexcept override;
    std::string getProcessorInfo() const override;
private:
    int ACNetTypeIndex;

    struct DataHolder;
    std::unique_ptr<DataHolder> dataHolder;
};

#endif // ENABLE_NCNN

#endif // !ANIME4KCPP_CORE_NCNN_ACNET_HPP
