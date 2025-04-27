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



// ----------------------------------------------------------------
// New function to implement mosaic detection and decensoring
// ----------------------------------------------------------------

/**
 * @brief Detects and removes mosaic censorship from an image.
 *
 * @param inimage Input image (BGR or BGRA).
 * @param outimage Output image with mosaic removed (same format as input).
 * @param process_only_mosaic_region If true, process only the bounding box
 *   of detected mosaic regions to save computation. If false, process
 *   a downscaled version of the entire image. Defaults to true.
 * @return 0 on success, -1 on failure or if no mosaic was detected.
 */
int MNNSR::decensor(const cv::Mat &inimage, cv::Mat &outimage) {
    if (inimage.empty()) {
        fprintf(stderr, "decensor error: Input image is empty.\n");
        return -1;
    }

    // --- Constants (matching Python script logic/tweaks) ---
    const int GBlur = 5;
    const int CannyTr1 = 8;
    const int CannyTr2 = 30;
    const int LowRange = 2;
    const int HighRange = 25;
    const float DetectionTr = 0.29f;
    const int PatternBorder = 2;
    const int PatternLineThickness = 1;

    // --- 1. Preprocessing for Detection ---
    cv::Mat img_rgb;
    cv::Mat img_gray;
    int original_channels = inimage.channels();

    if (original_channels == 4) {
        // Convert BGRA to BGR for processing, keep alpha aside
        cv::cvtColor(inimage, img_rgb, cv::COLOR_BGRA2BGR);
    } else if (original_channels == 3) {
        img_rgb = inimage.clone(); // Assuming BGR input
    } else {
        fprintf(stderr,
                "decensor error: Unsupported number of channels (%d). Input must be 3 (BGR) or 4 (BGRA).\n",
                original_channels);
        return -1;
    }

    cv::cvtColor(img_rgb, img_gray, cv::COLOR_BGR2GRAY);
    cv::Mat edges;
    cv::Canny(img_gray, edges, CannyTr1, CannyTr2);
    cv::Mat inverted_edges;
    cv::bitwise_not(edges, inverted_edges);
    cv::Mat blurred_edges;
    cv::GaussianBlur(inverted_edges, blurred_edges, cv::Size(GBlur, GBlur), 0);

    // --- 2. Pattern Generation and Matching ---
    std::vector<cv::Mat> pattern_mats;
    // Patterns for mask sizes from LowRange to HighRange+2
    for (int masksize = LowRange; masksize <= HighRange + 2; ++masksize) {
        int cell_size = masksize - 1;
        // Pattern size: 2*Border + 3*CellSize + 2*LineThickness (for 3x3 cells)
        int pattern_size = 2 * PatternBorder + 3 * cell_size + 2 * PatternLineThickness;
        if (pattern_size <= 0) { // Prevent issues with very small masksize
            pattern_mats.push_back(cv::Mat()); // Add empty placeholder
            continue;
        }

        cv::Mat pattern = cv::Mat(pattern_size, pattern_size, CV_8U,
                                  cv::Scalar(255)); // White background
        int line_pos1 = PatternBorder;
        int line_pos2 = PatternBorder + cell_size + PatternLineThickness;
        int line_pos3 = PatternBorder + 2 * (cell_size + PatternLineThickness);

        // Draw horizontal lines
        if (line_pos1 < pattern_size)
            cv::line(pattern, cv::Point(0, line_pos1), cv::Point(pattern_size - 1, line_pos1),
                     cv::Scalar(0), PatternLineThickness);
        if (line_pos2 < pattern_size)
            cv::line(pattern, cv::Point(0, line_pos2), cv::Point(pattern_size - 1, line_pos2),
                     cv::Scalar(0), PatternLineThickness);
        if (line_pos3 < pattern_size)
            cv::line(pattern, cv::Point(0, line_pos3), cv::Point(pattern_size - 1, line_pos3),
                     cv::Scalar(0), PatternLineThickness);

        // Draw vertical lines
        if (line_pos1 < pattern_size)
            cv::line(pattern, cv::Point(line_pos1, 0), cv::Point(line_pos1, pattern_size - 1),
                     cv::Scalar(0), PatternLineThickness);
        if (line_pos2 < pattern_size)
            cv::line(pattern, cv::Point(line_pos2, 0), cv::Point(line_pos2, pattern_size - 1),
                     cv::Scalar(0), PatternLineThickness);
        if (line_pos3 < pattern_size)
            cv::line(pattern, cv::Point(line_pos3, 0), cv::Point(line_pos3, pattern_size - 1),
                     cv::Scalar(0), PatternLineThickness);

        pattern_mats.push_back(pattern);
    }

    cv::Mat card_mask = cv::Mat::zeros(inimage.size(),
                                       CV_8U); // Mask to mark detected mosaic regions (255 = mosaic, 0 = non-mosaic)
    std::vector<int> resolutions(HighRange + 3,
                                 0); // Counts for sizes LowRange..HighRange+2. resolutions[s] stores count for size s.

    // Python loop is from HighRange+2 down to LowRange+1
    for (int masksize = HighRange + 2; masksize >= LowRange + 1; --masksize) {
        int pattern_idx = masksize - LowRange; // Index in pattern_mats vector (0-based)
        if (pattern_idx < 0 || pattern_idx >= pattern_mats.size() ||
            pattern_mats[pattern_idx].empty()) {
            continue; // Skip if pattern not generated or index out of bounds
        }

        cv::Mat template_mat = pattern_mats[pattern_idx];
        int w = template_mat.cols;
        int h = template_mat.rows;

        if (w > blurred_edges.cols || h > blurred_edges.rows) {
            fprintf(stderr,
                    "decensor warning: Pattern size (%d, %d) larger than image (%d, %d) for masksize %d. Skipping.\n",
                    w, h, blurred_edges.cols, blurred_edges.rows, masksize);
            continue;
        }

        cv::Mat img_detection_result;
        cv::matchTemplate(blurred_edges, template_mat, img_detection_result, cv::TM_CCOEFF_NORMED);

        // Find locations above threshold
        cv::Mat thresholded_detection;
        cv::threshold(img_detection_result, thresholded_detection, DetectionTr, 1.0,
                      cv::THRESH_BINARY);

        std::vector<cv::Point> locations;
        cv::findNonZero(thresholded_detection, locations);

        resolutions[masksize] = locations.size(); // Store count for this masksize

        // Draw detected rectangles onto the mask
        for (const auto &pt: locations) {
            cv::rectangle(card_mask, pt, cv::Point(pt.x + w, pt.y + h), cv::Scalar(255),
                          -1); // Draw filled white rectangle
        }
    }

    // Check if any mosaic region was detected
    if (cv::countNonZero(card_mask) == 0) {
        fprintf(stderr, "decensor: No mosaic regions detected. Copying input to output.\n");
        inimage.copyTo(outimage);
        return -1; // Indicate no mosaic found
    }

    // --- 3. Calculating Dominant Mosaic Resolution ---
    // Replicate Python's argrelextrema and grouping logic
    std::vector<int> extremaMIN_indices;
    // Find local minima in the relevant range of resolutions
    // Loop indices from LowRange+1 to HighRange+1 (corresponds to sizes LowRange+1 to HighRange+1)
    // Need at least 3 points to check local minimum: i-1, i, i+1
    for (int i = LowRange + 1; i <= HighRange + 1; ++i) {
        // Check bounds: i-1, i, i+1 must be valid indices
        if (i > 0 && i < resolutions.size() - 1) {
            if (resolutions[i] < resolutions[i - 1] && resolutions[i] < resolutions[i + 1]) {
                extremaMIN_indices.push_back(i); // Store the index (which corresponds to the size)
            }
        }
    }

    // Add boundary indices (sizes)
    std::vector<int> group_indices;
    group_indices.push_back(LowRange); // Start of the first group (size LowRange)
    group_indices.insert(group_indices.end(), extremaMIN_indices.begin(), extremaMIN_indices.end());
    group_indices.push_back(HighRange + 2); // End of the last group (size HighRange+2)

    std::vector<std::pair<int, std::vector<int>>> Extremas; // Pair: {start_size_of_group, vector_of_counts_in_group}
    for (size_t i = 0; i < group_indices.size() - 1; ++i) {
        int start_size = group_indices[i];
        int end_size = group_indices[i +
                                     1]; // Python uses end_size + 1, meaning inclusive. Let's match.
        std::vector<int> current_group_counts;
        // Collect counts for sizes from start_size to end_size inclusive
        for (int s = start_size; s <= end_size; ++s) {
            if (s >= 0 && s < resolutions.size()) { // Check bounds for resolutions vector
                current_group_counts.push_back(resolutions[s]);
            } else {
                current_group_counts.push_back(0); // Treat out of bounds as 0 count
            }
        }
        if (!current_group_counts.empty()) {
            Extremas.push_back({start_size, current_group_counts});
        }
    }

    long long BigExtremaSum = -1;
    int BigExtremaGroupStartSize = -1;
    int max_count_in_BigExtremaGroup = -1;
    int index_of_max_count_in_BigExtremaGroup = -1; // Index relative to group_indices[i]

    for (const auto &group: Extremas) {
        long long current_sum = 0;
        int current_max_count = -1;
        int current_max_idx = -1;

        for (size_t i = 0; i < group.second.size(); ++i) {
            current_sum += group.second[i];
            if (group.second[i] > current_max_count) {
                current_max_count = group.second[i];
                current_max_idx = i;
            }
        }

        // Python logic: 5% preference for smaller resolution group (by sum)
        // Max count comparison with 15% preference for group with higher max count
        bool choose_current = false;
        if (BigExtremaSum == -1 ||
            (current_sum + static_cast<long long>(current_sum * 0.05)) >= BigExtremaSum) {
            if (max_count_in_BigExtremaGroup == -1 ||
                (current_max_count + static_cast<int>(current_max_count * 0.15)) >=
                max_count_in_BigExtremaGroup) {
                choose_current = true;
            }
        }

        if (choose_current) {
            BigExtremaSum = current_sum;
            BigExtremaGroupStartSize = group.first;
            max_count_in_BigExtremaGroup = current_max_count;
            index_of_max_count_in_BigExtremaGroup = current_max_idx;
        }
    }

    int MosaicResolutionOfImage = HighRange + 1; // Default if no good extrema found
    if (BigExtremaGroupStartSize != -1 && index_of_max_count_in_BigExtremaGroup != -1) {
        // The resolution is the start size of the chosen group + the index of the max count within that group
        MosaicResolutionOfImage = BigExtremaGroupStartSize + index_of_max_count_in_BigExtremaGroup;
    }

    // Python default if MosaicResolutionOfImage is 0 (shouldn't happen with our default)
    // if (MosaicResolutionOfImage == 0) MosaicResolutionOfImage = HighRange + 1; // Redundant with our default initialization

    fprintf(stderr, "decensor: Detected Mosaic Resolution is: %d\n", MosaicResolutionOfImage);

    // --- 4. ESRGAN Processing ---
    cv::Mat processed_region_esr; // Will hold the ESRGAN output resized to the target area size

    {

        int loops = std::round(std::log(MosaicResolutionOfImage) / std::log(scale));
        float pre_scale = (std::pow(scale, loops) * 1.1 >= MosaicResolutionOfImage) ? 1.0f /
                                                                                      MosaicResolutionOfImage
                                                                                    :
                          sqrt(MosaicResolutionOfImage / (std::pow(scale, loops))) /
                          MosaicResolutionOfImage;
        // Standard path: Process a downscaled version of the entire image
        int Sx = static_cast<int>( inimage.cols * pre_scale);
        int Sy = static_cast<int>( inimage.rows * pre_scale);

        // Ensure Sx, Sy are at least 1
        Sx = std::max(1, Sx);
        Sy = std::max(1, Sy);

        fprintf(stderr,
                "decensor: Resizing full input to (%d, %d) for processing. pre_scale=%.3f, loops=%d\n",
                Sx, Sy, pre_scale, loops);

        cv::Mat shrinkedI;
        cv::resize(inimage, shrinkedI, cv::Size(Sx, Sy), 0, 0, cv::INTER_AREA);
//        outimage = cv::Mat(inimage.rows , inimage.cols , CV_8UC3);
        cv::Mat esr_output_shrunken_scaled = cv::Mat(shrinkedI.rows * scale, shrinkedI.cols * scale,
                                                     CV_8UC3);
        for (int i = 0; i < loops; i++) {
            fprintf(stderr, "decensor: processing loop %d/%d, %d*%d -> %d*%d\n", i, loops,
                    shrinkedI.rows, shrinkedI.cols, esr_output_shrunken_scaled.rows,
                    esr_output_shrunken_scaled.cols);

            if (process(shrinkedI, esr_output_shrunken_scaled, card_mask) != 0) {
                fprintf(stderr, "decensor error: MNNSR::process failed for full image.\n");
                inimage.copyTo(outimage); // Fallback
                return -1;
            }

            esr_output_shrunken_scaled.copyTo(shrinkedI);
//            fprintf(stderr, "decensor: processing loop %d/%d done, %d*%d & %d*%d\n", i,loops,shrinkedI.rows, shrinkedI.cols, esr_output_shrunken_scaled.rows, esr_output_shrunken_scaled.cols);

            esr_output_shrunken_scaled = cv::Mat(shrinkedI.rows * scale, shrinkedI.cols * scale,
                                                 CV_8UC3);
//            esr_output_shrunken_scaled.resize((shrinkedI.rows * scale, shrinkedI.cols * scale),scale);
//            fprintf(stderr, "decensor: processing loop %d/%d resize, %d*%d & %d*%d\n", i,loops,shrinkedI.rows, shrinkedI.cols, esr_output_shrunken_scaled.rows, esr_output_shrunken_scaled.cols);

        }

        fprintf(stderr, "decensor: combine\n");

        // Resize the scaled output back to the original full image size
        cv::resize(shrinkedI, processed_region_esr, inimage.size(), 0, 0,
                   cv::INTER_AREA);

        // Combine the processed ESRGAN output with the original image using the full card_mask
        outimage = inimage.clone(); // Start with the original image

        cv::Mat full_inv_mask; // Mask where mosaic is detected (255=mosaic, 0=not)
        cv::threshold(card_mask, full_inv_mask, 1, 255,
                      cv::THRESH_BINARY); // Ensure mask is binary 0/255

        // Copy ESRGAN output pixels onto outimage where full_inv_mask is non-zero
        if (processed_region_esr.channels() == outimage.channels()) {
            processed_region_esr.copyTo(outimage, full_inv_mask);
        } else {
            fprintf(stderr,
                    "decensor warning: Channel mismatch (%d vs %d) between processed region and output image. copyTo with mask may not work as expected.\n",
                    processed_region_esr.channels(), outimage.channels());
            // Fallback: simple copy which ignores the mask - not ideal
            processed_region_esr.copyTo(outimage); // This replaces the entire image
        }

    }

    fprintf(stderr, "decensor: Processing complete.\n");
    return 0; // Success
}
