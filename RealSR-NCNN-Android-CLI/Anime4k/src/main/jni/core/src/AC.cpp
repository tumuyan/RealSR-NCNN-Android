#include "AC.hpp"

Anime4KCPP::AC::AC(const Parameters& parameters) :
    height(0), width(0), param(parameters) {}

Anime4KCPP::AC::~AC()
{
    orgImg.release();
    dstImg.release();
    orgU.release();
    orgV.release();
    dstU.release();
    dstV.release();
    alphaChannel.release();
}

void Anime4KCPP::AC::setParameters(const Parameters& parameters)
{
    param = parameters;
}

Anime4KCPP::Parameters Anime4KCPP::AC::getParameters() const noexcept
{
    return param;
}

#ifdef ENABLE_IMAGE_IO
void Anime4KCPP::AC::loadImage(const std::string& srcFile)
{
    if (!param.alpha)
        orgImg = cv::imread(srcFile, cv::IMREAD_COLOR);
    else
        orgImg = cv::imread(srcFile, cv::IMREAD_UNCHANGED);

    if (orgImg.empty())
        throw ACException<ExceptionType::IO>("Failed to load file: file doesn't exist or incorrect file format.");

    switch (orgImg.channels())
    {
    case 4:
        cv::extractChannel(orgImg, alphaChannel, A);
        cv::resize(alphaChannel, alphaChannel, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
        cv::cvtColor(orgImg, orgImg, cv::COLOR_BGRA2BGR);
        inputGrayscale = false;
        checkAlphaChannel = true;
        break;
    case 3:
        inputGrayscale = false;
        checkAlphaChannel = false;
        break;
    case 1:
        inputGrayscale = true;
        checkAlphaChannel = false;
        break;
    default:
        throw ACException<ExceptionType::IO>("Failed to load file: incorrect file format.");
    }

    dstImg = orgImg;

    inputRGB32 = false;
    inputYUV = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}
#endif // ENABLE_IMAGE_IO

void Anime4KCPP::AC::loadImage(const cv::Mat& srcImage)
{
    orgImg = srcImage;

    if (orgImg.empty())
        throw ACException<ExceptionType::IO>("Failed to load data: empty data");

    switch (orgImg.channels())
    {
    case 4:
        if (param.alpha)
        {
            cv::extractChannel(orgImg, alphaChannel, A);
            cv::resize(alphaChannel, alphaChannel, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
        }
        cv::cvtColor(orgImg, orgImg, cv::COLOR_RGBA2RGB);
        inputRGB32 = !param.alpha;
        inputGrayscale = false;
        checkAlphaChannel = param.alpha;
        break;
    case 3:
        inputRGB32 = false;
        inputGrayscale = false;
        checkAlphaChannel = false;
        break;
    case 1:
        inputRGB32 = false;
        inputGrayscale = true;
        checkAlphaChannel = false;
        break;
    default:
        throw ACException<ExceptionType::IO>("Failed to load data: incorrect file format.");
    }

    dstImg = orgImg;

    inputYUV = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

void Anime4KCPP::AC::loadImage(const std::vector<std::uint8_t>& buf)
{
    if (!param.alpha)
        orgImg = cv::imdecode(buf, cv::IMREAD_COLOR);
    else
        orgImg = cv::imdecode(buf, cv::IMREAD_UNCHANGED);

    if (orgImg.empty())
        throw ACException<ExceptionType::IO>("Failed to load data: empty data");

    switch (orgImg.channels())
    {
    case 4:
        cv::extractChannel(orgImg, alphaChannel, A);
        cv::resize(alphaChannel, alphaChannel, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
        cv::cvtColor(orgImg, orgImg, cv::COLOR_BGRA2BGR);
        inputGrayscale = false;
        checkAlphaChannel = true;
        break;
    case 3:
        inputGrayscale = false;
        checkAlphaChannel = false;
        break;
    case 1:
        inputGrayscale = true;
        checkAlphaChannel = false;
        break;
    default:
        throw ACException<ExceptionType::IO>("Failed to load data: incorrect file format.");
    }

    dstImg = orgImg;

    inputRGB32 = false;
    inputYUV = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

void Anime4KCPP::AC::loadImage(const std::uint8_t* buf, std::size_t size)
{
    if (!param.alpha)
        loadImage(cv::imdecode(cv::Mat{ 1, static_cast<int>(size), CV_8UC1, (void*)buf }, cv::IMREAD_COLOR));
    else
        loadImage(cv::imdecode(cv::Mat{ 1, static_cast<int>(size), CV_8UC1, (void*)buf }, cv::IMREAD_UNCHANGED));
}

void Anime4KCPP::AC::loadImage(int rows, int cols, std::size_t stride, std::uint8_t* data, bool inputAsYUV444, bool inputAsRGB32, bool inputAsGrayscale)
{
    if(inputAsYUV444 + inputAsRGB32 + inputAsGrayscale > 1)
        throw ACException<ExceptionType::IO>("Failed to load data: Incompatible arguments.");

    if (inputAsYUV444)
    {
        inputYUV = true;
        inputRGB32 = false;
        inputGrayscale = false;

        cv::Mat yuv[3];
        cv::split(cv::Mat{ rows, cols, CV_8UC3, data, stride }, yuv);
        orgImg = yuv[Y];
        dstU = orgU = yuv[U];
        dstV = orgV = yuv[V];
    }
    else if (inputAsRGB32)
    {
        inputYUV = false;
        inputRGB32 = true;
        inputGrayscale = false;

        cv::cvtColor(cv::Mat(rows, cols, CV_8UC4, data, stride), orgImg, cv::COLOR_RGBA2RGB);
    }
    else if (inputAsGrayscale)
    {
        inputYUV = false;
        inputRGB32 = false;
        inputGrayscale = true;

        orgImg = cv::Mat(rows, cols, CV_8UC1, data, stride);
    }
    else
    {
        inputYUV = false;
        inputRGB32 = false;
        inputGrayscale = false;

        orgImg = cv::Mat(rows, cols, CV_8UC3, data, stride);
    }

    dstImg = orgImg;

    checkAlphaChannel = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

void Anime4KCPP::AC::loadImage(int rows, int cols, std::size_t stride, std::uint16_t* data, bool inputAsYUV444, bool inputAsRGB32, bool inputAsGrayscale)
{
    if (inputAsYUV444 + inputAsRGB32 + inputAsGrayscale > 1)
        throw ACException<ExceptionType::IO>("Failed to load data: Incompatible arguments.");

    if (inputAsYUV444)
    {
        inputYUV = true;
        inputRGB32 = false;
        inputGrayscale = false;

        cv::Mat yuv[3];
        cv::split(cv::Mat{ rows, cols, CV_16UC3, data, stride }, yuv);
        orgImg = yuv[Y];
        dstU = orgU = yuv[U];
        dstV = orgV = yuv[V];
    }
    else if (inputAsRGB32)
    {
        inputYUV = false;
        inputRGB32 = true;
        inputGrayscale = false;

        cv::cvtColor(cv::Mat(rows, cols, CV_16UC4, data, stride), orgImg, cv::COLOR_RGBA2RGB);
    }
    else if (inputAsGrayscale)
    {
        inputYUV = false;
        inputRGB32 = false;
        inputGrayscale = true;

        orgImg = cv::Mat(rows, cols, CV_16UC1, data, stride);
    }
    else
    {
        inputYUV = false;
        inputRGB32 = false;
        inputGrayscale = false;

        orgImg = cv::Mat(rows, cols, CV_16UC3, data, stride);
    }

    dstImg = orgImg;

    checkAlphaChannel = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

void Anime4KCPP::AC::loadImage(int rows, int cols, std::size_t stride, float* data, bool inputAsYUV444, bool inputAsRGB32, bool inputAsGrayscale)
{
    if (inputAsYUV444 + inputAsRGB32 + inputAsGrayscale > 1)
        throw ACException<ExceptionType::IO>("Failed to load data: Incompatible arguments.");

    if (inputAsYUV444)
    {
        inputYUV = true;
        inputRGB32 = false;
        inputGrayscale = false;

        cv::Mat yuv[3];
        cv::split(cv::Mat{ rows, cols, CV_32FC3, data, stride }, yuv);
        orgImg = yuv[Y];
        dstU = orgU = yuv[U];
        dstV = orgV = yuv[V];
    }
    else if (inputAsRGB32)
    {
        inputYUV = false;
        inputRGB32 = true;
        inputGrayscale = false;

        cv::cvtColor(cv::Mat(rows, cols, CV_32FC4, data, stride), orgImg, cv::COLOR_RGBA2RGB);
    }
    else if (inputAsGrayscale)
    {
        inputYUV = false;
        inputRGB32 = false;
        inputGrayscale = true;

        orgImg = cv::Mat(rows, cols, CV_32FC1, data, stride);
    }
    else
    {
        inputYUV = false;
        inputRGB32 = false;
        inputGrayscale = false;

        orgImg = cv::Mat(rows, cols, CV_32FC3, data, stride);
    }

    dstImg = orgImg;

    checkAlphaChannel = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

void Anime4KCPP::AC::loadImage(int rows, int cols, std::size_t stride, std::uint8_t* r, std::uint8_t* g, std::uint8_t* b, bool inputAsYUV444)
{
    if ((inputYUV = inputAsYUV444))
    {
        orgImg = cv::Mat(rows, cols, CV_8UC1, r, stride);
        dstU = orgU = cv::Mat(rows, cols, CV_8UC1, g, stride);
        dstV = orgV = cv::Mat(rows, cols, CV_8UC1, b, stride);
    }
    else
    {
        cv::merge(std::vector<cv::Mat>{
            cv::Mat(rows, cols, CV_8UC1, b, stride),
            cv::Mat(rows, cols, CV_8UC1, g, stride),
            cv::Mat(rows, cols, CV_8UC1, r, stride)}, orgImg);
    }

    dstImg = orgImg;

    inputGrayscale = false;
    inputRGB32 = false;
    checkAlphaChannel = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

void Anime4KCPP::AC::loadImage(int rows, int cols, std::size_t stride, std::uint16_t* r, std::uint16_t* g, std::uint16_t* b, bool inputAsYUV444)
{
    if ((inputYUV = inputAsYUV444))
    {
        orgImg = cv::Mat(rows, cols, CV_16UC1, r, stride);
        dstU = orgU = cv::Mat(rows, cols, CV_16UC1, g, stride);
        dstV = orgV = cv::Mat(rows, cols, CV_16UC1, b, stride);
    }
    else
    {
        cv::merge(std::vector<cv::Mat>{
            cv::Mat(rows, cols, CV_16UC1, b, stride),
            cv::Mat(rows, cols, CV_16UC1, g, stride),
            cv::Mat(rows, cols, CV_16UC1, r, stride)}, orgImg);
    }

    dstImg = orgImg;

    inputGrayscale = false;
    inputRGB32 = false;
    checkAlphaChannel = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

void Anime4KCPP::AC::loadImage(int rows, int cols, std::size_t stride, float* r, float* g, float* b, bool inputAsYUV444)
{
    if ((inputYUV = inputAsYUV444))
    {
        orgImg = cv::Mat(rows, cols, CV_32FC1, r, stride);
        dstU = orgU = cv::Mat(rows, cols, CV_32FC1, g, stride);
        dstV = orgV = cv::Mat(rows, cols, CV_32FC1, b, stride);
    }
    else
    {
        cv::merge(std::vector<cv::Mat>{
            cv::Mat(rows, cols, CV_32FC1, b, stride),
            cv::Mat(rows, cols, CV_32FC1, g, stride),
            cv::Mat(rows, cols, CV_32FC1, r, stride)}, orgImg);
    }

    dstImg = orgImg;

    inputGrayscale = false;
    inputRGB32 = false;
    checkAlphaChannel = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

void Anime4KCPP::AC::loadImage(
    int rowsY, int colsY, std::size_t strideY, std::uint8_t* y,
    int rowsU, int colsU, std::size_t strideU, std::uint8_t* u,
    int rowsV, int colsV, std::size_t strideV, std::uint8_t* v)
{
    dstImg = orgImg = cv::Mat(rowsY, colsY, CV_8UC1, y, strideY);
    dstU = orgU = cv::Mat(rowsU, colsU, CV_8UC1, u, strideU);
    dstV = orgV = cv::Mat(rowsV, colsV, CV_8UC1, v, strideV);

    inputGrayscale = false;
    inputYUV = true;
    inputRGB32 = false;
    checkAlphaChannel = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

void Anime4KCPP::AC::loadImage(
    int rowsY, int colsY, std::size_t strideY, std::uint16_t* y,
    int rowsU, int colsU, std::size_t strideU, std::uint16_t* u,
    int rowsV, int colsV, std::size_t strideV, std::uint16_t* v)
{
    dstImg = orgImg = cv::Mat(rowsY, colsY, CV_16UC1, y, strideY);
    dstU = orgU = cv::Mat(rowsU, colsU, CV_16UC1, u, strideU);
    dstV = orgV = cv::Mat(rowsV, colsV, CV_16UC1, v, strideV);

    inputGrayscale = false;
    inputYUV = true;
    inputRGB32 = false;
    checkAlphaChannel = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

void Anime4KCPP::AC::loadImage(
    int rowsY, int colsY, std::size_t strideY, float* y,
    int rowsU, int colsU, std::size_t strideU, float* u,
    int rowsV, int colsV, std::size_t strideV, float* v)
{
    dstImg = orgImg = cv::Mat(rowsY, colsY, CV_32FC1, y, strideY);
    dstU = orgU = cv::Mat(rowsU, colsU, CV_32FC1, u, strideU);
    dstV = orgV = cv::Mat(rowsV, colsV, CV_32FC1, v, strideV);

    inputGrayscale = false;
    inputYUV = true;
    inputRGB32 = false;
    checkAlphaChannel = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

void Anime4KCPP::AC::loadImage(const cv::Mat& y, const cv::Mat& u, const cv::Mat& v)
{
    dstImg = orgImg = y;
    dstU = orgU = u;
    dstV = orgV = v;

    inputGrayscale = false;
    inputYUV = true;
    inputRGB32 = false;
    checkAlphaChannel = false;

    height = static_cast<int>(std::round(param.zoomFactor * orgImg.rows));
    width = static_cast<int>(std::round(param.zoomFactor * orgImg.cols));
}

#ifdef ENABLE_IMAGE_IO
void Anime4KCPP::AC::saveImage(const std::string& dstFile)
{
    cv::Mat tmpImg = dstImg;
    if (inputYUV)
    {
        if (dstImg.size() != dstU.size())
            cv::resize(dstU, dstU, dstImg.size(), 0.0, 0.0, cv::INTER_CUBIC);
        if (dstImg.size() != dstV.size())
            cv::resize(dstV, dstV, dstImg.size(), 0.0, 0.0, cv::INTER_CUBIC);
        cv::merge(std::vector<cv::Mat>{ dstImg, dstU, dstV }, tmpImg);
        cv::cvtColor(tmpImg, tmpImg, cv::COLOR_YUV2BGR);
    }
    else if (checkAlphaChannel)
    {
        std::string fileSuffix = dstFile.substr(dstFile.rfind('.'));
        if (std::string(".jpg.jpeg.bmp").find(fileSuffix) != std::string::npos)
        {
            cv::Mat alpha, out;
            cv::cvtColor(alphaChannel, alpha, cv::COLOR_GRAY2BGR);
            alpha.convertTo(alpha, CV_32FC3, 1.0 / 255.0);
            cv::multiply(tmpImg, alpha, out, 1.0, CV_8U);
            tmpImg = out;
        }
        else
        {
            cv::merge(std::vector<cv::Mat>{ tmpImg, alphaChannel }, tmpImg);
        }
    }

    cv::imwrite(dstFile, tmpImg);
}
#endif // ENABLE_IMAGE_IO

void Anime4KCPP::AC::saveImage(const std::string suffix, std::vector<std::uint8_t>& buf)
{
    cv::Mat tmpImg = dstImg;
    if (inputYUV)
    {
        if (dstImg.size() != dstU.size())
            cv::resize(dstU, dstU, dstImg.size(), 0.0, 0.0, cv::INTER_CUBIC);
        if (dstImg.size() != dstV.size())
            cv::resize(dstV, dstV, dstImg.size(), 0.0, 0.0, cv::INTER_CUBIC);
        cv::merge(std::vector<cv::Mat>{ dstImg, dstU, dstV }, tmpImg);
        cv::cvtColor(tmpImg, tmpImg, cv::COLOR_YUV2BGR);
    }
    else if (checkAlphaChannel)
    {
        if (std::string(".jpg.jpeg.bmp").find(suffix) != std::string::npos)
        {
            cv::Mat alpha, out;
            cv::cvtColor(alphaChannel, alpha, cv::COLOR_GRAY2BGR);
            alpha.convertTo(alpha, CV_32FC3, 1.0 / 255.0);
            cv::multiply(tmpImg, alpha, out, 1.0, CV_8U);
            tmpImg = out;
        }
        else
        {
            cv::merge(std::vector<cv::Mat>{ tmpImg, alphaChannel }, tmpImg);
        }
    }

    if (!cv::imencode(suffix, tmpImg, buf))
        throw ACException<ExceptionType::RunTimeError>("Failed to encode image data");
}

void Anime4KCPP::AC::saveImage(cv::Mat& dstImage)
{
    cv::Mat tmpImg = dstImg;
    if (inputYUV)
    {
        if (dstImg.size() == dstU.size() && dstU.size() == dstV.size())
            cv::merge(std::vector<cv::Mat>{ dstImg, dstU, dstV }, tmpImg);
        else
            throw ACException<ExceptionType::IO>("Only YUV444 or RGB(BGR) can be saved to opencv Mat");
    }
    else if (inputRGB32)
    {
        cv::cvtColor(tmpImg, tmpImg, cv::COLOR_RGB2RGBA);
    }
    else if (checkAlphaChannel)
    {
        cv::merge(std::vector<cv::Mat>{ tmpImg, alphaChannel }, tmpImg);
    }

    dstImage = tmpImg;
}

void Anime4KCPP::AC::saveImage(cv::Mat& r, cv::Mat& g, cv::Mat& b)
{
    if (inputYUV)
    {
        r = dstImg;
        g = dstU;
        b = dstV;
    }
    else
    {
        cv::Mat bgr[3];
        cv::split(dstImg, bgr);
        r = bgr[R];
        g = bgr[G];
        b = bgr[B];
    }
}

void Anime4KCPP::AC::saveImage(std::uint8_t* data, std::size_t dstStride)
{
    cv::Mat tmpImg = dstImg;
    if (data == nullptr)
        throw ACException<ExceptionType::RunTimeError>("Pointer can not be nullptr");
    if (inputYUV)
    {
        if (dstImg.size() == dstU.size() && dstU.size() == dstV.size())
            cv::merge(std::vector<cv::Mat>{ dstImg, dstU, dstV }, tmpImg);
        else
            throw ACException<ExceptionType::IO>("Only YUV444 can be saved to data pointer");
    }
    else if (inputRGB32)
    {
        cv::cvtColor(tmpImg, tmpImg, cv::COLOR_RGB2RGBA);
    }
    else if (checkAlphaChannel)
    {
        cv::merge(std::vector<cv::Mat>{ tmpImg, alphaChannel }, tmpImg);
    }

    std::size_t stride = tmpImg.step;
    std::size_t step = dstStride > stride ? dstStride : stride;
    if (stride == step)
    {
        std::memcpy(data, tmpImg.data, stride * tmpImg.rows);
    }
    else
    {
        for (std::size_t i = 0; i < tmpImg.rows; i++)
        {
            std::memcpy(data, tmpImg.data + i * stride, stride);
            data += step;
        }
    }
}

void Anime4KCPP::AC::saveImage(
    std::uint8_t* r, std::size_t dstStrideR,
    std::uint8_t* g, std::size_t dstStrideG,
    std::uint8_t* b, std::size_t dstStrideB)
{
    if (r == nullptr || g == nullptr || b == nullptr)
        throw ACException<ExceptionType::RunTimeError>("Pointers can not be nullptr");
    if (inputYUV)
    {
        std::size_t strideY = dstImg.step;
        std::size_t strideU = dstU.step;
        std::size_t strideV = dstV.step;

        std::size_t stepY = dstStrideR > strideY ? dstStrideR : strideY;
        std::size_t stepU = dstStrideG > strideU ? dstStrideG : strideU;
        std::size_t stepV = dstStrideB > strideV ? dstStrideB : strideV;

        std::size_t HY = dstImg.rows;
        std::size_t HUV = dstU.rows;

        if (strideY == stepY && strideU == stepU && strideV == stepV)
        {
            std::memcpy(r, dstImg.data, strideY * HY);
            std::memcpy(g, dstU.data, strideU * HUV);
            std::memcpy(b, dstV.data, strideV * HUV);
        }
        else
        {
            for (std::size_t i = 0; i < HY; i++)
            {
                std::memcpy(r, dstImg.data + i * strideY, strideY);
                r += stepY;

                if (i < HUV)
                {
                    std::memcpy(g, dstU.data + i * strideU, strideU);
                    std::memcpy(b, dstV.data + i * strideV, strideV);
                    b += stepV;
                    g += stepU;
                }
            }
        }
    }
    else
    {
        cv::Mat bgr[3];
        cv::split(dstImg, bgr);

        std::size_t stride = bgr[R].step;
        std::size_t height = bgr[B].rows;
        std::size_t step = dstStrideR > stride ? dstStrideR : stride;

        if (stride == step)
        {
            std::memcpy(b, bgr[B].data, stride * height);
            std::memcpy(g, bgr[G].data, stride * height);
            std::memcpy(r, bgr[R].data, stride * height);
        }
        else
        {
            for (std::size_t i = 0; i < height; i++)
            {
                std::memcpy(b, bgr[B].data + i * stride, stride);
                std::memcpy(g, bgr[G].data + i * stride, stride);
                std::memcpy(r, bgr[R].data + i * stride, stride);

                b += step;
                g += step;
                r += step;
            }
        }
    }
}

void Anime4KCPP::AC::saveImageBufferSize(std::size_t& dataSize, std::size_t dstStride)
{
    std::size_t stride = dstImg.step;

    if (inputYUV)
    {
        stride = dstImg.step + dstU.step + dstV.step;
    }
    else if (inputRGB32 || checkAlphaChannel)
        stride += stride / 3;

    std::size_t step = dstStride > stride ? dstStride : stride;
    dataSize = step * dstImg.rows;;
}

void Anime4KCPP::AC::saveImageBufferSize(std::size_t& rSize, std::size_t dstStrideR, std::size_t& gSize, std::size_t dstStrideG, std::size_t& bSize, std::size_t dstStrideB)
{
    if (inputYUV)
    {
        std::size_t strideY = dstImg.step;
        std::size_t strideU = dstU.step;
        std::size_t strideV = dstV.step;

        std::size_t stepY = dstStrideR > strideY ? dstStrideR : strideY;
        std::size_t stepU = dstStrideG > strideU ? dstStrideG : strideU;
        std::size_t stepV = dstStrideB > strideV ? dstStrideB : strideV;

        std::size_t HY = dstImg.rows;
        std::size_t HUV = dstU.rows;

        rSize = stepY * HY;
        gSize = stepU * HUV;
        bSize = stepV * HUV;
    }
    else
    {
        std::size_t stride = dstImg.step / 3;
        std::size_t step = dstStrideR > stride ? dstStrideR : stride;

        rSize = step * dstImg.rows;
        gSize = step * dstImg.rows;
        bSize = step * dstImg.rows;
    }
}

void Anime4KCPP::AC::saveImageShape(int& cols, int& rows, int& channels)
{
    cols = dstImg.cols;
    rows = dstImg.rows;
    channels = (inputRGB32 || checkAlphaChannel) ? 4 : 3;
}

void Anime4KCPP::AC::showImage(bool R2B)
{
#ifdef ENABLE_PREVIEW_GUI
    cv::Mat tmpImg;
    if (R2B)
        cv::cvtColor(dstImg, tmpImg, cv::COLOR_BGR2RGB);
    else
        tmpImg = dstImg;

    if (inputYUV)
    {
        cv::Mat tmpU, tmpV;
        if (dstImg.size() != dstU.size())
            cv::resize(dstU, tmpU, dstImg.size(), 0.0, 0.0, cv::INTER_CUBIC);
        if (dstImg.size() != dstV.size())
            cv::resize(dstV, tmpV, dstImg.size(), 0.0, 0.0, cv::INTER_CUBIC);
        cv::merge(std::vector<cv::Mat>{ dstImg, tmpU, tmpV }, tmpImg);
        cv::cvtColor(tmpImg, tmpImg, cv::COLOR_YUV2BGR);
    }
    else if (checkAlphaChannel)
    {
        cv::Mat alpha, out;
        cv::cvtColor(alphaChannel, alpha, cv::COLOR_GRAY2BGR);
        alpha.convertTo(alpha, CV_32FC3, 1.0 / 255.0);
        cv::multiply(tmpImg, alpha, out, 1.0, CV_8U);
        tmpImg = out;
    }

    cv::imshow("preview", tmpImg);
    cv::waitKey();
#else
    throw ACException<ExceptionType::RunTimeError>("Preview image is not currently supported.");
#endif // ENABLE_PREVIEW_GUI
}

void Anime4KCPP::AC::process()
{
    if (inputYUV)
        processYUVImage();
    else if (inputGrayscale)
        processGrayscale();
    else
        processRGBImage();
}

std::string Anime4KCPP::AC::getInfo() const
{
    std::ostringstream oss;
    oss << "----------------------------------------------" << '\n'
        << "Parameter information" << '\n'
        << "----------------------------------------------" << '\n';
    if (orgImg.cols && orgImg.rows)
    {
        oss << orgImg.cols << "x" << orgImg.rows << " to " << width << "x" << height << '\n'
            << "----------------------------------------------" << '\n';
    }
    oss << "Processor info: \n "
        << getProcessorInfo() << '\n';

    return oss.str();
}

std::string Anime4KCPP::AC::getFiltersInfo() const
{
    std::ostringstream oss;
    oss << "----------------------------------------------" << '\n'
        << "Filter information" << '\n'
        << "----------------------------------------------" << '\n';

    return oss.str();
}

void Anime4KCPP::Parameters::reset() noexcept
{
    passes = 2;
    pushColorCount = 2;
    strengthColor = 0.3;
    strengthGradient = 1.0;
    zoomFactor = 2.0;
    fastMode = false;
    preprocessing = false;
    postprocessing = false;
    preFilters = 4;
    postFilters = 40;
    HDN = false;
    HDNLevel = 1;
    alpha = false;
}

Anime4KCPP::Parameters::Parameters(
    int passes,
    int pushColorCount,
    double strengthColor,
    double strengthGradient,
    double zoomFactor,
    bool fastMode,
    bool preprocessing,
    bool postprocessing,
    std::uint8_t preFilters,
    std::uint8_t postFilters,
    bool HDN,
    int HDNLevel,
    bool alpha
) noexcept :
    passes(passes), pushColorCount(pushColorCount),
    strengthColor(strengthColor), strengthGradient(strengthGradient),
    zoomFactor(zoomFactor), fastMode(fastMode), preprocessing(preprocessing),
    postprocessing(postprocessing), preFilters(preFilters), postFilters(postFilters),
    HDNLevel(HDNLevel), HDN(HDN), alpha(alpha) {}
