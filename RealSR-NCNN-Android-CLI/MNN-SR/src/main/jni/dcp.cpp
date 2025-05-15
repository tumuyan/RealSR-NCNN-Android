//
// Created by Yazii on 2025/5/1.
// DCP model implementation converted from https://github.com/nanoskript/deepcreampy-onnx-docker
// it couldn't works yet
//

#include "dcp.h"

#include <thread>
#include <vector>
#include <tuple>
#include <utility>
#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include <new>


DCP::DCP() {
    pretreat_ = std::shared_ptr<MNN::CV::ImageProcess>(
            MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::RGB, meanVals_, 3, normVals_,
                                          3));
    pretreat_1ch_ = std::shared_ptr<MNN::CV::ImageProcess>(
            MNN::CV::ImageProcess::create(MNN::CV::GRAY, MNN::CV::GRAY, meanVals_, 1, normVals_,
                                          1));
}

DCP::~DCP() {
    if (inputs_num > 1) {
        MNN::Tensor::destroy(input_tensor2);
    }
    MNN::Tensor::destroy(input_tensor);
    MNN::Tensor::destroy(output_tensor);
    interpreter->releaseSession(session);
    interpreter->releaseModel();
    MNN::Interpreter::destroy(interpreter);
}


#if _WIN32
#include <codecvt>

int DCP::load(const std::wstring& modelpath, bool cachemodel, bool nchw))
#else

int DCP::load(const std::string &modelpath, bool cachemodel, bool nchw)
#endif
{
    MNN::ScheduleConfig config;
    MNN::BackendConfig backendConfig;
    backendConfig.memory = MNN::BackendConfig::Memory_High;
    backendConfig.power = MNN::BackendConfig::Power_High;
    backendConfig.precision = MNN::BackendConfig::Precision_Low;
    config.backendConfig = &backendConfig;
    config.type = backend_type;
    config.backupType = MNN_FORWARD_CPU;
    int num_threads = std::thread::hardware_concurrency();
    if (num_threads < 1)
        num_threads = 2;
    config.numThread = num_threads;
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

    auto interpreter_inputs = interpreter->getSessionInputAll(session);
    // 获取interpreter_inputs 包含的元素的数量

    inputs_num = interpreter_inputs.size();
    fprintf(stderr, "model %d inputs (DCP)\n", inputs_num);

    if (inputs_num < 1) {
        fprintf(stderr, "interpreter_inputs null\n");
        return -1;
    }


    auto it = interpreter_inputs.begin();
    interpreter_input = it->second;
    interpreter->resizeTensor(interpreter_input, 1, model_channel, tilesize, tilesize);
    fprintf(stderr, "interpreter_input (b/c/h/w): %d/%d/%d/%d %s\n", interpreter_input->batch(),
            interpreter_input->channel(), interpreter_input->height(), interpreter_input->width(),
            it->first.c_str());

    if (inputs_num > 1) {
        std::advance(it, 1);
        interpreter_input2 = it->second;
        interpreter->resizeTensor(interpreter_input2, 1, model_channel2, tilesize, tilesize);
        fprintf(stderr, "interpreter_input2 (b/c/h/w): %d/%d/%d/%d %s\n",
                interpreter_input2->batch(), interpreter_input2->channel(),
                interpreter_input2->height(), interpreter_input2->width(), it->first.c_str());
    }

    interpreter->resizeSession(session);
    interpreter_output = interpreter->getSessionOutput(session, nullptr);

    if (nchw) {
        if (inputs_num > 1)
            input_tensor2 = new MNN::Tensor(interpreter_input2, MNN::Tensor::CAFFE);
        input_tensor = new MNN::Tensor(interpreter_input, MNN::Tensor::CAFFE);
        output_tensor = new MNN::Tensor(interpreter_output, MNN::Tensor::CAFFE);
    } else {
        if (inputs_num > 1)
            input_tensor2 = new MNN::Tensor(interpreter_input2, MNN::Tensor::TENSORFLOW);
        input_tensor = new MNN::Tensor(interpreter_input, MNN::Tensor::TENSORFLOW);
        output_tensor = new MNN::Tensor(interpreter_output, MNN::Tensor::TENSORFLOW);
    }
    if (inputs_num > 1)
        input_buffer2 = input_tensor2->host<float>();
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


cv::Mat DCP::TensorToCvMat(void) {
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

        // 如果是RGB输入，需要交换R和B通道
        std::swap(channels[0], channels[2]); // RGB -> BGR
        cv::merge(channels, result);

        // 转换为8位
        result.convertTo(result, CV_8UC3, 255.0);
    }

    return result;
}

