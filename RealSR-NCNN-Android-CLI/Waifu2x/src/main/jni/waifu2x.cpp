// waifu2x implemented with ncnn library

#include "waifu2x.h"

#include <algorithm>
#include <vector>

#include "waifu2x_preproc.comp.hex.h"
#include "waifu2x_postproc.comp.hex.h"
#include "waifu2x_preproc_tta.comp.hex.h"
#include "waifu2x_postproc_tta.comp.hex.h"

Waifu2x::Waifu2x(int gpuid, bool _tta_mode, int num_threads)
{
    vkdev = gpuid == -1 ? 0 : ncnn::get_gpu_device(gpuid);

    net.opt.num_threads = num_threads;

    waifu2x_preproc = 0;
    waifu2x_postproc = 0;
    tta_mode = _tta_mode;
}

Waifu2x::~Waifu2x()
{
    // cleanup preprocess and postprocess pipeline
    {
        delete waifu2x_preproc;
        delete waifu2x_postproc;
    }
}

#if _WIN32
int Waifu2x::load(const std::wstring& parampath, const std::wstring& modelpath)
#else
int Waifu2x::load(const std::string& parampath, const std::string& modelpath)
#endif
{

    net.opt.use_vulkan_compute = vkdev ? true : false;
    net.opt.use_fp16_packed = true;
    net.opt.use_fp16_storage = true;
    net.opt.use_fp16_arithmetic = true;
    net.opt.use_int8_storage = true;

    if (vkdev) net.set_vulkan_device(vkdev);

#if _WIN32
    {
        FILE* fp = _wfopen(parampath.c_str(), L"rb");
        if (!fp)
        {
            fwprintf(stderr, L"_wfopen %ls failed\n", parampath.c_str());
        }

        net.load_param(fp);

        fclose(fp);
    }
    {
        FILE* fp = _wfopen(modelpath.c_str(), L"rb");
        if (!fp)
        {
            fwprintf(stderr, L"_wfopen %ls failed\n", modelpath.c_str());
        }

        net.load_model(fp);

        fclose(fp);
    }
#else
    net.load_param(parampath.c_str());
    net.load_model(modelpath.c_str());
#endif

    // initialize preprocess and postprocess pipeline
    if (vkdev)
    {
        std::vector<ncnn::vk_specialization_type> specializations(1);
#if _WIN32
        specializations[0].i = 1;
#else
        specializations[0].i = 0;
#endif

        {
            static std::vector<uint32_t> spirv;
            static ncnn::Mutex lock;
            {
                ncnn::MutexLockGuard guard(lock);
                if (spirv.empty())
                {
                    if (tta_mode)
                        compile_spirv_module(waifu2x_preproc_tta_comp_data, sizeof(waifu2x_preproc_tta_comp_data), net.opt, spirv);
                    else
                        compile_spirv_module(waifu2x_preproc_comp_data, sizeof(waifu2x_preproc_comp_data), net.opt, spirv);
                }
            }

            waifu2x_preproc = new ncnn::Pipeline(vkdev);
            waifu2x_preproc->set_optimal_local_size_xyz(8, 8, 3);
            waifu2x_preproc->create(spirv.data(), spirv.size() * 4, specializations);
        }

        {
            static std::vector<uint32_t> spirv;
            static ncnn::Mutex lock;
            {
                ncnn::MutexLockGuard guard(lock);
                if (spirv.empty())
                {
                    if (tta_mode)
                        compile_spirv_module(waifu2x_postproc_tta_comp_data, sizeof(waifu2x_postproc_tta_comp_data), net.opt, spirv);
                    else
                        compile_spirv_module(waifu2x_postproc_comp_data, sizeof(waifu2x_postproc_comp_data), net.opt, spirv);
                }
            }

            waifu2x_postproc = new ncnn::Pipeline(vkdev);
            waifu2x_postproc->set_optimal_local_size_xyz(8, 8, 3);
            waifu2x_postproc->create(spirv.data(), spirv.size() * 4, specializations);
        }
    }

    return 0;
}

