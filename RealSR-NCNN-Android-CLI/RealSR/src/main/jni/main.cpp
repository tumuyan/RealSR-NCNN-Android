// realsr implemented with ncnn library

#include <stdio.h>
#include <algorithm>
#include <queue>
#include <vector>
#include <clocale>

#define _DEMO_PATH  false
#define _VERBOSE_LOG  true

#if _WIN32
// image decoder and encoder with wic
#include "wic_image.h"
#else // _WIN32
// image decoder and encoder with stb
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_STDIO

#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"

#endif // _WIN32

#include "webp_image.h"
#include <chrono>

using namespace std::chrono;

#if _WIN32
#include <wchar.h>
static wchar_t* optarg = NULL;
static int optind = 1;
static wchar_t getopt(int argc, wchar_t* const argv[], const wchar_t* optstring)
{
    if (optind >= argc || argv[optind][0] != L'-')
        return -1;

    wchar_t opt = argv[optind][1];
    const wchar_t* p = wcschr(optstring, opt);
    if (p == NULL)
        return L'?';

    optarg = NULL;

    if (p[1] == L':')
    {
        optind++;
        if (optind >= argc)
            return L'?';

        optarg = argv[optind];
    }

    optind++;

    return opt;
}

static std::vector<int> parse_optarg_int_array(const wchar_t* optarg)
{
    std::vector<int> array;
    array.push_back(_wtoi(optarg));

    const wchar_t* p = wcschr(optarg, L',');
    while (p)
    {
        p++;
        array.push_back(_wtoi(p));
        p = wcschr(p, L',');
    }

    return array;
}
#else // _WIN32

#include <unistd.h> // getopt()

static std::vector<int> parse_optarg_int_array(const char *optarg) {
    std::vector<int> array;
    array.push_back(atoi(optarg));

    const char *p = strchr(optarg, ',');
    while (p) {
        p++;
        array.push_back(atoi(p));
        p = strchr(p, ',');
    }

    return array;
}

#endif // _WIN32

// ncnn
#include "cpu.h"
#include "gpu.h"
#include "platform.h"

#include "realsr.h"

#include "filesystem_utils.h"

static void print_usage() {
    fprintf(stderr, "Usage: realsr-ncnn -i infile -o outfile [options]...\n\n");
    fprintf(stderr, "  -h                   show this help\n");
    fprintf(stderr, "  -v                   verbose output\n");
    fprintf(stderr, "  -i input-path        input image path (jpg/png/webp) or directory\n");
    fprintf(stderr, "  -o output-path       output image path (jpg/png/webp) or directory\n");
    fprintf(stderr, "  -s scale             upscale ratio (4, default=4)\n");
    fprintf(stderr,
            "  -t tile-size         tile size (>=32/0=auto, default=0) can be 0,0,0 for multi-gpu\n");
    fprintf(stderr, "  -m model-path        realsr model path (default=models-DF2K_JPEG)\n");
    fprintf(stderr,
            "  -g gpu-id            gpu device to use (-1=cpu, default=auto) can be 0,1,2 for multi-gpu\n");
    fprintf(stderr,
            "  -j load:proc:save    thread count for load/proc/save (default=1:2:2) can be 1:2,2,2:2 for multi-gpu\n");
    fprintf(stderr, "  -x                   enable tta mode\n");
    fprintf(stderr, "  -f format            output image format (jpg/png/webp, default=ext/png)\n");
//    fprintf(stderr, "  -c check             check output image match input image\n");
}

class Task {
public:
    int id;
    int webp;
//    bool check;

    path_t inpath;
    path_t outpath;

    ncnn::Mat inimage;
    ncnn::Mat outimage;
    ncnn::Mat in;
};

class TaskQueue {
public:
    TaskQueue() {
    }

    void put(const Task &v) {
        lock.lock();

        while (tasks.size() >= 8) // FIXME hardcode queue length
        {
            condition.wait(lock);
        }

        tasks.push(v);

        lock.unlock();

        condition.signal();
    }

    void get(Task &v) {
        lock.lock();

        while (tasks.size() == 0) {
            condition.wait(lock);
        }

        v = tasks.front();
        tasks.pop();

        lock.unlock();

        condition.signal();
    }

private:
    ncnn::Mutex lock;
    ncnn::ConditionVariable condition;
    std::queue<Task> tasks;
};

