# 图像一致性评估指标说明

## 概述

评估脚本 `assets/scripts/evaluate_image_consistency.py` 将输入图像缩放到输出图像的尺寸后，对比两者的差异，从 **Alpha 通道**、**RGB 通道**、**边缘区域** 三个维度进行评估，最终汇总为综合得分。

报告模板为 `assets/scripts/report_viewer2.html`，数据文件为同名 `.js` 文件。

---

## 一、Alpha 通道异常检测 (`detect_alpha_anomalies`)

将输入图像的 Alpha 通道缩放到输出尺寸后，与输出的 Alpha 通道进行比较。

### 1.1 突然透明 (`sudden_transparency`)

- **计算方式**: 在输入中不透明 (`alpha > 0.8`) 但在输出中变透明 (`alpha < 0.3`) 的像素比例
- **公式**: `ratio = sum(opaque_mask & transparent_mask) / (H × W)`
- **严重程度阈值**: `>0.05` severe, `>0.01` moderate, 其余 none

### 1.2 边缘变化 (`edge_change`)

- **计算方式**: 对输入和输出的 Alpha 通道做 Canny 边缘检测，计算边缘差异像素占比
- **参数**: Canny 阈值 100/200，差异阈值 50
- **公式**: `ratio = sum(|input_edges - output_edges| > 50) / (H × W)`

### 1.3 直方图差异 (`histogram_divergence`)

- **计算方式**: 将 Alpha 值归一化到 [0,1]，分 50 个 bin 统计密度直方图，计算两个直方图的绝对差值之和
- **公式**: `divergence = sum(|hist_input - hist_output|)`

### 1.4 熵变化 (`entropy_change`)

- **计算方式**: 基于 1.3 的直方图计算信息熵，取绝对差值
- **公式**: `change = |entropy(input_hist) - entropy(output_hist)|`

### 1.5 Alpha 综合评分 (`alpha_anomaly_score`)

- **公式**: `score = sudden_transparency_ratio × 1000 + edge_change_ratio × 500 + histogram_divergence × 10 + entropy_change × 20`
- **范围**: 0 ~ 100（cap）

---

## 二、RGB 通道异常检测 (`detect_rgb_anomalies`)

将输入图像的 RGB 缩放到输出尺寸后，与输出的 RGB 进行比较。

### 2.1 通道均值差异 (`{R,G,B}_mean_diff`)

- **计算方式**: 分别计算 R/G/B 三通道在输入和输出中的均值绝对差
- **公式**: `diff = |mean(input_ch) - mean(output_ch)|`
- **值域**: 0 ~ 255

### 2.2 通道标准差差异 (`{R,G,B}_std_diff`)

- **计算方式**: 分别计算 R/G/B 三通道的标准差绝对差
- **公式**: `diff = |std(input_ch) - std(output_ch)|`

### 2.3 通道比例差异 (`max_channel_ratio_diff`)

- **计算方式**: 计算三通道均值占总均值的比例，取最大比例差的绝对值
- **公式**: `diff = max(|ratio_input - ratio_output|)`，其中 `ratio = channel_mean / sum(all_means)`
- **严重程度阈值**: `>0.3` severe, `>0.15` moderate, 其余 none

### 2.4 SSIM 结构相似性 (`ssim_score`)

- **计算方式**: 将 RGB 转灰度后，计算输入与输出的结构相似性指数
- **参数**: `data_range=255`
- **值域**: -1 ~ 1，越接近 1 越相似
- **严重程度阈值**: `<0.7` severe, `<0.85` moderate, 其余 none

### 2.5 MSE 均方误差 (`mse`)

- **计算方式**: 输入与输出所有像素差的平方均值（RGB 三通道整体）
- **公式**: `mse = mean((input - output)^2)`

### 2.6 PSNR 峰值信噪比 (`psnr`)

- **计算方式**: 由 MSE 推导
- **公式**: `psnr = 10 × log10(255^2 / mse)`，MSE=0 时为 100
- **值域**: 0 ~ 100 dB，越高越好

### 2.7 不透明区域 SSIM (`ssim_opaque`)

- **计算方式**: 仅在输入和输出均为不透明 (`alpha > 0.5`) 的像素区域上计算灰度 SSIM
- **目的**: 排除透明区域对 SSIM 的干扰

### 2.8 噪声比 (`noise_ratio`)

