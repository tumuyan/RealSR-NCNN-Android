# RealSR-NCNN-Android

[English](https://github.com/tumuyan/RealSR-NCNN-Android/blob/master/README.md)

超分辨率是指将低质量压缩图片恢复成高分辨率图片的过程，用更常见的讲法就是放大图片并降噪。  
随着移动互联网的快速发展，智能设备逐渐普及到生活的每个角落。随之而来的是大量的图像数据。有的图片本身分辨率就比较低，难以看清楚细节；有的在存储和传输的过程中被反复压缩和劣化，最终不再清晰。  
为了获得更加高质量的视觉体验，或者出于更为基本的目的看清楚图片，图像恢复/超分辨率算法应运而生。而手机作为目前我们生活中最常使用的智能设备，显然有使用这一技术的迫切需求。  

这个仓库正是为安卓设备构建的一个图像超分辨率的应用。具有如下特点：  
  ✅ 内置超分算法和模型多。最初使用了[RealSR-NCNN](https://github.com/nihui/realsr-ncnn-vulkan)和[Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN)的成果，后来又添加了[SRMD-NCNN](https://github.com/nihui/srmd-ncnn-vulkan)和[RealCUGAN-NCNN](https://github.com/nihui/realcugan-ncnn-vulkan), [Anime4KCPP](https://github.com/TianZerL/Anime4KCPP)。同时也内置了[waifu2x-ncnn](https://github.com/nihui/waifu2x-ncnn-vulkan)（但是没有内置模型和预设命令，如有需求自行下载并添加）  
  ✅ 兼顾传统插值算法。包括常见的nearest、bilinear、bicubic算法，以及imagemagick的二十多种filter。  
  ✅ 内置缩小算法。除使用用户指定倍率和算法的缩小方式外，resize-ncnn设计了一种自动缩小的算法de-nearest。参见[笔记](https://note.youdao.com/s/6XlIFbWt)  
  ✅ 支持图形界面和命令行两种操作方式使用。  
  ✅ 转换结果先预览，满意再导出，不浪费存储空间。  
  ✅ 导出文件自动按照模型和时间命名，方便管理。  
  ✅ 自定义优先选用的超分算法和模型。    
  ✅ 自定义预设命令。  
  ✅ 图片处理过程完全在本地运行，无需担心隐私泄漏、服务器排队、服务收费；处理耗时取于决选择的模型、图片大小以及设备的性能。  

### 下载地址 
[Github Release](https://github.com/tumuyan/RealSR-NCNN-Android/releases)  

### 仓库结构
1. RealSR-NCNN-Android-CLI 包含RealSR、RealCUGAN、SRMD、Waifu2x、Resize和Animk4k共6个模块，可以分别编译出对应的命令行程序，编译结果可以在安卓设备的Termux等虚拟终端中使用。其中:
  - RealSR 可以使用RealSR和Real-ESRGAN的模型。
  - Resize 可以使用了`nearest/最邻近`、`bilinear/两次线性`、`bicubic/两次立方`三种经典放大（interpolation/插值）算法，以及Lanczos插值算法相似的`avir/lancir`。特别的，nearest和bilinear可以通过`-n`参数，不使用ncnn进行运算，得到点对点放大的结果;当不使用`-n`。参数时，`-s`参数可以使用小数。
2. RealSR-NCNN-Android-GUI 可以编译出APK文件，这样用户可以在图形环境下操作。（不过他的本质就是在给上述命令行程序套壳，而不是通过JNI调用库文件）
3. Resize-CLI 可以编译出resize命令行程序，包含`nearest/最邻近`、`bilinear/两次线性`两种算法，不需要ncnn，编译体积较大。此工程除Android使用外，也可使用VS2019编译，在PC端快速验证。

## 如何使用 RealSR-NCNN-Android-GUI
支持两种选择文件的方式：
1. 从其他应用（比如图库）分享一个或多个图片到本应用
2. 在本应用中，点击`选图`选择图片

支持两种操作方式:
1. 点击`放大`（视图片大小和设备性能需要等待不同时间——毕竟原项目是使用电脑显卡运行的）- 查看放大效果是否满意，如果满意点击`导出`保存到相册。也可以在运行前切换使用的模型。切换模型后无需重新选择图片。运行过程中点击右上角进度可以终止运行；运行过程中切换模型并点击运行，或者直接输入命令并回车，可以终止上次任务并开始执行新的任务。
2. 直接在输入框内输入命令完成调用(可以输入help查看更多信息)

应用依赖于vulkan API，所以对设备有如下要求（几年前游戏《光遇》上架时，很多人已经对vulkan有所了解了吧？）：
1. 使用了比较新的SOC。经过实际测试，骁龙853（GPU Adreno510）可以处理较小的图片
2. 系统支持vulkan。（Google在Android7.0中增加了vulkan的集成，但是您的设备厂商不一定提供了这项支持）


![](ScreenshotCHS.jpg)


## 为 RealSR-NCNN-Android-GUI 增加更多模型
RealSR-NCNN-Android-GUI 在 ver 1.7.6 以上的版本可以自动加载自定义模型。
你可以从 https://huggingface.co/tumuyan2/realsr-models 下载更多模型：
1. 在文件管理器里新建一个目录
2. 在App的设置中，自定义模型路径的选项里填入刚才新建目录的路径，点击保存
3. 下载模型并复制到刚才新建的目录里
5. 返回App，可以看到下拉菜单增加了新的模型

![目录结构](Screenshot_models.jpg)

你自己也可以把pth格式的ESRGAN模型转换为本应用可用的ncnn模型。
1. 从 [OpenModelDB](https://openmodeldb.info/) 下载模型
2. 下载  [cupscale](https://github.com/n00mkrad/cupscale) 并解压
3. 打开 CupscaleData\bin\pth2ncnn, 用 pth2ncnn.exe 转换 pth 文件为 ncnn 模型文件
4. 重命名文件，举例：
```
models-Real-ESRGAN-AnimeSharp  // 目录需要用 models- 开头
├─x4.bin                       // 模型名称为 x[n], n 是放大倍率
├─x4.param
```
另一种转换模型的方式可以转换更多种类的模型，但是更加复杂
1. 从 [OpenModelDB](https://openmodeldb.info/) 下载模型
2. 下载 [chaiNNer](https://chainner.app/) 并安装（这需要很好的网络）
3. 打开 chaiNNer，链接节点或者使用我提供的已经链接好的工程 https://github.com/tumuyan/RealSR-NCNN-Android/raw/main/chainner-pth2ncnn.chn
3. 把模型拖到最左侧的节点上，点击运行按钮，等待 chaiNNer 转换模型为 ncnn 模型。需要注意的是，并非每一个模型都能够转换为ncnn模型。
4. 重命名文件（与上一种方式相同）。如果使用了我提供的工程文件，无需此步骤。
   
实际上只要你使用过 chaiNNer ，就会被 chaiNNer 的强大所吸引 —— 一方面是他支持了太多的模型（当然不是、也不可能是所有的优秀模型）和传统图像的处理方法，链接节点就像堆积木一样实现各种各样的功能。

![](chainner-pth2ncnn.png)

## 如何编译 RealSR-NCNN-Android-GUI
从 github release 页面下载 `assets.zip` , 其中包含了模型和CLI程序, 解压并放置到如下路径, 然后使用 Android Studio 进行编译。
当前版本的下载连接为 https://github.com/tumuyan/RealSR-NCNN-Android/releases/download/1.10.0/assets.zip

```
RealSR-NCNN-Android-GUI\app\src\main\assets\
└─realsr
    │  Anime4k
    │  colors.xml
    │  delegates.xml
    │  libc++_shared.so
    │  libncnn.so
    │  libomp.so
    │  magick
    │  realcugan-ncnn
    │  realsr-ncnn
    │  resize-ncnn
    │  srmd-ncnn
    │  waifu2x-ncnn
    │  
    ├─models-nose
    │      up2x-no-denoise.bin
    │      up2x-no-denoise.param
    │      
    ├─models-pro
    │      up2x-conservative.bin
    │      up2x-conservative.param
    │      up2x-denoise3x.bin
    │      up2x-denoise3x.param
    │      up2x-no-denoise.bin
    │      up2x-no-denoise.param
    │      up3x-conservative.bin
    │      up3x-conservative.param
    │      up3x-denoise3x.bin
    │      up3x-denoise3x.param
    │      up3x-no-denoise.bin
    │      up3x-no-denoise.param
    │      
    ├─models-Real-ESRGAN
    │      x4.bin
    │      x4.param
    │      
    ├─models-Real-ESRGAN-anime
    │      x4.bin
    │      x4.param
    │      
    ├─models-Real-ESRGANv2-anime
    │      x2.bin
    │      x2.param
    │      x4.bin
    │      x4.param
    │      
    ├─models-Real-ESRGANv3-anime
    │      x2.bin
    │      x2.param
    │      x3.bin
    │      x3.param
    │      x4.bin
    │      x4.param
    │      
    ├─models-ESRGAN-Nomos8kSC
    │      x4.bin
    │      x4.param
    |
    └─models-se
           up2x-conservative.bin
           up2x-conservative.param
           up2x-denoise1x.bin
           up2x-denoise1x.param
           up2x-denoise2x.bin
           up2x-denoise2x.param
           up2x-denoise3x.bin
           up2x-denoise3x.param
           up2x-no-denoise.bin
           up2x-no-denoise.param
           up3x-conservative.bin
           up3x-conservative.param
           up3x-denoise3x.bin
           up3x-denoise3x.param
           up3x-no-denoise.bin
           up3x-no-denoise.param
           up4x-conservative.bin
           up4x-conservative.param
           up4x-denoise3x.bin
           up4x-denoise3x.param
           up4x-no-denoise.bin
           up4x-no-denoise.param
```

## 局限
这是一个非常简易的工具，在灵活和强大的同时，存在如下缺陷（并且没有计划去完善）：
### 关于批量处理
1. 可以通过打开相册-选择多个图片-分享-realsr来加载多个图片，不能在应用内选多个图片
2. 加载多个图片后，只能预览1个图片，不会显示图片列表，也无法切换显示不同图片
3. 处理结束后不会预览处理结果，会直接保存到相册
4. Magick（包含右上方快捷菜单）不支持多图处理

### 关于gif动图
1. 仅当打开1张动图时，才能以动图的方式进行处理；否则只能处理1帧
2. 无法预览动图
3. 处理结束后不会预览处理结果，会直接保存到相册
4. Magick（包含右上方快捷菜单）不支持动图处理

## 感谢
### 原始超分辨率项目
- https://github.com/xinntao/Real-ESRGAN
- https://github.com/jixiaozhong/RealSR
- https://github.com/cszn/SRMD
- https://github.com/bilibili/ailab/tree/main/Real-CUGAN
- https://github.com/bloc97/Anime4K

### ncnn项目以及模型
大部分C代码都来自nihui。由于Android直接编译比较困难，必须对项目目录做调整，因此破坏了原有Git。  
- https://github.com/nihui/realsr-ncnn-vulkan 
- https://github.com/nihui/srmd-ncnn-vulkan
- https://github.com/nihui/waifu2x-ncnn-vulkan
- https://github.com/nihui/realcugan-ncnn-vulkan
- https://github.com/TianZerL/Anime4KCPP

## 使用的其他开源项目
- [https://github.com/Tencent/ncnn](https://github.com/Tencent/ncnn)  for fast neural network inference on ALL PLATFORMS
- [https://github.com/nothings/stb](https://github.com/nothings/stb)  for decoding and encoding image on Linux / MacOS
- [https://github.com/tronkko/dirent](https://github.com/tronkko/dirent)  for listing files in directory on Windows
- [https://github.com/webmproject/libwebp](https://github.com/webmproject/libwebp) for encoding and decoding Webp images on ALL PLATFORMS
- [https://github.com/avaneev/avir](https://github.com/avaneev/avir) AVIR image resizing algorithm designed by Aleksey Vaneev
- [https://github.com/ImageMagick/ImageMagick6](https://github.com/ImageMagick/ImageMagick6) Use ImageMagick® to resize/convert images.
- [https://github.com/MolotovCherry/Android-ImageMagick7](https://github.com/MolotovCherry/Android-ImageMagick7) The repository has been archived, I use this fork [https://github.com/Miseryset/Android-ImageMagick7](https://github.com/Miseryset/Android-ImageMagick7)

## 使用的其他模型
- Real-ESRGAN 模型 [Nomos8kSC](https://github.com/Phhofm/models/tree/main/4xNomos8kSC), 由 Phhofm 训练的一个适合处理相片的模型.