TaskQueue toproc;
TaskQueue tosave;

class LoadThreadParams {
public:
    int scale;
    int jobs_load;
    int check_threshold;

    // session data
    std::vector<path_t> input_files;
    std::vector<path_t> output_files;
};

void *load(void *args) {
    const LoadThreadParams *ltp = (const LoadThreadParams *) args;
    const int count = ltp->input_files.size();
    const int scale = ltp->scale;
    const bool check = ltp->check_threshold > 0;

#pragma omp parallel for schedule(static, 1) num_threads(ltp->jobs_load)
    for (int i = 0; i < count; i++) {
        const path_t &imagepath = ltp->input_files[i];

        int webp = 0;

        unsigned char *pixeldata = 0;
        int w;
        int h;
        int c;

#if _WIN32
        FILE* fp = _wfopen(imagepath.c_str(), L"rb");
#else
        FILE *fp = fopen(imagepath.c_str(), "rb");
#endif
        if (fp) {
            // read whole file
            unsigned char *filedata = 0;
            int length = 0;
            {
                fseek(fp, 0, SEEK_END);
                length = ftell(fp);
                rewind(fp);
                filedata = (unsigned char *) malloc(length);
                if (filedata) {
                    fread(filedata, 1, length, fp);
                }
                fclose(fp);
            }

            if (filedata) {
                pixeldata = webp_load(filedata, length, &w, &h, &c);
                if (pixeldata) {
                    webp = 1;
                } else {
                    // not webp, try jpg png etc.
#if _WIN32
                    pixeldata = wic_decode_image(imagepath.c_str(), &w, &h, &c);
#else // _WIN32
                    pixeldata = stbi_load_from_memory(filedata, length, &w, &h, &c, 0);
                    if (pixeldata) {
                        if (_VERBOSE_LOG) {
                            fprintf(stderr, "channel=%d, stbi_load_from_memory\n", c);
                        }
                        // stb_image auto channel
                        if (c == 1) {
                            // grayscale -> rgb
                            stbi_image_free(pixeldata);
                            pixeldata = stbi_load_from_memory(filedata, length, &w, &h, &c, 3);
                            c = 3;
                        } else if (c == 2) {
                            // grayscale + alpha -> rgba
                            stbi_image_free(pixeldata);
                            pixeldata = stbi_load_from_memory(filedata, length, &w, &h, &c, 4);
                            c = 4;
                        }
                    } else if (_VERBOSE_LOG) {
                        fprintf(stderr, "no pixeldata 2\n");
                    }
#endif // _WIN32
                }
            } else if (_VERBOSE_LOG) {
#if _WIN32
                fwprintf(stderr, L"no filedata\n");
#else // _WIN32
                fprintf(stderr, "no filedata 1\n");
#endif // _WIN32
            }

            free(filedata);
        } else if (_VERBOSE_LOG) {
#if _WIN32
            fwprintf(stderr, L"fopen failed\n");
#else // _WIN32
            fprintf(stderr, "fopen failed\n");
#endif // _WIN32
        }
        if (pixeldata) {
            Task v;
            v.id = i;
            v.inpath = imagepath;
            v.outpath = ltp->output_files[i];


            v.inimage = ncnn::Mat(w, h, (void *) pixeldata, (size_t) c, c);
            v.outimage = ncnn::Mat(w * scale, h * scale, (size_t) c, c);

            if (check) {
                if (c == 4) {
                    v.in = ncnn::Mat::from_pixels(pixeldata, ncnn::Mat::PIXEL_RGBA, w, h);
                } else {
                    v.in = ncnn::Mat::from_pixels(pixeldata, ncnn::Mat::PIXEL_RGB, w, h);
                }
            }
            fprintf(stderr, "scale=%d, w/h/c %d/%d/%d -> %d/%d/%d\n", scale,
                    v.inimage.w, v.inimage.h, v.inimage.c,
                    v.outimage.w, v.outimage.h, v.outimage.c
            );

            path_t ext = get_file_extension(v.outpath);
            if (c == 4 &&
                (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") ||
                 ext == PATHSTR("JPEG"))) {
                path_t output_filename2 = ltp->output_files[i] + PATHSTR(".png");
                v.outpath = output_filename2;
#if _WIN32
                fwprintf(stderr, L"image %ls has alpha channel ! %ls will output %ls\n", imagepath.c_str(), imagepath.c_str(), output_filename2.c_str());
#else // _WIN32
                fprintf(stderr, "image %s has alpha channel ! %s will output %s\n",
                        imagepath.c_str(), imagepath.c_str(), output_filename2.c_str());
#endif // _WIN32
            }

            toproc.put(v);
        } else {
#if _WIN32
            fwprintf(stderr, L"decode image %ls failed\n", imagepath.c_str());
#else // _WIN32
            fprintf(stderr, "decode image %s failed\n", imagepath.c_str());
#endif // _WIN32
        }
    }

    return 0;
}

