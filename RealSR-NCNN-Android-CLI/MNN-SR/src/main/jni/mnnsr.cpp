//
// Created by Yazii on 2025/3/16.
//
#include "mnnsr.h"
#include "utils.hpp"
#include <thread>

#include "MNN/ErrorCode.hpp"

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

    fprintf(stderr, "set backend: %s, color type: %s\n", get_backend_name(config.type).c_str(), colorTypeToStr(color));

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
        if (color ==  RGB) {
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

int MNNSR::process(const cv::Mat &inimage, cv::Mat &outimage) {
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
                double time_span = duration_cast<duration<double>>(end - begin).count();
                fprintf(stderr, "%5.2f%%\t[%5.2fs /%5.2f ETA]\n", progress * 100, time_span,
                        time_span / progress - time_span);
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