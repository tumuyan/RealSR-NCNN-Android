//
// Created by Yazii on 2025/3/16.
//
#include "mnnsr.h"
#include "utils.hpp"
#include <thread>

#include "MNN/ErrorCode.hpp"


#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdio> // For fprintf

using namespace MNN;

MNNSR::MNNSR(int color_type) {
    color = static_cast<ColorType>(color_type);
    if (color == ColorType::RGB)
        pretreat_ = std::shared_ptr<MNN::CV::ImageProcess>(
                MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::RGB, meanVals_, 3, normVals_,
                                              3));
    else if (color == ColorType::GRAY || color == ColorType::Gray2YCbCr ||
             color == ColorType::Gray2YUV) {
        model_channel = 1;
        pretreat_ = std::shared_ptr<MNN::CV::ImageProcess>(
                MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::GRAY, meanVals_, 3, normVals_,
                                              3));
    } else if (color == ColorType::YCbCr) {
        pretreat_ = std::shared_ptr<MNN::CV::ImageProcess>(
                MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::YCrCb, meanVals_, 3, normVals_,
                                              3));
    } else if (color == ColorType::YUV) {
        pretreat_ = std::shared_ptr<MNN::CV::ImageProcess>(
                MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::YUV, meanVals_, 3, normVals_,
                                              3));
    } else {
        fprintf(stderr, "color space error\n");
        exit(1);
    }

}

MNNSR::~MNNSR() {
    MNN::Tensor::destroy(input_tensor);
    MNN::Tensor::destroy(output_tensor);
    interpreter->releaseSession(session);
    interpreter->releaseModel();
    MNN::Interpreter::destroy(interpreter);
}


#if _WIN32
#include <codecvt>

int MNNSR::load(const std::wstring& modelpath, bool cachemodel)
#else

int MNNSR::load(const std::string &modelpath, bool cachemodel)
#endif
{
    MNN::ScheduleConfig config;
    MNN::BackendConfig backendConfig;
    backendConfig.memory = MNN::BackendConfig::Memory_High;
    backendConfig.power = MNN::BackendConfig::Power_High;
    backendConfig.precision = MNN::BackendConfig::Precision_Low;
    config.backendConfig = &backendConfig;
//    config.type = MNN_FORWARD_NN;
//    config.type = MNN_FORWARD_VULKAN;
//    config.type = MNN_FORWARD_OPENCL;
//    config.type = MNN_FORWARD_AUTO;
    config.type = backend_type;
//    config.backupType = MNN_FORWARD_OPENCL;
//    config.backupType = MNN_FORWARD_VULKAN;
//    config.backupType = MNN_FORWARD_AUTO;
    config.backupType = MNN_FORWARD_CPU;
    int num_threads = std::thread::hardware_concurrency();
    if (num_threads < 1)
        num_threads = 2;
    config.numThread = num_threads;

    fprintf(stderr, "set backend: %s, color type: %s\n", get_backend_name(config.type).c_str(),
            colorTypeToStr(color));

    const auto start = std::chrono::high_resolution_clock::now();

#if _WIN32
    interpreter = MNN::Interpreter::createFromFile(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(modelpath).c_str());
#else
    interpreter = MNN::Interpreter::createFromFile((modelpath).c_str());
#endif


    if (interpreter == nullptr) {
        fprintf(stderr, "interpreter null\n");
    }

    this->cachemodel = cachemodel;
    if (cachemodel) {
        std::string cachefile = modelpath + ".cache";
        interpreter->setCacheFile(cachefile.c_str());
    }

    session = interpreter->createSession(config);
    if (session == nullptr) {
        fprintf(stderr, "session null\n");
    }


    interpreter_input = interpreter->getSessionInput(session, nullptr);
//    fprintf(stderr, "model input tensor(b/c/h/w): %d/%d/%d/%d -> 1/%d/%d/%d\n"
//            , input_tensor->batch(), input_tensor->channel(), input_tensor->height(), input_tensor->width()
//            ,model_channel, tilesize, tilesize
//            );
    interpreter->resizeTensor(interpreter_input, 1, model_channel, tilesize, tilesize);
    interpreter->resizeSession(session);
    interpreter_output = interpreter->getSessionOutput(session, nullptr);

    input_tensor = new MNN::Tensor(interpreter_input, MNN::Tensor::CAFFE);
    output_tensor = new MNN::Tensor(interpreter_output, MNN::Tensor::CAFFE);

    input_buffer = input_tensor->host<float>();
    output_buffer = output_tensor->host<float>();

    float memoryUsage = 0.0f;
    interpreter->getSessionInfo(session, MNN::Interpreter::MEMORY, &memoryUsage);
    float flops = 0.0f;
    interpreter->getSessionInfo(session, MNN::Interpreter::FLOPS, &flops);
    MNNForwardType backendType[2];
    interpreter->getSessionInfo(session, MNN::Interpreter::BACKENDS, backendType);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start);
    fprintf(stderr, "load model %.3f s, session memory %sB, flops %s, ",
            static_cast<double>(duration.count()) / 1000, float2str(memoryUsage, 6).c_str(),
            float2str(flops, 6).c_str());

    if (backendType[0] == MNN_FORWARD_CPU)
        fprintf(stderr, "backend: CPU, numThread=%d\n", config.numThread);
    else
        fprintf(stderr, "backend: %s, %s\n", get_backend_name(backendType[0]).c_str(),
                get_backend_name(backendType[1]).c_str());

    return 0;
}


