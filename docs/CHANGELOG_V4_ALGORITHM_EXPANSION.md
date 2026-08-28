# V4 Algorithm Expansion

本轮在 V3 全软件链仓库结构上继续扩展 8 动作算法层：

- 8 个动作从单头文件 detector 拆分为独立 `.h/.cpp` 模块。
- 每个动作加入独立起始姿势规则、动作阶段阈值、ROM/平面/代偿/躯干/速度/稳定性/对称性规则。
- 新增 `RepMetrics`，单次动作全过程累计峰值 ROM、速度、稳定性、平面偏差、躯干倾斜、左右差异、前倾、垂直位移、节律。
- 新增 bitmask `qualityFlags`，支持同一次动作保留多个错误证据，不再只输出一个瞬时错误。
- 新增 8 套动作校准策略 `exercise_calibration.*`。
- 新增滚动特征流水线 `motion_feature_pipeline.*`，输出速度离散度、稳定性与节律特征。
- 新增 `exercise_semantics.*`，统一传感器佩戴、阶段提示、评价指标和 JSON 命名空间。
- 扩展游戏规则：8 个动作分别映射游戏、触发动作、最低质量分与拒绝原因。
- 扩展遥测 JSON：加入 phase、quality_flags 和完整 RepMetrics。
- 主机侧 C++ 检查加入 8 动作校准流程、全过程错误保留、特征流水线检查。
