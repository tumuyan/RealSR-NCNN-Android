// classical resize implemented with ncnn library


#include <cstdio>
#include <algorithm>
#include <queue>
#include <vector>
#include <clocale>

#define _DEMO_PATH  false
#define _VERBOSE_LOG  true
#define _MODE 2

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

#include <opencv2/opencv.hpp>
#include <opencv2/core/hal/interface.h>
using namespace cv;

#endif // _WIN32

#include "webp_image.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "avir.h"
#include "lancir.h"
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
#include <limits>

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

//#include "realsr.h"
//#include "resize.h"

#include "filesystem_utils.h"

static void print_usage() {
    fprintf(stderr, "Usage: resize-ncnn -i infile -o outfile [options]...\n\n");
    fprintf(stderr, "  -h                   show this help\n");
    fprintf(stderr, "  -v                   verbose output\n");
    fprintf(stderr, "  -i input-path        input image path (jpg/png/webp) or directory\n");
    fprintf(stderr, "  -o output-path       output image path (jpg/png/webp) or directory\n");
    fprintf(stderr, "  -s scale             upscale ratio (4, default=4)\n");
    fprintf(stderr, "  -m mode        resize mode (bicubic/bilinear/nearest/avir/avir-lancir/de-nearest, default=nearest)\n");
    fprintf(stderr, "  -n not-use-ncnn        bicubic/bilinear not use ncnn\n");
    fprintf(stderr, "  -f format            output image format (jpg/png/webp, default=ext/png)\n");
}

#if _WIN32
const std::wstring& optarg_in (L"A:\\Media\\realsr-ncnn-vulkan-20210210-windows\\input3.jpg");
const std::wstring& optarg_out(L"A:\\Media\\realsr-ncnn-vulkan-20210210-windows\\output3.jpg");
const std::wstring& optarg_mo (L"A:\\Media\\realsr-ncnn-vulkan-20210210-windows\\models-Real-ESRGAN-anime");
#else
const char *optarg_in = "/sdcard/10086/input3.jpg";
const char *optarg_out = "/sdcard/10086/output3.jpg";
const char *optarg_mo = "nearest";

void pretty_print(ncnn::Mat mat);

#endif

#if _WIN32
int wmain(int argc, wchar_t** argv)
#else

int main(int argc, char **argv)
#endif
{
    path_t inputpath;
    path_t outputpath;
    int scale = 4;
    double scale_d = 4;
    std::vector<int> tilesize;
#if _DEMO_PATH
    path_t model = optarg_mo;
#else
    path_t model = PATHSTR("nearest");
#endif
    int verbose = 0;
    bool not_use_ncnn = false;
    path_t format = PATHSTR("png");

#if _WIN32
    setlocale(LC_ALL, "");
    wchar_t opt;
    while ((opt = getopt(argc, argv, L"i:o:s:t:m:g:j:f:vxhn")) != (wchar_t)-1)
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

        case L'f':
            format = optarg;
            break;
        case L'v':
            verbose = 1;
            break;
        case L'n':
            not_use_ncnn = true;
            break;
        case L't':
        case L'g':
            break;
        case L'h':
        default:
            print_usage();
            return -1;
        }
    }