cv::Mat MNNSR::TensorToCvMat(void) {
    interpreter_output->copyToHostTensor(output_tensor);
    int C = output_tensor->channel();
    int H = output_tensor->height();
    int W = output_tensor->width();
    float *data = output_tensor->host<float>();

    cv::Mat result;
    if (C == 1) {
        // 灰度模式直接处理
        cv::Mat gray = cv::Mat(H, W, CV_32FC1, data);
        gray.convertTo(gray, CV_8UC1, 255.0);
        cv::cvtColor(gray, result, cv::COLOR_GRAY2BGR);
        return result;
    } else {
        // 处理彩色图像
        std::vector<cv::Mat> channels;
        for (int i = 0; i < C; i++) {
            channels.emplace_back(H, W, CV_32FC1, data + i * H * W);
        }

        // 合并通道（注意OpenCV默认是BGR顺序）
        if (color == RGB) {
            // 如果是RGB输入，需要交换R和B通道
            std::swap(channels[0], channels[2]); // RGB -> BGR
            cv::merge(channels, result);
        } else {
            // 先合并为RGB格式
            cv::Mat rgb;
            cv::merge(channels, rgb);
            rgb.convertTo(rgb, CV_8UC3, 255.0);

            // 转换为目标颜色空间
            if (color == YCbCr) {
                cv::cvtColor(rgb, result, cv::COLOR_BGR2YCrCb);
            } else if (color == YUV) {
                cv::cvtColor(rgb, result, cv::COLOR_BGR2YUV);
            }
            return result;
        }

        // 转换为8位
        result.convertTo(result, CV_8UC3, 255.0);
    }

    return result;
}

