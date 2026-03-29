# Waifu2x Tile Padding黑边问题修复

## 问题描述

Waifu2x模块在处理图像时，输出图像的右侧和底部会出现几个像素宽度的黑边。

### 复现条件

- 图像尺寸不是tile大小的整数倍（如403x401、511x509）
- 边缘tile被合并到前一个tile后，内容尺寸超过TILE_SIZE但不是4的倍数

## 1. Bug #1：合并边缘tile时截断输出像素（🔴 关键）

### 1.1 根因

当图像尺寸略大于tile大小的整数倍（如 `w=403, tilesize=400`）时，上一个修复已将小边缘tile合并到最后一个完整tile中（`xtiles`从2减为1），但tile内容尺寸的计算仍使用 `min(TILE_SIZE, w)` 方式：

```cpp
const int tile_w_nopad = std::min((xi + 1) * TILE_SIZE_X, w) - xi * TILE_SIZE_X;
// w=403, xi=0: min(400, 403) - 0 = 400  ← 丢失了最后3个像素！
```

模型输入400像素，输出800像素（2x放大），但实际需要输出 `403 * 2 = 806` 像素。最后6个像素从未被写入，保持为0（黑色）。

同样的问题也存在于 `out_tile_y1` 的计算：

```cpp
int out_tile_y1 = std::min((yi + 1) * TILE_SIZE_Y, h);  // 可能截断底部
```

### 1.2 修复

对最后一个tile使用实际剩余尺寸：

```cpp
// tile内容尺寸：最后一个tile用实际剩余宽度，而非截断到TILE_SIZE
const int tile_w_nopad = (xi == xtiles - 1) ? (w - xi * TILE_SIZE_X) : TILE_SIZE_X;
const int tile_h_nopad = (yi == ytiles - 1) ? (h - yi * TILE_SIZE_Y) : TILE_SIZE_Y;
```

GPU路径的 `out_gpu` 输出strip高度也做同样修复：

```cpp
int out_tile_y0 = yi * TILE_SIZE_Y;
int out_tile_y1 = (yi == ytiles - 1) ? h : (yi + 1) * TILE_SIZE_Y;
```

## 2. Bug #2：Tile内容尺寸非4的倍数导致池化对齐错误（🔴 关键）

### 2.1 根因

Waifu2x的cunet模型内部使用stride-2的max-pool层（2个level），要求输入尺寸满足 `(content + 2 * prepadding)` 能被4整除（即 `content` 本身是4的倍数）。

当tile内容不是4的倍数时（如 `pixel2.png` 511x509，最后一个tile内容为 `111x109`），`109 + 18*2 = 145` 不能被4整除。经过多层stride-2池化后，floor除法导致输出尺寸偏移，边缘出现伪影。

### 2.2 修复

将tile内容向上对齐到4的倍数，模型处理后再裁剪到实际内容尺寸：

```cpp
// 对齐到4的倍数，使 (aligned + 2*prepadding) 可被4整除
const int aligned_tile_w = ((tile_w_nopad + 3) / 4) * 4;
const int aligned_tile_h = ((tile_h_nopad + 3) / 4) * 4;
```

**影响范围**：

| 位置 | 修改内容 |
|------|----------|
| GPU preproc（TTA） | `in_tile_gpu` 创建尺寸从 `TILE_SIZE + 2*prepadding` 改为 `aligned_tile + 2*prepadding` |
| GPU preproc（非TTA） | `in_tile_gpu` 创建尺寸从 `TILE_SIZE + 2*prepadding` 改为 `aligned_tile_w + 2*prepadding` x `aligned_tile_h + 2*prepadding` |
| GPU postproc | dispatcher的 `w/h` 从 `min(TILE_SIZE*scale, ...)` 改为 `tile_w_nopad * scale` / `tile_h_nopad * scale` |
| CPU padding | pad_right/pad_bottom 计算确保总padding尺寸 = `aligned_tile + 2*prepadding` |

TTA模式由于需要对tile进行转置（width↔height），需要统一的方形尺寸：

```cpp
// TTA需要统一维度用于转置tile
const int aligned_tile = std::max(aligned_tile_w, aligned_tile_h);
```

## 3. Bug #3：Alpha通道per-tile处理导致接缝（🟡 次要，已重构）

### 3.1 根因

RGBA图像（4通道）的alpha通道在每个tile中独立切割、独立bicubic放大、独立拼接。在tile边界处产生接缝和不连续。

### 3.2 修复

~原始修复为使用 `tile_w_nopad` 替代 `TILE_SIZE_X` 作为alpha tile尺寸。~

**最终修复**：彻底移除引擎内的per-tile alpha处理，改为在`main.cpp`层面整体处理alpha（见第6节）。

## 4. Bug #4：Alpha crop边界计算错误（🟡 次要，已重构）

### 4.1 根因

CPU路径的alpha通道裁剪使用了 `TILE_SIZE_X/Y` 作为内容边界，当tile内容超过 `TILE_SIZE` 时裁剪边界错误。

### 4.2 修复

~原始修复为使用 `tile_w_nopad` / `tile_h_nopad` 作为裁剪边界。~

**最终修复**：同Bug #3，随per-tile alpha代码一起移除（见第6节）。

## 5. 修改清单

