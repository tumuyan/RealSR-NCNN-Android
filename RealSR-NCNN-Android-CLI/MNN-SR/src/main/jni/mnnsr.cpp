//
// Created by Yazii on 2025/3/16.
//
#include "mnnsr.h"
#include <thread>

#include "MNN/ErrorCode.hpp"

MNNSR::MNNSR(int gpuid, bool tta_mode, int num_threads) {

    pretreat_ = std::shared_ptr<MNN::CV::ImageProcess>(
            MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::RGB, meanVals_, 3, normVals_, 3));


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
//    config.type = backend_type;
//    config.type = MNN_FORWARD_AUTO;
    config.backupType = MNN_FORWARD_OPENCL;
    config.backupType = MNN_FORWARD_VULKAN;
//    config.backupType = MNN_FORWARD_AUTO;
    config.numThread = std::thread::hardware_concurrency();

    fprintf(stderr, "interpreter numThread=%d\n", config.numThread);
    const auto start = std::chrono::high_resolution_clock::now();

#if _WIN32
    interpreter = MNN::Interpreter::createFromFile(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(modelpath).c_str());
#else
//    std::shared_ptr<MNN::Interpreter> net1 = MNN::Interpreter::createFromFile("1.mnn");
//    interpreter = MNN::Interpreter::createFromFile(("/data/data/com.tumuyan.ncnn.realsr/cache/realsr/"+modelpath).c_str());
    interpreter = MNN::Interpreter::createFromFile((modelpath).c_str());
#endif


    if (interpreter == nullptr) {
        fprintf(stderr, "interpreter null\n");
    }

    session = interpreter->createSession(config);
    if (session == nullptr) {
        fprintf(stderr, "session null\n");
    }

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
    fprintf(stderr, "tilesize %d\n", tilesize);
    fprintf(stderr, "input_tensor w %d h %d c%d\n", input_tensor->width(), input_tensor->height(),
            input_tensor->channel());
    fprintf(stderr, "output_tensor w %d h %d c%d\n", output_tensor->width(),
            output_tensor->height(), output_tensor->channel());
    fprintf(stderr, "input_buffer %d\n", sizeof(input_buffer));
    fprintf(stderr, "output_buffer %d\n", sizeof(output_buffer));
    fprintf(stderr, "interpreter_input w %d h %d c%d   %d\n", interpreter_input->size(),
            interpreter_input->width(), interpreter_input->height(), interpreter_input->channel());
    fprintf(stderr, "interpreter_output w %d h %d c%d   %d \n", interpreter_output->size(),
            interpreter_output->width(), interpreter_output->height(),
            interpreter_output->channel());
    return 0;
}

void MNNSR::transform(const cv::Mat &mat) {
//    cv::Mat canvas;
//    cv::resize(mat, canvas, cv::Size(tilesize, tilesize));
    // (1,3,224,224)
    fprintf(stderr,
            "pretreat_->convert... mat w %d h %d c %d , strip %d, input_tensor w %d h %d c%d\n",
            mat.cols, mat.rows, mat.channels(), mat.step[0], input_tensor->width(),
            input_tensor->height(), input_tensor->channel());
    MNN::ErrorCode err = pretreat_->convert(mat.data, mat.cols, mat.rows, mat.step[0],
                                            input_tensor);
    if (err != MNN::NO_ERROR)
        fprintf(stderr, "pretreat_->convert: %d\n", err);

}

