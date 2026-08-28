# RehabMotion 提交版代码实现地图

## 一条完整运行链

```text
WT901BLECL × 5
  → BLE连接与重连
  → 人体方向/重力参考
  → V5MotionSnapshot
  → MotionFrame
  → 8动作 ExerciseDetector
  → RehabRuntime
  → RehabSystemOrchestrator
      ├─ 7寸屏实时状态/质量反馈
      ├─ SD 单次记录与断电检查点
      ├─ GameEvent 游戏触发
      ├─ TwinPacket 数字孪生
      ├─ SessionAccumulator 报告统计
      └─ Cloud/Offline Queue 联网同步
```

## 主控核心入口

- `firmware/controller/RehabMotion_v5/RehabMotion_v5.ino`
  - 五节点连接与主训练循环
  - `SELECT_EXERCISE:0..7` / `SELECT_MODE:0..2`
  - 8动作起始姿态粗门控
  - 校准、正式训练、暂停、恢复、休息、结束
  - 主控 ↔ 7寸屏 UART 实时协议
  - `updateProductApplicationBridge()` 将成熟 V5 实时量接入统一产品管线
- `rehab_v5_action_catalog.h`
  - 8动作名称、传感器链、默认组次、目标角与质量阈值

## 8动作算法

每个动作均有独立 detector 源文件，包含起始条件、阶段推进、ROM、平面、代偿、稳定性/节律等质量评价：

1. `dumbbell_curl.cpp`
2. `triceps_extension.cpp`
3. `scaption_raise.cpp`
4. `wall_crawl.cpp`
5. `knee_flex_extend.cpp`
6. `sit_to_stand.cpp`
7. `box_squat.cpp`
8. `step_up.cpp`

统一接口位于 `motion_engine/core/`，避免 UI、报告、游戏分别实现一套动作判断。

## 7寸屏

- `firmware/display/src/main.cpp`：屏幕启动与实时训练接管
- `firmware/display/src/rehab_uart_link.cpp`：CRC、ACK/重发、LIVE数据解析
- `firmware/display/src/product_ui/rehab_ui_model.h`：统一屏幕数据模型
- `firmware/display/src/product_ui/rehab_product_pages.cpp`：20个产品页
- `firmware/display/src/product_ui/rehab_ui_router.cpp`：主控事件 → 页面/模型状态

## 数据与软件端

- `software/backend/`：处方、单次遥测、周报、推荐、设备状态、同步队列
- `software/miniapp/`：首页、计划、训练、报告、设备、AI建议
- `software/miniapp/pages-therapist/`：患者趋势、方案确认与下发
- `data/schemas/`：处方、MotionFrame、设备状态的结构约束

## 自动检查

执行：

```bash
bash scripts/validate_repo.sh
```

检查 8 个动作 detector、Runtime、Protocol、Report、Game、Digital Twin、Orchestrator、后端逻辑以及主控/屏幕集成结构。
