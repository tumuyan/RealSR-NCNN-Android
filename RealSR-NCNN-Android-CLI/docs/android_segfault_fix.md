# Android端 Segmentation Fault 修复

## 问题描述

- **平台**: Android (Adreno 740 GPU)
- **现象**: 处理图像时在最后一个 tile 行发生 Segmentation fault (exit code 139)
- **条件**: 启用 `fp16_storage`/`fp16_packed` + `int8_storage` 时触发
- **Windows端**: 正常，不触发

## 崩溃日志

```
yi=6/8 tile_h_nopad=64 in_tile_y0=374 in_tile_y1=456
yi=6 ncnn::Mat in created: 342x82
yi=6 VkCompute cmd created
yi=6 record_clone start
Segmentation fault
```

- 输入图像: 342x456, channels=1
- tilesize=64, prepadding=10, scale=4
- 崩溃位置: `yi=6` (第7行tile), `record_clone` 阶段

## 根因分析

当 `yi=6` 时:
- `in_tile_y0 = 374`, `in_tile_y1 = min(458, 456) = 456`
- `safe_tile_h = 456 - 374 = 82`（非标准大小，标准应为 `64 + 20 = 84`）

FP16+INT8 路径直接使用外部指针构造 `ncnn::Mat`:
```cpp
// 原始代码 — 对非标准 tile 高度，GPU 内存对齐失败
in = ncnn::Mat(w, 82, (unsigned char*)pixeldata + 374 * w * channels, (size_t)channels, 1);
```

Android GPU 在 FP16 模式下对非标准行数的外部指针 Mat 执行 `record_clone` 时内存对齐异常，导致段错误。

## 修复方案

核心思路: 对非标准尺寸的 tile strip，使用 `memcpy` 创建独立内存的 Mat，避免外部指针 Mat 在 GPU 上传时因内存对齐失败而崩溃。标准尺寸的 tile 仍保持零拷贝路径以兼顾性能。

## 修复总览

| 模块 | 模式1 (输入 tile 上传) | 模式2a (输出 tile 下载) | 状态 |
|------|----------------------|------------------------|------|
| `RealSR/src/main/jni/realsr.cpp` | 行 ~248 | 行 ~587 | ✅ 已修复 |
| `Waifu2x/src/main/jni/waifu2x.cpp` | 行 ~185 | 行 ~470 | ⏳ 模式1已修复, 模式2a待修复 |
| `SRMD/src/main/jni/srmd.cpp` | 行 ~235 | 行 ~553 | ⏳ 模式1已修复, 模式2a待修复 |
| `RealCUGAN/src/main/jni/realcugan.cpp` | 行 321, 1417, 1704, 2329 (4处) | 行 725, 2125 (2处) | ⏳ 模式1已修复, 模式2a待修复 |

### 模式1: 输入 tile — 非标准尺寸使用 memcpy

**RealSR** (标准尺寸 = `TILE_SIZE_Y + 2 * prepadding`):
```cpp
if ((opt.use_fp16_storage || opt.use_fp16_packed) && opt.use_int8_storage)
{
    const int safe_tile_h = in_tile_y1 - in_tile_y0;
    const bool is_standard_size = (safe_tile_h == TILE_SIZE_Y + 2 * prepadding);
    if (is_standard_size) {
        // 标准tile: 零拷贝外部指针（性能优先）
        in = ncnn::Mat(w, safe_tile_h, (unsigned char*)pixeldata + in_tile_y0 * w * channels, (size_t)channels, 1);
    } else {
        // 边界tile: memcpy确保内存对齐（安全优先）
        in.create(w, safe_tile_h, (size_t)channels, 1);
        const unsigned char* src = (unsigned char*)pixeldata + in_tile_y0 * w * channels;
        memcpy(in.data, src, (size_t)w * safe_tile_h * channels);
    }
}
```

**Waifu2x / SRMD** — 同上模式，条件为 `opt.use_fp16_storage && opt.use_int8_storage`。

**RealCUGAN** — 标准尺寸判断使用 `TILE_SIZE_Y + prepadding + prepadding_bottom`（因 `prepadding_bottom` 根据 `scale` 和实际 tile 高度做额外对齐填充，与 `prepadding` 不同）。

### 模式2a: 输出 tile 下载 offset

download 阶段同样使用外部指针 Mat + `record_clone`，非标准输出 tile 高度可能存在相同风险。

```cpp
// 修复前（固定公式，可能越界）
out = ncnn::Mat(out_gpu.w, out_gpu.h,
    (unsigned char*)outimage.data + yi * scale * TILE_SIZE_Y * w * scale * channels,
    (size_t)channels, 1);

// 修复后（使用实际 tile 起始位置）
int out_tile_y0 = std::max(yi * TILE_SIZE_Y, 0);
size_t offset = out_tile_y0 * scale * w * scale * channels;
out = ncnn::Mat(out_gpu.w, out_gpu.h,
    (unsigned char*)outimage.data + offset,
    (size_t)channels, 1, opt.blob_allocator);
```

### 模式2b: `to_pixels` 路径

`to_pixels` 不涉及外部指针 Mat 构造（内部会自行拷贝），风险较低。
