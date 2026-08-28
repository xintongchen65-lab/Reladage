# RehabMotion 8 动作库

| ID | 动作 | 主要部位 | 主要传感器 | 主要指标 | 常见错误 |
|---|---|---|---|---|---|
| 01 | 哑铃弯举 / 屈肘训练 | 肘关节 | A/B/E | 肘 ROM、动作平面、上臂稳定、躯干稳定 | 幅度不足、上臂代偿、平面偏移、躯干代偿 |
| 02 | 肱三头肌伸展 | 肘关节 | A/B/E | 肘伸展 ROM、上臂稳定、动作平面 | 幅度不足、上臂移动、平面偏移 |
| 03 | 肩胛平面抬手 | 肩关节 | A/B/E | 肩部抬举角、肩胛平面偏差、躯干侧倾 | 抬肩代偿、离开肩胛平面、躯干代偿 |
| 04 | 墙面爬手 | 肩关节 / 功能训练 | A/B/E | 上肢抬举高度、上升稳定性、躯干代偿 | 借力、速度过快、躯干前倾 |
| 05 | 膝关节屈伸 | 膝关节 | C/D/E | 膝 ROM、下肢平面、躯干稳定 | 幅度不足、腿部偏摆、躯干代偿 |
| 06 | 坐到站训练 | 下肢功能 | A/B/C/D/E | 起立相位、膝伸展、躯干前倾、左右差异 | 偏侧负重、过度前倾、不完全站立 |
| 07 | 箱式深蹲 | 下肢功能 | A/B/C/D/E | 下蹲深度、膝/髋控制、躯干稳定 | 深度不足、左右不对称、躯干代偿 |
| 08 | 台阶踩踏 Step Up | 下肢功能 | A/B/C/D/E | 抬腿高度、支撑相位、返回控制、躯干稳定 | 借力、躯干摆动、左右差异、返回不完整 |

## 统一状态机

```text
READY
  ↓ 超过动作激活阈值
ACTIVE
  ↓ 达到目标区
PEAK
  ↓ 开始回程
RETURNING
  ↓ 回到起始阈值
COMPLETED
  ↓
质量评价 + 次数统计 + SD记录 + UI反馈 + 同步队列
```

## 统一质量字段

- `GOOD`
- `ROM_LOW`
- `UPPER_ARM_COMPENSATION`
- `PLANE_DEVIATION`
- `TORSO_COMPENSATION`
- `TOO_FAST`
- `TOO_SLOW`
- `UNSTABLE`
- `ASYMMETRY`
- `START_POSE_INVALID`
- `SENSOR_UNAVAILABLE`

## 统一输出字段

```json
{
  "exercise": "dumbbell_curl",
  "exercise_name": "哑铃弯举 / 屈肘训练",
  "rep_count": 8,
  "rep_completed": true,
  "rep_accepted": true,
  "current_angle_deg": 12.4,
  "peak_rom_deg": 86.2,
  "quality": "GOOD",
  "quality_score": 92.5,
  "warning": "none"
}
```

## 参数组织

每个动作都通过 `ExerciseProfile` 定义：

- 使用 IMU
- 激活角度
- 目标角度
- 回程阈值
- 近端代偿阈值
- 动作平面阈值
- 躯干代偿阈值
- 最大速度
- 稳定性阈值

这样新增动作时无需复制整套训练主循环，只需要补动作 profile 和必要的专用指标映射。

## 独立 Detector 实现

| 动作 | 实现文件 | 专用质量项 |
|---|---|---|
| 哑铃弯举 | `exercises/dumbbell_curl.cpp` | ROM、动作平面、上臂代偿、躯干、节律 |
| 肱三头肌伸展 | `exercises/triceps_extension.cpp` | 伸肘ROM、上臂稳定、平面、躯干、速度 |
| 肩胛平面抬手 | `exercises/scaption_raise.cpp` | 抬举ROM、肩胛平面、耸肩/近端代偿、稳定性 |
| 墙面爬手 | `exercises/wall_crawl.cpp` | 抬举高度、平面、躯干借力、平滑度 |
| 膝关节屈伸 | `exercises/knee_flex_extend.cpp` | 膝ROM、下肢平面、躯干、稳定性、节律 |
| 坐到站 | `exercises/sit_to_stand.cpp` | 起立幅度、前倾、左右差异、稳定性 |
| 箱式深蹲 | `exercises/box_squat.cpp` | 深度、前倾、左右差异、节律 |
| 台阶踩踏 | `exercises/step_up.cpp` | 抬腿/踩踏幅度、躯干、左右差异、步频 |

8个 detector 统一继承 `ExerciseDetector`，因此每个动作都输出同样的 `ExerciseFeedback`，但评分权重和错误判定独立。