int Waifu2x::process(const ncnn::Mat& inimage, ncnn::Mat& outimage) const
{

    if (!vkdev)
    {
        // cpu only
        return process_cpu(inimage, outimage);
    }

    if (noise == -1 && scale == 1)
    {
        outimage = inimage;
        return 0;
    }

    const unsigned char* pixeldata = (const unsigned char*)inimage.data;
    const int w = inimage.w;
    const int h = inimage.h;
    const int channels = inimage.elempack;

    const int TILE_SIZE_X = tilesize;
    const int TILE_SIZE_Y = tilesize;

    ncnn::VkAllocator* blob_vkallocator = vkdev->acquire_blob_allocator();
    ncnn::VkAllocator* staging_vkallocator = vkdev->acquire_staging_allocator();

    ncnn::Option opt = net.opt;
    opt.blob_vkallocator = blob_vkallocator;
    opt.workspace_vkallocator = blob_vkallocator;
    opt.staging_vkallocator = staging_vkallocator;

    // each tile 400x400
    int xtiles = (w + TILE_SIZE_X - 1) / TILE_SIZE_X;
    int ytiles = (h + TILE_SIZE_Y - 1) / TILE_SIZE_Y;

    // avoid very small edge tiles that the model cannot process
    while (xtiles > 1 && (w - (xtiles - 1) * TILE_SIZE_X) < prepadding * 2 + 1)
        xtiles--;
    while (ytiles > 1 && (h - (ytiles - 1) * TILE_SIZE_Y) < prepadding * 2 + 1)
        ytiles--;

    const size_t in_out_tile_elemsize = opt.use_fp16_storage ? 2u : 4u;

    high_resolution_clock::time_point begin = high_resolution_clock::now();
    high_resolution_clock::time_point time_print_progress = begin;

    //#pragma omp parallel for num_threads(2)
    for (int yi = 0; yi < ytiles; yi++)
    {

        const int tile_h_nopad = std::min((yi + 1) * TILE_SIZE_Y, h) - yi * TILE_SIZE_Y;

        int in_tile_y0 = std::max(yi * TILE_SIZE_Y - prepadding, 0);
        int in_tile_y1 = std::min((yi + 1) * TILE_SIZE_Y + prepadding, h);
        


        ncnn::Mat in;
        if (opt.use_fp16_storage && opt.use_int8_storage)
        {
            const int safe_tile_h = in_tile_y1 - in_tile_y0;
            const bool is_standard_size = (safe_tile_h == TILE_SIZE_Y + 2 * prepadding);
            if (is_standard_size) {
                in = ncnn::Mat(w, safe_tile_h, (unsigned char*)pixeldata + in_tile_y0 * w * channels, (size_t)channels, 1);
            } else {
                in.create(w, safe_tile_h, (size_t)channels, 1);
                const unsigned char* src = (unsigned char*)pixeldata + in_tile_y0 * w * channels;
                memcpy(in.data, src, (size_t)w * safe_tile_h * channels);
            }
        }
        else
        {
            if (channels == 3)
            {
#if _WIN32
                in = ncnn::Mat::from_pixels(pixeldata + in_tile_y0 * w * channels, ncnn::Mat::PIXEL_BGR2RGB, w, (in_tile_y1 - in_tile_y0));
#else
                in = ncnn::Mat::from_pixels(pixeldata + in_tile_y0 * w * channels, ncnn::Mat::PIXEL_RGB, w, (in_tile_y1 - in_tile_y0));
#endif
            }
        }


        ncnn::VkCompute cmd(vkdev);


        // upload
        ncnn::VkMat in_gpu;
        {
            cmd.record_clone(in, in_gpu, opt);

            if (xtiles > 1)
            {
                cmd.submit_and_wait();
                cmd.reset();
            }
        }

        int out_tile_y0 = yi * TILE_SIZE_Y;
        int out_tile_y1 = (yi == ytiles - 1) ? h : (yi + 1) * TILE_SIZE_Y;

        ncnn::VkMat out_gpu;
        if (opt.use_fp16_storage && opt.use_int8_storage)
        {
            out_gpu.create(w * scale, (out_tile_y1 - out_tile_y0) * scale, (size_t)channels, 1, blob_vkallocator);
        }
        else
        {
            out_gpu.create(w * scale, (out_tile_y1 - out_tile_y0) * scale, channels, (size_t)4u, 1, blob_vkallocator);
        }

        for (int xi = 0; xi < xtiles; xi++)
        {
            // actual tile content dimensions (not capped at TILE_SIZE for edge tiles)
            const int tile_w_nopad = (xi == xtiles - 1) ? (w - xi * TILE_SIZE_X) : TILE_SIZE_X;
            const int tile_h_nopad = (yi == ytiles - 1) ? (h - yi * TILE_SIZE_Y) : TILE_SIZE_Y;
            // align to multiple of 4 so that (aligned + 2*prepadding) is divisible by 4
            // required for models with stride-2 pooling layers (e.g. cunet has 2 levels)
            const int aligned_tile_w = ((tile_w_nopad + 3) / 4) * 4;
            const int aligned_tile_h = ((tile_h_nopad + 3) / 4) * 4;
            // TTA needs unified dimension for transposed tiles
            const int aligned_tile = std::max(aligned_tile_w, aligned_tile_h);

            if (tta_mode)
            {
                // preproc
                ncnn::VkMat in_tile_gpu[8];
                ncnn::VkMat in_alpha_tile_gpu;
                {
                    // crop tile - use aligned tile size with full prepadding
                    int tile_x0 = xi * TILE_SIZE_X - prepadding;
                    int tile_x1 = (xi + 1) * TILE_SIZE_X + prepadding;
                    int tile_y0 = yi * TILE_SIZE_Y - prepadding;
                    int tile_y1 = (yi + 1) * TILE_SIZE_Y + prepadding;

                    in_tile_gpu[0].create(aligned_tile + prepadding * 2, aligned_tile + prepadding * 2, 3, in_out_tile_elemsize, 1, blob_vkallocator);
                    in_tile_gpu[1].create(aligned_tile + prepadding * 2, aligned_tile + prepadding * 2, 3, in_out_tile_elemsize, 1, blob_vkallocator);
                    in_tile_gpu[2].create(aligned_tile + prepadding * 2, aligned_tile + prepadding * 2, 3, in_out_tile_elemsize, 1, blob_vkallocator);
                    in_tile_gpu[3].create(aligned_tile + prepadding * 2, aligned_tile + prepadding * 2, 3, in_out_tile_elemsize, 1, blob_vkallocator);
                    in_tile_gpu[4].create(aligned_tile + prepadding * 2, aligned_tile + prepadding * 2, 3, in_out_tile_elemsize, 1, blob_vkallocator);
                    in_tile_gpu[5].create(aligned_tile + prepadding * 2, aligned_tile + prepadding * 2, 3, in_out_tile_elemsize, 1, blob_vkallocator);
                    in_tile_gpu[6].create(aligned_tile + prepadding * 2, aligned_tile + prepadding * 2, 3, in_out_tile_elemsize, 1, blob_vkallocator);
                    in_tile_gpu[7].create(aligned_tile + prepadding * 2, aligned_tile + prepadding * 2, 3, in_out_tile_elemsize, 1, blob_vkallocator);

                    // alpha is handled in main.cpp as whole-image bicubic
                    ncnn::VkMat in_alpha_tile_gpu; // dummy, keeps binding index for shader compat

                    std::vector<ncnn::VkMat> bindings(10);
                    bindings[0] = in_gpu;
                    bindings[1] = in_tile_gpu[0];
                    bindings[2] = in_tile_gpu[1];
                    bindings[3] = in_tile_gpu[2];
                    bindings[4] = in_tile_gpu[3];
                    bindings[5] = in_tile_gpu[4];
                    bindings[6] = in_tile_gpu[5];
                    bindings[7] = in_tile_gpu[6];
                    bindings[8] = in_tile_gpu[7];
                    bindings[9] = in_alpha_tile_gpu;

                    std::vector<ncnn::vk_constant_type> constants(13);
                    constants[0].i = in_gpu.w;
                    constants[1].i = in_gpu.h;
                    constants[2].i = in_gpu.cstep;
                    constants[3].i = in_tile_gpu[0].w;
                    constants[4].i = in_tile_gpu[0].h;
                    constants[5].i = in_tile_gpu[0].cstep;
                    constants[6].i = prepadding;
                    constants[7].i = prepadding;
                    constants[8].i = xi * TILE_SIZE_X;
                    constants[9].i = std::min(yi * TILE_SIZE_Y, prepadding);
                    constants[10].i = channels;
                    constants[11].i = 0;
                    constants[12].i = 0;

                    ncnn::VkMat dispatcher;
                    dispatcher.w = in_tile_gpu[0].w;
                    dispatcher.h = in_tile_gpu[0].h;
                    dispatcher.c = channels;

                    cmd.record_pipeline(waifu2x_preproc, bindings, constants, dispatcher);
                }

                // waifu2x
                ncnn::VkMat out_tile_gpu[8];
                for (int ti = 0; ti < 8; ti++)
                {
                    ncnn::Extractor ex = net.create_extractor();

                    ex.set_blob_vkallocator(blob_vkallocator);
                    ex.set_workspace_vkallocator(blob_vkallocator);
                    ex.set_staging_vkallocator(staging_vkallocator);

                    ex.input("Input1", in_tile_gpu[ti]);

                    ex.extract("Eltwise4", out_tile_gpu[ti], cmd);
                }

                // postproc
                {
                    // alpha is handled in main.cpp as whole-image bicubic
                    ncnn::VkMat out_alpha_tile_gpu; // dummy, keeps binding index for shader compat

                    std::vector<ncnn::VkMat> bindings(10);
                    bindings[0] = out_tile_gpu[0];
                    bindings[1] = out_tile_gpu[1];
                    bindings[2] = out_tile_gpu[2];
                    bindings[3] = out_tile_gpu[3];
                    bindings[4] = out_tile_gpu[4];
                    bindings[5] = out_tile_gpu[5];
                    bindings[6] = out_tile_gpu[6];
                    bindings[7] = out_tile_gpu[7];
                    bindings[8] = out_alpha_tile_gpu;
                    bindings[9] = out_gpu;

                    std::vector<ncnn::vk_constant_type> constants(11);
                    constants[0].i = out_tile_gpu[0].w;
                    constants[1].i = out_tile_gpu[0].h;
                    constants[2].i = out_tile_gpu[0].cstep;
                    constants[3].i = out_gpu.w;
                    constants[4].i = out_gpu.h;
                    constants[5].i = out_gpu.cstep;
                    constants[6].i = xi * TILE_SIZE_X * scale;
                    constants[7].i = tile_w_nopad * scale;
                    constants[8].i = channels;
                    constants[9].i = 0;
                    constants[10].i = 0;

                    ncnn::VkMat dispatcher;
                    dispatcher.w = tile_w_nopad * scale;
                    dispatcher.h = tile_h_nopad * scale;
                    dispatcher.c = channels;

                    cmd.record_pipeline(waifu2x_postproc, bindings, constants, dispatcher);
                }
            }
            else
            {
                // preproc
                ncnn::VkMat in_tile_gpu;
                {
                    // crop tile - use aligned tile size with full prepadding
                    int tile_x0 = xi * TILE_SIZE_X - prepadding;
                    int tile_x1 = (xi + 1) * TILE_SIZE_X + prepadding;
                    int tile_y0 = yi * TILE_SIZE_Y - prepadding;
                    int tile_y1 = (yi + 1) * TILE_SIZE_Y + prepadding;

                    in_tile_gpu.create(aligned_tile_w + prepadding * 2, aligned_tile_h + prepadding * 2, 3, in_out_tile_elemsize, 1, blob_vkallocator);

                    // alpha is handled in main.cpp as whole-image bicubic
                    ncnn::VkMat in_alpha_tile_gpu; // dummy, keeps binding index for shader compat

                    std::vector<ncnn::VkMat> bindings(3);
                    bindings[0] = in_gpu;
                    bindings[1] = in_tile_gpu;
                    bindings[2] = in_alpha_tile_gpu;

                    std::vector<ncnn::vk_constant_type> constants(13);
                    constants[0].i = in_gpu.w;
                    constants[1].i = in_gpu.h;
                    constants[2].i = in_gpu.cstep;
                    constants[3].i = in_tile_gpu.w;
                    constants[4].i = in_tile_gpu.h;
                    constants[5].i = in_tile_gpu.cstep;
                    constants[6].i = prepadding;
                    constants[7].i = prepadding;
                    constants[8].i = xi * TILE_SIZE_X;
                    constants[9].i = std::min(yi * TILE_SIZE_Y, prepadding);
                    constants[10].i = channels;
                    constants[11].i = 0;
                    constants[12].i = 0;

                    ncnn::VkMat dispatcher;
                    dispatcher.w = in_tile_gpu.w;
                    dispatcher.h = in_tile_gpu.h;
                    dispatcher.c = channels;

                    cmd.record_pipeline(waifu2x_preproc, bindings, constants, dispatcher);
                }

                // waifu2x
                ncnn::VkMat out_tile_gpu;
                {
                    ncnn::Extractor ex = net.create_extractor();

                    ex.set_blob_vkallocator(blob_vkallocator);
                    ex.set_workspace_vkallocator(blob_vkallocator);
                    ex.set_staging_vkallocator(staging_vkallocator);

                    ex.input("Input1", in_tile_gpu);

                    ex.extract("Eltwise4", out_tile_gpu, cmd);
                }

                // postproc
                {
                    // alpha is handled in main.cpp as whole-image bicubic
                    ncnn::VkMat out_alpha_tile_gpu; // dummy, keeps binding index for shader compat

                    std::vector<ncnn::VkMat> bindings(3);
                    bindings[0] = out_tile_gpu;
                    bindings[1] = out_alpha_tile_gpu;
                    bindings[2] = out_gpu;

                    std::vector<ncnn::vk_constant_type> constants(11);
                    constants[0].i = out_tile_gpu.w;
                    constants[1].i = out_tile_gpu.h;
                    constants[2].i = out_tile_gpu.cstep;
                    constants[3].i = out_gpu.w;
                    constants[4].i = out_gpu.h;
                    constants[5].i = out_gpu.cstep;
                    constants[6].i = xi * TILE_SIZE_X * scale;
                    constants[7].i = tile_w_nopad * scale;
                    constants[8].i = channels;
                    constants[9].i = 0;
                    constants[10].i = 0;

                    ncnn::VkMat dispatcher;
                    dispatcher.w = tile_w_nopad * scale;
                    dispatcher.h = tile_h_nopad * scale;
                    dispatcher.c = channels;

                    cmd.record_pipeline(waifu2x_postproc, bindings, constants, dispatcher);
                }
            }

            if (xtiles > 1)
            {
                cmd.submit_and_wait();
                cmd.reset();
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

        // download
        {
            ncnn::Mat out;

            if (opt.use_fp16_storage && opt.use_int8_storage)
            {
                out = ncnn::Mat(out_gpu.w, out_gpu.h, (unsigned char*)outimage.data + yi * scale * TILE_SIZE_Y * w * scale * channels, (size_t)channels, 1);
            }

            cmd.record_clone(out_gpu, out, opt);

            cmd.submit_and_wait();

            if (!(opt.use_fp16_storage && opt.use_int8_storage))
            {
                if (channels == 3)
                {
#if _WIN32
                    out.to_pixels((unsigned char*)outimage.data + yi * scale * TILE_SIZE_Y * w * scale * channels, ncnn::Mat::PIXEL_RGB2BGR);
#else
                    out.to_pixels((unsigned char*)outimage.data + yi * scale * TILE_SIZE_Y * w * scale * channels, ncnn::Mat::PIXEL_RGB);
#endif
                }
            }
        }
    }

    vkdev->reclaim_blob_allocator(blob_vkallocator);
    vkdev->reclaim_staging_allocator(staging_vkallocator);

    return 0;
}


int Waifu2x::process_cpu(const ncnn::Mat& inimage, ncnn::Mat& outimage) const
{
    if (noise == -1 && scale == 1)
    {
        outimage = inimage;
        return 0;
    }

    const unsigned char* pixeldata = (const unsigned char*)inimage.data;
    const int w = inimage.w;
    const int h = inimage.h;
    const int channels = inimage.elempack;

    const int TILE_SIZE_X = tilesize;
    const int TILE_SIZE_Y = tilesize;

    ncnn::Option opt = net.opt;

    // each tile 400x400
    int xtiles = (w + TILE_SIZE_X - 1) / TILE_SIZE_X;
    int ytiles = (h + TILE_SIZE_Y - 1) / TILE_SIZE_Y;

    // avoid very small edge tiles that the model cannot process
    while (xtiles > 1 && (w - (xtiles - 1) * TILE_SIZE_X) < prepadding * 2 + 1)
        xtiles--;
    while (ytiles > 1 && (h - (ytiles - 1) * TILE_SIZE_Y) < prepadding * 2 + 1)
        ytiles--;

    high_resolution_clock::time_point begin = high_resolution_clock::now();
    high_resolution_clock::time_point time_print_progress = begin;

    for (int yi = 0; yi < ytiles; yi++)
    {
        int in_tile_y0 = std::max(yi * TILE_SIZE_Y - prepadding, 0);
        int in_tile_y1 = std::min((yi + 1) * TILE_SIZE_Y + prepadding, h);

        for (int xi = 0; xi < xtiles; xi++)
        {
            // actual tile content dimensions (not capped at TILE_SIZE for edge tiles)
            const int tile_w_nopad = (xi == xtiles - 1) ? (w - xi * TILE_SIZE_X) : TILE_SIZE_X;
            const int tile_h_nopad = (yi == ytiles - 1) ? (h - yi * TILE_SIZE_Y) : TILE_SIZE_Y;
            // align to multiple of 4 so that (aligned + 2*prepadding) is divisible by 4
            const int aligned_tile_w = ((tile_w_nopad + 3) / 4) * 4;
            const int aligned_tile_h = ((tile_h_nopad + 3) / 4) * 4;
            // TTA needs unified dimension for transposed tiles
            const int aligned_tile = std::max(aligned_tile_w, aligned_tile_h);

            int in_tile_x0 = std::max(xi * TILE_SIZE_X - prepadding, 0);
            int in_tile_x1 = std::min((xi + 1) * TILE_SIZE_X + prepadding, w);

            // crop tile
            ncnn::Mat in;
            {
                if (channels == 3)
                {
#if _WIN32
                    in = ncnn::Mat::from_pixels_roi(pixeldata, ncnn::Mat::PIXEL_BGR2RGB, w, h, in_tile_x0, in_tile_y0, in_tile_x1 - in_tile_x0, in_tile_y1 - in_tile_y0);
#else
                    in = ncnn::Mat::from_pixels_roi(pixeldata, ncnn::Mat::PIXEL_RGB, w, h, in_tile_x0, in_tile_y0, in_tile_x1 - in_tile_x0, in_tile_y1 - in_tile_y0);
#endif
                }
            }

            ncnn::Mat out;

            // border padding for cpu
            // TTA needs square padded tile for transposed variants
            const int eff_aligned_w = tta_mode ? aligned_tile : aligned_tile_w;
            const int eff_aligned_h = tta_mode ? aligned_tile : aligned_tile_h;
            const int crop_w = in_tile_x1 - in_tile_x0;
            const int crop_h = in_tile_y1 - in_tile_y0;
            const int padded_w = eff_aligned_w + prepadding * 2;
            const int padded_h = eff_aligned_h + prepadding * 2;
            int pad_left = std::max(prepadding - xi * TILE_SIZE_X, 0);
            int pad_right = padded_w - crop_w - pad_left;
            int pad_top = std::max(prepadding - yi * TILE_SIZE_Y, 0);
            int pad_bottom = padded_h - crop_h - pad_top;

            if (tta_mode)
            {
                // split and preproc (RGB only, alpha handled in main.cpp)
                ncnn::Mat in_tile[8];
                {
                    in_tile[0].create(in.w, in.h, 3);
                    for (int q = 0; q < 3; q++)
                    {
                        const float* ptr = in.channel(q);
                        float* outptr0 = in_tile[0].channel(q);

                        for (int i = 0; i < in.h; i++)
                        {
                            for (int j = 0; j < in.w; j++)
                            {
                                *outptr0++ = *ptr++ * (1 / 255.f);
                            }
                        }
                    }
                }

                // border padding
                {
                    ncnn::Mat in_tile_padded;
                    ncnn::copy_make_border(in_tile[0], in_tile_padded, pad_top, pad_bottom, pad_left, pad_right, ncnn::BORDER_REPLICATE, 0.f, net.opt);
                    in_tile[0] = in_tile_padded;
                }

                // the other 7 directions
                {
                    in_tile[1].create(in_tile[0].w, in_tile[0].h, 3);
                    in_tile[2].create(in_tile[0].w, in_tile[0].h, 3);
                    in_tile[3].create(in_tile[0].w, in_tile[0].h, 3);
                    in_tile[4].create(in_tile[0].h, in_tile[0].w, 3);
                    in_tile[5].create(in_tile[0].h, in_tile[0].w, 3);
                    in_tile[6].create(in_tile[0].h, in_tile[0].w, 3);
                    in_tile[7].create(in_tile[0].h, in_tile[0].w, 3);

                    for (int q = 0; q < 3; q++)
                    {
                        const ncnn::Mat in_tile_0 = in_tile[0].channel(q);
                        ncnn::Mat in_tile_1 = in_tile[1].channel(q);
                        ncnn::Mat in_tile_2 = in_tile[2].channel(q);
                        ncnn::Mat in_tile_3 = in_tile[3].channel(q);
                        ncnn::Mat in_tile_4 = in_tile[4].channel(q);
                        ncnn::Mat in_tile_5 = in_tile[5].channel(q);
                        ncnn::Mat in_tile_6 = in_tile[6].channel(q);
                        ncnn::Mat in_tile_7 = in_tile[7].channel(q);

                        for (int i = 0; i < in_tile[0].h; i++)
                        {
                            const float* outptr0 = in_tile_0.row(i);
                            float* outptr1 = in_tile_1.row(in_tile[0].h - 1 - i);
                            float* outptr2 = in_tile_2.row(i) + in_tile[0].w - 1;
                            float* outptr3 = in_tile_3.row(in_tile[0].h - 1 - i) + in_tile[0].w - 1;

                            for (int j = 0; j < in_tile[0].w; j++)
                            {
                                float* outptr4 = in_tile_4.row(j) + i;
                                float* outptr5 = in_tile_5.row(in_tile[0].w - 1 - j) + i;
                                float* outptr6 = in_tile_6.row(j) + in_tile[0].h - 1 - i;
                                float* outptr7 = in_tile_7.row(in_tile[0].w - 1 - j) + in_tile[0].h - 1 - i;

                                float v = *outptr0++;

                                *outptr1++ = v;
                                *outptr2-- = v;
                                *outptr3-- = v;
                                *outptr4 = v;
                                *outptr5 = v;
                                *outptr6 = v;
                                *outptr7 = v;
                            }
                        }
                    }
                }

                // waifu2x
                ncnn::Mat out_tile[8];
                for (int ti = 0; ti < 8; ti++)
                {
                    ncnn::Extractor ex = net.create_extractor();

                    ex.input("Input1", in_tile[ti]);

                    ex.extract("Eltwise4", out_tile[ti]);
                }

                // postproc (RGB only, alpha handled in main.cpp)
                {
                    out.create(tile_w_nopad * scale, tile_h_nopad * scale, 3);
                    for (int q = 0; q < 3; q++)
                    {
                        const ncnn::Mat out_tile_0 = out_tile[0].channel(q);
                        const ncnn::Mat out_tile_1 = out_tile[1].channel(q);
                        const ncnn::Mat out_tile_2 = out_tile[2].channel(q);
                        const ncnn::Mat out_tile_3 = out_tile[3].channel(q);
                        const ncnn::Mat out_tile_4 = out_tile[4].channel(q);
                        const ncnn::Mat out_tile_5 = out_tile[5].channel(q);
                        const ncnn::Mat out_tile_6 = out_tile[6].channel(q);
                        const ncnn::Mat out_tile_7 = out_tile[7].channel(q);
                        float* outptr = out.channel(q);

                        for (int i = 0; i < out.h; i++)
                        {
                            const float* ptr0 = out_tile_0.row(i);
                            const float* ptr1 = out_tile_1.row(out_tile[0].h - 1 - i);
                            const float* ptr2 = out_tile_2.row(i) + out_tile[0].w - 1;
                            const float* ptr3 = out_tile_3.row(out_tile[0].h - 1 - i) + out_tile[0].w - 1;

                            for (int j = 0; j < out.w; j++)
                            {
                                const float* ptr4 = out_tile_4.row(j) + i;
                                const float* ptr5 = out_tile_5.row(out_tile[0].w - 1 - j) + i;
                                const float* ptr6 = out_tile_6.row(j) + out_tile[0].h - 1 - i;
                                const float* ptr7 = out_tile_7.row(out_tile[0].w - 1 - j) + out_tile[0].h - 1 - i;

                                float v = (*ptr0++ + *ptr1++ + *ptr2-- + *ptr3-- + *ptr4 + *ptr5 + *ptr6 + *ptr7) / 8;

                                *outptr++ = v * 255.f + 0.5f;
                            }
                        }
                    }
                }
            }
            else
            {
                // split and preproc (RGB only, alpha handled in main.cpp)
                ncnn::Mat in_tile;
                {
                    in_tile.create(in.w, in.h, 3);
                    for (int q = 0; q < 3; q++)
                    {
                        const float* ptr = in.channel(q);
                        float* outptr = in_tile.channel(q);

                        for (int i = 0; i < in.w * in.h; i++)
                        {
                            *outptr++ = *ptr++ * (1 / 255.f);
                        }
                    }
                }

                // border padding
                {
                    ncnn::Mat in_tile_padded;
                    ncnn::copy_make_border(in_tile, in_tile_padded, pad_top, pad_bottom, pad_left, pad_right, ncnn::BORDER_REPLICATE, 0.f, net.opt);
                    in_tile = in_tile_padded;
                }

                // waifu2x
                ncnn::Mat out_tile;
                {
                    ncnn::Extractor ex = net.create_extractor();

                    ex.input("Input1", in_tile);

                    ex.extract("Eltwise4", out_tile);
                }

                if (out_tile.empty())
                    continue;

                // postproc (RGB only, alpha handled in main.cpp)
                {
                    out.create(tile_w_nopad * scale, tile_h_nopad * scale, 3);
                    for (int q = 0; q < 3; q++)
                    {
                        float* outptr = out.channel(q);

                        for (int i = 0; i < out.h; i++)
                        {
                            const float* ptr = out_tile.channel(q).row(i);

                            for (int j = 0; j < out.w; j++)
                            {
                                *outptr++ = *ptr++ * 255.f + 0.5f;
                            }
                        }
                    }
                }
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

            {
                if (channels == 3)
                {
#if _WIN32
                    out.to_pixels((unsigned char*)outimage.data + yi * scale * TILE_SIZE_Y * w * scale * channels + xi * scale * TILE_SIZE_X * channels, ncnn::Mat::PIXEL_RGB2BGR, w * scale * channels);
#else
                    out.to_pixels((unsigned char*)outimage.data + yi * scale * TILE_SIZE_Y * w * scale * channels + xi * scale * TILE_SIZE_X * channels, ncnn::Mat::PIXEL_RGB, w * scale * channels);
#endif
                }
            }
        }
    }

    return 0;
}