class ProcThreadParams {
public:
    const RealSR *realsr;
};

void *proc(void *args) {
    const ProcThreadParams *ptp = (const ProcThreadParams *) args;
    const RealSR *realsr = ptp->realsr;

    for (;;) {
        Task v;

        toproc.get(v);

        if (v.id == -233)
            break;

        realsr->process(v.inimage, v.outimage);

        tosave.put(v);
    }

    return 0;
}

class SaveThreadParams {
public:
    int verbose;
//    bool check;
    int check_threshold;

};

float compareNcnnMats(const ncnn::Mat &mat1, const ncnn::Mat &mat2) {
    fprintf(stderr, "check result: mat1 %dx%dx%d, mat2 %dx%dx%d, elempack: %d, elemsize: %zu",
            mat1.w, mat1.h, mat1.c, mat2.w, mat2.h, mat2.c, mat1.elempack, mat1.elemsize);

    // 检查两个Mat对象的维度是否相同
    if (mat1.w != mat2.w || mat1.h != mat2.h) {
        return -1;
    }

    // 检查两个Mat对象的数据类型是否相同
    if (mat1.elemsize != mat2.elemsize || mat1.elempack != mat2.elempack) {
        return -2;
    }

    // 比较每个通道的像素差异
    long sum_diff = 0;
    int channels = std::min(mat1.c, mat2.c);
    size_t plane_size = mat1.w * mat1.h * mat1.elemsize * mat1.elempack;
    for (int c = 0; c < channels; c++) {

        const unsigned char *data1 = mat1.channel(c);
        const unsigned char *data2 = mat2.channel(c);

//        fprintf(stderr, ", q=%d", sizeof(data1) / mat1.w / mat1.h);
        for (size_t i = 0; i < plane_size; i++) {
            int diff = abs(data1[i] - data2[i]);
            sum_diff += diff;
//            if(i<48)
//                fprintf(stderr, "compare[%d]: %d, %d, diff=%d\n", i, data1[i],data2[i],diff);
        }
    }

/*    for (int c = 0; c < channels; ++c) {
        const unsigned char *data1 = mat1.channel(c);
        const unsigned char *data2 = mat2.channel(c);
        for (int i = 0; i < mat1.h; ++i) {
            for (int j = 0; j < mat1.w; ++j) {
                int diff = abs(data1[i * mat1.w + j] - data2[i * mat2.w + j]);
                sum_diff += diff;
            }
        }
    }*/

    // 计算平均差异
    float avg_diff = (float) sum_diff / (plane_size * channels);
    return avg_diff;
}

/*

float compareImageFiles(const std::string& file1, const std::string& file2)
{
    // 加载图片文件
    ncnn::Mat mat1 = ncnn::Mat::from_pixels_file(file1.c_str(), ncnn::Mat::PIXEL_BGR);
    ncnn::Mat mat2 = ncnn::Mat::from_pixels_file(file2.c_str(), ncnn::Mat::PIXEL_BGR);

    // 检查两个Mat对象的维度是否相同
    if (mat1.w != mat2.w || mat1.h != mat2.h) {
        return -1;
    }

    // 比较每个像素的差异
    float sum_diff = 0;
    int channels = std::min(mat1.c, mat2.c);
    for (int c = 0; c < channels; c++) {
        size_t plane_size = mat1.w * mat1.h * mat1.elemsize * mat1.elempack;
        const unsigned char* data1 = mat1.channel(c);
        const unsigned char* data2 = mat2.channel(c);
        for (size_t i = 0; i < plane_size; i++) {
            float diff = fabs(data1[i] - data2[i]);
            sum_diff += diff;
        }
    }

    // 计算平均差异
    float avg_diff = sum_diff / (mat1.w * mat1.h * channels);

    return avg_diff;
}
*/


