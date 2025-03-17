//
// Created by Yazii on 2025/3/16.
//
#include "mnnsr.h"
#include <thread>

MNNSR::MNNSR(int gpuid, bool tta_mode, int num_threads) {

    pretreat_ = std::shared_ptr<MNN::CV::ImageProcess> (MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::RGB, meanVals_, 3,normVals_, 3));

}

MNNSR::~MNNSR() {


}

#if _WIN32
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


    interpreter = MNN::Interpreter::createFromFile(modelpath.c_str());
    session = interpreter->createSession(config);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start);

    fprintf(stderr, "Load model %.3f ms\n",
              static_cast<double>(duration.count()));

    interpreter_input = interpreter->getSessionInput(session, nullptr);
    interpreter->resizeTensor(interpreter_input, 1, 3, tilesize, tilesize);
    interpreter->resizeSession(session);
    interpreter_output = interpreter->getSessionOutput(session, nullptr);

    fprintf(stderr, "load model get tensor\n");
    input_tensor = new MNN::Tensor(interpreter_input, MNN::Tensor::CAFFE);
    output_tensor = new MNN::Tensor(interpreter_output, MNN::Tensor::CAFFE);

    input_buffer = input_tensor->host<float>();
    output_buffer = output_tensor->host<float>();

    fprintf(stderr, "load model finish\n");
    return 0;
}

void MNNSR::preProcessTile(const cv::Mat &tile) {

    fprintf(stderr, "preProcessTile\n");
    for (int c = 0; c < 3; ++c) {

        fprintf(stderr, "preProcessTile channel %d\n",c);
        // 获取对应的通道
        cv::Mat channel(tile.rows, tile.cols, CV_8UC1, tile.data + c);

        fprintf(stderr, "preProcessTile memcpy %d\n",c);
        // 复制数据到 MNN Tensor
        std::memcpy(input_tensor->host<float>() + c * tile.rows * tile.cols, channel.data,
                    tile.rows * tile.cols);
    }
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

    for (int yi = 0; yi < ytiles; yi++) {

        fprintf(stderr, "process tiles y=%d\n",yi);
        int in_tile_y0 = std::max(yi * tileHeight, 0);
        int in_tile_y1 = std::min((yi + 1) * tileHeight + prepadding, inHeight);
        int out_tile_y0 = (yi > 0) ? scale * prepadding / 2 : 0;
        int out_y0 = in_tile_y0 * scale + out_tile_y0;

        for (int xi = 0; xi < xtiles; xi++) {

            fprintf(stderr, "process tiles y=%d, x=%d\n",yi, xi);
            int in_tile_x0 = std::max(xi * tileWidth, 0);
            int in_tile_x1 = std::min((xi + 1) * tileWidth + prepadding, inWidth);

            cv::Mat inputTile = inimage(cv::Rect(in_tile_x0, in_tile_y0, in_tile_x1 - in_tile_x0,
                                                 in_tile_y1 - in_tile_y0));


            fprintf(stderr, "process inputTile y=%d, x=%d\n",yi, xi);

            if (inputTile.cols < tilesize || inputTile.rows < tilesize) {

                fprintf(stderr, "process y=%d, x=%d copyMakeBorder\n",yi, xi);
                cv::Mat paddedTile;
                cv::copyMakeBorder(inputTile, paddedTile, 0, tilesize - inputTile.rows, 0,
                                   tilesize - inputTile.cols, cv::BORDER_CONSTANT);

//                preProcessTile(paddedTile);

                pretreat_->convert(paddedTile.data, inputTile.cols, inputTile.rows, 0, input_tensor);

            } else {
//                preProcessTile(inputTile);


                pretreat_->convert(inputTile.data, inputTile.cols, inputTile.rows, 0, input_tensor);
            }

//            fprintf(stderr, "process y=%d, x=%d copyFromHostTensor\n",yi, xi);
//            // Feed data to the interpreter
//            interpreter_input->copyFromHostTensor(input_tensor);

            fprintf(stderr, "process y=%d, x=%d runSession\n",yi, xi);
            // Run the interpreter
            interpreter->runSession(session);

            fprintf(stderr, "process y=%d, x=%d copyToHostTensor\n",yi, xi);
            // Extract result from interpreter
            interpreter_output->copyToHostTensor(output_tensor);
            fprintf(stderr, "process y=%d, x=%d output_tensor\n",yi, xi);

            // 直接操作 Tensor 数据
            float *tensor_data = output_tensor->host<float>();

            fprintf(stderr, "process y=%d, x=%d to mat\n",yi, xi);
// 将 Tensor 数据转换为 OpenCV Mat
            cv::Mat tile_out;
            memcpy(tile_out.data, tensor_data, sizeof(tensor_data));

            fprintf(stderr, "process y=%d, x=%d to int\n",yi, xi);
// 如果需要将数据转换为 8 位无符号整型（0-255）
            cv::Mat outputTile;
            tile_out.convertTo(outputTile, CV_8UC(3), 255.0);

            int out_tile_x0 = (xi > 0) ? scale * prepadding / 2 : 0;
            int out_x0 = in_tile_x0 * scale + out_tile_x0;

            fprintf(stderr, "process y=%d, x=%d crop output\n",yi, xi);
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