int MNNSR::process(const cv::Mat &inimage, cv::Mat &outimage, const cv::Mat &mask) {
    int skiped_tile = 0;
    cv::Mat inMask;
    if (mask.empty()) {
        inMask = cv::Mat();
    } else {
        cv::resize(mask, inMask, inimage.size(), 0, 0, cv::INTER_AREA);
    }

    int inWidth = inimage.cols;
    int inHeight = inimage.rows;

    int outWidth = inWidth * scale;
    int outHeight = inHeight * scale;

    // 每个tile的有效大小
    int tileWidth = tilesize - prepadding * 2;
    int tileHeight = tilesize - prepadding * 2;


    int xtiles = (inWidth + tileWidth - 1) / tileWidth;
    int ytiles = (inHeight + tileHeight - 1) / tileHeight;


    int xPrepadding = prepadding, yPrepadding = prepadding;

    int left = inWidth % tileWidth;
    if (left > 0) {
        if (left < prepadding) {
            // 倒数第2个tile的prepadding已经包含了推理结果
            xtiles--;
        } else {
            if (left / 2 <= prepadding)
                xtiles--;
            // xtiles * (tilesize - 2 * xPrepadding) + xPrepadding = inWidth
            xPrepadding = (xtiles * tilesize - inWidth) / (2 * xtiles - 1);
            tileWidth = tilesize - xPrepadding * 2;
        }
    }
    left = inHeight % tileHeight;
    if (left > 0) {
        if (left < prepadding) {
            // 倒数第2个tile的prepadding已经包含了推理结果
            ytiles--;
        } else {
            if (left / 2 <= prepadding)
                ytiles--;
            // ytiles * (tilesize - 2 * yPrepadding) + yPrepadding = inHeight
            yPrepadding = (ytiles * tilesize - inHeight) / (2 * ytiles - 1);
            tileHeight = tilesize - yPrepadding * 2;
        }
    }

    fprintf(stderr,
            "process tiles: %d x %d, tilesize: %d -> %d %d, prepadding: %d -> %d %d\n",
            xtiles, ytiles, tilesize, tileWidth, tileHeight, prepadding, xPrepadding, yPrepadding);

    high_resolution_clock::time_point begin = high_resolution_clock::now();
    high_resolution_clock::time_point time_print_progress;

//    cv::Mat imageOut(outHeight, outWidth, inimage.type()); // 填充灰色背景

    for (uint yi = 0; yi < ytiles; yi++) {
        // 从inimage中裁剪出含padding的tile （但是四边的tile需要再次padding）
        int in_tile_y0 = (yi * tileHeight - yPrepadding);
        if (in_tile_y0 < 0)
            in_tile_y0 = 0;
        int in_tile_y1 = (yi + 1) * tileHeight + yPrepadding;
        if (in_tile_y1 > inHeight)
            in_tile_y1 = inHeight;
        // 从tile推理结果去除padding部分
        int out_tile_y0 = scale * yPrepadding;

        // 绘制到outimage的位置
        int out_y0 = yi * tileHeight * scale;
        int out_tile_h = (yi + 1 == ytiles) ? inHeight * scale - out_y0 : tileHeight * scale;

        for (uint xi = 0; xi < xtiles; xi++) {


            if (!inMask.empty()) {
                int x0 = xi * tileWidth, x = xi == xtiles - 1 ? inWidth - xi * tileWidth :
                                             (xi + 1) * tileWidth, y0 = yi * tileHeight, y =
                        yi == ytiles - 1 ? inHeight - yi * tileHeight : (yi + 1) * tileHeight;
                cv::Mat maskTile = inMask(cv::Rect(x0, y0, x, y));

                // 判断maskTile是否全部为0
                if (cv::countNonZero(maskTile) == 0) {
                    // 如果maskTile全部为0，跳过该tile
                    cv::Mat inputTile = inimage(
                            cv::Rect(x0, y0, x, y));
                    cv::Mat outputTile;
                    cv::resize(inputTile, outputTile, cv::Size(x * scale, y * scale), 0, 0,
                               cv::INTER_CUBIC);

                    outputTile.copyTo(
                            outimage(cv::Rect(x0 * scale, y0 * scale, outputTile.cols,
                                              outputTile.rows)));
                    skiped_tile++;
                    continue;
                }
            }


            // 从inimage中裁剪出含padding的tile （但是四边的tile需要再次padding）
            int in_tile_x0 = (xi * tileWidth - xPrepadding);
            if (in_tile_x0 < 0)
                in_tile_x0 = 0;
            int in_tile_x1 = ((xi + 1) * tileWidth + xPrepadding);
            if (in_tile_x1 > inWidth)
                in_tile_x1 = inWidth;

            // 从tile推理结果去除padding部分
            int out_tile_x0 = scale * xPrepadding;

            // 绘制到outimage的位置
            int out_x0 = xi * tileWidth * scale;
            int out_tile_w = (xi + 1 == xtiles) ? inWidth * scale - out_x0 : tileWidth * scale;




//            fprintf(stderr, "\nprocess y=%d, x=%d inputTile: x0=%d y0=%d x1=%d y1=%d w=%d h=%d\n",
//                    yi, xi,
//                    in_tile_x0, in_tile_y0, in_tile_x1, in_tile_y1, in_tile_x1 - in_tile_x0,
//                    in_tile_y1 - in_tile_y0);

            cv::Mat inputTile = inimage(cv::Rect(in_tile_x0, in_tile_y0, in_tile_x1 - in_tile_x0,
                                                 in_tile_y1 - in_tile_y0));

//            fprintf(stderr, "process inputTile y=%d, x=%d, inputTile: %d/%d/%d\n", yi, xi,
//                    inputTile.cols, inputTile.rows, inputTile.channels());
            cv::Mat paddedTile;
            if (inputTile.cols < tilesize || inputTile.rows < tilesize) {
                int t = (yi == 0) ? yPrepadding : 0;
                int b = tilesize + in_tile_y0 - in_tile_y1 - t;
                int l = (xi == 0) ? xPrepadding : 0;
                int r = tilesize + in_tile_x0 - in_tile_x1 - l;

//                fprintf(stderr, "process y=%d, x=%d copyMakeBorder %d %d %d %d\n", yi, xi, t, b, l,
//                        r);
//                cv::Mat paddedTile;
                cv::copyMakeBorder(inputTile, paddedTile, t, b, l, r, cv::BORDER_CONSTANT);

                pretreat_->convert(paddedTile.data, paddedTile.cols, paddedTile.rows,
                                   paddedTile.cols * paddedTile.channels(),
                                   input_tensor);

            } else {
//                cv::Mat paddedTile;
                cv::copyMakeBorder(inputTile, paddedTile, 0, 0, 0, 0, cv::BORDER_CONSTANT);

                pretreat_->convert(paddedTile.data, paddedTile.cols, paddedTile.rows,
                                   paddedTile.cols * paddedTile.channels(),
                                   input_tensor);
            }

            bool r = interpreter_input->copyFromHostTensor(input_tensor);

            interpreter->runSession(session);
            cv::Mat outputTile = TensorToCvMat();

            if (outputTile.cols != tilesize * scale || outputTile.rows != tilesize * scale) {
                fprintf(stderr,
                        "[err] The model is x%.2f not x%d. input tile: %d x %d, output tile: %d x %d.\n",
                        sqrt(outputTile.cols * outputTile.rows / paddedTile.cols / paddedTile.rows),
                        scale,
                        paddedTile.cols, paddedTile.rows, outputTile.cols, outputTile.rows);
                return -1;
            }

//            fprintf(stderr,
//                    "croppe outputTile from x0=%d, y0=%d, x1=%d, y1=%d, %d x %d\n",
//                    out_tile_x0, out_tile_y0, out_tile_x0 + out_tile_w, out_tile_y0 + out_tile_h,
//                    outputTile.cols, outputTile.rows);
            cv::Rect cropRect(out_tile_x0, out_tile_y0, out_tile_w, out_tile_h);
            cv::Mat croppedTile = outputTile(cropRect);

//            fprintf(stderr,
//                    "cropped to outimage to x0=%d, y0=%d, x1=%d, y1=%d, %d x %d\n",
//                    out_x0, out_y0, out_x0 + croppedTile.cols, out_y0 + croppedTile.rows,
//                    croppedTile.cols, croppedTile.rows);
            croppedTile.copyTo(
                    outimage(cv::Rect(out_x0, out_y0, croppedTile.cols, croppedTile.rows)));


            high_resolution_clock::time_point end = high_resolution_clock::now();
            float time_span_print_progress = duration_cast<duration<double>>(
                    end - time_print_progress).count();
            float progress_tile = (float) (yi * xtiles + xi + 1);
            if (time_span_print_progress > 0.5 || (yi + 1 == ytiles && xi + 3 > xtiles)) {
                double progress = progress_tile / (ytiles * xtiles);
                // progress2 用于计算剩余时间，由于跳过的tile不会运行这段函数，因此不会出现分母为0或者分子为0的情况
                double progress2 = (progress_tile - skiped_tile) / (ytiles * xtiles - skiped_tile);
                double time_span = duration_cast<duration<double>>(end - begin).count();
                fprintf(stderr, "%5.2f%%\t[%5.2fs /%5.2f ETA]\n", progress * 100, time_span,
                        time_span / progress2 - time_span);
                time_print_progress = end;
            }

        }
    }

    if (color == Gray2YUV) {
        // 把inimage转为YCbCr格式，放大scale倍，把通道2通道3复制给outimage的通道2通道3
        cv::Mat yuv;
        cv::cvtColor(inimage, yuv, cv::COLOR_BGR2YUV);
        cv::Mat yuv2;
        yuv2.create(inimage.rows * scale, inimage.cols * scale, CV_8UC3);
        cv::resize(yuv, yuv2, cv::Size(inimage.cols * scale, inimage.rows * scale), 0, 0,
                   cv::INTER_CUBIC);
        cv::cvtColor(yuv2, outimage, cv::COLOR_YUV2BGR);
    } else if (color == Gray2YCbCr) {
        // 把inimage转为YCbCr格式，放大scale倍，把通道2通道3复制给outimage的通道2通道3
        cv::Mat ycc;
        cv::cvtColor(inimage, ycc, cv::COLOR_BGR2YCrCb);
        cv::Mat ycc2;
        ycc2.create(inimage.rows * scale, inimage.cols * scale, CV_8UC3);
        cv::resize(ycc, ycc2, cv::Size(inimage.cols * scale, inimage.rows * scale), 0, 0,
                   cv::INTER_CUBIC);
        cv::cvtColor(ycc2, outimage, cv::COLOR_YCrCb2BGR);
    }


    if (cachemodel)
        interpreter->updateCacheFile(session);
    return 0;
}