void *save(void *args) {
    const SaveThreadParams *stp = (const SaveThreadParams *) args;
    const int verbose = stp->verbose;
    const int check_threshold = stp->check_threshold;

    for (;;) {
        Task v;

        tosave.get(v);

        if (v.id == -233)
            break;


        high_resolution_clock::time_point begin = high_resolution_clock::now();

        // free input pixel data
        {
            unsigned char *pixeldata = (unsigned char *) v.inimage.data;


            fprintf(stderr, "save result...\n");

            if (v.webp == 1) {
                free(pixeldata);
            } else {
#if _WIN32
                free(pixeldata);
#else
                stbi_image_free(pixeldata);
#endif
            }
        }

        int success = 0;

        path_t ext = get_file_extension(v.outpath);

        if (ext == PATHSTR("webp") || ext == PATHSTR("WEBP")) {
            success = webp_save(v.outpath.c_str(), v.outimage.w, v.outimage.h, v.outimage.elempack,
                                (const unsigned char *) v.outimage.data);
        } else if (ext == PATHSTR("png") || ext == PATHSTR("PNG")) {
#if _WIN32
            success = wic_encode_image(v.outpath.c_str(), v.outimage.w, v.outimage.h, v.outimage.elempack, v.outimage.data);
#else
            success = stbi_write_png(v.outpath.c_str(), v.outimage.w, v.outimage.h,
                                     v.outimage.elempack, v.outimage.data, 0);
#endif
        } else if (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") ||
                   ext == PATHSTR("JPEG")) {
#if _WIN32
            success = wic_encode_jpeg_image(v.outpath.c_str(), v.outimage.w, v.outimage.h, v.outimage.elempack, v.outimage.data);
#else
            success = stbi_write_jpg(v.outpath.c_str(), v.outimage.w, v.outimage.h,
                                     v.outimage.elempack, v.outimage.data, 100);
#endif
        }
        if (success) {
            high_resolution_clock::time_point end = high_resolution_clock::now();
            duration<double> time_span = duration_cast<duration<double>>(end - begin);
            fprintf(stderr, "save result use time: %.3f\n", time_span);

            if (verbose) {
#if _WIN32
                fwprintf(stdout, L"%ls -> %ls done\n", v.inpath.c_str(), v.outpath.c_str());
#else
                fprintf(stdout, "%s -> %s done\n", v.inpath.c_str(), v.outpath.c_str());
#endif
            }


            if (check_threshold > 0) {
                fprintf(stderr, "check result...\n");
                ncnn::Mat checkimage1, checkimage2, outimage;

                int w = v.inimage.w, h = v.inimage.h, c = v.inimage.elemsize;
                if (c == 4) {
                    outimage = ncnn::Mat::from_pixels((const unsigned char *) v.outimage.data,
                                                      ncnn::Mat::PIXEL_RGBA, w, h);
                } else {
                    outimage = ncnn::Mat::from_pixels((const unsigned char *) v.outimage.data,
                                                      ncnn::Mat::PIXEL_RGB, w, h);
                }
                ncnn::resize_bilinear(outimage, checkimage2, w / 2, h / 2);
                ncnn::resize_bilinear(v.in, checkimage1, w / 2, h / 2);
                float avg_diff = compareNcnnMats(checkimage1, checkimage2);
                if (avg_diff > check_threshold)
                    fprintf(stderr, "\nResult is not similar to input, avg diff: %f\n",
                            avg_diff);
                else if (avg_diff < 0)
                    fprintf(stderr, "\n[Error]compare result and input, error code: %f\n",
                            avg_diff);
                else
                    fprintf(stderr, "\ncompare result and input, avg diff: %f\n", avg_diff);

            }
        } else {
#if _WIN32
            fwprintf(stderr, L"save result failed: %ls\n", v.outpath.c_str());
#else
            fprintf(stderr, "save result failed: %s\n", v.outpath.c_str());
#endif
        }
    }

    return 0;
}