#else // _WIN32
    int opt;
    while ((opt = getopt(argc, argv, "i:o:s:t:m:g:j:f:vxhn")) != -1) {
        switch (opt) {
            case 'i':
                inputpath = optarg;
                break;
            case 'o':
                outputpath = optarg;
                break;
            case 's':
                scale = atoi(optarg);
                char *endptr;
                scale_d = strtod(optarg, &endptr);
                // printf("scale endptr=%s=,arg=%s=,scale=%f\n", endptr ,optarg,scale_d);
                break;
            case 'm':
                model = optarg;
                break;
            case 'f':
                format = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'n':
                not_use_ncnn = true;
                break;
            case 'g':
            case 't':
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

                // filename list is sorted, check if output image path conflicts
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

    if (model.find(PATHSTR("bicubic")) != path_t::npos ||
        model.find(PATHSTR("bilinear")) != path_t::npos ||
        model.find(PATHSTR("nearest")) != path_t::npos ||
        model.find(PATHSTR("avir")) != path_t::npos
            ) {
        prepadding = 10;
    } else {
        fprintf(stderr, "unknown mode type\n");
        return -1;
    }


//    ncnn::create_gpu_instance();


    if (verbose)
        fprintf(stderr, "init classical resize\n");
    else
        fprintf(stderr, "busy...\n");

    for (int i = 0; i < input_files.size(); i++) {
        const path_t &imagepath = input_files[i];
        path_t &outputpath = output_files[i];

//        int webp = 0;

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
                if (!pixeldata) {
//                    webp = 1;
//                } else {
                    // not webp, try jpg png etc.
#if _WIN32
                    pixeldata = wic_decode_image(imagepath.c_str(), &w, &h, &c);
#else // _WIN32
                    pixeldata = stbi_load_from_memory(filedata, length, &w, &h, &c, 0);


                    fprintf(stderr, "pixeldata stbi_load w/h/c %d/%d/%d \n",
                            w, h, c
                    );

                    if (pixeldata) {
                        // stb_image auto channel
                        if (c == 1) {
                            // grayscale -> rgb
                            stbi_image_free(pixeldata);
                            pixeldata = stbi_load_from_memory(filedata, length, &w, &h, &c, 3);
                            c = 3;
                            fprintf(stderr, "pixeldata c==1 w/h/c %d/%d/%d \n",
                                    w, h, c
                            );
                        } else if (c == 2) {
                            // grayscale + alpha -> rgba
                            stbi_image_free(pixeldata);
                            pixeldata = stbi_load_from_memory(filedata, length, &w, &h, &c, 4);
                            c = 4;
                            fprintf(stderr, "pixeldata c==2 w/h/c %d/%d/%d \n",
                                    w, h, c
                            );
                        }
                    }
#endif // _WIN32
                }
            } else if (_VERBOSE_LOG) {
#if _WIN32
                fwprintf(stderr, L"no filedata\n");
#else // _WIN32
                fprintf(stderr, "no filedata\n");
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


        // 计算降采样的倍率
        if (pixeldata) {
            if (model.find(PATHSTR("de-nearest")) != path_t::npos) {
                double lost_y[h - 1], lost_x[w - 1];
                double avg_y = 0, avg_x = 0;
                double scale_y = 1, scale_x = 1;
                int line_size = w * c;

                double **lost_x0 = new double *[h];
                for (int i = 0; i < h; i++) {
                    lost_x0[i] = new double[w - 1];
                }

//                fprintf(stderr, "de-nearest:");

                //row
                int p = line_size;
                int q = 0;
                for (int i = 0; i < h - 1; i++) {
                    //line
                    double l = 0;
                    for (int j = 0; j < line_size; j++) {
                        l += pow(pixeldata[p] - pixeldata[q], 2);
                        p++;
                        q++;
                        //   fprintf(stderr," [%d,%d]",i ,j);
                    }

                    double m = l / line_size;
                    lost_y[i] = m;
                    avg_y += m;
                }

                avg_y = avg_y / (h - 1);

                for (int i = 0; i < h - 1; i++) {
                    if (lost_y[i] > avg_y) {
//                         fprintf(stderr," [%.2f]", lost_y[i]);
                        scale_y++;
                    } //else
//                         fprintf(stderr," %.2f", lost_y[i]);
                }
                fprintf(stderr, "scale_y: %d/%f", h, scale_y);
                scale_y = h / scale_y;
                fprintf(stderr, " = %f; avg_lost_y=[%f]\n", scale_y, avg_y);

                for (int i = 0; i < h; i++) {
                    p = i * line_size;
                    for (int j = 0; j < w - 1; j++) {
                        double l = 0;
                        for (int k = 0; k < c; k++) {
//                            l += pow(pixeldata[p] - pixeldata[p + c], 2);
                            l = fmax(l, pow(pixeldata[p] - pixeldata[p + c], 2));
                            p++;
                        }
                        lost_x0[i][j] = l;

                        // fprintf(stderr," [%d,%d]",i ,j);
                    }
//                    fprintf(stderr, " %d", i);
                }

                for (int j = 0; j < w - 1; j++) {
                    double l = 0;
                    for (int i = 0; i < h; i++) {
                        l += lost_x0[i][j];
                    }
                    double m = l / h;
                    lost_x[j] = m;
                    avg_x += m;
                }

                avg_x = avg_x / (w - 1);

                for (int i = 0; i < w - 1; i++) {
                    if (lost_x[i] > avg_x) {
//                    fprintf(stderr," [%.2f]", lost_x[i]);
                        scale_x++;
                    } // else
//                    fprintf(stderr," %.2f", lost_x[i]);
                }
                fprintf(stderr, "scale_x: %d/%f", w, scale_x);
                scale_x = w / scale_x;
                fprintf(stderr, " = %f; avg_lost_x=[%f]\n", scale_x, avg_x);

                for (int i = 0; i < h - 1; i++) {
                    delete[] lost_x0[i];
                }
                delete[] lost_x0;

                if (scale_x < 1.5 || scale_y < 1.5) {
                    fprintf(stderr, "image is not interpolated by nearest\n");
                } else {
                    not_use_ncnn = false;
                    if (abs(scale_y - scale_x) < 1)
                        scale = round((scale_x + scale_y) / 2);
                    else if (scale_x > scale_y)
                        scale = round(scale_y);
                    else
                        scale = round(scale_x);

                    scale_d = 1 / scale_d;
                    fprintf(stderr, "image might be interpolated by nearest x%d\n", scale);
                }
            }
        }


        int out_w, out_h;
        if ( scale_d<1 ) {
            out_w = (int) (w * scale_d);
            out_h = (int) (h * scale_d);
        } else {
            out_w = w * scale;
            out_h = h * scale;
        }
        int out_line_size = out_w * c;
        unsigned char *buf = new unsigned char[out_line_size * out_h];
        fprintf(stderr, "output w/h/s/s = %d/%d/%d/%f, mode=%s\n", out_w, out_h, scale, scale_d,
                model.c_str());

        if (pixeldata) {

            if (verbose) {
                fprintf(stderr, "pixeldata: ");
                for (int i = 0; i < 32; i++) {
                    fprintf(stderr, " %08X", pixeldata[i]);
                }
                fprintf(stderr, "\n");
            }

            high_resolution_clock::time_point process_begin = high_resolution_clock::now();


            if (not_use_ncnn && model.find(PATHSTR("nearest")) != path_t::npos) {

                if (_MODE == 1) {

                    //row
                    for (int i = 0; i < h; i++) {
                        //line
                        for (int j = 0; j < w; j++) {
                            int p = (w * i + j) * c;
                            int q = (out_w * i + j) * c * scale;

                            //sub-pixel
                            for (int k = 0; k < c; k++) {
                                int m = pixeldata[p + k];

                                // output x offset
                                int x_offset = 0;
                                for (int n = 0; n < scale; n++) {
                                    int o = q + k + x_offset;

                                    // output y offset
                                    int y_offset = 0;
                                    for (int g = 0; g < scale; g++) {
                                        buf[o + y_offset] = m;
                                        y_offset += out_line_size;
                                    }
                                    x_offset += c;
                                }

                                if (verbose) {
                                    fprintf(stderr, "i:%d j:%d :", i, j);
                                    for (int i = 0; i < 32; i++) {
                                        fprintf(stderr, " %02X", buf[i]
                                        );
                                    }
                                    fprintf(stderr, " ,m=%d %d\n", p + k, k + q);
                                }
                            }
                        }
                    }
                } else {
                    if(scale_d>=1)
                    {
                        int p = 0;
                        int q = 0;
                        //row
                        for (int i = 0; i < h; i++) {
                            //line
                            void *l = buf + q;
                            for (int j = 0; j < w; j++) {

                                //pixel
                                for (int k = 0; k < scale; k++) {
                                    memcpy(buf + q, pixeldata + p, sizeof(unsigned char) * c);
                                    q += c;
                                }
                                p += c;
                            }
                            // mult-line
                            for (int k = 1; k < scale; k++) {
                                memcpy(buf + q, l, sizeof(unsigned char) * out_line_size);
                                q += out_line_size;
                            }
                        }
                    }else{
                        // de-nearest
                        int p=(scale-1)/2, q=0;
                        // double step = 1/scale_d;
                        // row
                        for(int i=0;i<out_h;i++){
                            for(int j=0;j<out_w;j++){
                                memcpy(buf + q, pixeldata + p+j*scale, sizeof(unsigned char) * c);
                                q+=c;
                            }
                            p+=w*c;
                        }
                    }
                }

            } else if (not_use_ncnn && model.find(PATHSTR("bilinear")) != path_t::npos) {
                if (w < 2 && h < 2) {
                    fprintf(stderr, "[Err]image is too small\n");
                    return -1;
                }
                if (scale < 2) {
                    fprintf(stderr, "[Err]scale <2\n");
                    return -1;
                }


                out_w = out_w - scale + 1;
                out_h = out_h - scale + 1;
                out_line_size = out_w * c;
                free(buf);
                buf = new unsigned char[out_line_size * out_h];

                // 子像素a点，b点
                unsigned char a, b;
                int p = 0;
                int q = 0;

                int block_size = scale * out_line_size;

                // channel
                for (int l = 0; l < c; l++) {
                    p = l;
                    q = l;
                    for (int i = 0; i < h; i++) {
                        for (int j = 1; j < w; j++) {

                            a = pixeldata[p];
                            b = pixeldata[p + c];
                            if (a == b) {
                                for (int k = 0; k < scale; k++) {
                                    buf[q] = a;
                                    q += c;
                                }
                            } else {
                                float d = ((float) (b - a)) / scale;
                                float e = 0;
                                for (int k = 0; k < scale; k++) {
                                    buf[q] = a + (int) (e);
                                    e += d;
                                    q += c;
                                }
                            }

                            p += c;
                        }
                        buf[q] = b;
                        p += c;
                        q = q + block_size - out_line_size + c;
                        //                        fprintf(stderr, " %d-%d",l,i);
                    }

                }

                //                fprintf(stderr, "\n 2nd\n");
                //
                //                fprintf(stderr, "out_line_size=%d block=%d\n", out_line_size, block_size);
                p = 0;
                for (int i = 0; i < h; i++) {

                    for (int j = 0; j < out_line_size; j++) {
                        a = buf[p];
                        q = out_line_size;
                        b = buf[p + block_size];
                        if (a == b) {
                            for (int k = 1; k < scale; k++) {
                                buf[p + q] = a;
                                q += out_line_size;
                            }
                        } else {
                            float d = ((float) (b - a)) / scale;
                            float e = d;
                            for (int k = 1; k < scale; k++) {
                                buf[p + q] = a + (int) (e);
                                //                            fprintf(stderr, "i-j:%d-%d p-q-k:%d-%d-%d\n",i,j, p, q,k);
                                e += d;
                                q += out_line_size;
                            }

                        }

                        p++;
                    }
                    p = p + block_size - out_line_size;

                }

            } else if (model.find(PATHSTR("lancir")) != path_t::npos) {
                avir::CLancIR ImageResizer;
                ImageResizer.resizeImage(pixeldata, w, h, 0, buf, out_w, out_h, 0, c);
            } else if (model.find(PATHSTR("avir")) != path_t::npos) {
                avir::CImageResizer<> ImageResizer(8);
                ImageResizer.resizeImage(pixeldata, w, h, 0, buf, out_w, out_h, c, 0);
            } else {
                ncnn::Mat in, out;
                if (c == 4) {
                    in = ncnn::Mat::from_pixels(pixeldata, ncnn::Mat::PIXEL_RGBA, w, h);
                } else {
                    in = ncnn::Mat::from_pixels(pixeldata, ncnn::Mat::PIXEL_RGB, w, h);
                }

                if (model.find(PATHSTR("nearest")) != path_t::npos) {
                    ncnn::resize_nearest(in, out, out_w, out_h);
                } else if (model.find(PATHSTR("bilinear")) != path_t::npos) {
                    ncnn::resize_bilinear(in, out, out_w, out_h);
                } else {
                    ncnn::resize_bicubic(in, out, out_w, out_h);
                }

                if (c == 4) {
                    out.to_pixels(buf, ncnn::Mat::PIXEL_RGBA);
                } else {
                    out.to_pixels(buf, ncnn::Mat::PIXEL_RGB);
                }
            }

            high_resolution_clock::time_point process_end = high_resolution_clock::now();
            duration<double> process_time_span = duration_cast<duration<double>>(process_end - process_begin);
            fprintf(stderr, "%s use time: %.3lf\n", model.c_str(),process_time_span.count());



            if (verbose) {
                fprintf(stderr, "buf data: ");
                for (int i = 0; i < 32; i++) {
                    fprintf(stderr, " %02X", buf[i]
                    );
                }
                fprintf(stderr, "\n");
            }

            path_t ext = get_file_extension(outputpath);
            if (c == 4 &&
                (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") ||
                 ext == PATHSTR("JPEG"))) {
                path_t output_filename2 = output_files[i] + PATHSTR(".png");
                outputpath = output_filename2;
#if _WIN32
                fwprintf(stderr, L"image %ls has alpha channel ! %ls will output %ls\n", imagepath.c_str(), imagepath.c_str(), output_filename2.c_str());
#else // _WIN32
                fprintf(stderr, "image %s has alpha channel ! %s will output %s\n",
                        imagepath.c_str(),
                        imagepath.c_str(), output_filename2.c_str());
#endif // _WIN32
            }

            // save
            high_resolution_clock::time_point save_begin = high_resolution_clock::now();

            {
                int success = 0;

                path_t ext = get_file_extension(imagepath);

#if _WIN32

#else
                if (ext != PATHSTR("gif")) {
                    // 使用opencv保存图片，速度比默认的stb更快
                    cv::Mat image;
                    switch (c) {
                        case 1:
                            image = cv::Mat( out_h, out_w, CV_8UC1, buf); // 单通道图像
                            break;
                        case 3:
                            image = cv::Mat(out_h, out_w, CV_8UC3, buf); // 3通道图像
                            cv::cvtColor(image, image, cv::COLOR_RGB2BGR);
                            break;
                        case 4:
                            image = cv::Mat(out_h, out_w, CV_8UC4, buf); // 4通道图像
                            cv::cvtColor(image, image, cv::COLOR_RGBA2BGRA);
                            break;
                    }
                    if (image.empty()) {
                        std::cerr << "Error: Image data not loaded." << std::endl;
                        success = false;
                    } else {
                        success = imwrite(outputpath.c_str(), image);
                        fprintf(stderr, "opencv save image success, c=%d, w=%d, h=%d\n", c, out_w, out_h);
                    }
                }else
#endif
                if (ext == PATHSTR("webp") || ext == PATHSTR("WEBP")) {
                    success = webp_save(outputpath.c_str(), out_w, out_h, c, buf);

                } else if (ext == PATHSTR("png") || ext == PATHSTR("PNG")) {
#if _WIN32
                    success = wic_encode_image(outputpath.c_str(), outimage.w, outimage.h, outimage.elempack, outimage.data);
#else
//                    success = stbi_write_png(outputpath.c_str(), out_w, out_h, c, buf, out_w * c);
                    success = stbi_write_png(outputpath.c_str(), out_w, out_h, c, buf, 0);

#endif
                } else if (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") ||
                           ext == PATHSTR("jpeg") ||
                           ext == PATHSTR("JPEG")) {
#if _WIN32
                    success = wic_encode_jpeg_image(outputpath.c_str(), outimage.w, outimage.h, outimage.elempack, outimage.data);
#else
                    success = stbi_write_jpg(outputpath.c_str(), out_w, out_h, c, buf, 100);
#endif
                }
                if (success) {
                    if (verbose) {
#if _WIN32
                        fwprintf(stderr, L"%ls -> %ls done\n", imagepath.c_str(), outputpath.c_str());
#else
                        fprintf(stderr, "%s -> %s done\n", imagepath.c_str(), outputpath.c_str());
#endif
                    }
                } else {
#if _WIN32
                    fwprintf(stderr, L"encode image %ls failed\n", outputpath.c_str());
#else
                    fprintf(stderr, "encode image %s failed\n", outputpath.c_str());
#endif
                }
            }

            high_resolution_clock::time_point save_end = high_resolution_clock::now();
            duration<double> time_span = duration_cast<duration<double>>(save_end - save_begin);
            fprintf(stderr, "Total use time: %.3lf\n",time_span.count());



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

void pretty_print(ncnn::Mat m) {
    for (int q = 0; q < m.c; q++) {
        const int *ptr = m.channel(q);
        for (int y = 0; y < m.h; y++) {
            for (int x = 0; x < m.w; x++) {
                printf("%08X ", ptr[x]);
            }
            ptr += m.w;
            printf("\n");

        }
        printf("------------------------\n");
    }
}
