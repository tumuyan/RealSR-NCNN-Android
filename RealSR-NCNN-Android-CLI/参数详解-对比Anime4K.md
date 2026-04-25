# Anime4K 参数详解与跨程序对比

> **适用程序**：Anime4K vs RealSR / Waifu2x / SRMD / RealCUGAN / MNN-SR / Resize

---

## 📋 目录

1. [Anime4K 完整参数列表](#1-anime4k-完整参数列表)
2. [通用参数横向对比](#2-通用参数横向对比)
3. [核心处理参数对比](#3-核心处理参数对比)
4. [GPU与性能参数对比](#4-gpu与性能参数对比)
5. [输出控制参数对比](#5-输出控制参数对比)
6. [Anime4K 独有特性](#6-anime4k-独有特性)
7. [功能等价映射表](#7-功能等价映射表)
8. [选择建议](#8-选择建议)

***

## 1. Anime4K 完整参数列表

### 1.1 基本输入输出

| 参数 | 全称 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| `-i` | input | 字符串 | `p1.png` | 输入文件路径（图片/视频/URL） |
| `-o` | output | 字符串 | 自动生成 | 输出文件路径 |
| `-v` | videoMode | 标志 | 关闭 | 视频处理模式 |
| `-W` | web | 标志 | 关闭 | 从网络URL下载并处理 |
| `-s` | preview | 标志 | 关闭 | 预览模式（仅图片） |
| `-S` | start | 整数 | 0 | 视频预览起始帧号 |

### 1.2 核心算法参数 ⭐ Anime4K特色

| 参数 | 全称 | 类型 | 默认值 | 范围 | 说明 |
|------|------|------|--------|------|------|
| `-p` | passes | 整数 | 2 | - | 处理迭代次数（更多=更慢但质量更高） |
| `-n` | pushColorCount | 整数 | 2 | - | 颜色推送次数限制 |
| `-c` | strengthColor | 浮点数 | 0.3 | 0.0\~1.0 | **颜色增强强度**（越高线条越细） |
| `-g` | strengthGradient | 浮点数 | 1.0 | 0.0\~1.0 | **梯度增强强度**（越高边缘越锐利） |
| `-z` | zoomFactor | 浮点数 | 2.0 | - | **缩放因子**（支持非整数，如2.5） |
| `-f` | fastMode | 标志 | 关闭 | - | 快速模式（更快但质量略低） |

### 1.3 AI模型相关

| 参数 | 全称 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| `-w` | CNNMode | 标志 | 关闭 | 启用ACNet深度学习模型（替代传统Anime4K09算法） |
| `-H` | HDN | 标志 | 关闭 | 启用HDN模式（High-Denoise，强去噪） |
| `-L` | HDNLevel | 整数 | 1 | 1\~3 | HDN去噪等级（1=轻，3=重） |
| `-N` | ncnn | 标志 | 关闭 | 强制使用NCNN后端+ACNet模型 |
| `-Z` | ncnnModelPath | 字符串 | `./ncnn-models` | NCNN自定义模型文件路径 |

### 1.4 图像滤镜系统 ⭐ 独有特性

| 参数 | 全称 | 类型 | 默认值 | 范围 | 说明 |
|------|------|------|--------|------|------|
| `-b` | preprocessing | 标志 | 关闭 | - | **启用预处理滤镜** |
| `-a` | postprocessing | 标志 | 关闭 | - | **启用后处理滤镜** |
| `-r` | preFilters | 整数(二进制) | 4 (CAS) | 1\~127 | **预处理滤镜组合**（需启用-b） |
| `-e` | postFilters | 整数(二进制) | 72 (推荐) | 1\~127 | **后处理滤镜组合**（需启用-a） |

**滤镜编码表（二进制位运算）**：

| 位值 | 滤镜名称 | 说明 | 推荐场景 |
|------|---------|------|---------|
| 1 (001) | Median blur | 中值模糊 | 去除椒盐噪声 |
| 2 (010) | Mean blur | 均值模糊 | 轻微平滑 |
| 4 (100) | CAS Sharpening | CAS锐化 | 增强细节 |
| 8 (1000) | Gaussian blur weak | 弱高斯模糊 | <1080P图像推荐 |
| 16 (10000) | Gaussian blur | 高斯模糊 | >=1080P图像推荐 |
| 32 (100000) | Bilateral filter | 双边滤波 | 保边去噪 |
| 64 (1000000) | Bilateral filter faster | 快速双边滤波 | 视频推荐（性能优化） |

**组合示例**：
```bash
# 推荐配置：弱高斯 + 双边滤波 = 8 + 32 = 40
-r 40 -b    # 预处理：<1080P图像
-e 72 -a    # 后处理：>=1080P图像（快速双边+高斯）

# 自定义组合：中值模糊 + CAS锐化 + 双边滤波 = 1 + 4 + 32 = 37
-r 37 -b
```

### 1.5 GPU加速参数

| 参数 | 全称 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| `-q` | GPUMode | 标志 | 关闭 | 启用GPU加速 |
| `-M` | GPGPUModel | 字符串 | `opencl` | GPU后端：`opencl` / `cuda` / `ncnn` |
| `-h` | platformID | 整数 | 0 | OpenCL平台ID |
| `-d` | deviceID | 整数 | 0 | GPU设备ID（-1=CPU） |
| `-Q` | OpenCLQueueNumber | 整数 | 1 | OpenCL命令队列数量 |
| `-P` | OpenCLParallelIO | 标志 | 关闭 | OpenCL并行IO模式 |
| `-T` | ncnnThreads | 整数 | 4 | NCNN线程数 |

### 1.6 视频专用参数 ⭐ 独有特性

| 参数 | 全称 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| `-C` | codec | 字符串 | `mp4v` | 视频编码格式 |
| `-F` | forceFps | 浮点数 | 0.0 | 强制输出帧率（0=保持原帧率） |
| `-x` | hardwareDecode | 标志 | 关闭 | 硬件视频解码 |
| `-X` | hardwareEncode | 标志 | 关闭 | 硬件视频编码 |
| `-t` | threads | 整数 | 自动检测 | 视频处理线程数 |
| `-D` | disableProgress | 标志 | 关闭 | 禁用进度显示 |

**支持的编解码器 (`-C`)**：

| 编码器 | 平台支持 | 特点 |
|--------|---------|------|
| `mp4v` | Windows推荐 | MPEG-4 Part 2 |
| `dxva` | Windows专属 | DirectX Video Acceleration |
| `avc1` (H.264) | Linux推荐 | 兼容性最好 |
| `vp09` (VP9) | 全平台 | 开源但速度较慢 |
| `hevc` (H.265) | 仅Linux | 高压缩率 |
| `av01` (AV1) | 仅Linux | 最新标准 |
| `other` | 全平台 | 其他格式 |

### 1.7 输出与Alpha通道

| 参数 | 全称 | 类型 | 默认值 | 说明 |
|------|------|------|--------|------|
| `-E` | suffix | 字符串 | 空 | **强制指定输出文件后缀** |
| `-A` | alpha | 标志 | 关闭 | **保留Alpha透明通道** |

### 1.8 工具与调试

| 参数 | 全称 | 说明 |
|------|------|------|
| `-l` | listGPUs | 列出所有可用GPU设备 |
| `-V` | version | 显示版本信息 |
| `-B` | benchmark | 运行性能基准测试 |

---

## 2. 通用参数横向对比

### 2.1 必备参数（所有程序都有）

| 功能 | Anime4K | RealSR | Waifu2x | SRMD | RealCUGAN | MNN-SR | Resize |
|------|---------|--------|--------|------|-----------|--------|--------|
| **输入路径** | `-i` ✅ | `-i` ✅ | `-i` ✅ | `-i` ✅ | `-i` ✅ | `-i` ✅ | `-i` ✅ |
| **输出路径** | `-o` ✅ | `-o` ✅ | `-o` ✅ | `-o` ✅ | `-o` ✅ | `-o` ✅ | `-o` ✅ |
| **帮助信息** | ❌ 无独立参数 | `-h` ✅ | `-h` ✅ | `-h` ✅ | `-h` ✅ | ❌ | `-h` ✅ |
| **详细输出** | ❌ | `-v` ✅ | `-v` ✅ | `-v` ✅ | `-v` ✅ | `-v` ✅ | `-v` ✅ |

**差异说明**：
- ❌ Anime4K **没有** `-h` 帮助参数和 `-v` 详细输出参数
- ✅ Anime4K 使用 `-V` 显示版本信息（其他程序无此功能）
- ✅ Anime4K 支持从URL输入 (`-W`)，其他程序不支持

---

### 2.2 格式控制参数对比

| 功能 | Anime4K | 其他6个程序 |
|------|---------|------------|
| **强制格式** | ❌ **无直接对应** | `-f` (force format) |
| **建议格式** | ❌ **无直接对应** | `-e` (expected format) |
| **指定后缀** | `-E` suffix ✅ | ❌ 无此参数 |
| **Alpha通道保留** | `-A` alpha ✅ | 自动检测（可被`-f`覆盖）|

**关键差异**：

```bash
# Anime4K: 通过 -E 指定输出后缀
anime4k -i photo.png -o result -E .jpg

# 其他程序: 通过 -f/-e 控制格式
realsr -i photo.png -o result -f jpg     # 强制JPG
waifu2x -i photo.png -o result -e jpg    # 建议JPG（智能alpha处理）
```

**⚠️ 注意**：Anime4K的 `-E` 参数行为与其他程序的 `-f/-e` 不同！
- Anime4K `-E`：**只改变文件扩展名**，不参与格式决策逻辑
- 其他程序 `-f/-e`：**影响整个格式判断流程**（包括alpha检测）

---

## 3. 核心处理参数对比

### 3.1 缩放/放大倍数

| 程序 | 参数 | 默认值 | 支持范围 | 特色 |
|------|------|--------|---------|------|
| **Anime4K** | `-z` zoomFactor | 2.0 | **任意浮点数** | ✨ 最灵活（如2.5、3.7） |
| RealSR | `-s` scale | 4 | 2, 4 | 固定整数倍 |
| Waifu2x | `-s` scale | 2 | 1, 2, 4, 8, 16, 32 | 多种整数倍 |
| SRMD | `-s` scale | 2 | 2, 3, 4 | 固定整数倍 |
| RealCUGAN | `-s` scale | 2 | 1, 2, 3, 4 | 包含1倍（仅增强） |
| MNN-SR | `-s` scale | 4 | 固定4倍 | 不可调整 |
| Resize | `-s` scale | 4 | **任意浮点数** | 与Anime4K相同 |

**对比结论**：

```
灵活性排名：
🥇 Anime4K / Resize  → 任意浮点数（最灵活）
🥈 Waifu2x            → 6种整数倍可选
🉑 RealCUGAN          → 4种整数倍（含1倍）
📊 RealSR/SRMD        → 2-3种固定倍数
❄️ MNN-SR             → 固定4倍（不可调）
```

**实际应用示例**：

```bash
# Anime4K: 2.5倍放大（独特能力）
anime4k -i image.png -o output.png -z 2.5

# Resize: 同样支持2.5倍
resize-ncnn -i image.png -o output.png -s 2.5

# 其他程序: 只能选固定倍数
waifu2x -i image.png -o output.png -s 2   # 只能2倍或4倍
```

---

### 3.2 去噪/降噪参数

| 程序 | 参数 | 默认值 | 可选值 | 控制粒度 |
|------|------|--------|--------|---------|
| **Anime4K** | `-H` + `-L` | 关闭 | HDN等级1\~3 | **粗粒度**（3级） |
| Waifu2x | `-n` noise | 0 | -1, 0, 1, 2, 3 | 中等（5级） |
| SRMD | `-n` noise | 3 | -1\~10 | **最细**（12级） |
| RealCUGAN | `-n` noise | -1 | -1, 0, 1, 2, 3 | 中等（5级） |
| 其他 | ❌ 无 | - | - | 不支持 |

**Anime4K的去噪机制**：

```bash
# Anime4K: 启用HDN模式 + 选择等级
anime4k -i noisy.png -o clean.png -H -L 2  # HDN等级2（中等去噪）

# 对比其他程序:
srmd-ncnn -i noisy.png -o clean.png -n 5    # SRMD第5级去噪
waifu2x-ncnn -i noisy.png -o clean.png -n 2 # Waifu2x第2级去噪
```

**关键区别**：
- ✅ Anime4K 的去噪是**集成在ACNet模型中的**（HDN = High-Denoise Network）
- ✅ 其他程序的去噪是**独立的参数化模块**
- ⚠️ Anime4K **必须**先启用 `-w` (CNNMode) 才能使用 `-H` (HDN)

---

### 3.3 质量vs速度权衡

| 程序 | 快速模式参数 | 效果 | 说明 |
|------|-------------|------|------|
| **Anime4K** | `-f` fastMode | 更快但质量略低 | 简化算法流程 |
| 所有AI程序 | 减小 `-t` tilesize | 减少内存占用 | 可能影响质量 |
| Resize | `-m nearest` | 最快插值 | 最近邻（最低质量） |

**Anime4K独有的质量控制参数**：

| 参数 | 作用 | 调优建议 |
|------|------|---------|
| `-p` passes | 迭代次数 | 2=默认，4=高质量（慢4倍）|
| `-c` strengthColor | 线条细度 | 0.3=默认，0.5=更细线条 |
| `-g` strengthGradient | 边缘锐度 | 1.0=默认，0.8=柔和边缘 |

```bash
# Anime4K: 高质量设置（慢但效果好）
anime4k -i anime.png -o hd.png -p 4 -c 0.5 -g 1.0

# Anime4K: 快速预览
anime4k -i anime.png -o preview.png -f -p 1
```

---

## 4. GPU与性能参数对比

### 4.1 GPU后端支持矩阵

| GPU后端 | Anime4K | RealSR | Waifu2x | SRMD | RealCUGAN | MNN-SR | Resize |
|---------|---------|--------|--------|------|-----------|--------|--------|
| **CPU** | ✅ 默认 | ✅ (-1) | ✅ (-1) | ✅ (默认) | ✅ (-1) | ✅ (b=0) | ✅ (-n) |
| **OpenCL** | ✅ (-q -M opencl) | ✅ (-g 0) | ✅ (-g 0) | ✅ (-g 0) | ✅ (-g 0) | ✅ (b=3) | ❌ |
| **CUDA** | ✅ (-q -M cuda) | ❌ | ❌ | ❌ | ❌ | ✅ (b=2) | ❌ |
| **NCNN/Vulkan** | ✅ (-q -M ncnn) | ❌ | ❌ | ❌ | ❌ | ✅ (b=7) | ❌ |
| **OpenGL** | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ (b=6) | ❌ |
| **TensorRT** | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ (b=9) | ❌ |

**Anime4K的GPU优势**：
- 🏆 **唯一同时支持3种GPU后端的程序**
- 🎯 可通过 `-M` 参数灵活切换：`opencl` / `cuda` / `ncnn`
- 🔧 NCNN后端**只能配合ACNet模型使用**（`-w` 必须启用）

---

### 4.2 设备选择方式对比

#### Anime4K 方式：
```bash
# 两阶段选择：先选后端(-M)，再选设备(-h/-d)
anime4k -q -M opencl -h 0 -d 1   # OpenCL平台0的设备1
anime4k -q -M cuda -d 0           # CUDA设备0
anime4k -N -d -1                  # NCNN CPU模式
anime4k -N -d 0                   # NCNN GPU模式
```

#### 其他程序方式：
```bash
# 直接通过-g参数指定（部分支持多GPU）
realsr-ncnn -g 0                  # GPU 0
realsr-ncnn -g 0,1,2              # 多GPU并行
mnnsr -b 7                        # Vulkan后端
mnnsr -g -1                       # CPU模式
```

**差异总结**：

| 特性 | Anime4K | 其他程序 |
|------|---------|---------|
| **多GPU并行** | ❌ 不支持 | ✅ RealSR/Waifu2x/SRMD/RealCUGAN支持 |
| **后端切换** | ✅ 运行时切换 | ❌ 编译时确定（除MNN-SR） |
| **设备枚举** | ✅ `-l` 列出所有GPU | ❌ 需手动尝试 |
| **平台概念** | ✅ 有platformID | ❌ 无（OpenCL特有）|

---

### 4.3 并行与线程控制

| 程序 | 线程参数 | 默认值 | 最大值 | 用途 |
|------|---------|--------|--------|------|
| **Anime4K** | `-t` threads | 自动检测(CPU核心数) | 32×核心数 | **视频处理**线程 |
| **Anime4K** | `-T` ncnnThreads | 4 | CPU核心数 | **NCNN推理**线程 |
| RealSR | `-j load:proc:save` | 1:2:2 | - | 三阶段线程分离 |
| Waifu2x | `-j load:proc:save` | 1:2:2 | - | 同上 |
| SRMD | `-j load:proc:save` | 1:2:2 | - | 同上 |
| RealCUGAN | `-j load:proc:save` | 1:2:2 | - | 同上 |

**Anime4K的独特性**：
- ✅ **区分两种线程**：视频IO线程(`-t`) vs AI推理线程(`-T`)
- ✅ **自动适配**：默认使用CPU核心数作为上限
- ❌ **不支持**三阶段分离（加载/处理/保存独立线程数）

**其他程序的优势**：
- ✅ **细粒度控制**：可分别设置加载、处理、保存的线程数
- ✅ **适合多GPU**：每个GPU可分配独立的处理线程

---

## 5. 输出控制参数对比

### 5.1 Alpha通道处理

| 程序 | 参数 | 行为 | 说明 |
|------|------|------|------|
| **Anime4K** | `-A` alpha | **显式开关** | 用户主动决定是否保留 |
| 其他6个程序 | **自动检测** | **隐式处理** | 有alpha时自动转PNG（除非`-f`强制） |

**代码逻辑对比**：

```cpp
// Anime4K: 显式控制（main.cpp 第463行）
auto alpha = config.exist("alpha");
// 如果用户没加-A，就不保留alpha

// 其他程序: 自动检测（以RealSR为例）
if (v.has_alpha && format.empty()) {
    // 有alpha且未强制格式 → 自动转PNG
}
```

**使用建议**：
- 📸 **PNG透明素材**：Anime4K用 `-A`，其他程序用 `-e png`
- 🖼️ **JPG批量输出**：Anime4K不加`-A`，其他程序用 `-f jpg`

---

### 5.2 文件命名规则

#### Anime4K的自动命名规则（第559-572行）：

```cpp
if (defaultOutputName) {  // 用户未指定-o
    std::ostringstream oss;
    if (CNN)
        oss << "_ACNet_HDNL" << (HDN ? HDNLevel : 0);
    else
        oss << "_Anime4K09";
    
    oss << "_x" << zoomFactor      // 缩放倍数
        << (suffix.empty() ? 
            inputPath.extension().string() : 
            suffix);                // 后缀
    
    outputPath = inputPath.stem().string() + oss.str();
}
```

**生成的文件名示例**：

```bash
# 输入: photo.png
# 命令: anime4k -i photo.png （不带-o）

# 输出: photo_Anime4K09_x2.00.png          （默认模式）
# 输出: photo_ACNet_HDNL1_x2.00.png         （-w -H -L 1）
# 输出: photo_ACNet_HDNL0_x2.50.jpg         （-w -z 2.5 -E .jpg）
```

**与其他程序的对比**：

| 程序 | 未指定-o时的行为 | 命名规则 |
|------|------------------|---------|
| **Anime4K** | ✅ 自动生成 | `{原名}_{算法}_x{倍数}{后缀}` |
| 其他6个程序 | ❌ 报错退出 | 要求必须指定`-o` |

**Anime4K优势**：✅ 更方便快速测试，不需要每次都写输出路径

---

### 5.3 批量处理时的目录结构

**所有程序的行为一致**（保持相对路径结构）：

```bash
# 输入目录:
photos/
├── vacation/
│   └── img001.png
└── portrait/
    └── img002.png

# 输出目录（所有程序都会生成）:
output/
├── vacation/
│   └── img001_*.png
└── portrait/
    └── img002_*.png
```

**差异点**：

| 特性 | Anime4K | 其他程序 |
|------|---------|---------|
| **跳过已处理文件** | ❌ **不支持** | ✅ `-k` 参数 |
| **自定义命名模板** | ❌ **不支持** | ✅ `-p` 参数 |
| **进度显示** | ✅ 默认开启 | ✅ `-v` 控制 |

---

## 6. Anime4K 独有特性

### 6.1 🎬 视频处理能力 ⭐⭐⭐ 核心优势

**Anime4K是唯一支持原生视频处理的程序！**

支持的输入格式：
- 📹 **视频容器**：MP4, AVI, MKV, MOV, WMV, FLV, WebM, GIF, TS, MTS, M2TS, 3GP, M4V, OGV, VOB
- 🖼️ **图片格式**：JPG, PNG, BMP, WebP, TIFF
- 🌐 **网络URL**：HTTP/HTTPS链接（需编译时启用libcurl）

**视频处理专属参数**：

```bash
# 基础视频处理
anime4k -i video.mp4 -o output.mp4 -v

# 高质量视频放大（4倍 + ACNet + HDN去噪）
anime4k -i video.mp4 -o output.mkv -v -z 4 -w -H -L 2 \
  -C avc1 -F 30          # H.264编码 + 30fps

# GIF动画增强
anime4k -i animation.gif -o enhanced.gif -v -z 2

# 硬件加速（推荐）
anime4k -i video.mp4 -o output.mp4 -v -q -x -X
# -q: GPU加速
# -x: 硬件解码
# -X: 硬件编码
```

**其他程序的限制**：
- ❌ 只能处理静态图片
- ❌ 需要先用FFmpeg提取帧→处理→再合成视频

---

### 6.2 🎨 图像滤镜系统 ⭐⭐ 独家功能

Anime4K拥有完整的**预处理/后处理滤镜管线**：

```bash
# 完整滤镜管线示例
anime4k -i old_photo.png -o restored.png \
  -b -r 40 \              # 预处理：弱高斯(8) + 双边滤波(32)
  -a -e 72 \              # 后处理：高斯(16) + 快速双边(64) = 72
  -w -H -L 2              # ACNet + HDN等级2
```

**滤镜工作流程**：

```
原始图像
    ↓
[预处理滤镜] ← -b -r (可选)
    ↓
[Anime4K/ACNet算法] ← 核心处理
    ↓
[后处理滤镜] ← -a -e (可选)
    ↓
最终输出
```

**推荐的滤镜组合**：

| 场景 | 预处理(-r) | 后处理(-e) | 说明 |
|------|-----------|-----------|------|
| 普通<1080P图 | 40 (8+32) | 72 (8+64) | 平衡质量和速度 |
| 高清>=1080P图 | 48 (16+32) | 80 (16+64) | 更强的平滑 |
| 视频<1080P | 40 (8+32) | 72 (8+64) | 性能优化 |
| 视频>=1080P | 48 (16+32) | 80 (16+64) | 性能优化 |
| 极度降噪 | 63 (1+2+4+8+16+32) | 127 (全开) | 牺牲速度换质量 |
| 无滤镜 | 不加-b | 不加-a | 纯算法处理 |

---

### 6.3 🔧 灵活的算法参数调节 ⭐⭐

Anime4K提供**4个连续的质量调节旋钮**：

| 参数 | 作用 | 降低值效果 | 升高值效果 | 典型用途 |
|------|------|-----------|-----------|---------|
| `-p` passes | 迭代次数 | 更快但可能欠处理 | 更慢但更彻底 | 时间充裕时用4 |
| `-c` strengthColor | 颜色强度 | 线条变粗 | 线条变细锐利 | 线稿优化 |
| `-g` strengthGradient | 梯度强度 | 边缘变柔和 | 边缘变锐利 | 照片vs动漫 |
| `-z` zoomFactor | 放大倍数 | 小幅度放大 | 大幅度放大 | 按需调整 |

**典型配置预设**：

```bash
# 预设1: 动漫高清化（推荐）
anime4k -i anime.png -o hd.png -p 2 -c 0.3 -g 1.0 -z 2

# 预设2: 极致质量（慢）
anime4k -i masterpiece.png -o ultra.png -p 4 -c 0.5 -g 1.0 -z 4 -w -H -L 1

# 预设3: 快速预览
anime4k -i sketch.png -o preview.png -f -p 1 -z 2

# 预设4: 照片轻度增强
anime4k -i photo.png -o enhanced.png -c 0.2 -g 0.8 -z 2 -b -r 4 -a -e 8

# 预设5: 线稿提取风格
anime4k -i lineart.png -o sharp.png -c 0.8 -g 1.0 -z 2
```

---

### 6.4 🌐 网络URL支持

```bash
# 直接处理网络图片
anime4k -W -i "https://example.com/image.png" -o local.png

# 处理网络视频（需要编译时ENABLE_LIBCURL）
anime4k -W -v -i "https://example.com/video.mp4" -o enhanced.mp4
```

**其他程序**：❌ 完全不支持，必须先手动下载

---

## 7. 功能等价映射表

### 7.1 参数功能对照表

| 目标功能 | Anime4K | RealSR/Waifu2x/SRMD/RealCUGAN/MNN-SR/Resize |
|----------|---------|-----------------------------------------------|
| **输入文件** | `-i` | `-i` |
| **输出文件** | `-o` | `-i` |
| **放大倍数** | `-z` | `-s` |
| **快速模式** | `-f` | 减小`-t`(tilesize) |
| **GPU加速** | `-q -M xxx` | `-g 0` (OpenCL) |
| **CPU模式** | 不加`-q` | `-g -1` 或 MNN-SR的`-b 0` |
| **保留Alpha** | `-A` | 使用`-e png`（自动检测）|
| **强制格式** | `-E .ext` | `-f ext` |
| **详细日志** | ❌ 无 | `-v` |
| **帮助信息** | ❌ 无 | `-h` |
| **版本信息** | `-V` | ❌ 无 |
| **列出GPU** | `-l` | ❌ 无 |
| **基准测试** | `-B` | ❌ 无 |
| **视频处理** | `-v` (videoMode) | ❌ 不支持 |
| **去噪** | `-H -L` | `-n` |
| **滤镜** | `-b/-a -r/-e` | ❌ 不支持 |
| **URL输入** | `-W` | ❌ 不支持 |
| **硬件编解码** | `-x -X` | ❌ 不支持 |
| **跳过已处理** | ❌ 不支持 | `-k` |
| **TTA模式** | ❌ 不支持 | `-x` |
| **多GPU** | ❌ 不支持 | `-g 0,1,2` |
| **同步间隙** | ❌ 不支持 | RealCUGAN的`-c` |
| **颜色空间** | ❌ 不支持 | MNN-SR的`-c` |
| **码赛克去除** | ❌ 不支持 | MNN-SR的`-d` |
| **缩放算法** | ❌ 不支持（内置） | Resize的`-m` |

---

### 7.2 场景迁移指南

#### 场景A：单张图片2倍放大

```bash
# Anime4K
anime4k -i photo.png -o output.png -z 2

# 等效的其他程序
waifu2x-ncnn -i photo.png -o output.png -s 2
realcugan-ncnn -i photo.png -o output.png -s 2
resize-ncnn -i photo.png -o output.png -s 2
```

#### 场景B：批量处理目录

```bash
# Anime4K
anime4k -i photos/ -o output/

# 等效的其他程序
realsr-ncnn -i photos/ -o output/ -e png -v
```

**差异**：
- ✅ Anime4K自动创建output目录
- ✅ Anime4K显示处理进度（默认）
- ❌ Anime4K无法跳过已有文件（其他程序可以`-k`）

#### 场景C：带Alpha通道的PNG

```bash
# Anime4K: 显式保留
anime4k -i transparent.png -o output.png -A

# 其他程序: 智能处理
waifu2x-ncnn -i transparent.png -o output.png -e png
# 自动检测到alpha → 保持PNG格式

# 强制丢弃alpha
realsr-ncnn -i transparent.png -o output.jpg -f jpg
```

#### 场景D：视频放大（Anime4K独家）

```bash
# Anime4K: 一站式解决
anime4k -i video.mp4 -o hd_video.mp4 -v -z 2 -C avc1

# 其他程序: 需要多步操作
# 1. 用ffmpeg提取帧
ffmpeg -i video.mp4 frames/%04d.png

# 2. 逐帧处理
waifu2x-ncnn -i frames/ -o frames_hd/ -s 2 -e png

# 3. 合成视频
ffmpeg -i frames_hd/%04d.png -i video.mp4 \
  -map 0:v -map 1:a -c copy hd_video.mp4
```

---

## 8. 选择建议

### 8.1 决策树

```
需要处理什么？
    │
    ├─📹 视频 → ⭐ 必须 Anime4K
    │         └─ 唯一支持原生视频处理的程序
    │
    ├─🖼️ 图片 → 继续判断
    │
    │   ├─ 动漫/插画？
    │   │   ├─ 是 → Anime4K 或 Waifu2x 或 RealCUGAN
    │   │   │   ├─ 需要2.5倍等非整数？→ Anime4K / Resize
    │   │   │   ├─ 需要精细滤镜控制？→ ⭐ Anime4K
    │   │   │   ├─ 需要最高32倍？→ ⭐ Waifu2x
    │   │   │   └─ 通用高质量？→ RealCUGAN
    │   │   │
    │   │   └─ 否（照片/真实图像）→ 继续
    │   │       ├─ JPEG压缩损伤？→ ⭐ RealSR
    │   │       ├─ 老旧照片/强噪声？→ ⭐ SRMD
    │   │       ├─ 仅需简单缩放？→ Resize
    │   │       └─ 移动端部署？→ MNN-SR
    │   │
    │   └─ 特殊需求？
    │       ├─ 需要CUDA加速？→ Anime4K / MNN-SR
    │       ├─ 需要多GPU并行？→ RealSR / Waifu2x / SRMD / RealCUGAN
    │       ├─ 需要从URL处理？→ ⭐ Anime4K
    │       ├─ 需要断点续传？→ 其他程序（-k参数）
    │       └─ 需要自定义命名？→ 其他程序（-p参数）
```

### 8.2 程序定位总结

| 程序 | 核心定位 | 最佳场景 | 独家优势 |
|------|---------|---------|---------|
| **Anime4K** | 🎬 **全能型**（图片+视频） | 动漫/插画增强、视频 upscale | 视频处理、滤镜系统、URL支持 |
| **RealSR** | 📷 **JPEG修复专家** | 压缩损伤恢复、真实照片 | JPEG artifact专门优化 |
| **Waifu2x** | 🎨 **动漫超分专家** | 二次元图像、高倍放大 | 最高32倍、多种噪声级别 |
| **SRMD** | 🔧 **专业去噪工具** | 老照片修复、噪点抑制 | 12级细粒度去噪 |
| **RealCUGAN** | ⚖️ **平衡之选** | 通用场景、速度质量兼顾 | 1-4倍灵活、同步优化 |
| **MNN-SR** | 📱 **移动端方案** | Android/iOS部署、多后端 | 跨平台、码赛克去除 |
| **Resize** | 🔧 **纯缩放工具** | 快速尺寸调整、非整数倍 | 无AI依赖、最快速度 |

### 8.3 组合使用策略

在实际项目中，经常需要**组合使用多个程序**：

#### 策略1：视频处理流水线

```bash
# 步骤1: Anime4K 处理视频（唯一选择）
anime4k -v -i raw_video.mp4 -o upscaled.mkv \
  -z 2 -w -H -L 1 -C avc1 -x -X

# 步骤2: 如需进一步处理截图
waifu2x-ncnn -i screenshots/ -o final/ -s 2 -e png -k 5000
```

#### 策略2：混合图片批处理

```bash
# 动漫插图用 Anime4K（更好的滤镜控制）
anime4k -i anime_art/ -o anime_hd/ -z 2 -b -r 40 -a -e 72

# 真实照片用 RealSR（JPEG优化）
realsr-ncnn -i photos/ -o photos_enhanced/ -s 4 -e png

# 简单图标用 Resize（最快）
resize-ncnn -i icons/ -o icons_2x/ -s 2 -m bicubic
```

#### 策略3：质量优先级链

```bash
# 第一遍: Anime4K 2倍（利用其优秀的滤镜系统）
anime4k -i original.png -o pass1.png -z 2 -w -H -L 2 -b -r 48 -a -e 80

# 第二遍: Waifu2x 再2倍（达到4倍总放大）
waifu2x-ncnn -i pass1.png -o final.png -s 2 -n -1 -f png
```

---

## 📊 快速参考卡片

### Anime4K 常用命令速查

| 任务 | 命令 |
|------|------|
| 图片2倍放大 | `anime4k -i img.png -o out.png -z 2` |
| 图片4倍+HDN | `anime4k -i img.png -o out.png -z 4 -w -H -L 2` |
| 视频放大 | `anime4k -v -i vid.mp4 -o out.mp4 -z 2 -C avc1` |
| GIF增强 | `anime4k -v -i anim.gif -o out.gif -z 2` |
| URL处理 | `anime4k -W -i "url" -o out.png` |
| 快速模式 | `anime4k -i img.png -o out.png -f -z 2` |
| 批量处理 | `anime4k -i dir/ -o out/` |
| GPU加速 | `anime4k -q -M cuda -i img.png -o out.png` |
| 列出GPU | `anime4k -l` |
| 基准测试 | `anime4k -B` |
| 版本信息 | `anime4k -V` |

### 参数冲突提醒 ⚠️

当Anime4K与其他程序**参数字母相同时但含义不同**时：

| 参数 | Anime4K含义 | 其他程序含义 | 冲突程度 |
|------|------------|-------------|---------|
| `-f` | **fastMode**（快速模式）| **format**（输出格式） | ⚠️ **完全不同！** |
| `-v` | **videoMode**（视频模式）| **verbose**（详细输出） | ⚠️ **完全不同！** |
| `-e` | **postFilters**（后处理滤镜）| **expected format**（建议格式） | ⚠️ **完全不同！** |
| `-h` | **platformID**（OpenCL平台）| **help**（帮助信息） | ⚠️ **完全不同！** |
| `-s` | **preview**（预览） | **scale**（放大倍数） | ⚠️ **完全不同！** |
| `-z` | **zoomFactor**（缩放因子） | ❌ 无此参数 | - |
| `-E` | **suffix**（文件后缀） | ❌ 无此参数 | - |

**💡 重要提示**：在不同程序间切换时，务必确认参数的实际含义！

---

## 📝 文档信息

- **文档版本**：v1.0
- **创建日期**：2026-04-24
- **适用范围**：Anime4KCPP CLI 及其与项目中其他6个程序的对比
- **数据来源**：源代码分析（[Anime4K/main.cpp](Anime4k/src/main/jni/CLI/src/main.cpp)）

---

> 💡 **提示**：本文档专注于**参数级别的技术对比**。关于各程序的内部实现细节、算法原理、性能基准测试等内容，请参阅[参数与输出规则详解.md](./参数与输出规则详解.md)。
>
> 🎯 **核心结论**：Anime4K是一个**功能全面且独特**的工具，特别是在**视频处理、滤镜系统、GPU后端灵活性**方面具有不可替代的优势。但对于**纯静态图片批处理**场景，其他程序在某些特定领域（如多GPU、断点续传、JPEG优化）仍有其存在价值。