#include "mnnsr.h"
#include "utils.hpp"
#include <thread>

#include "MNN/ErrorCode.hpp"


#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdio> // For fprintf
#include <numeric> // For std::accumulate
#include <set> // For std::set to unique indices easily


/**
 * @brief Detects and removes mosaic censorship from an image.
 *
 * @param inimage Input image (BGR or BGRA).
 * @param outimage Output image with mosaic removed (same format as input).
 * @return 0 on success, -1 on failure or if no mosaic was detected.
 */
int MNNSR::decensor(const cv::Mat &inimage, cv::Mat &outimage) {
    if (inimage.empty()) {
        fprintf(stderr, "decensor error: Input image is empty.\n");
        return -1;
    }

    // --- Constants (matching detectMosaicResolution logic) ---
    // Kept original names where possible, added comments based on detectMosaicResolution
    const int GBlur = 5;         // Gaussian blur kernel size (must be odd)
    const int CannyTr1 = 8;      // Canny lower threshold
    const int CannyTr2 = 30;     // Canny upper threshold
    const int LowRange = 2;      // Minimum mosaic block size to check
    const int HighRange = 25;    // Maximum mosaic block size to check
    const float DetectionTr = 0.29f; // Threshold for template matching correlation (kept float)

    // Validate Gaussian blur kernel size (from detectMosaicResolution)
    if (GBlur % 2 == 0 || GBlur <= 0) {
        fprintf(stderr,
                "decensor error: GBlur_kernel_size must be a positive odd number. Got %d.\n",
                GBlur);
        // Can't proceed with invalid blur kernel, but detection might still work somewhat without it.
        // Let's just warn for now, as the Canny step might still provide useful input.
        // If we wanted strict failure on bad constants, we would return -1 here.
    }

    // --- 1. Preprocessing for Detection ---
    cv::Mat img_detection_input; // Will be CV_8U after processing
    int original_channels = inimage.channels();

    // Convert input to BGR (or keep BGR if already) for color space conversions
    cv::Mat img_bgr;
    if (original_channels == 4) {
        cv::cvtColor(inimage, img_bgr, cv::COLOR_BGRA2BGR);
    } else if (original_channels == 3) {
        img_bgr = inimage.clone(); // Assuming BGR input
    } else {
        fprintf(stderr,
                "decensor error: Unsupported number of channels (%d). Input must be 3 (BGR) or 4 (BGRA).\n",
                original_channels);
        return -1;
    }

    // Convert to grayscale
    cv::Mat img_gray;
    cv::cvtColor(img_bgr, img_gray, cv::COLOR_BGR2GRAY);

    // Canny edge detection
    cv::Canny(img_gray, img_detection_input, CannyTr1, CannyTr2);

    // Invert edges: non-edges become bright (255), edges become dark (0)
    img_detection_input = 255 - img_detection_input; // In-place inversion

    // Gaussian Blur (on the inverted edges)
    cv::GaussianBlur(img_detection_input, img_detection_input, cv::Size(GBlur, GBlur), 0);

    // --- 2. Pattern Generation and Matching ---

    // Pattern vector size: HighRange + 2. Indices 0 to HighRange+1.
    // We only generate patterns for masksize from LowRange+2 up to HighRange+2.
    // Index for masksize M is M - 2. Relevant indices are [LowRange, HighRange].
    std::vector<cv::Mat> patterns(HighRange + 2);

    // Loop through potential mask sizes, matching detectMosaicResolution's loop range
    // Note: Loop is reversed compared to original decensor, matching detectMosaicResolution
    fprintf(stderr, "decensor: Generating patterns for mask sizes from %d down to %d...\n",
            HighRange + 2, LowRange + 2);
    for (int masksize = HighRange + 2; masksize >= LowRange + 2; --masksize) {
        int pattern_idx = masksize - 2; // Index in patterns vector [LowRange, HighRange]

        // Pattern size calculation from detectMosaicResolution: 2*masksize + 3
        int pattern_size = 2 * masksize + 3;

        // Check if pattern size is larger than the image. If so, it cannot be matched.
        if (pattern_size > img_detection_input.cols || pattern_size > img_detection_input.rows) {
            fprintf(stderr,
                    "decensor warning: Pattern size %d for masksize %d exceeds image dimensions (%dx%d). Skipping pattern creation and matching.\n",
                    pattern_size, masksize, img_detection_input.cols, img_detection_input.rows);
            // patterns[pattern_idx] will remain empty, which is handled below.
            continue;
        }

        // Create CV_8U pattern (Grayscale), matching the type of img_detection_input
        cv::Mat pattern = cv::Mat(pattern_size, pattern_size, CV_8U,
                                  cv::Scalar(255)); // White background

        // Draw black lines (0) with thickness 1, starting at index 2, stepping by masksize - 1
        // This directly implements the drawing logic from detectMosaicResolution
        for (int i = 2; i < pattern_size; i += masksize - 1) {
            cv::line(pattern, cv::Point(i, 0), cv::Point(i, pattern_size - 1), cv::Scalar(0),
                     1); // Vertical line
        }
        for (int j = 2; j < pattern_size; j += masksize - 1) {
            cv::line(pattern, cv::Point(0, j), cv::Point(pattern_size - 1, j), cv::Scalar(0),
                     1); // Horizontal line
        }
        patterns[pattern_idx] = pattern; // Store the created pattern (CV_8U)
    }
    fprintf(stderr, "decensor: Pattern generation complete.\n");


    // Mask to mark detected mosaic regions (CV_8U, 255 = mosaic, 0 = non-mosaic)
    // This mask will be used later to blend the processed region back.
    cv::Mat card_mask = cv::Mat::zeros(inimage.size(), CV_8U);

    // resolutions vector stores the count of matches for each potential masksize.
    // Size HighRange+3 (indices 0 to HighRange+2) initialized to 0.
    // Count for masksize M is stored at index M - 1. Relevant indices [LowRange+1, HighRange+1].
    // This matches the indexing structure used in detectMosaicResolution's analysis section.
    std::vector<int> resolutions(HighRange + 3, 0);

    fprintf(stderr, "decensor: Starting template matching...\n");

    // Loop through potential mask sizes again (matching pattern loop range)
    for (int masksize = HighRange + 2; masksize >= LowRange + 2; --masksize) {
        int pattern_idx = masksize - 2;      // Index in patterns vector [LowRange, HighRange]
        int resolution_idx =
                masksize - 1;   // Index in resolutions vector [LowRange+1, HighRange+1]

        // Ensure indices are within bounds (defensive check)
        if (pattern_idx < 0 || pattern_idx >= patterns.size() ||
            resolution_idx < 0 || resolution_idx >= resolutions.size()) {
            // Should not happen with correct constants and logic, but check anyway
            fprintf(stderr,
                    "decensor Error: Index calculation out of bounds during matching. pattern_idx=%d, resolution_idx=%d. Skipping masksize %d.\n",
                    pattern_idx, resolution_idx, masksize);
            continue;
        }

        // Skip if pattern was not created (e.g., too large for image)
        if (patterns[pattern_idx].empty()) {
            continue;
        }

        cv::Mat templateImg = patterns[pattern_idx]; // Already CV_8U

        int w = templateImg.cols;
        int h = templateImg.rows;

        // Ensure template is smaller than or equal to the image dimensions (redundant with pattern creation check, but safe)
        if (w > img_detection_input.cols || h > img_detection_input.rows) {
            // Should not happen if patterns[pattern_idx].empty() check works, but safe
            continue;
        }

        cv::Mat img_detection_result; // Result of matchTemplate (CV_32F)
        // Match the processed image (inverted, blurred edges) against the pattern (dark lines on white)
        cv::matchTemplate(img_detection_input, templateImg, img_detection_result,
                          cv::TM_CCOEFF_NORMED);

        // Threshold the result to find locations above the detection threshold
        cv::Mat detection_locations_mask; // CV_8U, 255 for matches, 0 otherwise
        cv::threshold(img_detection_result, detection_locations_mask, DetectionTr, 255,
                      cv::THRESH_BINARY);

        // Find all points that are above the threshold
        std::vector<cv::Point> points;
        cv::findNonZero(detection_locations_mask, points);

        int rects = points.size(); // Count of good matches for this masksize
        resolutions[resolution_idx] = rects; // Store count at index masksize - 1

        // Draw detected rectangles onto the card_mask (for visualization/masking later)
        // Draw white filled rectangles (255) on the CV_8U mask
        for (const auto &pt: points) {
            // Check bounds before drawing rectangle
            if (pt.x >= 0 && pt.y >= 0 && pt.x + w <= card_mask.cols &&
                pt.y + h <= card_mask.rows) {
                cv::rectangle(card_mask, pt, cv::Point(pt.x + w, pt.y + h), cv::Scalar(255),
                              -1); // Draw filled white rectangle
            } else {
                fprintf(stderr,
                        "decensor warning: Drawing rectangle out of bounds at (%d, %d) with size (%d, %d) for masksize %d. Skipping draw.\n",
                        pt.x, pt.y, w, h, masksize);
            }
        }
    }
    fprintf(stderr, "decensor: Template matching complete.\n");


    // --- 3. Calculating Dominant Mosaic Resolution ---

    // Check if any mosaic region was detected based on the populated resolutions vector
    // We check indices [LowRange + 1, HighRange + 1] which are where counts are stored.
    bool mosaic_detected_any_size = false;
    for (int i = LowRange + 1; i <= HighRange + 1; ++i) {
        if (i < resolutions.size() && resolutions[i] > 0) {
            mosaic_detected_any_size = true;
            break;
        }
    }

    if (!mosaic_detected_any_size) {
        // Also check the card_mask just in case findNonZero missed something, though unlikely
        if (cv::countNonZero(card_mask) == 0) {
            fprintf(stderr, "decensor: No mosaic regions detected. Copying input to output.\n");
            inimage.copyTo(outimage);
            return -1; // Indicate no mosaic found
        }
        // If mask has non-zero but resolutions don't, something unexpected happened.
        // Proceed anyway, maybe a very large pattern matched the whole image?
        fprintf(stderr,
                "decensor warning: card_mask has non-zero pixels, but no matches recorded in resolutions [LowRange+1, HighRange+1]. Proceeding with default resolution.\n");
    }

    fprintf(stderr, "decensor: Calculating resolution...\n");
    // Debugging: Print populated resolutions vector segment
    fprintf(stderr, "decensor: Resolutions counts (indices %d to %d): [", LowRange + 1,
            HighRange + 1);
    for (int i = LowRange + 1; i <= HighRange + 1; ++i) {
        if (i < resolutions.size()) { // Safety check
            fprintf(stderr, "%d%s", resolutions[i], i == HighRange + 1 ? "" : ", ");
        }
    }
    fprintf(stderr, "]\n");


    // Find local minima indices to define groups.
    // Matching detectMosaicResolution: find indices i where resolutions[i] < resolutions[i-1] AND resolutions[i] <= resolutions[i+1]
    // Loop range for finding minima is indices [1, resolutions.size() - 2], which is [1, HighRange + 1].
    // Add LowRange (index 2) and HighRange+2 (index HighRange+2) as boundary extrema.
    std::set<int> extrema_indices_set; // Use set to handle duplicates and keep sorted

    // Add boundary extrema as in detectMosaicResolution's final extrema list
    extrema_indices_set.insert(LowRange); // Index 2
    extrema_indices_set.insert(HighRange + 2); // Index HighRange + 2

    // Find true local minima within indices [1, HighRange + 1]
    for (int i = 1; i <= HighRange + 1; ++i) {
        // Ensure indices i-1, i, i+1 are valid
        if (i > 0 && i < resolutions.size() - 1) {
            // Strict less than left, less than or equal to right (matching detectMosaicResolution)
            if (resolutions[i] < resolutions[i - 1] && resolutions[i] <= resolutions[i + 1]) {
                extrema_indices_set.insert(i);
            }
        }
    }

    // Convert set to vector for easier indexing of groups
    std::vector<int> extrema_indices(extrema_indices_set.begin(), extrema_indices_set.end());

    // Debugging: Print final extrema indices
    fprintf(stderr, "decensor: Final Extrema indices defining groups: [");
    for (size_t i = 0; i < extrema_indices.size(); ++i) {
        fprintf(stderr, "%d%s", extrema_indices[i], i == extrema_indices.size() - 1 ? "" : ", ");
    }
    fprintf(stderr, "]\n");


    // Find the "biggest extrema group"
    int MosaicResolutionOfImage = HighRange + 1; // Default value
    int best_group_sum_score = -1; // Stores sum + int(sum*0.05)
    int best_group_max_val_score = -1; // Stores max_val + int(max_val*0.15)
    int best_original_index_of_max = -1; // The index in `resolutions` where the max count of the best group was found

    if (extrema_indices.size() < 2) {
        fprintf(stderr,
                "decensor: Not enough extrema points (%zu) to form groups. Cannot calculate resolution reliably. Using default HighRange + 1 (%d).\n",
                extrema_indices.size(), HighRange + 1);
        // Keep default resolution HighRange + 1
    } else {
        // Iterate through pairs of extrema indices to define groups [start_res_idx, end_res_idx] inclusive
        for (size_t i = 0; i < extrema_indices.size() - 1; ++i) {
            int group_start_res_index = extrema_indices[i];
            int group_end_res_index = extrema_indices[i + 1]; // Inclusive range [start, end]

            // Check bounds for group indices
            if (group_start_res_index < 0 || group_start_res_index >= (int) resolutions.size() ||
                group_end_res_index < 0 || group_end_res_index >= (int) resolutions.size() ||
                group_start_res_index > group_end_res_index) {
                fprintf(stderr,
                        "decensor ERROR: Invalid extrema group indices [%d, %d] for resolutions size %zu. Skipping group.\n",
                        group_start_res_index, group_end_res_index, resolutions.size());
                continue;
            }

            int current_group_sum = 0;
            int current_max_val_in_group = -1;
            int current_original_index_of_max_in_group = -1; // Index in `resolutions` for max of this group

            // Calculate sum and find max value + its original index within this group range
            for (int res_idx = group_start_res_index; res_idx <= group_end_res_index; ++res_idx) {
                current_group_sum += resolutions[res_idx];
                if (current_max_val_in_group == -1 ||
                    resolutions[res_idx] > current_max_val_in_group) {
                    current_max_val_in_group = resolutions[res_idx];
                    current_original_index_of_max_in_group = res_idx;
                } else if (resolutions[res_idx] == current_max_val_in_group) {
                    // Tie-breaker for max value within the group: prefer smaller index
                    if (current_original_index_of_max_in_group == -1 ||
                        res_idx < current_original_index_of_max_in_group) {
                        current_original_index_of_max_in_group = res_idx;
                    }
                }
            }

            // If the current group has no positive matches, skip it
            if (current_max_val_in_group <= 0) {
                // fprintf(stderr, "decensor: Skipping group [%d, %d] with no matches.\n", group_start_res_index, group_end_res_index);
                continue;
            }

            // Calculate scores based on Python's logic
            int current_sum_score = current_group_sum + static_cast<int>(current_group_sum * 0.05);
            int current_max_val_score =
                    current_max_val_in_group + static_cast<int>(current_max_val_in_group * 0.15);

            // Compare current group against the best found so far
            bool update_best = false;
            if (best_original_index_of_max == -1) { // First valid group
                update_best = true;
            } else {
                if (current_sum_score > best_group_sum_score) {
                    update_best = true;
                } else if (current_sum_score == best_group_sum_score) {
                    if (current_max_val_score > best_group_max_val_score) {
                        update_best = true;
                    } else if (current_max_val_score == best_group_max_val_score) {
                        // Tie-breaker: prefer group whose peak (max value) is at a smaller index
                        if (current_original_index_of_max_in_group < best_original_index_of_max) {
                            update_best = true;
                        }
                    }
                }
            }

            if (update_best) {
                best_group_sum_score = current_sum_score;
                best_group_max_val_score = current_max_val_score;
                best_original_index_of_max = current_original_index_of_max_in_group;
                // Debugging:
                // fprintf(stderr, "decensor: New best group found. Peak at res_idx %d (masksize %d). Sum Score %d, Max Score %d.\n",
                //         best_original_index_of_max, best_original_index_of_max + 1, best_group_sum_score, best_max_val_score);
            }
        } // End loop through extrema groups

        // Determine the final mosaic resolution from the best group's peak index
        if (best_original_index_of_max != -1) {
            // The index in `resolutions` corresponds to masksize = index + 1.
            // The index range for counts is [LowRange+1, HighRange+1].
            // So the masksize range is [LowRange+2, HighRange+2].
            MosaicResolutionOfImage = best_original_index_of_max + 1;

            // Python's final check: if MosaicResolutionOfImage == 0, set to HighRange+1.
            // Given our logic, it should be >= LowRange+2 if best_original_index_of_max != -1.
            // But keep check for safety/matching Python state.
            if (MosaicResolutionOfImage == 0) {
                fprintf(stderr,
                        "decensor: Calculated MosaicResolutionOfImage was unexpectedly 0. Setting to default HighRange + 1 (%d).\n",
                        HighRange + 1);
                MosaicResolutionOfImage = HighRange + 1;
            }

        } else {
            // No valid group found (e.g., no matches above threshold). Use default.
            fprintf(stderr,
                    "decensor: No clear best group found during resolution calculation. Using default HighRange + 1 (%d).\n",
                    HighRange + 1);
            // MosaicResolutionOfImage is already initialized to HighRange + 1
        }
    } // End if extrema_indices.size() >= 2

    fprintf(stderr, "decensor: Detected Mosaic Resolution is: %d\n", MosaicResolutionOfImage);


    // --- 4. ESRGAN Processing ---
    // This part uses the detected MosaicResolutionOfImage.
    // Keeping the original decensor's approach of downscaling the whole image,
    // processing, upscaling, and blending based on the detected mask.
    // The 'process_only_mosaic_region' logic from the original prompt was removed
    // in your initial MNNSR::decensor, so we stick to the full image downscale approach.

    cv::Mat processed_region_esr; // Will hold the ESRGAN output resized to the target area size

    // Calculate downscale factor and number of SR loops
    int loops = std::round(std::log(MosaicResolutionOfImage) / std::log(scale));
    loops = std::max(1, loops); // Ensure at least one SR pass if mosaic is detected
    float total_model_upscale = std::pow(scale, loops);
    float pre_scale = total_model_upscale * 1.1 >= MosaicResolutionOfImage
                      ? 1.0f / MosaicResolutionOfImage
                      : sqrt(MosaicResolutionOfImage / total_model_upscale) /
                        MosaicResolutionOfImage;

    int Sx = static_cast<int>(inimage.cols * pre_scale);
    int Sy = static_cast<int>(inimage.rows * pre_scale);

    // Ensure Sx, Sy are at least 1
    Sx = std::max(1, Sx);
    Sy = std::max(1, Sy);

    fprintf(stderr,
            "decensor: Resizing full input to (%d, %d) for processing. pre_scale=%.3f, loops=%d\n",
            Sx, Sy, pre_scale, loops);

    cv::Mat shrinkedI;
    // Use INTER_AREA for downsampling, INTER_CUBIC for upsampling (later resize)
    cv::resize(inimage, shrinkedI, cv::Size(Sx, Sy), 0, 0, cv::INTER_AREA);

    cv::Mat esr_output_shrunken_scaled = shrinkedI; // Start the loop with the downscaled image

    for (int i = 0; i < loops; i++) {
        fprintf(stderr, "decensor: processing SR loop %d/%d, input: %d*%d\n",
                i + 1, loops, esr_output_shrunken_scaled.rows, esr_output_shrunken_scaled.cols);

        // Prepare output buffer for the current SR pass
        cv::Mat current_esr_output(esr_output_shrunken_scaled.rows * scale,
                                   esr_output_shrunken_scaled.cols * scale,
                                   CV_8UC3);

        // Call MNNSR::process on the intermediate result
        // We pass the detected card_mask. The process function uses it to skip tiles.
        if (process(esr_output_shrunken_scaled, current_esr_output, card_mask) != 0) {
            fprintf(stderr, "decensor error: MNNSR::process failed during SR loop %d.\n", i + 1);
            inimage.copyTo(outimage); // Fallback
            return -1;
        }

        // The output of this pass becomes the input for the next pass
        // or the final result if this is the last pass.
        esr_output_shrunken_scaled = current_esr_output;
    }

    fprintf(stderr, "decensor: Combining processed image with original...\n");

    // The final result `esr_output_shrunken_scaled` is now total_model_upscale times
    // the initial `shrinkedI` size. We need to resize it back to the original image size.
    // Use INTER_CUBIC for upsampling or potentially INTER_LANCZOS4 for better quality if needed.
    cv::resize(esr_output_shrunken_scaled, processed_region_esr, inimage.size(), 0, 0,
               cv::INTER_CUBIC);

    // Combine the processed ESRGAN output with the original image using the full card_mask
    // Regions in card_mask that are 255 will take pixels from processed_region_esr.
    // Regions in card_mask that are 0 will keep pixels from the original image.
    outimage = inimage.clone(); // Start with the original image content

    // Ensure mask is strictly 0 or 255 if needed by copyTo, though CV_8U mask usually works.
    // cv::Mat binary_mask;
    // cv::threshold(card_mask, binary_mask, 1, 255, cv::THRESH_BINARY);

    // Copy ESRGAN output pixels onto outimage where card_mask is non-zero (255)
    if (processed_region_esr.channels() == outimage.channels()) {
        // Use the mask to only copy pixels in the detected mosaic regions
        processed_region_esr.copyTo(outimage, card_mask);
    } else {
        fprintf(stderr,
                "decensor warning: Channel mismatch (%d vs %d) between processed region and output image. copyTo with mask may not work as expected. Falling back to simple copy (entire image replaced).\n",
                processed_region_esr.channels(), outimage.channels());
        // Fallback: simple copy which replaces the entire image - not ideal for blending
        processed_region_esr.copyTo(outimage);
    }

    // Handle Alpha channel if original image had one
    if (original_channels == 4) {
        // Split the original BGRA image
        std::vector<cv::Mat> original_channels_split(4);
        cv::split(inimage, original_channels_split);
        cv::Mat alpha_channel = original_channels_split[3]; // The alpha channel

        // Convert the output BGR image to BGRA
        cv::cvtColor(outimage, outimage, cv::COLOR_BGR2BGRA);

        // Copy the original alpha channel to the output BGRA image's alpha channel
        std::vector<cv::Mat> output_channels_split(4);
        cv::split(outimage, output_channels_split);
        alpha_channel.copyTo(output_channels_split[3]);
        cv::merge(output_channels_split, outimage);
    }


    fprintf(stderr, "decensor: Processing complete.\n");
    return 0; // Success
}