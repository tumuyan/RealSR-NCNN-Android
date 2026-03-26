# Waifu2x CPU路径控制流错误与模型边界问题修复

## 问题描述

Waifu2x模块在CPU模式下（`-g -1`）存在以下问题：
1. 处理RGBA图像（4通道，如nest.png 401x403）时发生段错误（segfault）
2. 处理某些图像时进度输出不稳定，偶尔崩溃
3. 模型提取失败时（`extract()` 返回-100）未做错误处理，导致后续数组越界

RGB 3通道图像（如pixel2.png 511x509）在GPU路径下可正常工作，但CPU路径同样受影响。

## 1. Bug #1：`else` 分支错位（段错误的根本原因）

### 1.1 根因

`process_cpu` 函数中，非TTA（Test-Time Augmentation）的处理代码被错误地放在了**进度打印的 `if-else` 分支内**，而非 `if (tta_mode)` 的 `else` 分支内。

原始代码结构：

```cpp
if (tta_mode)                    // 第612行
{
    // TTA处理代码（8个方向推理）
    // ...
    if (time_span_print_progress > 0.5 || ...)   // 第772行 - 进度打印
    {
        fprintf(stderr, "%5.2f%%\t...", ...);
        time_print_progress = end;
    }
    else                         // 第779行 - BUG：这个else匹配的是进度打印if，不是tta_mode if！
    {
        // 非TTA处理代码 - 只有进度不打印时才执行！
        // 模型推理、后处理等...
    }
}
```

**问题**：当 `tta_mode == 0`（绝大多数情况），整个 `if (tta_mode)` 块被跳过，非TTA处理代码永远不会执行。但当GPU路径下进度打印被跳过（timing-dependent），`else` 分支可能意外执行，导致 `out` 变量未初始化，在 `to_pixels` 时段错误。

### 1.2 修复

将 `else` 分支正确关联到 `if (tta_mode)`，并将进度打印代码独立出来：

```cpp
if (tta_mode)
{
    // TTA处理代码...
}
else
{
    // 非TTA处理代码 - 正确匹配tta_mode的else
}

// 进度打印 - 独立于TTA逻辑
high_resolution_clock::time_point end = high_resolution_clock::now();
float time_span_print_progress = duration_cast<duration<double>>(
        end - time_print_progress).count();
if (time_span_print_progress > 0.5 || (yi + 1 == ytiles && xi + 3 > xtiles)) {
    fprintf(stderr, "%5.2f%%\t[%5.2fs /%5.2f ETA]\n", ...);
    time_print_progress = end;
}
```

## 2. Bug #2：后处理偏移量错误（堆缓冲区溢出）

### 2.1 根因

非TTA路径的后处理代码在读取模型输出时，使用了 `pad_top * scale` 和 `pad_left * scale` 作为偏移量：

```cpp
const float* ptr = out_tile.channel(q).row(i + pad_top * scale) + pad_left * scale;
```

但waifu2x模型（cunet/upconv）的输出**已经去除了padding**，只包含有效区域。以cunet为例：

- 输入tile大小（含padding）：436x436（tile 400 + prepadding 18 × 2）
- 模型输出大小：800x800（= 2 × (436 - 2×18) = 2 × 400）
- 后处理期望的输出大小：`tile_w_nopad * scale` × `tile_h_nopad * scale` = 800 × 800
- 偏移 `pad_top * scale = 36`，但行高只有800，访问第836行时越界

### 2.2 修复

移除所有 `pad_* * scale` 偏移，模型输出与后处理尺寸完全匹配，偏移应为0：

```cpp
// 非TTA路径
const float* ptr = out_tile.channel(q).row(i);

// TTA路径同理
const float* ptr0 = out_tile_0.row(i);
const float* ptr1 = out_tile_1.row(out_tile[0].h - 1 - i);
// ... 其他方向也移除偏移
```

## 3. Bug #3：小尺寸边缘tile导致模型推理失败

### 3.1 根因

当图像尺寸略大于tile大小（如401px，tile为400px）时，最后一个tile只有1px宽。加上prepadding（cunet为18）后，实际输入为 `1 + 18×2 = 37px`。但cunet模型要求输入至少为 `prepadding × 2 + 1 = 37px`，边界情况下模型无法处理，`extract()` 返回错误-100，输出为空Mat。

