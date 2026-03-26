# RealCUGAN Alpha通道与右边缘像素错误修复


## 问题描述

RealCUGAN模块在处理带有alpha通道的RGBA图像时，存在两个问题：
1. 输出图片最右侧几像素区域RGB值错误
2. Alpha通道存在接缝/错误

## 1. 问题根因

### 1.1 Alpha通道错误

RGBA 4通道图像直接通过NCNN的tile处理流程。每个tile的alpha通道被独立切割（`copy_cut_border`）、分别进行bicubic双三次插值放大，在tile拼接处产生接缝和不连续。

### 1.2 RGB右边缘像素错误（4x scale特有）

`realcugan.cpp` 中4x scale的残差跳连代码：

```cpp
const float* inptr = in_tile.channel(q).row(prepadding + i / 4) + prepadding;
*outptr++ = *ptr++ * 255.f + 0.5f + inptr[j / 4] * 255.f;
```

当最后一个tile的宽度不是4的整数倍时，`j/4` 会索引到零填充区域（border type 2），导致残差值偏移，使右边缘几像素的RGB值不正确。

对于2x/3x scale，不存在此残差跳连问题。

## 2. 解决方案

绕过NCNN的tile流程处理alpha通道，将alpha分离出来独立处理：

```
旧流程：输入RGBA → NCNN tile(4通道, alpha在tile边界被切割) → 输出RGBA(有bug)
新流程：输入RGBA → 分离alpha → RGB走NCNN tile(3通道) → OpenCV bicubic放大alpha → 合并 → 输出RGBA
```

## 3. main.cpp 修改内容

### 3.1 Task类修改

```cpp
class Task
{
public:
    int id;
    int webp;
    int scale;
    int has_alpha;        // 新增：标记是否有alpha通道

    path_t inpath;
    path_t outpath;

    ncnn::Mat inimage;
    ncnn::Mat outimage;
    ncnn::Mat inalpha;   // 新增：存储原始alpha通道
};
```

### 3.2 load函数修改

检测到4通道图像时，在送入NCNN之前分离alpha：

```cpp
if (c == 4)
{
    v.has_alpha = 1;

    // 提取alpha通道
    unsigned char* alphadata = (unsigned char*)malloc(w * h);
    for (int j = 0; j < w * h; j++)
        alphadata[j] = pixeldata[j * 4 + 3];
    v.inalpha = ncnn::Mat(w, h, (void*)alphadata, (size_t)1, 1);

    // RGBA转RGB（丢弃alpha，只保留R/G/B）
    unsigned char* rgbdata = (unsigned char*)malloc(w * h * 3);
    for (int j = 0; j < w * h; j++)
    {
        rgbdata[j * 3 + 0] = pixeldata[j * 4 + 0];
        rgbdata[j * 3 + 1] = pixeldata[j * 4 + 1];
        rgbdata[j * 3 + 2] = pixeldata[j * 4 + 2];
    }
    free(pixeldata);
    pixeldata = rgbdata;
    c = 3;   // NCNN只处理3通道RGB
    webp = 0; // 标记数据来源为malloc
}
```

**关键点**：将RGBA转RGB后，`c=3`，NCNN管线只会走3通道处理路径（不进入任何4通道alpha tile处理代码）。

### 3.3 save函数修改

在保存时用OpenCV独立处理alpha，然后合并：

```cpp
if (v.has_alpha)
{
    // 从RealCUGAN输出获取RGB（3通道）
    cv::Mat rgb_image(v.outimage.h, v.outimage.w, CV_8UC3, v.outimage.data);
#ifndef _WIN32
    cv::cvtColor(rgb_image, rgb_image, cv::COLOR_RGB2BGR);
#endif

    // 用OpenCV bicubic插值放大原始alpha
    cv::Mat alpha_image(v.inalpha.h, v.inalpha.w, CV_8UC1, (unsigned char*)v.inalpha.data);
    cv::Mat scaled_alpha = resize_alpha_bicubic(alpha_image, v.scale);

    // 释放alpha数据（在使用完毕后）
    if (v.inalpha.data) free(v.inalpha.data);

    // 合并RGB和alpha
    merge_rgb_alpha(rgb_image, scaled_alpha, image);
}
```

### 3.4 内存释放注意事项

由于RGBA转RGB时使用了`malloc`分配新缓冲区（而非`stbi_image_malloc`），在save中释放时需要区分：

```cpp
if (v.webp == 1)
    free(pixeldata);
else if (v.has_alpha)
    free(pixeldata);   // alpha分离后的RGB数据，由malloc分配
else
    stbi_image_free(pixeldata);  // 原始stb数据
```

## 4. 关键改进点

1. **Alpha通道完全独立于NCNN tile处理**
   - 原始alpha数据被保存，不经过NCNN的GPU/CPU tile管线
   - 避免了tile边界处的alpha接缝和错误

