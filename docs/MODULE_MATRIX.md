# Module Matrix

| 模块 | 代码位置 | 主要职责 |
|---|---|---|
| 5 IMU BLE | `firmware/controller/RehabMotion_v5/real_imu_abcde.h` | A/B/C/D/E 扫描、连接、重连与数据接收 |
| 动态人体参考 | `torso_reference.h` | 腰腹参考、人体语义方向、姿态质量指标 |
| K11 肘角引擎 | `gyro_elbow_k11.h` | A/B 语义轴、相对旋转与肘角 |
| 8 动作引擎 | `RehabMotion_v5/src/rehab/motion_engine/` | 8个独立 detector、动作阶段、单次聚合指标、质量位图与统一输出 |
| 动作校准策略 | `motion_engine/core/exercise_calibration.*` | 8动作静止门、学习动作、返回确认与校准参数 |
| 运动特征流水线 | `motion_engine/core/motion_feature_pipeline.*` | 滚动窗口速度、稳定性、姿态偏差与节律特征 |
| 动作语义定义 | `motion_engine/core/exercise_semantics.*` | 传感器佩戴、阶段提示、指标与错误语义 |
| 训练运行时 | `RehabMotion_v5/src/rehab/application/rehab_runtime.*` | 处方执行、组次推进、暂停/恢复、断网状态 |
| 产品编排层 | `RehabMotion_v5/src/rehab/application/rehab_system_orchestrator.*` | MotionFrame统一分发到报告、游戏、数字孪生、SD记录与云/离线同步队列 |
| 屏幕协议 | `RehabMotion_v5/src/rehab/protocol/` | 主控↔屏幕命令和事件消息 |
| 训练报告 | `RehabMotion_v5/src/rehab/storage/` | ROM、达标率、动作质量统计与 JSON 汇总 |
| 游戏映射 | `RehabMotion_v5/src/rehab/games/` | 康复动作结果映射到游戏事件 |
| 数字孪生 | `RehabMotion_v5/src/rehab/digital_twin/` | MotionFrame → 数字孪生数据包 |
| SD / 断电续训 | `sd_logger.h`, `power_resume_checkpoint.h` | 本地日志、检查点与恢复 |
| 7 寸屏主流程 | `firmware/display/src/` | 首页、佩戴、定位、校准、训练、反馈、语音、处方同步 |
| 产品页面 | `firmware/display/src/product_ui/` | 20页：训练前检查、佩戴、人体定位、校准、训练、休息、结果、动作库、计划、数字孪生、报告、设备、AI助手、离线同步等 |
| 小程序 | `software/miniapp/` | 计划、训练、报告、设备、AI助手与处方下发界面 |
| 康复师端 | `software/miniapp/pages-therapist/` | 患者趋势、方案确认与下发 |
| 后端服务 | `software/backend/` | 处方、遥测、报告、设备状态与训练参数推荐 |

工程参数会继续通过实机日志做迭代调优；代码模块本身保持统一接口，避免每次调参破坏 UI、协议或数据结构。
