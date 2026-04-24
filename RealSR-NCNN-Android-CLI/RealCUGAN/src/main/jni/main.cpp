// realcugan implemented with ncnn library

#include <stdio.h>
#include <algorithm>
#include <queue>
#include <vector>
#include <clocale>

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

static std::vector<int> parse_optarg_int_array(const char* optarg)
{
    std::vector<int> array;
    array.push_back(atoi(optarg));

    const char* p = strchr(optarg, ',');
    while (p)
    {
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

#include "realcugan.h"

#include "filesystem_utils.h"
#include "image_processor.h"
#include <opencv2/opencv.hpp>
#include <opencv2/core/hal/interface.h>
#include "utils.hpp"
using namespace cv;

static void print_usage()
{
    fprintf(stdout, "Usage: realcugan-ncnn -i infile -o outfile [options]...\n\n");
    fprintf(stdout, "  -h                   show this help\n");
    fprintf(stdout, "  -v                   verbose output\n");
    fprintf(stdout, "  -i input-path        input image path (jpg/png/webp/bmp) or directory (recursive)\n");
    fprintf(stdout, "  -o output-path       output image path (jpg/png/webp/bmp) or directory\n");
    fprintf(stdout, "  -n noise-level       denoise level (-1/0/1/2/3, default=-1)\n");
    fprintf(stdout, "  -s scale             upscale ratio (1/2/3/4, default=2)\n");
    fprintf(stdout, "  -t tile-size         tile size (>=32/0=auto, default=0) can be 0,0,0 for multi-gpu\n");
    fprintf(stdout, "  -c syncgap-mode      sync gap mode (0/1/2/3, default=3)\n");
    fprintf(stdout, "  -m model-path        realcugan model path (default=models-se)\n");
    fprintf(stdout, "  -g gpu-id            gpu device to use (-1=cpu, default=auto) can be 0,1,2 for multi-gpu\n");
    fprintf(stdout, "  -j load:proc:save    thread count for load/proc/save (default=1:2:2) can be 1:2,2,2:2 for multi-gpu\n");
    fprintf(stdout, "  -x                   enable tta mode\n");
    fprintf(stdout, "  -f format            force output format (ignore alpha channel detection)\n");
    fprintf(stdout, "  -e format            suggested output format (auto-convert to png if alpha detected)\n");
    fprintf(stdout, "  -k skip-size         skip if output file exists and size >= threshold bytes (0=disable)\n");
    fprintf(stdout, "  -p pattern           output name pattern for batch mode, placeholders: {name} {prog} {index} {timestamp} {datetime} {date} {time}\n");
}

class Task
{
public:
    int id;
    int scale;
    int has_alpha;

    path_t inpath;
    path_t outpath;

    ncnn::Mat inimage;
    ncnn::Mat outimage;
    ncnn::Mat inalpha;
};

class TaskQueue
{
public:
    TaskQueue()
    {
    }

    void put(const Task& v)
    {
        lock.lock();

        while (tasks.size() >= 8) // FIXME hardcode queue length
        {
            condition.wait(lock);
        }

        tasks.push(v);

        lock.unlock();

        condition.signal();
    }

    void get(Task& v)
    {
        lock.lock();

        while (tasks.size() == 0)
        {
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

class LoadThreadParams
{
public:
    int scale;
    path_t output_format;
    int jobs_load;

    // session data
    std::vector<path_t> input_files;
    std::vector<path_t> output_files;
};

void* load(void* args)
{
    const LoadThreadParams* ltp = (const LoadThreadParams*)args;
    const int count = ltp->input_files.size();
    const int scale = ltp->scale;

    #pragma omp parallel for schedule(static,1) num_threads(ltp->jobs_load)
    for (int i=0; i<count; i++)
    {
        const path_t& imagepath = ltp->input_files[i];

        cv::Mat inBGR, inAlpha;
        imread(imagepath, inBGR, inAlpha);

        if (!inBGR.empty())
        {
            Task v;
            v.id = i;
            v.scale = scale;
            v.inpath = imagepath;
            v.outpath = ltp->output_files[i];
            v.has_alpha = !inAlpha.empty();

            path_t ext = get_file_extension(v.outpath);
            if (v.has_alpha && ltp->output_format.empty())
            {
                if (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") || ext == PATHSTR("JPEG"))
                {
                    path_t output_filename2 = ltp->output_files[i] + PATHSTR(".png");
                    v.outpath = output_filename2;
#if _WIN32
                    fwprintf(stderr, L"image %ls has alpha channel ! %ls will output %ls\n", imagepath.c_str(), imagepath.c_str(), output_filename2.c_str());
#else // _WIN32
                    fprintf(stderr, "image %s has alpha channel ! %s will output %s\n", imagepath.c_str(), imagepath.c_str(), output_filename2.c_str());
#endif // _WIN32
                }

                int w = inBGR.cols;
                int h = inBGR.rows;

                v.inalpha = ncnn::Mat(w, h, (size_t)1, 1);
                if (inAlpha.data)
                {
                    memcpy(v.inalpha.data, inAlpha.data, w * h);
                }
            }

            int w = inBGR.cols;
            int h = inBGR.rows;
            int c = inBGR.channels();

            unsigned char* pixeldata = (unsigned char*)malloc(w * h * c);
            memcpy(pixeldata, inBGR.data, w * h * c);

#ifndef _WIN32
            for (int j = 0; j < w * h * c; j += c)
            {
                std::swap(pixeldata[j], pixeldata[j + 2]);
            }
#endif

            v.inimage = ncnn::Mat(w, h, (void*)pixeldata, (size_t)c, c);

            toproc.put(v);
        }
        else
        {
#if _WIN32
            fwprintf(stderr, L"decode image %ls failed\n", imagepath.c_str());
#else // _WIN32
            fprintf(stderr, "decode image %s failed\n", imagepath.c_str());
#endif // _WIN32
        }
    }

    return 0;
}

class ProcThreadParams
{
public:
    const RealCUGAN* realcugan;
};

void* proc(void* args)
{
    const ProcThreadParams* ptp = (const ProcThreadParams*)args;
    const RealCUGAN* realcugan = ptp->realcugan;

    for (;;)
    {
        Task v;

        toproc.get(v);

        if (v.id == -233)
            break;

        const int scale = v.scale;
        if (scale == 1)
        {
            v.outimage = ncnn::Mat(v.inimage.w, v.inimage.h, (size_t)v.inimage.elemsize, (int)v.inimage.elemsize);
            realcugan->process(v.inimage, v.outimage);

            tosave.put(v);
            continue;
        }

        v.outimage = ncnn::Mat(v.inimage.w * scale, v.inimage.h * scale, (size_t)v.inimage.elemsize, (int)v.inimage.elemsize);
        realcugan->process(v.inimage, v.outimage);

        tosave.put(v);
    }

    return 0;
}

class SaveThreadParams
{
public:
    int verbose;
};

void* save(void* args)
{
    const SaveThreadParams* stp = (const SaveThreadParams*)args;
    const int verbose = stp->verbose;

    for (;;)
    {
        Task v;

        tosave.get(v);

        if (v.id == -233)
            break;

        fprintf(stderr, "save result...\n");
        float begin = clock();

        // free input pixel data
        {
            unsigned char* pixeldata = (unsigned char*)v.inimage.data;

            free(pixeldata);
        }

        int success = 0;

        path_t ext = get_file_extension(v.outpath);

        if (ext != PATHSTR("gif")) {
            cv::Mat image;
            if (v.has_alpha)
            {
                // get RGB from realcugan output (3 channels)
                cv::Mat rgb_image(v.outimage.h, v.outimage.w, CV_8UC3, v.outimage.data);
#ifndef _WIN32
                cv::cvtColor(rgb_image, rgb_image, cv::COLOR_RGB2BGR);
#endif

                // upscale original alpha with bicubic interpolation
                cv::Mat alpha_image(v.inalpha.h, v.inalpha.w, CV_8UC1, (unsigned char*)v.inalpha.data);
                cv::Mat scaled_alpha = resize_alpha_bicubic(alpha_image, v.scale);

                // free alpha data after use
                if (v.inalpha.data)
                {
                    free(v.inalpha.data);
                }

                // merge RGB and alpha
                merge_rgb_alpha(rgb_image, scaled_alpha, image);
            }
            else
            {
                switch (v.outimage.elempack) {
                    case 1:
                        image = cv::Mat(v.outimage.h, v.outimage.w, CV_8UC1, v.outimage.data);
                        break;
                    case 3:
                        image = cv::Mat(v.outimage.h, v.outimage.w, CV_8UC3, v.outimage.data);
#ifndef  _WIN32
                        cv::cvtColor(image, image, cv::COLOR_RGB2BGR);
#endif
                        break;
                    case 4:
                        image = cv::Mat(v.outimage.h, v.outimage.w, CV_8UC4, v.outimage.data);
#ifndef  _WIN32
                        cv::cvtColor(image, image, cv::COLOR_RGBA2BGRA);
#endif
                        break;
                }
            }
            if (image.empty()) {
                std::cerr << "Error: Image data not loaded." << std::endl;
                success = false;
            } else {
                #if _WIN32
                    success = imwrite_unicode(v.outpath, image);
                #else
                    success = imwrite(v.outpath.c_str(), image);
                #endif
            }

        }

        if (success)
        {
            float end = clock();
            fprintf(stderr, "save result use time: %.3lf\n", (end - begin) / CLOCKS_PER_SEC);

            if (verbose)
            {
#if _WIN32
                fwprintf(stdout, L"%ls -> %ls done\n", v.inpath.c_str(), v.outpath.c_str());
#else
                fprintf(stdout, "%s -> %s done\n", v.inpath.c_str(), v.outpath.c_str());
#endif
            }
        }
        else
        {
#if _WIN32
            fwprintf(stderr, L"encode image %ls failed\n", v.outpath.c_str());
#else
            fprintf(stderr, "encode image %s failed\n", v.outpath.c_str());
#endif
        }
    }

    return 0;
}


#if _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    path_t inputpath;
    path_t outputpath;
    int noise = -1;
    int scale = 2;
    std::vector<int> tilesize;
    path_t model = PATHSTR("models-se");
    std::vector<int> gpuid;
    int jobs_load = 1;
    std::vector<int> jobs_proc;
    int jobs_save = 2;
    int verbose = 0;
    int syncgap = 3;
    int tta_mode = 0;
    path_t output_format;
    path_t suggested_format;
    long long skip_size = 0;
    path_t name_pattern = PATHSTR("{name}");

#if _WIN32
    setlocale(LC_ALL, "");
    wchar_t opt;
    while ((opt = getopt(argc, argv, L"i:o:n:s:t:c:m:g:j:f:vxhk:e:p:")) != (wchar_t)-1)
    {
        switch (opt)
        {
        case L'i':
            inputpath = optarg;
            break;
        case L'o':
            outputpath = optarg;
            break;
        case L'n':
            noise = _wtoi(optarg);
            break;
        case L's':
            scale = _wtoi(optarg);
            break;
        case L't':
            tilesize = parse_optarg_int_array(optarg);
            break;
        case L'c':
            syncgap = _wtoi(optarg);
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
            output_format = optarg;
            break;
        case L'e':
            suggested_format = optarg;
            break;
        case L'v':
            verbose = 1;
            break;
        case L'x':
            tta_mode = 1;
            break;
        case L'k':
            skip_size = _wtoi64(optarg);
            break;
        case L'p':
            name_pattern = optarg;
            break;
        case L'h':
        default:
            print_usage();
            return -1;
        }
    }
#else // _WIN32
    int opt;
    while ((opt = getopt(argc, argv, "i:o:n:s:t:c:m:g:j:f:vxhk:e:p:")) != -1)
    {
        switch (opt)
        {
            case 'i':
                inputpath = optarg;
                break;
            case 'o':
                outputpath = optarg;
                break;
            case 'n':
                noise = atoi(optarg);
                break;
            case 's':
                scale = atoi(optarg);
                break;
            case 't':
                tilesize = parse_optarg_int_array(optarg);
                break;
            case 'c':
                syncgap = atoi(optarg);
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
                output_format = optarg;
                break;
            case 'e':
                suggested_format = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'x':
                tta_mode = 1;
                break;
            case 'k':
                skip_size = atoll(optarg);
                break;
            case 'p':
                name_pattern = optarg;
                break;
        case 'h':
        default:
            print_usage();
            return -1;
        }
    }
#endif // _WIN32

    if (inputpath.empty() || outputpath.empty())
    {
        print_usage();
        return -1;
    }

    if (noise < -1 || noise > 3)
    {
        fprintf(stderr, "invalid noise argument\n");
        return -1;
    }

    if (!(scale == 1 || scale == 2 || scale == 3 || scale == 4))
    {
        fprintf(stderr, "invalid scale argument\n");
        return -1;
    }

    if (tilesize.size() != (gpuid.empty() ? 1 : gpuid.size()) && !tilesize.empty())
    {
        fprintf(stderr, "invalid tilesize argument\n");
        return -1;
    }

    if (!(syncgap == 0 || syncgap == 1 || syncgap == 2 || syncgap == 3))
    {
        fprintf(stderr, "invalid syncgap argument\n");
        return -1;
    }

    for (int i=0; i<(int)tilesize.size(); i++)
    {
        if (tilesize[i] != 0 && tilesize[i] < 32)
        {
            fprintf(stderr, "invalid tilesize argument\n");
            return -1;
        }
    }

    if (jobs_load < 1 || jobs_save < 1)
    {
        fprintf(stderr, "invalid thread count argument\n");
        return -1;
    }

    if (jobs_proc.size() != (gpuid.empty() ? 1 : gpuid.size()) && !jobs_proc.empty())
    {
        fprintf(stderr, "invalid jobs_proc thread count argument\n");
        return -1;
    }

    for (int i=0; i<(int)jobs_proc.size(); i++)
    {
        if (jobs_proc[i] < 1)
        {
            fprintf(stderr, "invalid jobs_proc thread count argument\n");
            return -1;
        }
    }

    if (!path_is_directory(outputpath))
    {
        path_t ext = get_file_extension(outputpath);
        if (!ext.empty() && output_format.empty())
        {
            if (ext == PATHSTR("png") || ext == PATHSTR("PNG"))
                output_format = PATHSTR("png");
            else if (ext == PATHSTR("webp") || ext == PATHSTR("WEBP"))
                output_format = PATHSTR("webp");
            else if (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") || ext == PATHSTR("JPEG"))
                output_format = PATHSTR("jpg");
            else if (ext == PATHSTR("bmp") || ext == PATHSTR("BMP"))
                output_format = PATHSTR("bmp");
        }
        if (!output_format.empty() && !is_supported_encode_format(output_format))
        {
            fprintf(stderr, "invalid output format\n");
            return -1;
        }
    }

    std::vector<path_t> input_files;
    std::vector<path_t> output_files;
    {
        path_t effective_format = output_format.empty() ? suggested_format : output_format;

        path_t prog_name = path_t(argv[0]);
        {
            size_t last_sep = prog_name.find_last_of(PATHSTR("/\\"));
            if (last_sep != path_t::npos)
                prog_name = prog_name.substr(last_sep + 1);
            size_t dot = prog_name.rfind(PATHSTR('.'));
            if (dot != path_t::npos)
                prog_name = prog_name.substr(0, dot);
            const path_t ncnn_suffix = PATHSTR("-ncnn");
            if (prog_name.size() > ncnn_suffix.size() &&
                prog_name.compare(prog_name.size() - ncnn_suffix.size(), ncnn_suffix.size(), ncnn_suffix) == 0)
                prog_name = prog_name.substr(0, prog_name.size() - ncnn_suffix.size());
        }

        int ret = collect_input_output_files(inputpath, outputpath, effective_format, name_pattern, prog_name, input_files, output_files);
        if (ret != 0)
            return -1;

        ret = filter_files_by_size_threshold(input_files, output_files, skip_size, verbose);
        if (ret != 0)
            return -1;
    }

    int prepadding = 0;

    if (model.find(PATHSTR("models-se")) != path_t::npos
        || model.find(PATHSTR("models-nose")) != path_t::npos
        || model.find(PATHSTR("models-pro")) != path_t::npos)
    {
        if (scale == 2)
        {
            prepadding = 18;
        }
        if (scale == 3)
        {
            prepadding = 14;
        }
        if (scale == 4)
        {
            prepadding = 19;
        }
    }
    else
    {
        fprintf(stderr, "unknown model dir type\n");
        return -1;
    }

    if (model.find(PATHSTR("models-nose")) != path_t::npos)
    {
        // force syncgap off for nose models
        syncgap = 0;
    }

#if _WIN32
    wchar_t parampath[256];
    wchar_t modelpath[256];
    if (noise == -1)
    {
        swprintf(parampath, 256, L"%s/up%dx-conservative.param", model.c_str(), scale);
        swprintf(modelpath, 256, L"%s/up%dx-conservative.bin", model.c_str(), scale);
    }
    else if (noise == 0)
    {
        swprintf(parampath, 256, L"%s/up%dx-no-denoise.param", model.c_str(), scale);
        swprintf(modelpath, 256, L"%s/up%dx-no-denoise.bin", model.c_str(), scale);
    }
    else
    {
        swprintf(parampath, 256, L"%s/up%dx-denoise%dx.param", model.c_str(), scale, noise);
        swprintf(modelpath, 256, L"%s/up%dx-denoise%dx.bin", model.c_str(), scale, noise);
    }
#else
    char parampath[256];
    char modelpath[256];
    if (noise == -1)
    {
        sprintf(parampath, "%s/up%dx-conservative.param", model.c_str(), scale);
        sprintf(modelpath, "%s/up%dx-conservative.bin", model.c_str(), scale);
    }
    else if (noise == 0)
    {
        sprintf(parampath, "%s/up%dx-no-denoise.param", model.c_str(), scale);
        sprintf(modelpath, "%s/up%dx-no-denoise.bin", model.c_str(), scale);
    }
    else
    {
        sprintf(parampath, "%s/up%dx-denoise%dx.param", model.c_str(), scale, noise);
        sprintf(modelpath, "%s/up%dx-denoise%dx.bin", model.c_str(), scale, noise);
    }
#endif

    path_t paramfullpath = sanitize_filepath(parampath);
    path_t modelfullpath = sanitize_filepath(modelpath);

#if _WIN32
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

    // only create gpu instance when gpu is requested
    bool use_gpu = false;
    if (gpuid.empty())
    {
        // auto-detect: try to create gpu instance, use gpu if available
        ncnn::create_gpu_instance();
        int default_gpu = ncnn::get_default_gpu_index();
        if (default_gpu != -1)
        {
            gpuid.push_back(default_gpu);
            use_gpu = true;
        }
        else
        {
            ncnn::destroy_gpu_instance();
            gpuid.push_back(-1);
        }
    }
    else
    {
        // check if any gpu is requested (not -1)
        for (size_t i = 0; i < gpuid.size(); i++)
        {
            if (gpuid[i] != -1)
            {
                use_gpu = true;
                break;
            }
        }
        if (use_gpu)
        {
            ncnn::create_gpu_instance();
        }
    }

    const int use_gpu_count = (int)gpuid.size();

    if (jobs_proc.empty())
    {
        jobs_proc.resize(use_gpu_count, 2);
    }

    if (tilesize.empty())
    {
        tilesize.resize(use_gpu_count, 0);
    }

    int cpu_count = std::max(1, ncnn::get_cpu_count());
    jobs_load = std::min(jobs_load, cpu_count);
    jobs_save = std::min(jobs_save, cpu_count);

    // validate gpu device only when gpu is actually used
    if (use_gpu)
    {
        int gpu_count = ncnn::get_gpu_count();
        for (int i = 0; i < use_gpu_count; i++)
        {
            if (gpuid[i] == -1) continue;

            if (gpuid[i] < 0 || gpuid[i] >= gpu_count)
            {
                fprintf(stderr, "invalid gpu device\n");

                ncnn::destroy_gpu_instance();
                return -1;
            }
        }
    }

    int total_jobs_proc = 0;
    int jobs_proc_per_gpu[16] = {0};
    for (int i=0; i<use_gpu_count; i++)
    {
        if (gpuid[i] == -1)
        {
            jobs_proc[i] = std::min(jobs_proc[i], cpu_count);
            total_jobs_proc += 1;
        }
        else
        {
            total_jobs_proc += jobs_proc[i];
            jobs_proc_per_gpu[gpuid[i]] += jobs_proc[i];
        }
    }

    for (int i=0; i<use_gpu_count; i++)
    {
        if (tilesize[i] != 0)
            continue;

        if (gpuid[i] == -1)
        {
            // cpu only
            tilesize[i] = 400;
            continue;
        }

        uint32_t heap_budget = ncnn::get_gpu_device(gpuid[i])->get_heap_budget();

        if (input_files.size() > 1)
        {
            // multiple gpu jobs share the same heap
            heap_budget /= jobs_proc_per_gpu[gpuid[i]];
        }

        // more fine-grained tilesize policy here
        if (model.find(PATHSTR("models-nose")) != path_t::npos || model.find(PATHSTR("models-se")) != path_t::npos || model.find(PATHSTR("models-pro")) != path_t::npos)
        {
            if (scale == 2)
            {
                if (heap_budget > 1300)
                    tilesize[i] = 400;
                else if (heap_budget > 800)
                    tilesize[i] = 300;
                else if (heap_budget > 400)
                    tilesize[i] = 200;
                else if (heap_budget > 200)
                    tilesize[i] = 100;
                else
                    tilesize[i] = 32;
            }
            if (scale == 3)
            {
                if (heap_budget > 3300)
                    tilesize[i] = 400;
                else if (heap_budget > 1900)
                    tilesize[i] = 300;
                else if (heap_budget > 950)
                    tilesize[i] = 200;
                else if (heap_budget > 320)
                    tilesize[i] = 100;
                else
                    tilesize[i] = 32;
            }
            if (scale == 4)
            {
                if (heap_budget > 1690)
                    tilesize[i] = 400;
                else if (heap_budget > 980)
                    tilesize[i] = 300;
                else if (heap_budget > 530)
                    tilesize[i] = 200;
                else if (heap_budget > 240)
                    tilesize[i] = 100;
                else
                    tilesize[i] = 32;
            }
        }
    }

    {
        std::vector<RealCUGAN*> realcugan(use_gpu_count);

        for (int i=0; i<use_gpu_count; i++)
        {
            int num_threads = gpuid[i] == -1 ? jobs_proc[i] : 1;

            realcugan[i] = new RealCUGAN(gpuid[i], tta_mode, num_threads);

            realcugan[i]->load(paramfullpath, modelfullpath);

            realcugan[i]->noise = noise;
            realcugan[i]->scale = scale;
            realcugan[i]->tilesize = tilesize[i];
            realcugan[i]->prepadding = prepadding;
            realcugan[i]->syncgap = syncgap;
        }

        // main routine
        {
            // load image
            LoadThreadParams ltp;
            ltp.scale = scale;
            ltp.output_format = output_format;
            ltp.jobs_load = jobs_load;
            ltp.input_files = input_files;
            ltp.output_files = output_files;

            ncnn::Thread load_thread(load, (void*)&ltp);

            // realcugan proc
            std::vector<ProcThreadParams> ptp(use_gpu_count);
            for (int i=0; i<use_gpu_count; i++)
            {
                ptp[i].realcugan = realcugan[i];
            }

            std::vector<ncnn::Thread*> proc_threads(total_jobs_proc);
            {
                int total_jobs_proc_id = 0;
                for (int i=0; i<use_gpu_count; i++)
                {
                    if (gpuid[i] == -1)
                    {
                        proc_threads[total_jobs_proc_id++] = new ncnn::Thread(proc, (void*)&ptp[i]);
                    }
                    else
                    {
                        for (int j=0; j<jobs_proc[i]; j++)
                        {
                            proc_threads[total_jobs_proc_id++] = new ncnn::Thread(proc, (void*)&ptp[i]);
                        }
                    }
                }
            }

            // save image
            SaveThreadParams stp;
            stp.verbose = verbose;

            std::vector<ncnn::Thread*> save_threads(jobs_save);
            for (int i=0; i<jobs_save; i++)
            {
                save_threads[i] = new ncnn::Thread(save, (void*)&stp);
            }

            // end
            load_thread.join();

            Task end;
            end.id = -233;

            for (int i=0; i<total_jobs_proc; i++)
            {
                toproc.put(end);
            }

            for (int i=0; i<total_jobs_proc; i++)
            {
                proc_threads[i]->join();
                delete proc_threads[i];
            }

            for (int i=0; i<jobs_save; i++)
            {
                tosave.put(end);
            }

            for (int i=0; i<jobs_save; i++)
            {
                save_threads[i]->join();
                delete save_threads[i];
            }
        }

        for (int i=0; i<use_gpu_count; i++)
        {
            delete realcugan[i];
        }
        realcugan.clear();
    }

    if (use_gpu)
    {
        ncnn::destroy_gpu_instance();
    }

    return 0;
}