| 位置 | 修改内容 |
|------|----------|
| GPU `out_tile_y1` | 使用 `(yi == ytiles - 1) ? h : (yi + 1) * TILE_SIZE_Y` |
| GPU/CPU `tile_w_nopad` / `tile_h_nopad` | 最后一个tile使用实际剩余尺寸 |
| GPU/CPU 新增 `aligned_tile_w` / `aligned_tile_h` | 对齐到4的倍数 |
| GPU/CPU 新增 `aligned_tile`（TTA用） | `max(aligned_tile_w, aligned_tile_h)` |
| GPU TTA preproc `in_tile_gpu` 创建 | 使用 `aligned_tile + 2*prepadding` |
| GPU 非TTA preproc `in_tile_gpu` 创建 | 使用 `aligned_tile_w + 2*prepadding` x `aligned_tile_h + 2*prepadding` |
| GPU TTA/非TTA postproc dispatcher | 使用 `tile_w_nopad * scale` / `tile_h_nopad * scale` |
| GPU alpha tile | 使用 `tile_w_nopad` / `tile_h_nopad` |
| CPU padding 计算 | 确保总尺寸 = `eff_aligned + 2*prepadding` |
| CPU TTA/非TTA alpha crop | 使用 `tile_w_nopad` / `tile_h_nopad` 作为裁剪边界 |

## 6. Alpha通道处理重构

### 6.1 问题

原始代码在引擎内部（`waifu2x.cpp`）逐tile处理alpha通道：每个tile独立切割alpha、独立bicubic放大、独立拼接。这导致：
- tile边界处alpha接缝
- 与tile尺寸对齐问题耦合，增加复杂度

### 6.2 解决方案

与RealCUGAN/SRMD保持一致，将alpha处理提升到`main.cpp`层面：

```
旧流程：输入RGBA → NCNN tile(4通道, alpha被逐tile切割/bicubic) → 输出RGBA
新流程：输入RGBA → main.cpp分离alpha → RGB走NCNN tile(3通道) → OpenCV bicubic整体放大alpha → 合并 → 输出RGBA
```

### 6.3 引擎层清理

`waifu2x.cpp`中移除了所有per-tile alpha处理代码：

| 位置 | 移除内容 |
|------|----------|
| `waifu2x.h` | `ncnn::Layer* bicubic_2x;` 声明 |
| 构造函数 | `bicubic_2x = 0;` |
| 析构函数 | `bicubic_2x->destroy_pipeline()` / `delete bicubic_2x;` |
| `load()` | bicubic_2x layer初始化 |
| GPU TTA preproc | `in_alpha_tile_gpu.create()` 和 `if (channels == 4)` 分支 |
| GPU TTA postproc | `out_alpha_tile_gpu` bicubic放大 |
| GPU 非TTA preproc | `in_alpha_tile_gpu.create()` 和 `if (channels == 4)` 分支 |
| GPU 非TTA postproc | `out_alpha_tile_gpu` bicubic放大 |
| CPU TTA preproc | `in_alpah_tile_nocrop` alpha分离和crop |
| CPU TTA postproc | `out_alpha_tile` bicubic和alpha memcpy |
| CPU 非TTA preproc | alpha分离和crop |
| CPU 非TTA postproc | alpha bicubic和memcpy |
| 输出to_pixels | `if (channels == 4)` RGBA输出分支 |

**GPU shader兼容**：preproc/postproc的SPIR-V shader仍包含alpha binding索引。保持bindings vector的相同大小，alpha binding填入空`ncnn::VkMat()`作为dummy。由于`constants[channels] == 3`，shader内部跳过alpha处理，不会实际访问dummy binding。

## 7. 经验总结

### 7.1 边缘tile合并后的尺寸处理

当通过减少tile数量将小边缘像素合并到最后一个tile时，必须全面检查所有使用 `TILE_SIZE` 的地方，替换为实际tile内容尺寸。容易遗漏的位置：

- 输出buffer创建尺寸
- 后处理dispatcher的w/h
- alpha通道tile尺寸
- 裁剪边界计算

**检查方法**：搜索代码中所有 `TILE_SIZE` 的引用，逐个判断是否应为实际tile内容尺寸。

### 7.2 模型输入尺寸与池化层对齐

使用stride-2池化层的超分模型（waifu2x/cunet、RealCUGAN等），其输入尺寸需要满足对齐约束：

- 含N层stride-2池化的模型：`(input + 2 * prepadding)` 需要能被 `2^N` 整除
- cunet有2层：`(input + 2 * prepadding)` 需要被4整除，即 `input` 本身是4的倍数

**处理策略**：将tile内容向上对齐到4的倍数，模型处理后的输出通过postproc dispatcher裁剪到实际尺寸。

### 7.3 TTA模式的特殊约束

TTA（Test-Time Augmentation）需要对tile做转置（transpose），转置后原来的width变为height、height变为width。因此：

- 8个方向的tile需要统一尺寸（方形）
- 使用 `max(aligned_tile_w, aligned_tile_h)` 作为统一维度
- 非TTA模式可以使用独立的 `aligned_tile_w` 和 `aligned_tile_h`

### 7.4 与上一个修复的关系

上一个修复（`waifu2x_control_flow_fix.md`）通过合并小边缘tile解决了段错误问题，但引入了新问题：

| 上一个修复 | 本次修复 |
|------------|----------|
| `xtiles--` 合并小tile | `tile_w_nopad` 使用实际剩余尺寸 |
| 防止tile太小模型推理失败 | tile内容对齐到4的倍数 |
| `out_tile.empty()` 防御性检查 | postproc裁剪到实际内容尺寸 |

两个修复共同构成了完整的边缘tile处理方案：合并→对齐→裁剪。

### 7.5 Alpha通道处理原则

所有基于ncnn tile的超分模块应统一采用"alpha外置"策略：

1. `main.cpp`在加载时分离alpha（RGBA→RGB），只将3通道送入引擎
2. `main.cpp`在保存时用`resize_alpha_bicubic()`整体放大alpha，再`merge_rgb_alpha()`合并
3. 引擎内部只处理3通道RGB，移除所有per-tile alpha代码
4. GPU shader的alpha binding用dummy空VkMat占位，保持SPIR-V接口兼容