int DCP::process(const cv::Mat &inimage, cv::Mat &outimage, const cv::Mat &mask) {
    cv::Mat paddedTile;
    cv::copyMakeBorder(inimage, paddedTile, 0, 0, 0, 0, cv::BORDER_CONSTANT);

    pretreat_->convert(paddedTile.data, paddedTile.cols, paddedTile.rows,
                       paddedTile.cols * paddedTile.channels(),
                       input_tensor);

    bool r = interpreter_input->copyFromHostTensor(input_tensor);

    if (inputs_num > 1) {
        cv::Mat inMask;
        cv::copyMakeBorder(mask, inMask, 0, 0, 0, 0, cv::BORDER_CONSTANT);
//        pretreat_1ch_->convert(inMask.data, inMask.cols, inMask.rows,
//                               inMask.cols * inMask.channels(),
//                               input_tensor2);
//

        pretreat_->convert(inMask.data, inMask.cols, inMask.rows,
                           inMask.cols * inMask.channels(),
                           input_tensor2);
    }
    interpreter->runSession(session);
    outimage = TensorToCvMat();


    if (cachemodel)
        interpreter->
                updateCacheFile(session);
    return 0;
}


// 假设的预测函数，需要根据实际情况实现
cv::Mat DCP::predict(const cv::Mat &crop_img_array, const cv::Mat &mask_array, bool is_mosaic) {
    // 这里需要实现实际的预测逻辑
    fprintf(stderr, "predict() crop_img_array shape %d*%d*%d, mask_array shape %d*%d*%d",
            crop_img_array.cols, crop_img_array.rows, crop_img_array.channels(), mask_array.cols,
            mask_array.rows, mask_array.channels()
    );


    return cv::Mat::zeros(crop_img_array.size(), crop_img_array.type());
}

// 查找掩码
cv::Mat DCP::find_mask(const cv::Mat &colored, const cv::Scalar &mask_color) {
    cv::Mat mask = cv::Mat::ones(colored.size(), CV_8U);
    cv::Mat mask_channels[3];
    cv::split(colored, mask_channels);
    cv::Mat condition = (mask_channels[0] == mask_color[0]) & (mask_channels[1] == mask_color[1]) &
                        (mask_channels[2] == mask_color[2]);
    mask.setTo(0, condition);
    return mask;
}

// 查找区域
std::vector<std::vector<cv::Point>>
DCP::find_regions(const cv::Mat &image, const cv::Scalar &mask_color) {
    cv::Mat binary;
    cv::inRange(image, mask_color, mask_color, binary);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    return contours;
}

// 扩展边界框
cv::Rect DCP::expand_bounding(const cv::Mat &ori, const std::vector<cv::Point> &region,
                              double expand_factor) {
    cv::Rect bounding = cv::boundingRect(region);
    int width = bounding.width;
    int height = bounding.height;
    int center_x = bounding.x + width / 2;
    int center_y = bounding.y + height / 2;
    int new_width = static_cast<int>(width * expand_factor);
    int new_height = static_cast<int>(height * expand_factor);
    int new_x = std::max(0, center_x - new_width / 2);
    int new_y = std::max(0, center_y - new_height / 2);
    int new_right = std::min(ori.cols, new_x + new_width);
    int new_bottom = std::min(ori.rows, new_y + new_height);
    return cv::Rect(new_x, new_y, new_right - new_x, new_bottom - new_y);
}

