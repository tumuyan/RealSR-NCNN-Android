# SRMD Alpha通道处理修复说明



## 问题描述

SRMD模块在处理带有alpha通道的图像时，存在边缘alpha通道错误的问题。原因是SRMD的tile处理流程中，alpha通道在边界tile处的处理存在bug。

## 1. 解决方案
绕过SRMD的tile处理流程，使用OpenCV独立处理alpha通道。

## 2. SRMD/src/main/jni/main.cpp 修改内容

### 2.1 Task类修改

```cpp
class Task
{
public:
    int id;
    // int webp;  // 移除

    path_t inpath;
    path_t outpath;

    ncnn::Mat inimage;
    ncnn::Mat outimage;
    ncnn::Mat inalpha;   // 新增：存储原始alpha通道
    int has_alpha;        // 新增：标记是否有alpha通道
};
```

### 2.2 load函数重构

**原逻辑**：使用wic/stb/webp直接加载像素数据，SRMD处理4通道图像

**新逻辑**：
```cpp
void* load(void* args)
{
    // ...
    for (int i=0; i<count; i++)
    {
        const path_t& imagepath = ltp->input_files[i];

        cv::Mat inBGR, inAlpha;
        imread(imagepath, inBGR, inAlpha);  // 使用新的imread函数
        
        if (inBGR.empty()) continue;
        
        Task v;
        v.id = i;
        v.inpath = imagepath;
        v.outpath = ltp->output_files[i];
        v.has_alpha = 0;

        int w = inBGR.cols;
        int h = inBGR.rows;
        int c = inBGR.channels();  // 始终为3

        if (!inAlpha.empty())
        {
            v.has_alpha = 1;
            
            // 复制alpha数据
            unsigned char* alphadata = (unsigned char*)malloc(w * h);
            memcpy(alphadata, inAlpha.data, w * h);
            v.inalpha = ncnn::Mat(w, h, (void*)alphadata, (size_t)1, 1);
            
            // 如果输出格式是JPG，改为PNG
            path_t ext = get_file_extension(v.outpath);
            if (ext == PATHSTR("jpg") || ...) {
                v.outpath = ltp->output_files[i] + PATHSTR(".png");
            }
        }

        // 复制BGR数据
        unsigned char* pixeldata = (unsigned char*)malloc(w * h * c);
        memcpy(pixeldata, inBGR.data, w * h * c);
        
        v.inimage = ncnn::Mat(w, h, (void*)pixeldata, (size_t)c, c);
        v.outimage = ncnn::Mat(w * scale, h * scale, (size_t)c, c);

        toproc.put(v);
    }
    return 0;
}
```

### 2.3 save函数重构

```cpp
void* save(void* args)
{
    // ...
    for (;;)
    {
        Task v;
        tosave.get(v);
        if (v.id == -233) break;

        int success = 0;
        path_t ext = get_file_extension(v.outpath);

        if (ext != PATHSTR("gif")) {
            cv::Mat image;
            
            if (v.has_alpha)
            {
                // 从SRMD输出获取RGB
                cv::Mat rgb_image(v.outimage.h, v.outimage.w, CV_8UC3, v.outimage.data);
                
                // 从原始alpha获取并缩放
                cv::Mat alpha_image(v.inalpha.h, v.inalpha.w, CV_8UC1, v.inalpha.data);
                cv::Mat scaled_alpha;
                cv::resize(alpha_image, scaled_alpha, cv::Size(rgb_image.cols, rgb_image.rows), 
                           0, 0, cv::INTER_CUBIC);
                
                // 合并RGB和alpha
                std::vector<cv::Mat> channels;
                cv::split(rgb_image, channels);
                channels.push_back(scaled_alpha);
                cv::merge(channels, image);
            }
            else
            {
                image = cv::Mat(v.outimage.h, v.outimage.w, CV_8UC3, v.outimage.data);
            }
            
            // 保存图像
            #if _WIN32
                success = imwrite_unicode(v.outpath, image);
            #else
                success = imwrite(v.outpath.c_str(), image);
            #endif
        }
        
        // 释放内存
        if (v.inimage.data) free(v.inimage.data);
        if (v.has_alpha && v.inalpha.data) free(v.inalpha.data);
        
        // ...
    }
    return 0;
}
```

---

## 3. 处理流程对比

### 原流程
```
输入RGBA → SRMD tile处理(含alpha) → 输出RGBA (边缘有bug)
```

### 新流程
```
输入RGBA
    ↓
imread分离
    ├── RGB → SRMD超分 → 放大的RGB
    └── Alpha → 保存原始 → OpenCV bicubic缩放 → 放大的Alpha
                                              ↓
                                    合并RGB和Alpha
                                              ↓
                                        输出RGBA
```

---

## 4. 关键改进点

1. **alpha通道完全独立于SRMD的tile处理**
   - 原始alpha数据被保存，不经过SRMD的GPU处理
   - 避免了tile边界处的alpha通道错误

2. **使用OpenCV进行alpha缩放**
   - 使用`cv::INTER_CUBIC`双三次插值
   - 简单可靠，无边界问题

3. **自动检测全不透明alpha**
   - 如果alpha通道所有像素都是255，则忽略alpha通道
   - 减少不必要的处理

4. **输出格式自动转换**
   - 如果输入有alpha但输出格式是JPG，自动改为PNG

---

## 5. 其他模块适配说明

其他模块（Waifu2x、RealSR、RealCUGAN等）如需应用相同修复：

1. 将`common/utils.hpp`复制到项目中（或共享）
2. 修改`main.cpp`：
   - Task类添加`inalpha`和`has_alpha`成员
   - load函数使用`imread()`替代原有图像加载逻辑
   - save函数添加alpha合并逻辑
3. 确保包含`#include "utils.hpp"`

---

## 6. 注意事项
### 定义问题
- `utils.hpp`中定义了`path_t`类型，与各模块的`filesystem_utils.h`中的定义相同
- 如果编译出现`path_t`重复定义错误，需要确保只在一个地方定义
- Windows下使用`imread_unicode`支持中文路径

### 文件编码问题

**重要：源代码文件如果含中文，优先使用UTF-8无BOM编码！**

| 编码格式 | BOM | MSVC编译 | 说明 |
|----------|-----|----------|------|
| UTF-8无BOM | 无 | ✅ 正常 | 推荐 |
| UTF-8带BOM | `EF BB BF` | ❌ 可能失败 | BOM被视为无效字符 |
| GBK | 无 | ✅ 正常 | 中文Windows默认 |

---