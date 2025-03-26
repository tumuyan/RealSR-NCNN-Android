//
// Created by Yazii on 2025/3/16.
//
#include "mnnsr.h"
#include <thread>

#include "MNN/ErrorCode.hpp"

MNNSR::MNNSR() {
    pretreat_ = std::shared_ptr<MNN::CV::ImageProcess>(
            MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::RGB, nullptr, 0, normVals_, 3));

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

int MNNSR::load(const std::wstring& modelpath)
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
//    config.type = MNN_FORWARD_AUTO;
    config.type = backend_type;
    config.backupType = MNN_FORWARD_VULKAN;
//    config.backupType = MNN_FORWARD_AUTO;
    int num_threads = std::thread::hardware_concurrency();
    if (num_threads < 1)
        num_threads = 2;
    config.numThread = num_threads;
/*
    switch (backend_type) {
        case MNN_FORWARD_CPU:
            fprintf(stderr, "backend: cpu, numThread=%d\n", config.numThread);
            break;
        case MNN_FORWARD_OPENCL:
            fprintf(stderr, "backend: opencl\n");
            break;
        case MNN_FORWARD_OPENGL:
            fprintf(stderr, "backend: opengl\n");
            break;
        case MNN_FORWARD_VULKAN:
            fprintf(stderr, "backend: vulkan\n");
            break;
        case MNN_FORWARD_METAL:
            fprintf(stderr, "backend: metal\n");
            break;
        case MNN_FORWARD_CUDA:
            fprintf(stderr, "backend: cuda\n");
            break;
        case MNN_FORWARD_NN:
            fprintf(stderr, "backend: nn\n");
            break;
        default:
            fprintf(stderr, "backend: %d\n", backend_type);
    }*/
    const auto start = std::chrono::high_resolution_clock::now();

#if _WIN32
    interpreter = MNN::Interpreter::createFromFile(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(modelpath).c_str());
#else
    interpreter = MNN::Interpreter::createFromFile((modelpath).c_str());
#endif


    if (interpreter == nullptr) {
        fprintf(stderr, "interpreter null\n");
    }

//    if (cachemodel) {
//        std::string cachefile = modelpath + ".cache";
//        interpreter->setCacheFile(cachefile.c_str());
//    }

    session = interpreter->createSession(config);
    if (session == nullptr) {
        fprintf(stderr, "session null\n");
    }


    interpreter_input = interpreter->getSessionInput(session, nullptr);
    interpreter->resizeTensor(interpreter_input, 1, 3, tilesize, tilesize);
    interpreter->resizeSession(session);
    interpreter_output = interpreter->getSessionOutput(session, nullptr);

    input_tensor = new MNN::Tensor(interpreter_input, MNN::Tensor::CAFFE);
    output_tensor = new MNN::Tensor(interpreter_output, MNN::Tensor::CAFFE);

    input_buffer = input_tensor->host<float>();
    output_buffer = output_tensor->host<float>();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start);
    fprintf(stderr, "Load model %.3f ms\n", static_cast<double>(duration.count()));

//    if (cachemodel)
//        interpreter->updateCacheFile(session);
    return 0;
}


cv::Mat MNNSR::TensorToCvMat(void) {
    // 确保数据在主机内存中
    interpreter_output->copyToHostTensor(output_tensor);

    // 获取 Tensor 维度信息（假设为 NCHW）
    int C = output_tensor->channel();  // 通道数
    int H = output_tensor->height();
    int W = output_tensor->width();

    float *data = output_tensor->host<float>();

//    fprintf(stderr, "TensorToCvMat: %d/%d/%d\n", C, H, W);

    cv::Mat r(H, W, CV_32FC1, data + 0 * H * W); // 红色通道
    cv::Mat g(H, W, CV_32FC1, data + 1 * H * W); // 绿色通道
    cv::Mat b(H, W, CV_32FC1, data + 2 * H * W); // 蓝色通道

    // 合并为 HWC 格式的 Mat
    std::vector<cv::Mat> channels{b, g, r};
    cv::Mat merged;
    cv::merge(channels, merged); // 合并后维度为 [H, W, 3]
    return merged;
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

    if (inWidth % tileWidth < prepadding) {
        // 倒数第2个tile的prepadding已经包含了推理结果
        xtiles--;
    } else {
        if (inWidth % tileWidth >> 1 <= prepadding)
            xtiles--;
        // xtiles * (tilesize - 2 * xPrepadding) + xPrepadding = inWidth
        xPrepadding = (xtiles * tilesize - inWidth) / (2 * xtiles - 1);
        tileWidth = tilesize - xPrepadding * 2;
    }

    if (inHeight % tileHeight < prepadding) {
        // 倒数第2个tile的prepadding已经包含了推理结果
        ytiles--;
    } else {
        if (inHeight % tileHeight >> 1 <= prepadding)
            ytiles--;
        // ytiles * (tilesize - 2 * yPrepadding) + yPrepadding = inHeight
        yPrepadding = (ytiles * tilesize - inHeight) / (2 * ytiles - 1);
        tileHeight = tilesize - yPrepadding * 2;
    }

//    fprintf(stderr,
//            "process tiles: %d %d, tilesize: %d -> %d %d, prepadding: %d -> %d %d\n",
//            xtiles, ytiles, tilesize, tileWidth, tileHeight, prepadding, xPrepadding, yPrepadding);

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

            if (inputTile.cols < tilesize || inputTile.rows < tilesize) {
                int t = (yi == 0) ? yPrepadding : 0;
                int b = tilesize + in_tile_y0 - in_tile_y1 - t;
                int l = (xi == 0) ? xPrepadding : 0;
                int r = tilesize + in_tile_x0 - in_tile_x1 - l;

//                fprintf(stderr, "process y=%d, x=%d copyMakeBorder %d %d %d %d\n", yi, xi, t, b, l,
//                        r);
                cv::Mat paddedTile;
                cv::copyMakeBorder(inputTile, paddedTile, t, b, l, r, cv::BORDER_CONSTANT);

                pretreat_->convert(paddedTile.data, paddedTile.cols, paddedTile.rows,
                                   paddedTile.cols * paddedTile.channels(),
                                   input_tensor);

            } else {
                cv::Mat paddedTile;
                cv::copyMakeBorder(inputTile, paddedTile, 0, 0, 0, 0, cv::BORDER_CONSTANT);

                pretreat_->convert(paddedTile.data, paddedTile.cols, paddedTile.rows,
                                   paddedTile.cols * paddedTile.channels(),
                                   input_tensor);
            }

            bool r = interpreter_input->copyFromHostTensor(input_tensor);

            interpreter->runSession(session);
            cv::Mat outputTile = TensorToCvMat();
            cv::Rect cropRect(out_tile_x0, out_tile_y0, out_tile_w, out_tile_h);
            cv::Mat croppedTile = outputTile(cropRect);

//            fprintf(stderr, "process inputTile y=%d, x=%d, copyTo outimage\n", yi, xi,
//                    inputTile.cols, inputTile.rows, inputTile.channels());
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


    return 0;
}

