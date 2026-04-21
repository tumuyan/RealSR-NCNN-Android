// srmd implemented with ncnn library

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

#include "srmd.h"

#include "filesystem_utils.h"
#include "image_processor.h"
#include <opencv2/opencv.hpp>
#include <opencv2/core/hal/interface.h>
#include "utils.hpp"
using namespace cv;

static void print_usage()
{
    fprintf(stderr, "Usage: srmd-ncnn -i infile -o outfile [options]...\n\n");
    fprintf(stderr, "  -h                   show this help\n");
    fprintf(stderr, "  -v                   verbose output\n");
    fprintf(stderr, "  -i input-path        input image path (jpg/png/webp/bmp) or directory (recursive)\n");
    fprintf(stderr, "  -o output-path       output image path (jpg/png/webp/bmp) or directory\n");
    fprintf(stderr, "  -n noise-level       denoise level (-1/0/1/2/3/4/5/6/7/8/9/10, default=3)\n");
    fprintf(stderr, "  -s scale             upscale ratio (2/3/4, default=2)\n");
    fprintf(stderr, "  -t tile-size         tile size (>=32/0=auto, default=0) can be 0,0,0 for multi-gpu\n");
    fprintf(stderr, "  -m model-path        srmd model path (default=models-srmd)\n");
    fprintf(stderr, "  -g gpu-id            gpu device to use (default=auto) can be 0,1,2 for multi-gpu\n");
    fprintf(stderr, "  -j load:proc:save    thread count for load/proc/save (default=1:2:2) can be 1:2,2,2:2 for multi-gpu\n");
    fprintf(stderr, "  -x                   enable tta mode\n");
    fprintf(stderr, "  -f format            output image format (jpg/png/webp/bmp, default=input extension)\n");
}

class Task
{
public:
    int id;

    path_t inpath;
    path_t outpath;

    ncnn::Mat inimage;
    ncnn::Mat outimage;
    ncnn::Mat inalpha;  
    int has_alpha;
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
        
        if (inBGR.empty())
        {
            continue;
        }
        
        Task v;
        v.id = i;
        v.inpath = imagepath;
        v.outpath = ltp->output_files[i];
        v.has_alpha = 0;

        int w = inBGR.cols;
        int h = inBGR.rows;
        int c = inBGR.channels();

        if (!inAlpha.empty())
        {
            v.has_alpha = 1;
            
            unsigned char* alphadata = (unsigned char*)malloc(w * h);
            memcpy(alphadata, inAlpha.data, w * h);
            v.inalpha = ncnn::Mat(w, h, (void*)alphadata, (size_t)1, 1);
            
            path_t ext = get_file_extension(v.outpath);
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
        }

        unsigned char* pixeldata = (unsigned char*)malloc(w * h * c);
        memcpy(pixeldata, inBGR.data, w * h * c);
        
        v.inimage = ncnn::Mat(w, h, (void*)pixeldata, (size_t)c, c);
        v.outimage = ncnn::Mat(w * scale, h * scale, (size_t)c, c);

        toproc.put(v);
    }

    return 0;
}

class ProcThreadParams
{
public:
    const SRMD* srmd;
};

