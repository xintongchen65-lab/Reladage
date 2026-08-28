# Motion Engine

统一动作语义层与 8 动作检测引擎。

## 组成

- `core/motion_types.h`：IMU / MotionFrame / ExerciseFeedback 统一数据结构
- `core/exercise_profile.h + exercise_catalog.cpp`：8 动作参数与传感器配置
- `core/exercise_detector.*`：READY → ACTIVE → PEAK → RETURNING 状态机
- `core/exercise_engine.*`：动作选择与检测器生命周期管理
- `core/prescription.h`：处方、组数、次数、目标角度和训练进度结构
- `core/telemetry_schema.*`：统一训练 JSON 输出
- `exercises/`：8 个动作的独立检测器

动作阈值集中在 `exercise_catalog.cpp`，便于后续通过日志和实机数据继续调参，而不需要修改页面和协议代码。
