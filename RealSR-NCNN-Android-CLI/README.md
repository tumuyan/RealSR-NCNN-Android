[中文说明](./README_CHS.md)
# MNN-SR

This module is a [mnn](https://github.com/alibaba/MNN) implementation for super resolution. 
1. MNN can be compatible with more model architectures than NCNN.  

2. This module supports compilation with GCC, VS, and AS, meaning that not only Android but also Windows, Linux, and even other PC systems now have a more universal super-resolution tool that operates independently of Python!  

3. MNN supports multiple backends. The current configuration for Android includes Vulkan, OpenCL, and CPU, with OpenCL recommended. For Windows, it supports Vulkan, OpenCL, CPU, and CUDA. However, CUDA produces incorrect output on my computer and requires further testing and feedback. OpenCL does not show GPU usage in the system task manager, but the GPU can be fully utilized as seen in the nvidia-smi tool. On Linux, it supports Vulkan, OpenCL, CPU, and CUDA. In my test environment, CUDA is the fastest, Vulkan fails to utilize the GPU, and OpenCL performs well. Overall, all platforms function properly with speeds no lower than NCNN.
 **🙏Waiting your help!**

### How to build in Android Studio

1. download opencv lib and others libs and extract them to RealSR-NCNN-Android/3rdparty just like the ncnn modules.

2. download mnn lib and extract them to RealSR-NCNN-Android/3rdparty
   
   ```
   ├─libwebp
   ├─mnn_android
   │  ├─arm64-v8a
   │  ├─armeabi-v7a
   │  └─include
   ```

3. sync and build this module

4. copy models and *.so from mnn_android  to assets dir and build the GUI App.

5. run mnnsr commands in App


### How to build in VS for Windows
1. download Windows libs just like Android. if you need cuda, you should build mnn by yourself.
2. modify the path in cmake file according to actually libs path.
3. open the jni path and build


### How to build in Linux
refer to ci script


### Usages

The usage of others program is same as realsr-ncnn. Add these param: 

     -b backend           forward backend type(CPU=0,AUTO=4,CUDA=2,OPENCL=3,OPENGL=6,VULKAN=7,NN=5,USER_0=8,USER_1=9, default=3)
     -c color-type        model & output color space type (RGB=1, BGR=2, YCbCr=5, YUV=6, GRAY=10, GRAY model & YCbCr output=11, GRAY model & YUV output=12, default=1)
     -d decensor-mode     remove censor mode (Not=-1, Mosaic=0, default=-1)


# NCNN-Modules

## How to build RealSR-NCNN-Android-CLI

### step1

https://github.com/Tencent/ncnn/releases  
download ncnn-yyyymmdd-android-vulkan-shared.zip.  
https://github.com/webmproject/libwebp
download the source of libwebp.  
https://opencv.org/releases/
download opencv-android-sdk.

### step2

extract `ncnn-yyyymmdd-android-vulkan-shared.zip` into `../3rdparty/ncnn-android-vulkan-shared`  
extract the source of libwebp into `../3rdparty/libwebp`  
extract `opencv-version-android-sdk` into `../3rdparty/opencv-android-sdk`

```
RealSR-NCNN-Android
├─3rdparty
│   ├─opencv-android-sdk
│   │   └─sdk
│   ├─libwebp
│   └─ncnn-android-vulkan-shared
│       └─arm64-v8a
├─RealSR-NCNN-Android-CLI
│   ├─Anime4k
│   ├─RealCUGAN
│   ├─Waifu2x
│   ├─RealSR
│   ├─SRMD
│   └─ReSize
└─RealSR-NCNN-Android-GUI
```

### step3

Open this project with Android Studio, rebuild it and the build result in `RealSR-NCNN-Android-CLI\*\build\intermediates\cmake\release\obj\arm64-v8a` or `RealSR-NCNN-Android-CLI\*\build\intermediates\cmake\debug\obj\arm64-v8a` could copy to the GUI project automatilly.    
Click `3rdparty/copy_cli_build_result.bat` and it could copy the other files to GUI project.



## How to use RealSR-NCNN-Android-CLI

### Download models

You could download `assets.zip` from github release page and unzip it to get models, or download models from https://github.com/tumuyan/realsr-models .

### Example Command

Make sure the elf file has execute permission. Then input command

```shell
./realsr-ncnn -i input.jpg -o output.jpg
```

### Full Usages

The usage of others program is same as realsr-ncnn.

```console
Usage: realsr-ncnn -i infile -o outfile [options]...

  -h                   show this help
  -v                   verbose output
  -i input-path        input image path (jpg/png/webp) or directory
  -o output-path       output image path (jpg/png/webp) or directory
  -s scale             upscale ratio (4, default=4)
  -t tile-size         tile size (>=32/0=auto, default=0) can be 0,0,0 for multi-gpu
  -m model-path        realsr model path (default=models-DF2K_JPEG)
  -g gpu-id            gpu device to use (default=0) can be 0,1,2 for multi-gpu, -1 use cpu
  -j load:proc:save    thread count for load/proc/save (default=1:2:2) can be 1:2,2,2:2 for multi-gpu
  -x                   enable tta mode
  -f format            output image format (jpg/png/webp, default=ext/png)
```

- `input-path` and `output-path` accept either file path or directory path
- `scale` = scale level, 4 = upscale 4x
- `tile-size` = tile size, use smaller value to reduce GPU memory usage, default selects automatically
- `load:proc:save` = thread count for the three stages (image decoding + realsr upscaling + image encoding), using larger values may increase GPU usage and consume more GPU memory. You can tune this configuration with "4:4:4" for many small-size images, and "2:2:2" for large-size images. The default setting usually works fine for most situations. If you find that your GPU is hungry, try increasing thread count to achieve faster processing.
- `format` = the format of the image to be output, png is better supported, however webp generally yields smaller file sizes, both are losslessly encoded

If you encounter crash or error, try to upgrade your derive