void* proc(void* args)
{
    const ProcThreadParams* ptp = (const ProcThreadParams*)args;
    const SRMD* srmd = ptp->srmd;

    for (;;)
    {
        Task v;

        toproc.get(v);

        if (v.id == -233)
            break;

        srmd->process(v.inimage, v.outimage);

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

        int success = 0;

        path_t ext = get_file_extension(v.outpath);

        // if (ext != PATHSTR("gif")) 
        {
            cv::Mat image;
            
            // outimage.elempack 永远为3
            if (v.has_alpha)
            {
                cv::Mat rgb_image(v.outimage.h, v.outimage.w, CV_8UC3, v.outimage.data);
                cv::Mat alpha_image(v.inalpha.h, v.inalpha.w, CV_8UC1, v.inalpha.data);
                cv::Mat scaled_alpha;
                cv::resize(alpha_image, scaled_alpha, cv::Size(rgb_image.cols, rgb_image.rows), 0, 0, cv::INTER_CUBIC);
                
                std::vector<cv::Mat> channels;
                cv::split(rgb_image, channels);
                channels.push_back(scaled_alpha);
                cv::merge(channels, image);
            }
            else
            {
                image = cv::Mat(v.outimage.h, v.outimage.w, CV_8UC3, v.outimage.data);
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
        
        // free input pixel data after processing
        {
            if (v.inimage.data)
            {
                free(v.inimage.data);
            }
            
            if (v.has_alpha && v.inalpha.data)
            {
                free(v.inalpha.data);
            }
        }
        
        if (success)
        {
            if (verbose)
            {
#if _WIN32
                fwprintf(stderr, L"%ls -> %ls done\n", v.inpath.c_str(), v.outpath.c_str());
#else
                fprintf(stderr, "%s -> %s done\n", v.inpath.c_str(), v.outpath.c_str());
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
    int noise = 3;
    int scale = 2;
    std::vector<int> tilesize;
    path_t model = PATHSTR("models-srmd");
    std::vector<int> gpuid;
    int jobs_load = 1;
    std::vector<int> jobs_proc;
    int jobs_save = 2;
    int verbose = 0;
    int tta_mode = 0;
    path_t format;

#if _WIN32
    setlocale(LC_ALL, "");
    wchar_t opt;
    while ((opt = getopt(argc, argv, L"i:o:n:s:t:m:g:j:f:vxh")) != (wchar_t)-1)
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
        case L'h':
        default:
            print_usage();
            return -1;
        }
    }
#else // _WIN32
    int opt;
    while ((opt = getopt(argc, argv, "i:o:n:s:t:m:g:j:f:vxh")) != -1)
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

    if (noise < -1 || noise > 10 || scale < 2 || scale > 4)
    {
        fprintf(stderr, "invalid noise or scale argument\n");
        return -1;
    }

    if (tilesize.size() != (gpuid.empty() ? 1 : gpuid.size()) && !tilesize.empty())
    {
        fprintf(stderr, "invalid tilesize argument\n");
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
        if (!ext.empty() && format.empty())
        {
            if (ext == PATHSTR("png") || ext == PATHSTR("PNG"))
                format = PATHSTR("png");
            else if (ext == PATHSTR("webp") || ext == PATHSTR("WEBP"))
                format = PATHSTR("webp");
            else if (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") || ext == PATHSTR("JPEG"))
                format = PATHSTR("jpg");
            else if (ext == PATHSTR("bmp") || ext == PATHSTR("BMP"))
                format = PATHSTR("bmp");
        }
        if (!format.empty() && !is_supported_encode_format(format))
        {
            fprintf(stderr, "invalid output format\n");
            return -1;
        }
    }

    std::vector<path_t> input_files;
    std::vector<path_t> output_files;
    {
        int ret = collect_input_output_files(inputpath, outputpath, format, input_files, output_files);
        if (ret != 0)
            return -1;
    }

    int prepadding = 0;

    if (model.find(PATHSTR("models-srmd")) != path_t::npos)
    {
        prepadding = 12;
    }
    else
    {
        fprintf(stderr, "unknown model dir type\n");
        return -1;
    }

#if _WIN32
    wchar_t parampath[256];
    wchar_t modelpath[256];
    if (noise == -1)
    {
        swprintf(parampath, 256, L"%s/srmdnf_x%d.param", model.c_str(), scale);
        swprintf(modelpath, 256, L"%s/srmdnf_x%d.bin", model.c_str(), scale);
    }
    else
    {
        swprintf(parampath, 256, L"%s/srmd_x%d.param", model.c_str(), scale);
        swprintf(modelpath, 256, L"%s/srmd_x%d.bin", model.c_str(), scale);
    }
#else
    char parampath[256];
    char modelpath[256];
    if (noise == -1)
    {
        sprintf(parampath, "%s/srmdnf_x%d.param", model.c_str(), scale);
        sprintf(modelpath, "%s/srmdnf_x%d.bin", model.c_str(), scale);
    }
    else
    {
        sprintf(parampath, "%s/srmd_x%d.param", model.c_str(), scale);
        sprintf(modelpath, "%s/srmd_x%d.bin", model.c_str(), scale);
    }
#endif

    path_t paramfullpath = sanitize_filepath(parampath);
    path_t modelfullpath = sanitize_filepath(modelpath);

#if _WIN32
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

    ncnn::create_gpu_instance();

    if (gpuid.empty())
    {
        gpuid.push_back(ncnn::get_default_gpu_index());
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

    int gpu_count = ncnn::get_gpu_count();
    for (int i=0; i<use_gpu_count; i++)
    {
        if (gpuid[i] < 0 || gpuid[i] >= gpu_count)
        {
            fprintf(stderr, "invalid gpu device\n");

            ncnn::destroy_gpu_instance();
            return -1;
        }
    }

    int total_jobs_proc = 0;
    for (int i=0; i<use_gpu_count; i++)
    {
        int gpu_queue_count = ncnn::get_gpu_info(gpuid[i]).compute_queue_count();
        jobs_proc[i] = std::min(jobs_proc[i], gpu_queue_count);
        total_jobs_proc += jobs_proc[i];
    }

    for (int i=0; i<use_gpu_count; i++)
    {
        if (tilesize[i] != 0)
            continue;

        uint32_t heap_budget = ncnn::get_gpu_device(gpuid[i])->get_heap_budget();

        // more fine-grained tilesize policy here
        if (model.find(PATHSTR("models-srmd")) != path_t::npos)
        {
            if (heap_budget > 2600)
                tilesize[i] = 400;
            else if (heap_budget > 740)
                tilesize[i] = 200;
            else if (heap_budget > 250)
                tilesize[i] = 100;
            else
                tilesize[i] = 32;
        }
    }

    {
        std::vector<SRMD*> srmd(use_gpu_count);

        for (int i=0; i<use_gpu_count; i++)
        {
            srmd[i] = new SRMD(gpuid[i], tta_mode);

            srmd[i]->load(paramfullpath, modelfullpath);

            srmd[i]->noise = noise;
            srmd[i]->scale = scale;
            srmd[i]->tilesize = tilesize[i];
            srmd[i]->prepadding = prepadding;
        }

        // main routine
        {
            // load image
            LoadThreadParams ltp;
            ltp.scale = scale;
            ltp.jobs_load = jobs_load;
            ltp.input_files = input_files;
            ltp.output_files = output_files;

            ncnn::Thread load_thread(load, (void*)&ltp);

            // srmd proc
            std::vector<ProcThreadParams> ptp(use_gpu_count);
            for (int i=0; i<use_gpu_count; i++)
            {
                ptp[i].srmd = srmd[i];
            }

            std::vector<ncnn::Thread*> proc_threads(total_jobs_proc);
            {
                int total_jobs_proc_id = 0;
                for (int i=0; i<use_gpu_count; i++)
                {
                    for (int j=0; j<jobs_proc[i]; j++)
                    {
                        proc_threads[total_jobs_proc_id++] = new ncnn::Thread(proc, (void*)&ptp[i]);
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
            delete srmd[i];
        }
        srmd.clear();
    }

    ncnn::destroy_gpu_instance();

    return 0;
}
