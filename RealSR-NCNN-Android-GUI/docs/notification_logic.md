# 通知逻辑说明

本文档详细说明了应用中各通知模式的行为、通道配置及优先级逻辑。

## 1. 通知模式详细行为表

| 模式名称 | 阶段 | 通知通道 (ID) | 优先级 | 视觉效果 | 声音/震动 | 行为描述 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Silent**<br>(Background Only) | 处理中 | Background Service<br>(`channel_service_alive`) | LOW | 常驻 (Ongoing)<br>仅提醒一次 | 无 | 显示 "Processing..."，不更新进度。<br>点击打开主界面。 |
| | 结果 | - | - | - | - | 无结果通知。任务结束时处理通知自动消失。 |
| **Result Only** | 处理中 | Background Service<br>(`channel_service_alive`) | LOW | 常驻 (Ongoing)<br>仅提醒一次 | 无 | 显示 "Processing..."，不更新进度。<br>点击打开主界面。 |
| | 结果 | Processing Result<br>(`channel_result`) | **HIGH** | 自动消失 (AutoCancel)<br>小图标 | **有**<br>(默认声音+震动) | 显示 "Done" 或 "Fail"。<br>点击打开主界面。 |
| **Detailed**<br>(Progress + Result) | 处理中 | Processing Progress<br>(`channel_progress`) | LOW | 常驻 (Ongoing)<br>仅提醒一次 | 无 | **实时更新**进度百分比/日志。<br>点击打开主界面。 |
| | 结果 | Processing Result<br>(`channel_result`) | **HIGH** | 自动消失 (AutoCancel)<br>小图标 | **有**<br>(默认声音+震动) | 显示 "Done" 或 "Fail"。<br>点击打开主界面。 |
| **Detailed**<br>(Auto Dismiss) | 处理中 | Processing Progress<br>(`channel_progress`) | LOW | 常驻 (Ongoing)<br>仅提醒一次 | 无 | **实时更新**进度百分比/日志。<br>点击打开主界面。 |
| | 结果 | - | - | - | - | **成功时**：无通知，处理通知直接消失。<br>**失败时**：同 Result Only 模式 (强制显示错误)。 |

## 2. 通道与优先级说明

应用使用了三个不同的通知通道，以区分处理过程和处理结果的干扰程度：

1.  **Processing Progress (`channel_progress`)**
    *   **用途**：用于 "Detailed" 模式下的实时进度更新。
    *   **优先级**：`IMPORTANCE_LOW` (低)。
    *   **表现**：静默显示，无声音，无震动。防止进度频繁更新时产生噪音。

2.  **Background Service (`channel_service_alive`)**
    *   **用途**：用于 "Silent" 和 "Result Only" 模式下的后台保活显示。
    *   **优先级**：`IMPORTANCE_LOW` (低)。
    *   **表现**：静默显示，无声音，无震动。

3.  **Processing Result (`channel_result`)**
    *   **用途**：用于任务结束时的结果通知 ("Done" 或 "Fail")。
    *   **优先级**：`IMPORTANCE_HIGH` (高)。
    *   **表现**：有声音，有震动，可能会在屏幕上方弹出。确保用户能及时获知任务完成。

## 3. 特殊逻辑

*   **Auto Dismiss 模式**：设计初衷是“成功即忽略，失败才提醒”。适合批量处理或后台挂机场景，成功时不打扰用户，只有出错时才会通过高优先级的 Result 通道发出警报。
