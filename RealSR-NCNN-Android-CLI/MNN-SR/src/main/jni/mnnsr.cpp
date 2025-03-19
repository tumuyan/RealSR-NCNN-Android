//
// Created by Yazii on 2025/3/16.
//
#include "mnnsr.h"
#include <thread>

//#include <math.h>
MNNSR::MNNSR(int gpuid, bool tta_mode, int num_threads) {

    pretreat_ = std::shared_ptr<MNN::CV::ImageProcess>(
            MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::RGB, meanVals_, 3, normVals_, 3));

}

MNNSR::~MNNSR() {


}

#if _WIN32
#include <algorithm>
#include <string>
#include <codecvt>
#include <cstring>
std::string wstringToString(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
}

int MNNSR::load(const std::wstring& modelpath)
#else

int MNNSR::load(const std::string &modelpath)
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
    config.type = backend_type;
//    config.type = MNN_FORWARD_AUTO;
//    config.backupType = MNN_FORWARD_OPENCL;
    config.backupType = MNN_FORWARD_VULKAN;
//    config.backupType = MNN_FORWARD_AUTO;
    config.numThread = std::thread::hardware_concurrency();

    const auto start = std::chrono::high_resolution_clock::now();


    //interpreter = MNN::Interpreter::createFromFile(modelpath.c_str());
#if _WIN32
    interpreter = MNN::Interpreter::createFromFile(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(modelpath).c_str());
#else
    interpreter = MNN::Interpreter::createFromFile(modelpath.c_str());
#endif
    session = interpreter->createSession(config);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start);

    fprintf(stderr, "Load model %.3f ms\n",
            static_cast<double>(duration.count()));

    interpreter_input = interpreter->getSessionInput(session, nullptr);
    interpreter->resizeTensor(interpreter_input, 1, 3, tilesize, tilesize);
    interpreter->resizeSession(session);
    interpreter_output = interpreter->getSessionOutput(session, nullptr);

    input_tensor = new MNN::Tensor(interpreter_input, MNN::Tensor::CAFFE);
    output_tensor = new MNN::Tensor(interpreter_output, MNN::Tensor::CAFFE);

    input_buffer = input_tensor->host<float>();
    output_buffer = output_tensor->host<float>();
    return 0;
}

int MNNSR::process(const cv::Mat &inimage, cv::Mat &outimage) {
    int inWidth = inimage.cols;
    int inHeight = inimage.rows;

    int outWidth = inWidth * scale;
    int outHeight = inHeight * scale;

    int tileWidth = tilesize - prepadding;
    int tileHeight = tilesize - prepadding;

    int xtiles = (inWidth + tileWidth - 1) / tileWidth;
    int ytiles = (inHeight + tileHeight - 1) / tileHeight;

    high_resolution_clock::time_point begin = high_resolution_clock::now();
    high_resolution_clock::time_point time_print_progress;

    cv::Mat imageOut(outHeight, outWidth, inimage.type()); // 填充灰色背景

    for (uint yi = 0; yi < ytiles; yi++) {
        int in_tile_y0 = (yi == 0) ? 0 : yi * tileHeight;
        int in_tile_y1 = (yi + 1 == ytiles) ? inHeight : (yi + 1) * tileHeight + prepadding;
        int out_tile_y0 = (yi > 0) ? scale * prepadding / 2 : 0;
        int out_y0 = in_tile_y0 * scale + out_tile_y0;

        for (uint xi = 0; xi < xtiles; xi++) {
            int in_tile_x0 = (xi == 0) ? 0 : xi * tileWidth;
            int in_tile_x1 = (xi + 1 == xtiles) ? inWidth : (xi + 1) * tileWidth + prepadding;

           cv::Mat inputTile = inimage(cv::Rect(in_tile_x0, in_tile_y0, in_tile_x1 - in_tile_x0,
                                                 in_tile_y1 - in_tile_y0));


            fprintf(stderr, "process inputTile y=%d, x=%d\n", yi, xi);

            if (inputTile.cols < tilesize || inputTile.rows < tilesize) {

                fprintf(stderr, "process y=%d, x=%d copyMakeBorder\n", yi, xi);
                cv::Mat paddedTile;
                cv::copyMakeBorder(inputTile, paddedTile, 0, tilesize - inputTile.rows, 0,
                                   tilesize - inputTile.cols, cv::BORDER_CONSTANT);
                pretreat_->convert(paddedTile.data, inputTile.cols, inputTile.rows, 0,
                                   input_tensor);

            } else {
                pretreat_->convert(inputTile.data, inputTile.cols, inputTile.rows, 0, input_tensor);
            }

            interpreter_input->copyFromHostTensor(input_tensor);

            fprintf(stderr, "process y=%d, x=%d runSession, size=%d %d %d\n", yi, xi,interpreter_input->size(), input_tensor->size(),sizeof(inputTile.data));
            // Run the interpreter
            interpreter->runSession(session);

            fprintf(stderr, "process y=%d, x=%d copyToHostTensor\n", yi, xi);
            // Extract result from interpreter
            interpreter_output->copyToHostTensor(output_tensor);
            fprintf(stderr, "process y=%d, x=%d output_tensor, size=%d %d\n", yi, xi,interpreter_output->size(), output_tensor->size());

            // 直接操作 Tensor 数据
            float *tensor_data = output_tensor->host<float>();

            fprintf(stderr, "process y=%d, x=%d to float mat, tensor_data size %lu\n", yi, xi, sizeof (tensor_data));
            for (size_t i = 0; i < sizeof (tensor_data); i++) {
                printf("%f ", tensor_data[i]);
            }
            cv::Mat mat_f32(outHeight, outWidth, CV_32FC3, tensor_data);

            fprintf(stderr, "process y=%d, x=%d to 8u mat\n", yi, xi);
            // 将浮点数转换为 8 位无符号整数
            cv::Mat outputTile;
            mat_f32.convertTo(outputTile, CV_8UC3, 255.0);
//            mat_f32.convertTo(outputTile, CV_8UC(3), 255.0);

            int out_tile_x0 = (xi > 0) ? scale * prepadding / 2 : 0;
            int out_x0 = in_tile_x0 * scale + out_tile_x0;

            fprintf(stderr, "process y=%d, x=%d crop output\n", yi, xi);
            if (xi > 0 || yi > 0) {
                cv::Rect cropRect(out_tile_x0, out_tile_y0, (tilesize * scale - out_tile_x0),
                                  (tilesize * scale - out_tile_y0));
                cv::Mat croppedTile = outputTile(cropRect);
                croppedTile.copyTo(
                        imageOut(cv::Rect(out_x0, out_y0, croppedTile.cols, croppedTile.rows)));
            } else {
                outputTile.copyTo(
                        imageOut(cv::Rect(out_x0, out_y0, outputTile.cols, outputTile.rows)));
            }

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

bool MNNSR::finish() {
    MNN::Tensor::destroy(input_tensor);
    MNN::Tensor::destroy(output_tensor);
    interpreter->releaseSession(session);
    interpreter->releaseModel();
    MNN::Interpreter::destroy(interpreter);
    return true;
}