2. **解决4x scale右边缘RGB错误**
   - 只让RGB 3通道走NCNN管线，消除了4x残差跳连中零填充区域的影响
   - 2x/3x scale也受益于更干净的3通道处理

3. **使用OpenCV进行alpha缩放**
   - `resize_alpha_bicubic()` 使用 `cv::INTER_CUBIC` 双三次插值
   - 在完整图像上操作，无tile边界问题

4. **复用 `utils.hpp` 公共函数**
   - `resize_alpha_bicubic()` — 双三次缩放alpha
   - `merge_rgb_alpha()` — 合并RGB和alpha通道

## 5. 与SRMD修复的对比

| 项目 | SRMD | RealCUGAN |
|------|------|-----------|
| 加载方式 | 使用`utils.hpp`的`imread()` | 保留原始webp/stb加载，alpha分离后转RGB |
| Alpha分离 | load中直接从OpenCV Mat分离 | load中从原始pixeldata逐像素提取 |
| NCNN输入 | 3通道 | 3通道 |
| Alpha缩放 | `cv::resize` | `resize_alpha_bicubic()`（封装了`cv::resize`） |
| 内存管理 | 全部malloc | 需区分stbi/malloc来源 |

## 6. 其他模块适配

其他模块（Waifu2x、RealSR、SRMD等）如存在相同的alpha边缘问题，均可采用相同策略修复：

1. 在load阶段分离alpha，只将RGB送入NCNN
2. 在save阶段用`resize_alpha_bicubic()` + `merge_rgb_alpha()`合并
3. 注意内存释放方式的区分

## 7. 修复CPU模式下vkCreateInstance错误

### 7.1 问题描述

在无GPU环境中使用 `-g -1`（CPU模式）运行时，仍然输出 `vkCreateInstance failed -9` 错误信息。不影响功能（程序正常回退到CPU），但输出干扰测试结果。

### 7.2 根因

原始代码中 `ncnn::create_gpu_instance()` 在参数解析之后无条件调用：

```cpp
ncnn::create_gpu_instance();  // 无条件创建Vulkan实例

if (gpuid.empty())
    gpuid.push_back(ncnn::get_default_gpu_index());
```

即使指定了 `-g -1`，Vulkan实例仍被创建，在无GPU驱动环境下产生错误。

### 7.3 关键发现：ncnn GPU函数的隐式初始化

ncnn的部分函数会**隐式调用** `create_gpu_instance()`，包括：
- `ncnn::get_gpu_count()`
- `ncnn::get_default_gpu_index()`

因此在CPU模式下，不能调用这些函数，否则仍会触发Vulkan初始化。

### 7.4 解决方案

将Vulkan初始化改为**按需创建**，并根据是否实际使用GPU决定后续逻辑：

```cpp
bool use_gpu = false;
if (gpuid.empty())
{
    // 未指定GPU：自动检测，有GPU才初始化Vulkan
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
    // 检查是否有非-1的GPU请求
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

// GPU设备校验也只在use_gpu时执行
if (use_gpu)
{
    int gpu_count = ncnn::get_gpu_count();
    for (int i = 0; i < use_gpu_count; i++)
    {
        if (gpuid[i] == -1) continue;  // 跳过CPU设备
        if (gpuid[i] < 0 || gpuid[i] >= gpu_count)
        {
            fprintf(stderr, "invalid gpu device\n");
            return -1;
        }
    }
}

// 结尾也只在use_gpu时销毁
if (use_gpu)
    ncnn::destroy_gpu_instance();
```

### 7.5 原始代码的另一个Bug

原始的GPU设备校验逻辑有误：

```cpp
// 原始代码
if (gpuid[i] < -1 || gpuid[i] >= gpu_count)
```

当 `gpuid[i] = -1`（CPU模式）且 `gpu_count = 0` 时：
- `-1 < -1` → false
- `-1 >= 0` → **true** → 误判为无效设备

修复后跳过 `-1` 的校验：

```cpp
if (gpuid[i] == -1) continue;
if (gpuid[i] < 0 || gpuid[i] >= gpu_count)  // 注意改为 < 0
```

### 7.6 测试脚本配置

在 `test-all.sh` 的 `block_realcugan` 中添加 `-g -1`，避免测试时触发Vulkan初始化：

```bash
run_test "$prog" "-g -1 -m models-nose -s 2 -n 0" 1
run_test "$prog" "-g -1 -m models-se -s 2 -n -1" 2
# ...
```

### 7.7 其他模块适配

此模式适用于所有基于ncnn的模块（Waifu2x、RealSR、SRMD、Anime4k等），统一处理原则：
1. `create_gpu_instance()` 按需调用，CPU模式下不调用
2. `get_gpu_count()` / `get_default_gpu_index()` 只在GPU模式下调用
3. 测试脚本通过 `-g -1` 显式指定CPU模式