#if _WIN32
const std::wstring& optarg_in (L"A:\\Media\\realsr-ncnn-vulkan-20210210-windows\\input3.jpg");
const std::wstring& optarg_out(L"A:\\Media\\realsr-ncnn-vulkan-20210210-windows\\output3.jpg");
const std::wstring& optarg_mo (L"A:\\Media\\realsr-ncnn-vulkan-20210210-windows\\models-Real-ESRGAN-anime");
#else
const char *optarg_in = "/sdcard/10086/input3.jpg";
const char *optarg_out = "/sdcard/10086/output3.jpg";
const char *optarg_mo = "/sdcard/10086/models-Real-ESRGAN-anime";
#endif

#if _WIN32
int wmain(int argc, wchar_t** argv)
#else

int main(int argc, char **argv)
#endif
{

    high_resolution_clock::time_point prg_start = high_resolution_clock::now();

    path_t inputpath;
    path_t outputpath;
    int scale = 4;
    std::vector<int> tilesize;
#if _DEMO_PATH
    path_t model = optarg_mo;
#else
    path_t model = PATHSTR("models-Real-ESRGAN-anime");
#endif
    std::vector<int> gpuid;
    int jobs_load = 1;
    std::vector<int> jobs_proc;
    int jobs_save = 2;
    int verbose = 0;
    int tta_mode = 0;
    path_t format = PATHSTR("png");
    int check_threshold = 0;

#if _WIN32
    setlocale(LC_ALL, "");
    wchar_t opt;
    while ((opt = getopt(argc, argv, L"i:o:s:c:t:m:g:j:f:vxh")) != (wchar_t)-1)
    {
        switch (opt)
        {
        case L'i':
            inputpath = optarg;
            break;
        case L'o':
            outputpath = optarg;
            break;
        case L's':
            scale = _wtoi(optarg);
            break;
        case L't':
            tilesize = parse_optarg_int_array(optarg);
            break;
        case L'm':
            model = optarg;
            break;
        case L'g':
            gpuid = parse_optarg_int_array(optarg);
            break;
        case L'j':
            swscanf(optarg, L"%d:%*[^:]:%d", &jobs_load, &jobs_save);
            jobs_proc = parse_optarg_int_array(wcschr(optarg, L':') + 1);
            break;
        case L'f':
            format = optarg;
            break;
        case L'v':
            verbose = 1;
            break;
        case L'x':
            tta_mode = 1;
            break;
        case L'c':
            check_threshold = _wtoi(optarg);
            break;
        case L'h':
        default:
            print_usage();
            return -1;
        }
    }
#else // _WIN32
    int opt;
    while ((opt = getopt(argc, argv, "i:o:s:c:t:m:g:j:f:vxh")) != -1) {
        switch (opt) {
            case 'i':
                inputpath = optarg;
                break;
            case 'o':
                outputpath = optarg;
                break;
            case 's':
                scale = atoi(optarg);
                break;
            case 't':
                tilesize = parse_optarg_int_array(optarg);
                break;
            case 'm':
                model = optarg;
                break;
            case 'g':
                gpuid = parse_optarg_int_array(optarg);
                break;
            case 'j':
                sscanf(optarg, "%d:%*[^:]:%d", &jobs_load, &jobs_save);
                jobs_proc = parse_optarg_int_array(strchr(optarg, ':') + 1);
                break;
            case 'f':
                format = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'x':
                tta_mode = 1;
                break;
            case 'c':
                check_threshold = atoi(optarg);
                break;
            case 'h':
            default:
                print_usage();
                return -1;
        }
    }
#endif // _WIN32


    if (inputpath.empty()) {
        print_usage();
#if _DEMO_PATH
        fprintf(stderr, "demo input argument\n");
        inputpath = optarg_in;
#else
        return -1;
#endif
    }


    if (outputpath.empty()) {
        print_usage();
#if _DEMO_PATH
        fprintf(stderr, "demo output argument\n");
        outputpath = optarg_ou;
#else
        return -1;
#endif
    }

//    if (scale != 4) {
//        fprintf(stderr, "invalid scale argument\n");
//        return -1;
//    }

    if (tilesize.size() != (gpuid.empty() ? 1 : gpuid.size()) && !tilesize.empty()) {
        fprintf(stderr, "invalid tilesize argument\n");
        return -1;
    }

    for (int i = 0; i < (int) tilesize.size(); i++) {
        if (tilesize[i] != 0 && tilesize[i] < 32) {
            fprintf(stderr, "invalid tilesize argument\n");
            return -1;
        }
    }

    if (jobs_load < 1 || jobs_save < 1) {
        fprintf(stderr, "invalid thread count argument\n");
        return -1;
    }

    if (jobs_proc.size() != (gpuid.empty() ? 1 : gpuid.size()) && !jobs_proc.empty()) {
        fprintf(stderr, "invalid jobs_proc thread count argument\n");
        return -1;
    }

    for (int i = 0; i < (int) jobs_proc.size(); i++) {
        if (jobs_proc[i] < 1) {
            fprintf(stderr, "invalid jobs_proc thread count argument\n");
            return -1;
        }
    }

    if (!path_is_directory(outputpath)) {
        // guess format from outputpath no matter what format argument specified
        path_t ext = get_file_extension(outputpath);

        if (ext == PATHSTR("png") || ext == PATHSTR("PNG")) {
            format = PATHSTR("png");
        } else if (ext == PATHSTR("webp") || ext == PATHSTR("WEBP")) {
            format = PATHSTR("webp");
        } else if (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") ||
                   ext == PATHSTR("JPEG")) {
            format = PATHSTR("jpg");
        } else {
            fprintf(stderr, "invalid outputpath extension type\n");
            return -1;
        }
    }

    if (format != PATHSTR("png") && format != PATHSTR("webp") && format != PATHSTR("jpg")) {
        fprintf(stderr, "invalid format argument\n");
        return -1;
    }

    // collect input and output filepath
    std::vector<path_t> input_files;
    std::vector<path_t> output_files;
    {
        if (path_is_directory(inputpath) && path_is_directory(outputpath)) {
            std::vector<path_t> filenames;
            int lr = list_directory(inputpath, filenames);
            if (lr != 0)
                return -1;

            const int count = filenames.size();
            input_files.resize(count);
            output_files.resize(count);

            path_t last_filename;
            path_t last_filename_noext;
            for (int i = 0; i < count; i++) {
                path_t filename = filenames[i];
                path_t filename_noext = get_file_name_without_extension(filename);
                path_t output_filename = filename_noext + PATHSTR('.') + format;

                // filename list is sorted, check_threshold if output image path conflicts
                if (filename_noext == last_filename_noext) {
                    path_t output_filename2 = filename + PATHSTR('.') + format;
#if _WIN32
                    fwprintf(stderr, L"both %ls and %ls output %ls ! %ls will output %ls\n", filename.c_str(), last_filename.c_str(), output_filename.c_str(), filename.c_str(), output_filename2.c_str());
#else
                    fprintf(stderr, "both %s and %s output %s ! %s will output %s\n",
                            filename.c_str(), last_filename.c_str(), output_filename.c_str(),
                            filename.c_str(), output_filename2.c_str());
#endif
                    output_filename = output_filename2;
                } else {
                    last_filename = filename;
                    last_filename_noext = filename_noext;
                }

                input_files[i] = inputpath + PATHSTR('/') + filename;
                output_files[i] = outputpath + PATHSTR('/') + output_filename;
            }
        } else if (!path_is_directory(inputpath) && !path_is_directory(outputpath)) {
            input_files.push_back(inputpath);
            output_files.push_back(outputpath);
        } else {
            fprintf(stderr,
                    "inputpath and outputpath must be either file or directory at the same time\n");
            return -1;
        }
    }

    int prepadding = 0;

    if (model.find(PATHSTR("models-DF2K")) != path_t::npos ||
        model.find(PATHSTR("models-Real")) != path_t::npos ||
        model.find(PATHSTR("models-ESRGAN")) != path_t::npos) {
        prepadding = 10;
    } else {
        fprintf(stderr, "unknown model dir type\n");
        return -1;
    }


    int scales[] = {4, 2, 1, 8};
    int sp = 0;

#if _WIN32
    wchar_t modelpath[256];
    swprintf(modelpath, 256, L"%s/x%d.bin", model.c_str(), scale);
    fprintf(stderr, "search model: %s\n", modelpath);

    path_t modelfullpath = sanitize_filepath(modelpath);
    FILE* mp = _wfopen(modelfullpath.c_str(), L"rb");
#else
    char modelpath[256];
    sprintf(modelpath, "%s/x%d.bin", model.c_str(), scale);
    fprintf(stderr, "search model: %s\n", modelpath);

    path_t modelfullpath = sanitize_filepath(modelpath);
    FILE *mp = fopen(modelfullpath.c_str(), "rb");
#endif

    while (!mp && sp < 4) {
        int s = scales[sp];
#if _WIN32
        swprintf(modelpath, 256, L"%s/x%d.bin", model.c_str(), s);

        modelfullpath = sanitize_filepath(modelpath);
        mp = _wfopen(modelfullpath.c_str(), L"rb");
#else
        sprintf(modelpath, "%s/x%d.bin", model.c_str(), s);

        modelfullpath = sanitize_filepath(modelpath);
        mp = fopen(modelfullpath.c_str(), "rb");
#endif
        if (mp) {
            fprintf(stderr, "Fix scale: %d -> %d\n", scale, s);
            scale = s;
            break;
        } else {
            fprintf(stderr, "Fix scale fail -> %d\n", s);
            sp++;
        }
    };

    if (!mp) {
        fprintf(stderr, "Unknow scale for the model (%s)\n", modelfullpath.c_str());
        return -1;
    }

#if _WIN32
    wchar_t parampath[256];
    swprintf(parampath, 256, L"%s/x%d.param", model.c_str(), scale);
#else
    char parampath[256];
    sprintf(parampath, "%s/x%d.param", model.c_str(), scale);
#endif
    path_t paramfullpath = sanitize_filepath(parampath);


#if _WIN32
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

    ncnn::create_gpu_instance();
//    ncnn::set_cpu_powersave(0);

    if (gpuid.empty()) {
        gpuid.push_back(ncnn::get_default_gpu_index());
    }

    const int use_gpu_count = (int) gpuid.size();

    if (jobs_proc.empty()) {
        jobs_proc.resize(use_gpu_count, 2);
    }

    if (tilesize.empty()) {
        tilesize.resize(use_gpu_count, 0);
    }

    int cpu_count = std::max(1, ncnn::get_cpu_count());
    jobs_load = std::min(jobs_load, cpu_count);
    jobs_save = std::min(jobs_save, cpu_count);

    int gpu_count = ncnn::get_gpu_count();
    for (int i = 0; i < use_gpu_count; i++) {
        if (gpuid[i] < -1 || gpuid[i] >= gpu_count) {
            fprintf(stderr, "invalid gpu device\n");

            ncnn::destroy_gpu_instance();
            return -1;
        }
    }

    int total_jobs_proc = 0;
    for (int i = 0; i < use_gpu_count; i++) {
        if (gpuid[i] == -1) {
            jobs_proc[i] = std::min(jobs_proc[i], cpu_count);
            total_jobs_proc += 1;

            fprintf(stderr, "use CPU\n");
        } else {
            total_jobs_proc += jobs_proc[i];
        }
    }

    if (verbose)
        fprintf(stderr, "init heap_budget, use_gpu_count=%d\n", use_gpu_count);
    for (int i = 0; i < use_gpu_count; i++) {


        if (tilesize[i] != 0)
            continue;

        if (gpuid[i] == -1) {
            // cpu only
            tilesize[i] = 200;
            if (verbose)
                fprintf(stderr, "init cpu tilesize %d/%d = %d\n", i, tilesize.size(), tilesize[i]);
            continue;
        }

        uint32_t heap_budget = ncnn::get_gpu_device(gpuid[i])->get_heap_budget();

        // more fine-grained tilesize policy here
        if (model.find(PATHSTR("models-Real-ESRGANv")) != path_t::npos) {
            if (heap_budget > 3300)
                tilesize[i] = 400;
            else if (heap_budget > 1900)
                tilesize[i] = 200;
            else if (heap_budget > 550)
                tilesize[i] = 100;
            else if (heap_budget > 190)
                tilesize[i] = 64;
            else
                tilesize[i] = 32;
        } else {
            if (heap_budget > 2800)
                tilesize[i] = 200;
            else if (heap_budget > 900)
                tilesize[i] = 100;
            else if (heap_budget > 300)
                tilesize[i] = 64;
            else
                tilesize[i] = 32;
        }

        fprintf(stderr, "config gpu[%d], tilesize=%d, heap_budget=%d\n", i, tilesize[i],
                heap_budget);

        if (verbose) {
            const ncnn::GpuInfo &info = ncnn::get_gpu_info(gpuid[i]);
            fprintf(stderr,
                    "Gpu[%d]=%d, api_version= %d, driver_version=%d, vendor_id=%d ,device_id=%d, device_name=%s\n",
                    i, gpuid[i], info.api_version(), info.driver_version(), info.vendor_id(),
                    info.device_id(), info.device_name()
            );
        }
    }
    if (verbose)
        fprintf(stderr, "init realsr\n");
    else
        fprintf(stderr, "busy...\n");
    {
        std::vector<RealSR *> realsr(use_gpu_count);

        for (int i = 0; i < use_gpu_count; i++) {
            int num_threads = gpuid[i] == -1 ? jobs_proc[i] : 1;

            realsr[i] = new RealSR(gpuid[i], tta_mode, num_threads);

            realsr[i]->load(paramfullpath, modelfullpath);

            realsr[i]->scale = scale;
            realsr[i]->tilesize = tilesize[i];
            realsr[i]->prepadding = prepadding;
        }

        // main routine
        {
            // load image
            LoadThreadParams ltp;
            ltp.scale = scale;
            ltp.check_threshold = check_threshold;
            ltp.jobs_load = jobs_load;
            ltp.input_files = input_files;
            ltp.output_files = output_files;

            ncnn::Thread load_thread(load, (void *) &ltp);

            // realsr proc
            std::vector<ProcThreadParams> ptp(use_gpu_count);
            for (int i = 0; i < use_gpu_count; i++) {
                ptp[i].realsr = realsr[i];
            }

            std::vector<ncnn::Thread *> proc_threads(total_jobs_proc);
            {
                int total_jobs_proc_id = 0;
                for (int i = 0; i < use_gpu_count; i++) {
                    if (gpuid[i] == -1) {
                        proc_threads[total_jobs_proc_id++] = new ncnn::Thread(proc,
                                                                              (void *) &ptp[i]);
                    } else {
                        for (int j = 0; j < jobs_proc[i]; j++) {
                            proc_threads[total_jobs_proc_id++] = new ncnn::Thread(proc,
                                                                                  (void *) &ptp[i]);
                        }
                    }
                }
            }

            // save image
            SaveThreadParams stp;
            stp.verbose = verbose;
            stp.check_threshold = check_threshold;

            std::vector<ncnn::Thread *> save_threads(jobs_save);
            for (int i = 0; i < jobs_save; i++) {
                if (verbose)
                    fprintf(stderr, "init save_threads %d\n", i);
                save_threads[i] = new ncnn::Thread(save, (void *) &stp);
            }

            // end
            load_thread.join();

            Task end;
            end.id = -233;

            for (int i = 0; i < total_jobs_proc; i++) {
                toproc.put(end);
            }

            for (int i = 0; i < total_jobs_proc; i++) {
                proc_threads[i]->join();
                delete proc_threads[i];
            }

            for (int i = 0; i < jobs_save; i++) {
                tosave.put(end);
            }

            for (int i = 0; i < jobs_save; i++) {
                save_threads[i]->join();
                delete save_threads[i];
            }
        }

        for (int i = 0; i < use_gpu_count; i++) {
            delete realsr[i];
        }
        realsr.clear();
    }

    ncnn::destroy_gpu_instance();

    high_resolution_clock::time_point prg_end = high_resolution_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(prg_end - prg_start);
    fprintf(stderr, "Total use time: %.3f\n", time_span);


    return 0;
}