// 预测区域
std::pair<cv::Mat, cv::Rect>
DCP::predict_region(const cv::Mat &ori, const std::vector<cv::Point> &region, const cv::Mat &mask,
                    bool is_mosaic) {
    cv::Rect bounding_box = expand_bounding(ori, region, 1.5);
    cv::Mat crop_img = ori(bounding_box);
    cv::Mat resized_crop_img;
    cv::resize(crop_img, resized_crop_img, cv::Size(256, 256));

    cv::Mat mask_reshaped;
    mask.convertTo(mask_reshaped, CV_8U, 255.0);
    cv::Mat cropped_mask = mask_reshaped(bounding_box);
    cv::Mat resized_mask;
    cv::resize(cropped_mask, resized_mask, cv::Size(256, 256));

    if (!is_mosaic) {
        cv::Mat zero_mask = (resized_mask == 0);
        resized_crop_img.setTo(0, zero_mask);
    }

//    resized_crop_img = (resized_crop_img / 255.0) * 2.0 - 1.0;

    cv::Mat pred_img_array;

    process(resized_crop_img, pred_img_array, resized_mask);


//    cv::Mat pred_img_array = predict(resized_crop_img, resized_mask, is_mosaic);
//    pred_img_array = (pred_img_array + 1.0) / 2.0 * 255.0;
//    pred_img_array.convertTo(pred_img_array, CV_8U);

    cv::Mat resized_pred_img;
    cv::resize(pred_img_array, resized_pred_img, cv::Size(bounding_box.width, bounding_box.height),
               0, 0, cv::INTER_CUBIC);

    return std::make_pair(resized_pred_img, bounding_box);
}

// 解密函数
cv::Mat DCP::decensor(const cv::Mat &ori, const cv::Mat &colored, bool is_mosaic) {

    cv::Mat mask;
    if (is_mosaic) {
        cv::Mat color_array = colored.clone();
        mask = find_mask(color_array, cv::Scalar(0, 255, 0));
    } else {
        cv::Mat ori_array_mask = ori.clone();
        mask = find_mask(ori_array_mask, cv::Scalar(0, 255, 0));
    }

    std::vector<std::vector<cv::Point>> regions = find_regions(colored, cv::Scalar(0, 255, 0));
    std::cout << "Found " << regions.size() << " censored regions in this image!" << std::endl;
    if (regions.empty() && !is_mosaic) {
        std::cout
                << "No green (0,255,0) regions detected! Make sure you're using exactly the right color."
                << std::endl;
        return ori;
    }


//        std::vector<std::tuple> results;
    std::vector<std::tuple<cv::Mat, cv::Rect>> results;
    for (const auto &region: regions) {
        auto prediction = predict_region(ori, region, mask, is_mosaic);
        results.push_back(std::make_tuple(prediction.first, prediction.second));
    }

    cv::Mat output_img_array = ori.clone();
    for (const auto &result: results) {
        cv::Mat pred_img_array = std::get<0>(result);
        cv::Rect bounding_box = std::get<1>(result);

        for (const auto &point: regions) {
            for (const auto &p: point) {
                if (bounding_box.contains(p)) {
                    int x = p.x - bounding_box.x;
                    int y = p.y - bounding_box.y;
                    output_img_array.at<cv::Vec3b>(p) = pred_img_array.at<cv::Vec3b>(y, x);
                }
            }
        }
    }

    std::cout << "Decensored image. Returning it." << std::endl;
    return output_img_array;
}
/*

    int main() {
        // 示例：从字节数组读取图像
        std::vector<uchar> image_bytes; // 这里需要填充实际的图像字节数据
        cv::Mat image = cv::imdecode(image_bytes, cv::IMREAD_UNCHANGED);
        if (image.empty()) {
            std::cerr << "Failed to decode image." << std::endl;
            return -1;
        }

        cv::Mat result = decensor(image, image, false);

        // 保存结果
        cv::imwrite("decensored_image.png", result);

        return 0;
    }
*/