- **计算方式**: 对灰度图做 Laplacian 算子，计算标准差作为噪声指标，取输出/输入的比值
- **公式**: `ratio = std(laplacian(output)) / std(laplacian(input))`
- **严重程度阈值**: `>2.0` severe, `>1.5` moderate, 其余 none

### 2.9 RGB 综合评分 (`rgb_anomaly_score`)

- **公式**: `score = max_ratio_diff × 100 + (1 - ssim_score) × 50 + min(mse/1000, 1) × 20 + |1 - noise_ratio| × 30`
- **范围**: 0 ~ 100（cap）

---

## 三、边缘区域异常检测 (`detect_edge_anomalies`)

将输入图像缩放到输出尺寸后，取图像四周各 **4 像素** 宽度的边缘区域，比较输入与输出在这些区域上的差异。

### 边缘区域定义

通过布尔 mask 标记边缘像素：
- 上边缘: 行 0~3
- 下边缘: 行 H-4~H-1
- 左边缘: 列 0~3
- 右边缘: 列 W-4~W-1

提取方式: `pixels = image[edge_mask]`，结果为一维数组 `(N, 3)` / `(N,)`，N 为边缘像素总数。

### 3.1 边缘 RGB MAE (`edge_rgb_mae`)

- **计算方式**: 边缘区域所有像素的 RGB 三通道平均绝对误差
- **公式**: `mae = mean(|input_edge - output_edge|)`
- **值域**: 0 ~ 255，越低越好

### 3.2 边缘 RGB MSE (`edge_rgb_mse`)

- **计算方式**: 边缘区域所有像素的 RGB 三通道均方误差
- **公式**: `mse = mean((input_edge - output_edge)^2)`

### 3.3 边缘单通道 MAE (`edge_{r,g,b}_mae`)

- **计算方式**: 边缘区域各通道单独的 MAE

### 3.4 边缘 Alpha MAE (`edge_alpha_mae`)

- **计算方式**: 边缘区域 Alpha 通道的平均绝对误差
- **值域**: 0 ~ 1，越低越好

### 3.5 边缘 Alpha MSE (`edge_alpha_mse`)

- **计算方式**: 边缘区域 Alpha 通道的均方误差

### 3.6 边缘 RGB 评分 (`edge_rgb_score`)

- **公式**: `score = min(edge_rgb_mae × 2, 100)`
- **严重程度阈值**: `>30` severe, `>15` moderate, `>5` mild, 其余 normal

### 3.7 边缘 Alpha 评分 (`edge_alpha_score`)

- **公式**: `score = min(edge_alpha_mae × 200, 100)`
- **严重程度阈值**: `>30` severe, `>15` moderate, `>5` mild, 其余 normal

---

## 四、综合评分 (`overall_score`)

- **公式**: `score = alpha_anomaly_score × 0.6 + rgb_anomaly_score × 0.4`
- **范围**: 0 ~ 100
- **严重程度阈值**:

| 得分范围 | 严重程度 |
|---|---|
| > 60 | severe |
| > 30 | moderate |
| > 10 | mild |
| <= 10 | normal |

> 注意: 综合评分目前仅包含 Alpha 和 RGB 的权重，不包含边缘区域评分。

---

## 五、报告中显示的列

| 列名 | 数据来源 | 说明 |
|---|---|---|
| 序号 | 行号 | |
| 输出文件名 | result.output_filename | |
| 输入文件名 | result.input_filename | |
| 程序 | result.program | 推理程序名 |
| 测试参数 | result.params | 推理参数 |
| 输出尺寸 | result.output_size | 高×宽 |
| 输入尺寸 | result.input_size | 高×宽 |
| 透明度异常 | alpha_anomalies.sudden_transparency_severity | none/moderate/severe |
| 透明度比例 | alpha_anomalies.sudden_transparency_ratio | |
| Alpha异常分 | alpha_anomalies.alpha_anomaly_score | 0~100 |
| SSIM | rgb_anomalies.ssim_score | -1~1 |
| PSNR | rgb_anomalies.psnr | dB |
| 边缘RGB | edge_anomalies.edge_rgb_mae | 0~255 |
| 边缘Alpha | edge_anomalies.edge_alpha_mae | 0~1 |
| 综合得分 | overall.overall_score | 0~100 |
| 严重程度 | overall.severity | normal/mild/moderate/severe |
