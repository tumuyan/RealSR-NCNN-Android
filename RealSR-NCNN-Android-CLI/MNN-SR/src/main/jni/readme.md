# MNN-SR
This module is a [mnn](https://github.com/alibaba/MNN) implementation for super resolution. 
MNN could support more models than ncnn, but it is slower in my experiment.  
I am trying to make this module a CLI program that can be compiled in both VS(for Windows) and AS(for Android), 
but it is currently not works. The main issue is the gap between `cv::mat` and `mnn::tensor`.  **🙏Waiting your help!**
Therefore, I have hidden the module in AS by commenting it out in [settings.gradle](../../../../settings.gradle).

### How to build
1. download mnn lib and extract them to RealSR-NCNN-Android/3rdparty
```
├─libwebp
├─mnn_android
│  ├─arm64-v8a
│  ├─armeabi-v7a
│  └─include
```
2. open  [../../../../settings.gradle](../../../../settings.gradle). and enable mnn-sr module.
```
    include ':RealCUGAN'
    include ':RealSR'
    include ':Resize'
    include ':SRMD'
    include ':Waifu2x'
    include ':Anime4k'
    include ':MNN-SR'

```
3. sync and build this module
4. copy models and *.so from mnn_android  to assets dir and build the GUI App (I have upload a small model https://drive.google.com/file/d/1pgZKWLZ-2M2PtYozJ9H0TQGzEbqjfyhf/view?usp=sharing)
5. run mnnsr commands in App