后处理代码在 `out_tile.empty()` 时仍然尝试访问数据，导致段错误或浮点异常。

### 3.2 修复

**方案一：合并小边缘tile**

在计算tile数量时，如果最后一个tile太小（小于 `prepadding * 2 + 1`），则减少tile数量，将边缘像素合并到最后一个完整tile中：

```cpp
int xtiles = (w + TILE_SIZE_X - 1) / TILE_SIZE_X;
int ytiles = (h + TILE_SIZE_Y - 1) / TILE_SIZE_Y;

// 避免极小的边缘tile导致模型推理失败
while (xtiles > 1 && (w - (xtiles - 1) * TILE_SIZE_X) < prepadding * 2 + 1)
    xtiles--;
while (ytiles > 1 && (h - (ytiles - 1) * TILE_SIZE_Y) < prepadding * 2 + 1)
    ytiles--;
```

以401px为例：`xtiles = 2`，剩余 `401 - 400 = 1px < 37px`，所以 `xtiles` 减为1，整张图作为一个tile处理。

**方案二：跳过空输出（防御性编程）**

```cpp
if (out_tile.empty())
    continue;
```

两处修复同时应用：GPU路径和CPU路径均合并小tile，CPU路径额外增加空输出检查。

## 4. Bug #4：`time_print_progress` 未初始化（GPU路径）

### 4.1 根因

```cpp
high_resolution_clock::time_point time_print_progress;  // 未初始化！
```

首次比较 `end - time_print_progress` 时产生未定义行为。

### 4.2 修复

```cpp
high_resolution_clock::time_point time_print_progress = begin;
```

GPU路径和CPU路径均修复。

## 5. 修改文件与函数清单

| 文件 | 函数 | 修改内容 |
|------|------|----------|
| `waifu2x.cpp` | `process()` (GPU路径) | 合并小边缘tile、初始化 `time_print_progress` |
| `waifu2x.cpp` | `process_cpu()` (CPU路径) | 修复 `else` 分支错位、移除后处理偏移、合并小边缘tile、初始化 `time_print_progress`、增加 `out_tile.empty()` 检查 |

## 6. 测试结果

修复后通过所有12项测试（3个模型 × 2种噪声级别 × 2张图片）：

| 模型 | 噪声级别 | nest.png (RGBA 401×403) | pixel2.png (RGB 511×509) |
|------|----------|-------------------------|--------------------------|
| cunet | n0 | ✓ 通过 | ✓ 通过 |
| cunet | n2 | ✓ 通过 | ✓ 通过 |
| upconv_7_anime | n0 | ✓ 通过 | ✓ 通过 |
| upconv_7_anime | n2 | ✓ 通过 | ✓ 通过 |
| upconv_7_photo | n0 | ✓ 通过 | ✓ 通过 |
| upconv_7_photo | n2 | ✓ 通过 | ✓ 通过 |

修复前：cunet + nest.png 在第2个tile处段错误（FPE/segfault）。

## 7. 经验总结

### 7.1 C++ `if-else` 匹配规则

在长函数中，`else` 总是匹配最近的 `if`，容易在大段代码中产生错误关联。建议：
- 使用大括号 `{}` 明确标注 `if-else` 的作用范围
- 进度日志、性能统计等辅助代码应独立于核心业务逻辑

### 7.2 模型输出与padding

不同超分模型对padding的处理方式不同：
- **waifu2x/cunet**：模型内部处理padding，输出只包含有效区域，后处理偏移应为0
- **RealCUGAN**：输出包含padding区域，后处理需要 `prepadding` 偏移

在移植或修改超分模型代码时，必须确认模型输出是否包含padding。

### 7.3 边缘tile的最小尺寸

所有基于tile的超分模型都需要保证输入尺寸不低于 `prepadding × 2 + 1`。对于尺寸略大于tile整数倍的图像，必须处理剩余像素：
- 合并到最后一个完整tile（推荐）
- 或用 `REPLICATE` 边界填充扩展到最小尺寸

### 7.4 调试技巧

使用 `-DCMAKE_CXX_FLAGS="-g -fsanitize=address"` 编译可获得精确的内存错误报告：
- **heap-buffer-overflow**：定位到具体的数组越界行号
- **FPE（浮点异常）**：定位到除零或NaN操作
- **segfault**：通过ASAN可获取崩溃的调用栈
