[English](./README.md)

# NCNN 各模块
## 如何编译 RealSR-NCNN-Android-CLI
### step1
https://github.com/Tencent/ncnn/releases  
下载 `ncnn-yyyymmdd-android-vulkan-shared.zip` 或者你自己编译ncnn为so文件  
https://github.com/webmproject/libwebp  
下载libwebp的源码
https://opencv.org/releases/  
下载opencv-android-sdk

### step2
解压 `ncnn-yyyymmdd-android-vulkan-shared.zip` 到 `../3rdparty/ncnn-android-vulkan-shared`  
解压libwebp源码到`../3rdparty/libwebp`  
解压 `opencv-version-android-sdk` 到 `../3rdparty/opencv-android-sdk`

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
用 Android Studio 打开工程, rebuild 然后你就可以在 `RealSR-NCNN-Android-CLI\*\build\intermediates\cmake\release\obj\arm64-v8a` 或 `RealSR-NCNN-Android-CLI\*\build\intermediates\cmake\debug\obj\arm64-v8a` 找到编译好的二进制文件，这些文件会被编译脚本自动复制到 GUI 工程目录中。  
点击 `3rdparty/copy_cli_build_result.bat` 可以更新其他库文件的二进制文件到 GUI 工程目录中

## 如何使用 RealSR-NCNN-Android-CLI
### 下载模型
你可以从 github release 页面下载 `assets.zip`, 或者从 https://github.com/tumuyan/realsr-models 下载所需模型，需要注意不同程序需要用对应的模型

### 命令范例
确认程序有执行权限，然后输入命令：
```shell
./realsr-ncnn -i input.jpg -o output.jpg
```

### 完整用法
仅以 realsr-ncnn 为例说明，其他程序使用方法完全相同，故不重复说明
```console
用法: realsr-ncnn -i 输入的图片路径 -o 输出的图片路径 [其他可选参数]...

  -h                   显示帮助
  -v                   显示更多输出内容
  -i input-path        输入的图片路径（jpg/png/webp路径或者目录路径）
  -o output-path       输出的图片路径（jpg/png/webp路径或者目录路径）
  -s scale             缩放系数(默认4，即放大4倍)
  -t tile-size         tile size (>=32/0=auto, default=0) can be 0,0,0 for multi-gpu
  -m model-path        模型路径 (默认模型 models-Real-ESRGAN-anime)
  -g gpu-id            gpu，-1使用CPU，默认0 多GPU可选 0,1,2
  -j load:proc:save    解码/处理/保存的线程数 (默认1:2:2) 多GPU可以设 1:2,2,2:2
  -x                   开启tta模式
  -f format            输出格式(jpg/png/webp, 默认ext/png)
  
```


# MNN-SR
这个模块是用 [mnn](https://github.com/alibaba/MNN) 实现的超分辨率命令行程序。经测试确认，mnn要比ncnn慢，但是可以兼容更多模型。  
另外我想把这个模块能同时兼容VS和AS，这样不止是Android，连Windows也有脱离Python的通用性的超分辨率工具了！但是目前VS仍然有许多错误，**如果可能的话，请你帮帮我🙏！**

### 如何在AS中编译
1. 和前边的ncnn模块一样下载并解压依赖到 RealSR-NCNN-Android/3rdparty  
2. 下载mnn库并解压到RealSR-NCNN-Android/3rdparty
```
├─libwebp
├─mnn_android
│  ├─arm64-v8a
│  ├─armeabi-v7a
│  └─include

```
3. sync 并 build
4. 从mnn库中复制 *.so 文件到GUI项目的assest中.

### 如何转换模型  
参考 https://mnn-docs.readthedocs.io/en/latest/tools/convert.html
1. `pip install mnn`
2. `MNNConvert -f ONNX  --modelFile "{onnx_path}" --MNNModel "{mnn_path}"  --bizCode biz --fp16  --info  --detectSparseSpeedUp`

### 用法
和realsr-ncnn基本相同.