void MNNSR::copyOutputTile(cv::Mat outputTile, cv::Mat outimage, int out_x0, int out_y0) {
    fprintf(stderr,
            "process y=%d, x=%d copy output outputTile. outputTile : %d %d %d, outimage: %d %d %d, roi: x0=%d y0=%d x1=%d y1=%d\n",
            out_x0 / tilesize / scale, out_y0 / tilesize / scale, outputTile.cols, outputTile.rows,
            outputTile.channels(), outimage.cols, outimage.rows, outimage.channels(), out_x0,
            out_y0, out_x0 + outputTile.cols, out_y0 + outputTile.rows);
    outputTile.copyTo(
            outimage(cv::Rect(out_x0, out_y0, outputTile.cols, outputTile.rows)));

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

    fprintf(stderr,
            "process loops: xtiles %d, ytiles %d, tileWidth %d, tileHeight %d, xPrepadding %d, yPrepadding %d\n",
            xtiles, ytiles, tileWidth, tileHeight, xPrepadding, yPrepadding);

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

            fprintf(stderr, "process y=%d, x=%d inputTile, x=%d y=%d %d %d %d %d\n", xi, yi,
                    in_tile_x0, in_tile_y0, in_tile_x1 - in_tile_x0,
                    in_tile_y1 - in_tile_y0);

            cv::Mat inputTile = inimage(cv::Rect(in_tile_x0, in_tile_y0, in_tile_x1 - in_tile_x0,
                                                 in_tile_y1 - in_tile_y0));

            fprintf(stderr, "\nprocess inputTile y=%d, x=%d, inputTile=%d %d %d\n", yi, xi,
                    inputTile.cols, inputTile.rows, inputTile.channels());

            if (inputTile.cols < tilesize || inputTile.rows < tilesize) {
                int t = (yi == 0) ? yPrepadding : 0;
                int b = tilesize + in_tile_y0 - in_tile_y1 - t;
                int l = (xi == 0) ? xPrepadding : 0;
                int r = tilesize + in_tile_x0 - in_tile_x1 - l;

                fprintf(stderr, "process y=%d, x=%d copyMakeBorder %d %d %d %d\n", yi, xi, t, b, l,
                        r);
                cv::Mat paddedTile;
                cv::copyMakeBorder(inputTile, paddedTile, t, b, l, r, cv::BORDER_CONSTANT);

                pretreat_->convert(paddedTile.data, paddedTile.cols, paddedTile.rows,
                                   paddedTile.cols * paddedTile.channels(),
                                   input_tensor);

            } else {
                pretreat_->convert(inputTile.data, inputTile.cols, inputTile.rows,
                                   inputTile.cols * inputTile.channels(), input_tensor);
            }

            //  transform(inputTile);
            bool r = interpreter_input->copyFromHostTensor(input_tensor);
            // float* interpreter_input_data = interpreter_input->host<float>();

            fprintf(stderr, "process y=%d, x=%d runSession, size=%d %d %zu\n", yi, xi,
                    interpreter_input->elementSize(), input_tensor->elementSize(),
                    inputTile.elemSize());

            // Run the interpreter
            interpreter->runSession(session);

            fprintf(stderr, "process y=%d, x=%d copyToHostTensor\n", yi, xi);
            // Extract result from interpreter
            interpreter_output->copyToHostTensor(output_tensor);
            fprintf(stderr, "process y=%d, x=%d, interpreter_output & output_tensor size=%d %d\n",
                    yi, xi, interpreter_output->size(), output_tensor->size());

            // 假设output_tensor已经通过MNN库获得，并且是一个三通道的Tensor
// 获取Tensor的宽度和高度
            int width = output_tensor->width();
            int height = output_tensor->height();

// 创建一个三通道的Mat对象，数据类型为CV_32F（浮点型）
            cv::Mat outputMat(height, width, CV_32FC3);

// 获取Tensor的数据指针
            float *tensorData = output_tensor->host<float>();

// 将Tensor的数据拷贝到Mat中
            std::memcpy(outputMat.data, tensorData, width * height * 3 * sizeof(float));

// 如果需要将数据类型转换为8位无符号整数（CV_8UC3），可以进行如下转换
            cv::Mat outputTile;
            outputMat.convertTo(outputTile, CV_8UC3, 255.0); // 将浮点数转换为8位无符号整数，并进行缩放

            fprintf(stderr, "process y=%d, x=%d new outputMat8U done, %d %d %d\n", yi, xi,
                    outputTile.cols, outputTile.rows, outputTile.channels());

            cv::Rect cropRect(out_tile_x0, out_tile_y0, out_tile_w, out_tile_h);
            cv::Mat croppedTile = outputTile(cropRect);
            copyOutputTile(croppedTile, outimage, out_x0, out_y0);

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

