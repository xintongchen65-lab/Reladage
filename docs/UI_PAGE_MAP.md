# 7 寸屏页面地图

`firmware/display/src/main.cpp` 负责硬件启动、UART 与实时训练接管；`firmware/display/src/product_ui/` 提供完整产品页面集合。所有页面共享 `RehabUiModel`，动作编号、训练状态、质量位图、设备状态与主控保持同一套协议。

| ID | 页面 | 主要内容 | 关键动作 |
|---:|---|---|---|
| 01 | 首页 Home | 今日任务、设备、周数据、快捷入口 | 进入计划 / 动作库 / 记录 / 设置 |
| 02 | 8动作库 Exercise Library | 8 个动作、部位、目标角、组次 | `SELECT_EXERCISE:0..7` |
| 03 | 训练计划 Plan Detail | 处方动作、组数、次数、目标、休息 | 进入训练前检查 |
| 04 | 训练前检查 Precheck | IMU、电量、SD、网络、语音状态 | 继续 / 设备中心 |
| 05 | 佩戴指导 Wear Guide | A/B/C/D/E 佩戴与在线状态 | 继续人体定位 |
| 06 | 人体定位 Body Position | 腰腹 E 节点、人体前方、重力基准 | `BEGIN_POSITION` |
| 07 | 动作校准 Motion Calibration | 起始姿态、功能轴/动作模板学习 | `CALIBRATION_CONTINUE` / `RETRY` |
| 08 | 正式训练 Live Training | 角度、组次、质量、错误、进度 | `PAUSE` / `STOP` |
| 09 | 组间休息 Rest | 倒计时、下一组信息 | `SKIP_REST` |
| 10 | 训练结果 Session Result | 完成量、ROM、达标率、质量总结 | 报告 / 首页 |
| 11 | 数字孪生 Digital Twin | 关节角、平面、躯干、IMU在线 | 实时刷新 |
| 12 | 游戏训练 Game Hub | 8动作对应训练玩法与达标触发 | `SELECT_MODE:1` + 启动 |
| 13 | 训练记录 History | 历史训练、时长、完成率 | 查看详情 |
| 14 | 康复报告 Report | ROM趋势、合格率、质量问题 | 周/月查看 |
| 15 | AI康复助手 AI Coach | 基于报告的训练解释与建议 | 生成建议 |
| 16 | 设备中心 Device Center | 五IMU、电量、SD、网络、语音 | 设备状态检查 |
| 17 | 设置 Settings | 音量、亮度、反馈、网络 | 本地设置 |
| 18 | 新处方同步 Prescription Sync | 家属/康复师下发计划 | 接收并确认 |
| 19 | 离线同步 Offline Sync | SD缓存、待上传条目、网络恢复 | `SYNC_NOW` |
| 20 | 关于系统 About | 版本、设备ID、协议/版权信息 | 返回 |

## 主流程

```text
首页 → 计划/动作库 → 训练前检查 → 佩戴指导 → 人体定位
     → 动作校准 → 正式训练 ↔ 组间休息 → 训练结果 → 报告
```

页面数据统一来自主控 LIVE 包和 `RehabUiModel`。正式训练结果还会同步进入 Session Report、Game Mapping、Digital Twin 与 Offline Sync 模块，因此屏幕不是独立演示页，而是产品运行链路的一部分。
